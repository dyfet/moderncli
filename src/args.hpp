// Copyright (C) 2020 Tycho Softworks.
// This code is licensed under MIT license.

#ifndef TYCHO_ARGS_HPP_
#define TYCHO_ARGS_HPP_

#include <string>
#include <string_view>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <cstdlib>

namespace { // NOLINT   only use in mains...
class bad_arg final : public std::runtime_error {
public:
    explicit bad_arg(const std::string& text) noexcept :
    std::runtime_error(text) {}

private:
    friend class args;

    static auto format(std::string text, std::string_view value) {
        text += " ";
        if(value[0] != '+' && value[0] != '-')
            text += "--";

        text += value;
        return bad_arg(text);
    }

    static auto format(const std::string& text, char code) {
        return bad_arg(text + " -" + code);
    }

    static auto str_size(const char *cp, std::size_t max = 255) {
        std::size_t count = 0;
        while(cp && *cp && count++ <= max)
            ++cp;

        if(count > max) throw bad_arg::format("string size too large", cp);
        return count;
    }

    static auto str_value(const char *cp) {
        char *ep = nullptr;
        auto value = strtol(cp, &ep, 0);
        if(ep && *ep) throw bad_arg::format("value format wrong", cp);
        return value;
    }
};

class args final {
public:
    using text_t = std::initializer_list<std::string_view>;
    using char_p = const char *;

    struct value_t {
        value_t() = delete;
        value_t(const value_t&) = delete;

        explicit value_t(bool f) : index(0), flag(f) {}
        explicit value_t(unsigned n) : index(1), num(n) {}
        explicit value_t(const char *s) : index(2), str(s) {}

        auto operator=(const value_t&) -> auto& = delete;

        auto operator=(bool f) -> auto& {
            index = 0;
            flag = f;
            return *this;
        }

        auto operator=(unsigned n) -> auto& {
            index = 1;
            num = n;
            return *this;
        }

        auto operator=(const char *s) -> auto& {
            index = 2;
            str = s;
            return *this;
        }

        unsigned index;
        bool flag{false};
        const char *str{nullptr};
        unsigned num{0};
    };

    struct option final {
    friend class args;
        explicit option(char_p help) noexcept :
        value_(false), code_(0), counter_(false), usage_(nullptr), id_(nullptr), help_(help), next_(nullptr) {
            add_option();
        }

        option(char ch, char_p id, char_p help, bool counter = false) noexcept :
        value_(false), code_(ch), counter_(counter), usage_(nullptr), id_(id), help_(help), next_(nullptr) {
            add_option();
        }

        option(char ch, char_p id, char_p help, char_p usage) noexcept :
        value_(false), code_(ch), counter_(false), usage_(usage), id_(id), help_(help), next_(nullptr) {
            add_option();
        }

        option(const option&) = delete;
        auto operator=(const option&) -> auto& = delete;

        explicit operator bool() const {
            return value_.index > 0 || value_.flag;
        }

        auto operator *() const {
            return (value_.index < 2) ? nullptr : value_.str;
        }

        auto operator!() const {
            return value_.index < 1 && !value_.flag;
        }

        auto count() const {
            return (value_.index > 0 || value_.flag) ? value_.num : 0U;
        }

        auto value(long default_value = 0) const {
            return value_.index == 1 ? value_.num : default_value;
        }

        auto string() const {
            return value_.index > 1 ? std::string_view(value_.str) : std::string_view();
        }

        void set(long value) {
            if(value_.index < 1)
                value_.flag = value > 0;
            if(value_.index < 2) {
                value_.num = value;
            }
        }

        void set(const char *from) {
            if(!usage_) throw bad_arg("usage missing");
            value_ = from;
        }

        void set(const std::string& str) {
            if(!usage_) throw bad_arg("usage missing");
            value_ = str.c_str();
        }

        void set_if_unset(long value) {
            if(value_.index < 1 && !value_.flag) {
                value_.flag = value > 0;
                value_.num = value;
            }
        }

        void set_if_unset(const char *from) {
            if(!usage_) throw bad_arg("usage missing");
            if(value_.index < 1)
                value_ = from;
        }

        void set_if_unset(const std::string& str) {
            if(!usage_) throw bad_arg("usage missing");
            if(value_.index < 1)
                value_ = str.c_str();
        }

    private:
        value_t value_;
        char code_;
        bool counter_;
        char_p usage_, id_, help_;
        option *next_;

        void add_option() {
            if(last_) {
                last_->next_ = this;
                last_ = this;
            }
            else
                first_ = last_ = this;

            if(id_) {
                if(id_[0] == '-')
                    ++id_;
                if(id_[0] == '-')
                    ++id_;
            }
        }

        inline static option *first_ = nullptr;
        inline static option *last_ = nullptr;
    };

    static auto argv0() -> const auto& {
        return argv0_;
    }

    static auto count() {
        return argv_.size();
    }

