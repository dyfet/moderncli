// Copyright (C) 2023 Tycho Softworks.
// This code is licensed under MIT license.

#ifndef TYCHO_SLICE_HPP_
#define TYCHO_SLICE_HPP_

#include <list>
#include <memory>
#include <iterator>
#include <initializer_list>
#include <algorithm>
#include <stdexcept>

namespace tycho {
template<typename T>
class slice {
public:
    using value_type = T;
    using size_type = std::size_t;
    using pointer = typename std::shared_ptr<T>;
    using reference = T&;
    using const_reference = const T&;
    using iterator = typename std::list<std::shared_ptr<T>>::iterator;
    using const_iterator = typename std::list<std::shared_ptr<T>>::const_iterator;
    using reverse_iterator = typename std::list<std::shared_ptr<T>>::reverse_iterator;
    using const_reverse_iterator = typename std::list<std::shared_ptr<T>>::const_reverse_iterator;

    slice() = default;
    explicit slice(const std::list<std::shared_ptr<T>>& from) : list_(from) {}

    slice(std::initializer_list<T> init) {
        for(const auto& item : init) {
            assign(item);
        }
    }

    operator bool() const {
        return !empty();
    }

    auto operator!() const {
        return empty();
    }

    auto operator=(const std::list<std::shared_ptr<T>>& from) -> auto& {
        list_ = from;
        return *this;
    }

    auto operator()(size_type index) const {
        return at(index);
    }

    auto operator[](size_type index) const -> T& {
        if(index >= list_.size())
            throw std::out_of_range("Index out of range");
        auto it = list_.begin();
        std::advance(it, index);
        return **it;
    }

    auto operator*() const noexcept -> std::list<T>& {
        return list_;
    }

    auto operator*() noexcept -> std::list<T>& {
        return list_;
    }

    auto operator+=(const slice<T>& other) -> auto& {
        append(other);
        return *this;
    }

    auto operator+=(const T& item) -> auto& {
        append(item);
        return *this;
    }

    auto operator<<(const T& item) -> auto& {
        append(item);
        return *this;
    }

    auto size() const {
        return list_.size();
    }

    auto max_size() const {
        return list_.max_size();
    }

    auto at(size_type index) const {
        if(index >= list_.size())
            throw std::out_of_range("Index out of range");
        auto it = list_.begin();
        std::advance(it, index);
        return *it;
    }

    template<typename Func>
    auto each(Func func) const {
        for(const auto ptr : list_) {
            func(*ptr);
        }
    }

    auto empty() const {
        return list_.empty();
    }

    auto begin() noexcept {
        return list_.begin();
    }

    auto begin() const noexcept {
        return list_.cbegin();
    }

    auto rbegin() noexcept {
        return list_.rbegin();
    }

    auto rbegin() const noexcept {
        return list_.crbegin();
    }

    auto end() noexcept {
        return list_.end();
    }

    auto end() const noexcept {
        return list_.cend();
    }

    auto rend() noexcept {
        return list_.rend();
    }

    auto rend() const noexcept {
        return list_.crend();
    }

    void assign(const T& item) {
        list_.clear();
        list_.push_back(std::make_shared<T>(item));
    }

    void assign(const slice<T>& other) {
        list_.clear();
        std::copy(other.list_.begin(), other.list_.end(), std::back_inserter(list_));
    }

    void assign(const std::list<std::shared_ptr<T>>& from) {
        list_ = from;
    }

    void assign(std::initializer_list<T> items) {
        list_.clear();
        for(const auto& item : items) {
            append(item);
        }
    }

    void append(const T& item) {
        list_.push_back(std::make_shared<T>(item));
    }

    void append(const slice<T>& other) {
        std::copy(other.list_.begin(), other.list_.end(), std::back_inserter(list_));
    }

    void append(std::initializer_list<T> items) {
        for(const auto& item : items) {
            append(item);
        }
    }

    void prepend(const T& item) {
        list_.push_front(std::make_shared<T>(item));
    }

    void prepend(const slice<T>& other) {
        std::copy(other.list_.rbegin(), other.list_.rend(), std::front_inserter(list_));
    }

    auto insert(iterator it, const T& value) {
        return list_.insert(it, std::make_shared<T>(value));
    }

    auto erase(iterator it) {
        return list_.erase(it);
    }

    auto erase(iterator start, iterator end) {
        return list_.erase(start, end);
    }

    void erase(size_type start, size_type end) {
        if (start >= list_.size() || end > list_.size() || start > end)
            throw std::out_of_range("Invalid range");
        auto it_start = list_.begin();
        auto it_end = list_.begin();
        std::advance(it_start, start);
        std::advance(it_end, end);
        list_.erase(it_start, it_end);
    }

    void clear() {
        list_.clear();
    }

    void resize(size_type count) {
        list_.resize(count);
    }

    void swap(const slice<T>& other) noexcept {
        list_.swap(other.list_);
    }

    void remove(const T& value) {
        list_.remove_if([&](const std::shared_ptr<T>& item) {
            return *item == value;
        });
    }

    template<typename Predicate>
    void remove_if(Predicate pred) {
        list_.remove_if([&](const std::shared_ptr<T>& item) {
            return pred(*item);
        });
    }

    void copy(const slice<T>& other, size_type pos = 0) {
        if(pos > list_.size())
            throw std::out_of_range("Copy position out of range");
        auto it = list_.begin();
        std::advance(it, pos);
        std::copy(other.list_.begin(), other.list_.end(), std::inserter(list_, it));
    }

    auto clone(size_type start, size_type last) const {
        if(start >= list_.size() || last > list_.size() || start > last)
            throw std::out_of_range("Invalid subslice range");
        slice<T>  result;
        auto it = list_.begin();
        std::advance(it, start);
        for(auto i = start; i < last; ++i) {
            result.add(**it++);
        }
        return result;
    }

    auto clone(size_type start = 0) {
        return clone(start, list_.size());
    }

    auto subslice(size_type start, size_type last) const {
        if(start >= list_.size() || last > list_.size() || start > last)
            throw std::out_of_range("Invalid subslice range");
        slice<T>  result;
        auto it = list_.begin();
        std::advance(it, start);
        for(auto i = start; i < last; ++i) {
            result.list_.push_back(*it++);
        }
        return result;
    }

    auto subslice(iterator start_it, iterator end_it) const {
        if(std::distance(list_.begin(), start_it) > std::distance(start_it, end_it) || std::distance(list_.begin(), end_it) > list_.size())
            throw std::out_of_range("Invalid slice iterator range");
        slice<T> result;
        std::copy(start_it, end_it, std::back_inserter(result.list_));
        return result;
    }

private:
    std::list<std::shared_ptr<T>> list_;
};
} // end namespace
#endif
