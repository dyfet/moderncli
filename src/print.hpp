// Copyright (C) 2022 Tycho Softworks.
// This code is licensed under MIT license.

#ifndef TYCHO_PRINT_HPP_
#define TYCHO_PRINT_HPP_

#include "encoding.hpp"

#include <functional>
#include <iostream>
#include <string_view>
#include <mutex>
#include <cstdlib>
#include <cstdio>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) || defined(WIN32)
#ifndef quick_exit
#define quick_exit(x) ::exit(x)         // NOLINT
#define at_quick_exit(x) ::atexit(x)    // NOLINT
#endif
#endif

#if __has_include(<syslog.h>)
#define USE_SYSLOG
#include <syslog.h>
#else
#ifndef LOG_SYSLOG
#define LOG_SYSLOG
constexpr auto LOG_AUTH = 0;
constexpr auto LOG_AUTHPRIV = 0;
constexpr auto LOG_DAEMON = 0;
constexpr auto LOG_EMERG = 0;
constexpr auto LOG_CRIT = 0;
constexpr auto LOG_INFO = 0;
constexpr auto LOG_WARNING = 0;
constexpr auto LOG_NOTICE = 0;
constexpr auto LOG_ERR = 0;
constexpr auto LOG_NDELAY = 0;
constexpr auto LOG_NOWAIT = 0;
constexpr auto LOG_PERROR = 0;
constexpr auto LOG_PID = 0;
#endif
#endif

#if __cplusplus < 202002L
#include <fmt/ranges.h>
#include <fmt/format.h>

namespace tycho {
template<class... Args>
using format_string = const char *;

template<class... Args>
constexpr auto format(format_string<Args...> fmt, Args&&... args) {
    return fmt::format(fmt, std::forward<Args>(args)...);
}

template<class... Args>
constexpr void print(format_string<Args...> fmt, Args&&... args) {
    std::cout << format(fmt, std::forward<Args>(args)...);
}

// Should not be used in new code for post c++17 support
template<class... Args>
constexpr void print(std::ostream& out, format_string<Args...> fmt, Args&&... args) {
    out << format(fmt, std::forward<Args>(args)...);
}

// Should not be used in new code for post c++17 support
template<class... Args>
constexpr void print(FILE *fp, format_string<Args...> fmt, Args&&... args) {
    fputs(format(fmt, std::forward<Args>(args)...).c_str(), fp);
}

// Should not be used in new code for post c++17 support
template<class... Args>
//[[deprecated("use println_to instead")]]
constexpr void println(FILE *fp, format_string<Args...> fmt, Args&&... args) {
    fprintf(fp, "%s\n", format(fmt, std::forward<Args>(args)...).c_str());
}

template<class... Args>
[[noreturn]] constexpr void die(int code, format_string<Args...> fmt, Args&&... args) {
    std::cerr << format(fmt, std::forward<Args>(args)...);
    ::exit(code);
}

template<class... Args>
[[noreturn]] constexpr void crit(int code, format_string<Args...> fmt, Args&&... args) {
    std::cerr << format(fmt, std::forward<Args>(args)...);
    quick_exit(code);
}

class system_logger final {
public:
    using notify_t = std::function<void(const std::string&, const char *)>;

    system_logger() = default;
    system_logger(const system_logger&) = delete;
    auto operator=(const system_logger&) -> auto& = delete;

    template<class... Args>
    void debug(unsigned level, format_string<Args...> fmt, Args&&... args) {
#ifndef NDEBUG
        if(level <= logging_) {
            try {
                auto msg = format(fmt, std::forward<Args>(args)...);
                const std::lock_guard lock(locking_);
                print(std::cerr, "debug: {}\n", msg);
                notify_(msg, "debug");
            }
            catch(const std::exception& e) {
                print(std::cerr, "debug: {}\n", e.what());
                return;
            }
        }
#endif
    }

    template<class... Args>
    void info(format_string<Args...> fmt, Args&&... args) {
        auto msg = format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_INFO, "%s", msg.c_str());
#endif
        notify_(msg, "info");
        if(logging_ > 1)
            print(std::cerr, "info: {}\n", msg);
    }