    static auto list() -> const auto& {
        return argv_;
    }

    static auto parse([[maybe_unused]] int argc, const char **list) {
        if(list == nullptr || list[0] == nullptr) throw bad_arg("arguments missing");
        argv0_ = *(list++);
        while(*list) {
            auto arg = std::string_view(*(list++));
            if(arg == "--" || arg == "-") break;
            if(arg[0] != '-') {
                --list;
                break;
            }

            auto has_short = true;
            if(arg[1] == '-') {
                has_short = false;
                arg.remove_prefix(2);
            }
            else
                arg.remove_prefix(1);

            auto key_split = arg.find('=');
            auto keyword = arg;
            if(key_split != std::string_view::npos)
                keyword = arg.substr(0, key_split);

            auto op = option::first_;
            while(op) {
                if(!op->id_ || keyword != op->id_) {
                    op = op->next_;
                    continue;
                }

                if(op->counter_) {
                    if(op->value_.index < 1)
                        op->value_ = 1U;
                    else
                        op->value_ = op->count() + 1U;
                    break;
                }

                if(op->value_.index > 0 || op->value_.flag) throw bad_arg::format("already used", keyword);
                if(!op->usage_) {
                    if(key_split != std::string_view::npos) throw bad_arg::format("invalid value", keyword);

                    op->value_ = true;
                    break;
                }

                if(key_split != std::string_view::npos) {
                    op->value_ = arg.substr(key_split + 1).data();
                    break;
                }

                if(!*list) throw bad_arg::format("missing value", keyword);
                op->value_ = *(list++);
                break;
            }

            if(static_cast<bool>(op)) continue;
            if(!has_short) throw bad_arg::format("unknown argument", keyword);
            auto pos = 0U;
            while(pos < arg.size()) {
                op = option::first_;
                while(op) {
                    if(op->code_ == arg[pos]) break;
                    op = op->next_;
                }

                if(!op) throw bad_arg::format("unknown option", arg[pos]);
                if(op->counter_) {
                    if(op->value_.index < 1)
                        op->value_ = 1U;
                    else
                        op->value_ = op->count() + 1U;
                    ++pos;
                    continue;
                }

                if(op->value_.index > 0 || op->value_.flag) throw bad_arg::format("already used", arg[pos]);
                if(!op->usage_)
                    op->value_.flag = true;
                else {
                    if(!*list) throw bad_arg::format("missing value", arg[pos]);
                    op->value_ = *(list++);
                }
                ++pos;
            }
        }

        while(*list)
            argv_.emplace_back(*(list++));

        return count();
    }

    static void help(text_t usage, text_t describe) {
        auto op = option::first_;
        auto *prefix = "Usage: ";

        for(const auto& item : describe) {
            std::cout << item << '\n';
        }

        std::cout << '\n';
        for(const auto& item : usage) {
            std::cout << prefix << item << '\n';
            prefix = "       ";
        }

        std::cout << "\nOptions:\n";
        while(op != nullptr) {
            if(!op->help_) {
                op = op->next_;
                continue;
            }

            std::size_t hp = 0;
            if(op->code_ && op->id_) {
                std::cout << "  -" << op->code_ << ", ";
                hp = 6;
            } else if(op->id_) {
                std::cout << "  ";
                hp = 2;
            } else if(op->usage_) {
                std::cout << "  -" << op->code_ << " " << op->usage_;
                hp = 5 + bad_arg::str_size(op->usage_);
            } else if(op->code_) {
                std::cout << "  -" << op->code_;
                hp = 5;
            } else {      // grouping separator
                std::cout << "\n" << op->help_ << ":\n";
                op = op->next_;
                continue;
            }

            if(op->id_ && op->usage_) {
                std::cout << "--" << op->id_ << "=" << op->usage_;
                hp += bad_arg::str_size(op->id_, 64) + bad_arg::str_size(op->usage_) + 3;
            } else if(op->id_) {
                std::cout << "--" << op->id_;
                hp += bad_arg::str_size(op->id_, 64) + 2;
            }
            else
                --hp;

            if(hp > 29) {
                std::cout << '\n';
                hp = 0;
            }
            while(hp < 30) {
                std::cout << ' ';
                ++hp;
            }

            const char *hs = op->help_;
            while(*hs) {
                if(*hs == '\n' || (((*hs == ' ' || *hs == '\t')) && (hp > 75))) {
                    std::cout << '\n' << "                              ";
                    hp = 30;
                }
                else if(*hs == '\t') {
                    if(!(hp % 8)) {
                        std::cout << ' ';
                        ++hp;
                    }
                    while(hp % 8) {
                        std::cout << ' ';
                        ++hp;
                    }
                }
                else
                    std::cout << *hs;
                ++hs;
            }

            std::cout << '\n';
            op = op->next_;
        }
    }

private:
    inline static std::vector<std::string_view> argv_;
    inline static std::string_view argv0_;
};
} // end namespace
#endif