    template<class... Args>
    void notice(format_string<Args...> fmt, Args&&... args) {
        auto msg = format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_NOTICE, "%s", msg.c_str());
#endif
        notify_(msg, "notice");
        if(logging_ > 0)
            print(std::cerr, "notice: {}\n", msg);
    }

    template<class... Args>
    void warn(format_string<Args...> fmt, Args&&... args) {
        auto msg = format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_WARNING, "%s", msg.c_str());
#endif
        notify_(msg, "warning");
        if(logging_)
            print(std::cerr, "warn: {}\n", msg);
    }

    template<class... Args>
    void error(format_string<Args...> fmt, Args&&... args) {
        auto msg = format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_ERR, "%s", msg.c_str());
#endif
        notify_(msg, "error");
        if(logging_)
            print(std::cerr, "error: {}\n", msg);
    }

    template<class... Args>
    [[noreturn]] void fail(int exit_code, format_string<Args...> fmt, Args&&... args) {
        auto msg = format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_CRIT, "%s", msg.c_str());
#endif
        notify_(msg, "fatal");
        if(logging_)
            print(std::cerr, "fail: {}\n", msg);
        std::cerr << std::ends;
        ::exit(exit_code);
    }

    template<class... Args>
    [[noreturn]] void crit(int exit_code, format_string<Args...> fmt, Args&&... args) {
        auto msg = format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_CRIT, "%s", msg.c_str());
#endif
        notify_(msg, "fatal");
        if(logging_)
            print(std::cerr, "crit: {}\n", msg);
        std::cerr << std::ends;
        quick_exit(exit_code);
    }

    void set(unsigned level, notify_t notify) noexcept {
        logging_ = level;
        notify_ = notify;
    }

    void set(unsigned level) noexcept {
        logging_ = level;
    }

    auto level() const noexcept {
        return logging_;
    }

    void level(unsigned l) noexcept {
        logging_ = l;
    }

#ifdef  USE_SYSLOG
    static void open(const char *id, int level = LOG_INFO, int facility = LOG_DAEMON, int flags = LOG_CONS | LOG_NDELAY) {
        ::openlog(id, flags, facility);
        ::setlogmask(LOG_UPTO(level));
    }

    static void close() {
        ::closelog();
    }
#else
    static void open(const char *id, int level = LOG_INFO, int facility = 0, int flags = 0) {}
    static void close() {}
#endif

private:
    std::mutex locking_;
    unsigned logging_{1};
    notify_t notify_{[](const std::string& str, const char *type){}};
};

// cppcheck-suppress constParameterPointer
inline auto on_crit(void(*handler)()) {
    return at_quick_exit(handler) == 0;
}
} // end namespace
#else
#include <format>

namespace tycho {
template <class... Args>
auto format(const char* fmt, Args&&... args) {
    auto bound = std::make_tuple(std::decay_t<Args>(std::forward<Args>(args))...);
    return std::apply([&](auto&&... unpacked) {
        return std::vformat(fmt, std::make_format_args(unpacked...));
    }, bound);
}

template <class... Args>
void print(const char* fmt, Args&&... args) {
    auto args_tuple = std::make_tuple(std::forward<Args>(args)...);
    std::apply([&](auto&&... unpacked) {
        std::cout << std::vformat(fmt, std::make_format_args(unpacked...));
    }, args_tuple);
}

template<class... Args>
void print(std::ostream& out, const char* fmt, Args&&... args) {
    auto bound = std::make_tuple(std::decay_t<Args>(std::forward<Args>(args))...);
    std::apply([&](auto&&... unpacked) {
        out << std::vformat(fmt, std::make_format_args(unpacked...));
    }, bound);
}

template<class... Args>
void print(FILE* fp, const char* fmt, Args&&... args) {
    auto bound = std::make_tuple(std::decay_t<Args>(std::forward<Args>(args))...);
    std::apply([&](auto&&... unpacked) {
        fputs(std::vformat(fmt, std::make_format_args(unpacked...)).c_str(), fp);
    }, bound);
}


template<class... Args>
[[noreturn]] constexpr void die(int code, std::format_string<Args...> fmt, Args&&... args) {
    std::cerr << std::format(fmt, std::forward<Args>(args)...);
    ::exit(code);
}

template<class... Args>
[[noreturn]] constexpr void crit(int code, std::format_string<Args...> fmt, Args&&... args) {
    std::cerr << std::format(fmt, std::forward<Args>(args)...);
    quick_exit(code);
}

class system_logger final {
public:
    using notify_t = std::function<void(const std::string&, const char *)>;

    system_logger() = default;
    system_logger(const system_logger&) = delete;
    auto operator=(const system_logger&) -> auto& = delete;

    template<class... Args>
    void debug(unsigned level, std::format_string<Args...> fmt, Args&&... args) {
#ifndef NDEBUG
        if(level <= logging_) {
            try {
                auto msg = std::format(fmt, std::forward<Args>(args)...);
                const std::lock_guard lock(locking_);
                std::cerr << std::format("debug: {}\n", msg);
                notify_(msg, "debug");
            }
            catch(const std::exception& e) {
                std::cerr << std::format("debug: {}\n", e.what());
                return;
            }
        }
#endif
    }

    template<class... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_INFO, "%s", msg.c_str());
#endif
        notify_(msg, "info");
        if(logging_ > 1)
            std::cerr << std::format( "info: {}\n", msg);
    }

    template<class... Args>
    void notice(std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_NOTICE, "%s", msg.c_str());
#endif
        notify_(msg, "notice");
        if(logging_ > 0)
            std::cerr << std::format("notice: {}\n", msg);
    }

    template<class... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_WARNING, "%s", msg.c_str());
#endif
        notify_(msg, "warning");
        if(logging_)
            std::cerr << std::format("warn: {}\n", msg);
    }

    template<class... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_ERR, "%s", msg.c_str());
#endif
        notify_(msg, "error");
        if(logging_)
            std::cerr << std::format("error: {}\n", msg);
    }

    template<class... Args>
    [[noreturn]] void fail(int exit_code, std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_CRIT, "%s", msg.c_str());
#endif
        notify_(msg, "fatal");
        if(logging_)
            std::cerr << std::format("fail: {}\n", msg);
        std::cerr << std::ends;
        ::exit(exit_code);
    }

    template<class... Args>
    [[noreturn]] void crit(int exit_code, std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::format(fmt, std::forward<Args>(args)...);
        const std::lock_guard lock(locking_);
#ifdef  USE_SYSLOG
        ::syslog(LOG_CRIT, "%s", msg.c_str());
#endif
        notify_(msg, "fatal");
        if(logging_)
            std::cerr << std::format("crit: {}\n", msg);
        std::cerr << std::ends;
        quick_exit(exit_code);
    }

    void set(unsigned level, notify_t notify = [](const std::string& str, const char *type){}) noexcept {
        logging_ = level;
        notify_ = notify;
    }

    void set(unsigned level) noexcept {
        logging_ = level;
    }

    auto level() const noexcept {
        return logging_;
    }

    void level(unsigned l) noexcept {
        logging_ = l;
    }

#ifdef  USE_SYSLOG
    static void open(const char *id, int level = LOG_INFO, int facility = LOG_DAEMON, int flags = LOG_CONS | LOG_NDELAY) {
        ::openlog(id, flags, facility);
        ::setlogmask(LOG_UPTO(level));
    }

    static void close() {
        ::closelog();
    }
#else
    static void open(const char *id, int level, int facility, int flags) {}
    static void close() {}
#endif

private:
    std::mutex locking_;
    unsigned logging_{1};
    notify_t notify_{[](const std::string& str, const char *type){}};
};

// cppcheck-suppress constParameterPointer
inline auto on_crit(void(*handler)()) {
    return at_quick_exit(handler) == 0;
}
} // end namespace

#endif
#endif
