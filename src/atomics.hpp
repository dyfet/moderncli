// Copyright (C) 2023 Tycho Softworks.
// This code is licensed under MIT license.

#ifndef TYCHO_ATOMICS_HPP_
#define TYCHO_ATOMICS_HPP_

#include <atomic>
#include <optional>
#include <type_traits>
#include <stdexcept>
#include <list>

#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedOr)
#pragma intrinsic(_InterlockedExchange64)
#pragma intrinsic(_InterlockedExchangeAdd64)
#pragma intrinsic(_InterlockedCompareExchange64)
#endif

namespace tycho::atomic {
template<typename T = unsigned>
class sequence_t final {
public:
    sequence_t() noexcept = default;

    explicit sequence_t(T initial) noexcept : seq_(initial) {};

    sequence_t(const sequence_t& from) noexcept {
        seq_.store(from.seq_.load(std::memory_order_relaxed), std::memory_order_release);
    }

    auto operator=(const sequence_t& from) noexcept -> auto& {
        if(&from != this)
            seq_.store(from.seq_.load(std::memory_order_relaxed), std::memory_order_release);
        return *this;
    }

    auto operator*() noexcept {
        return seq_.fetch_add(1);
    }

    auto operator=(const T v) noexcept -> auto& {
        seq_.store(v, std::memory_order_relaxed);
        return *this;
    }

    operator T() const noexcept {
        return seq_.fetch_add(1);
    }

    void set(const T v) noexcept {
        seq_.store(v, std::memory_order_relaxed);
    }

    auto is_lock_free() const noexcept {
        return seq_.is_lock_free();
    }

private:
    static_assert(std::is_unsigned_v<T>, "T must be unsigned numeric");

    mutable std::atomic<T> seq_{0};
};

class once_t final {
public:
    once_t() = default;
    once_t(const once_t&) = delete;
    auto operator=(const once_t&) = delete;

    explicit operator bool() const noexcept {
        return flag_.exchange(false);
    }

    auto operator!() const noexcept {
        return !flag_.exchange(false);
    }

    void reset() noexcept {
        flag_.store(true, std::memory_order_release);
    }
private:
    mutable std::atomic<bool> flag_{true};
};

template<typename T, std::size_t S>
class stack_t final {
public:
    stack_t() = default;
    stack_t(const stack_t&) = delete;
    auto operator=(const stack_t&) -> auto& = delete;

    explicit operator bool() const noexcept {
        return count_.load() > 0;
    }

    auto operator!() const noexcept {
        return count_.load() < 1;
    }

    auto operator*() noexcept {
        return pop();
    }

    auto operator<=(const T& item) {
        return push(item);
    }

    auto size() const noexcept -> std::size_t {
        auto count = count_.load();
        if(count < 0) return std::size_t(0);
        if(count > S) return S;
        return count;
    }

    auto empty() const noexcept {
        return count_.load() < 1;
    }

    auto full() const noexcept {
        return count_.load() >= S;
    }

    auto push(const T& item) noexcept {
        const auto count = count_.fetch_add(1);
        if(count >= S) {
            count_.fetch_sub(1);
            return false;
        }
        data_[count] = item;
        return true;
    }

    auto pull(T& item) noexcept {
        const auto count = count_.fetch_sub(1);
        if(count < 0) {
            count_.fetch_add(1);
            return false;
        }
        item = data_[count];
        return true;
    }

    auto pop() noexcept -> std::optional<T> {
        const auto count = count_.fetch_sub(1);
        if(count < 0) {
            count_.fetch_add(1);
            return {};
        }
        return data_[count];
    }

private:
    static_assert(S > 2, "Queue size must be bigger than 2");

    std::atomic<int> count_{0};
    T data_[S];
};

template<typename T, std::size_t S>
class buffer_t final {
public:
    buffer_t() = default;
    buffer_t(const buffer_t&) = delete;
    auto operator=(const buffer_t&) -> auto& = delete;

    explicit operator bool() const noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto tail = tail_.load(std::memory_order_relaxed);
        return head != tail;
    }

    auto operator!() const noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto tail = tail_.load(std::memory_order_relaxed);
        return head == tail;
    }

    auto operator*() noexcept {
        return pop();
    }

    auto operator<=(const T& item) noexcept {
        return push(item);
    }

    auto empty() const noexcept {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    auto full() noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        auto next = tail;
        if(++next >= S)
            next -= S;

        return next == head_.load(std::memory_order_acquire);
    }

    auto push(const T& item) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        auto next = tail;
        if(++next >= S)
            next -= S;

        const auto head = head_.load(std::memory_order_acquire);
        if(next == head) return false;
        data_[tail] = item;
        tail_.store(next, std::memory_order_release);
        return true;
    }

    auto pull(T& item) noexcept {
        auto head = head_.load(std::memory_order_relaxed);
        const auto tail = tail_.load(std::memory_order_acquire);
        if(head == tail) return false;
        item = data_[head];
        if(++head >= S)
            head -= S;

        tail_.store(tail, std::memory_order_release);
        return true;
    }

    auto pop() noexcept -> std::optional<T> {
        auto head = head_.load(std::memory_order_relaxed);
        const auto tail = tail_.load(std::memory_order_acquire);
        if(head == tail) return {};
        auto item = data_[head];
        if(++head >= S)
            head -= S;

        tail_.store(tail, std::memory_order_release);
        return head;
    }

private:
    static_assert(S > 2, "Queue size must be bigger than 2");

    std::atomic<unsigned> head_{0U}, tail_{0U};
    T data_[S];
};

template<typename K, typename V, std::size_t S = 16>
class dictionary_t {
public:
    dictionary_t(const dictionary_t&) = delete;
    auto operator=(const dictionary_t&) -> auto& = delete;

    dictionary_t() {
        for(auto& bucket : table_) {
            bucket.store(nullptr);
        }
    }

    dictionary_t(dictionary_t&& other) noexcept :
    table_(std::move(other.table_)), count_(other.count_.load()) {
        for(auto& bucket : table_) {
            bucket.store(nullptr);
        }
        other.count_.store(0);
    }

    auto operator=(dictionary_t&& other) noexcept -> auto& {
        if(this != &other) {
            clear();
            table_ = std::move(other.table_);
            count_.store(other.count_.load());
            other.count_.store(0);
            for(auto& bucket : table_) {
                bucket.store(nullptr);
            }
        }
        return *this;
    }

    explicit operator bool() const noexcept {
        return count_.load() > 0;
    }

    auto operator!() const noexcept {
        return count_.load() == 0;
    }

    auto operator[](const K& key) const -> const V& {
        at(key);
    }

    auto operator[](const K& key) -> V& {
        at(key);
    }

    void clear() {
        for(auto& bucket : table_) {
            auto current = bucket.exchange(nullptr);
            while(current != nullptr) {
                auto next = current->next.load();
                delete current;
                current = next;
            }
        }
        count_.store(0, std::memory_order_relaxed);
    }

    auto insert(const K& key, const V& value) {
        auto index = key_index(key);
        auto made = new node(key, value);
        node* expected = nullptr;

        do {    // NOLINT
            expected = table_[index].load();
            made->next.store(expected);
        } while(!table_[index].compare_exchange_weak(expected, made));

        count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    auto insert_or_assign(const K& key, const V& value) {
        auto index = key_index(key);
        auto made = new node(key, value);
        auto current = table_[index].load();

        while(current != nullptr) {
            if(current->key == key) {
                current->value = value;
                delete made;
                return true;
            }
            current = current->next.load();
        }

        delete made;
        insert(key, value);
        return true;
    }

    auto emplace(K&& key, V&& value) {
        auto index = key_index(key);
        auto made = new node(std::move(key), std::move(value));
        node* expected = nullptr;

        do {    // NOLINT
            expected = table_[index].load();
            made->next.store(expected);
        } while(!table_[index].compare_exchange_weak(expected, made));

        count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    auto try_emplace(K&& key, V&& value) {
        auto index = key_index(key);
        auto made = new node(std::move(key), std::move(value));
        auto current = table_[index].load();

        while(current != nullptr) {
            if(current->key == key) {
                delete made;
                return false;
            }
            current = current->next.load();
        }

        emplace(std::move(made->key), std::move(made->value));
        delete made;
        return true;
    }

    auto find(const K& key) const -> std::optional<V> {
        auto index = key_index(key);
        auto current = table_[index].load();
        while(current != nullptr) {
            if(current->key == key) return current->value;
            current = current->next.load();
        }
        return std::nullopt;
    }

    auto contains(const K& key) const {
        auto index = key_index(key);
        auto current = table_[index].load();

        while(current != nullptr) {
            if(current->key == key) return true;
            current = current->next.load();
        }
        return false;
    }

    auto at(const K& key) const -> const V& {
        auto index = key_index(key);
        auto current = table_[index].load();
        while(current != nullptr) {
            if(current->key == key) return current->value;
            current = current->next.load();
        }
        throw std::out_of_range("Key not in dictionary");
    }

    auto at(const K& key) -> V& {
        auto index = key_index(key);
        auto current = table_[index].load();
        while(current != nullptr) {
            if(current->key == key) return current->value;
            current = current->next.load();
        }
        throw std::out_of_range("Key not in dictionary");
    }

    auto remove(const K& key) {
        auto index = key_index(key);
        auto current = table_[index].load();
        node* prev = nullptr;

        while(current != nullptr) {
            if(current->key == key) {
                auto next = current->next.load();
                if(prev != nullptr)
                    prev->next.store(next);
                else
                    table_[index].store(next);
                delete current;
                count_.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }
            prev = current;
            current = current->next.load();
        }
        return false;
    }

    auto empty() const noexcept {
        return count_.load() == 0;
    }

    auto size() const noexcept {
        return count_.load();
    }

    auto keys() const {
        std::list<K> list;
        for(auto& bucket : table_) {
            auto current = bucket.load();
            while(current != nullptr) {
                list.push_back(current->key);
                current = current->next.load();
            }
        }
        return list;
    }

    template<typename Func>
    void each(Func func) {
        for(auto& bucket : table_) {
            auto current = bucket.load();
            while(current != nullptr) {
                func(current->key, current->value);
                current = current->next.load();
            }
        }
    }

private:
    struct node {
        K key;
        V value;
        std::atomic<node *> next{nullptr};

        node(const K& k, const V& v) : key(k), value(v) {}
        node(K&& k, V&& v) : key(std::move(k)), value(std::move(v)) {}
    };

    std::atomic<node*> table_[S];
    std::atomic<std::size_t> count_{0};

    auto key_index(const K& key) const -> std::size_t {
        return std::hash<K>()(key) % S;
    }
};
} // end namespace

namespace tycho {
template<typename T=int>
class atomic_ref {
public:
    atomic_ref() = delete;
    atomic_ref(const atomic_ref&) = delete;
    auto operator=(const atomic_ref&) -> auto& = delete;

    explicit atomic_ref(T& value) noexcept : ref(value) {}

    auto operator++() const noexcept {
        return fetch_add(1) + 1; // Pre-increment
    }

    auto operator++(int) const noexcept {
        return fetch_add(1);     // Post-increment
    }

    auto operator--() const noexcept {
        return fetch_sub(1) - 1; // Pre-decrement
    }

    auto operator--(int) const noexcept {
        return fetch_sub(1);     // Post-decrement
    }

    auto operator=(const T& value) const noexcept -> auto& {
        store(value);
        return *this;
    }

    auto operator+=(const T& value) const noexcept -> auto& {
        fetch_add(value);
        return *this;
    }

    auto operator-=(const T& value) const noexcept -> auto& {
        fetch_sub(value);
        return *this;
    }

    operator T() const noexcept {
        return load();
    }

    explicit operator bool() const noexcept {
        return load() != 0;
    }

    auto operator!() const noexcept {
        return load() == 0;
    }

    auto operator==(const T& other) const noexcept {
        return load() == other;
    }

    auto operator!=(const T& other) const noexcept {
        return load() != other;
    }

    auto operator<(const T& other) const noexcept {
        return load() < other;
    }

    auto operator<=(const T& other) const noexcept {
        return load() <= other;
    }

    auto operator>(const T& other) const noexcept {
        return load() > other;
    }

    auto operator>=(const T& other) const noexcept {
        return load() >= other;
    }

    void store(T value) const noexcept {
#if defined(_MSC_VER)
        if constexpr (sizeof(T) == 4)
            _InterlockedExchange(reinterpret_cast<volatile long*>(&ref), static_cast<long>(value));
        else if constexpr (sizeof(T) == 8)
            _InterlockedExchange64(reinterpret_cast<volatile __int64*>(&ref), static_cast<__int64>(value));
        else
            ref = value; // fallback for 1- or 2-byte types (non-atomic)
#else
        __atomic_store_n(&ref, value, __ATOMIC_SEQ_CST);
#endif
    }

    auto load() const noexcept -> T {
#if defined(_MSC_VER)
        if constexpr (sizeof(T) == 4)
            return static_cast<T>(_InterlockedOr(reinterpret_cast<volatile long*>(&ref), 0));
        else if constexpr (sizeof(T) == 8)
            return static_cast<T>(_InterlockedOr64(reinterpret_cast<volatile __int64*>(&ref), 0));
        else
            return ref;
#else
        return __atomic_load_n(&ref, __ATOMIC_SEQ_CST);
#endif
    }

    auto exchange(T value) const noexcept -> T {
#if defined(_MSC_VER)
        if constexpr (sizeof(T) == 4)
            return static_cast<T>(_InterlockedExchange(reinterpret_cast<volatile long*>(&ref), static_cast<long>(value)));
        else if constexpr (sizeof(T) == 8)
            return static_cast<T>(_InterlockedExchange64(reinterpret_cast<volatile __int64*>(&ref), static_cast<__int64>(value)));
        else {
            T old = ref;
            ref = value;
            return old;
        }
#else
        return __atomic_exchange_n(&ref, value, __ATOMIC_SEQ_CST);
#endif
    }

    auto compare_exchange_strong(T& expected, T value) const noexcept {
#if defined(_MSC_VER)
        if constexpr (sizeof(T) == 4) {
            long original = _InterlockedCompareExchange(reinterpret_cast<volatile long*>(&ref), static_cast<long>(value), static_cast<long>(expected));
            if (original == static_cast<long>(expected)) return true;
            expected = static_cast<T>(original);
            return false;
        }
        else if constexpr (sizeof(T) == 8) {
            __int64 original = _InterlockedCompareExchange64(reinterpret_cast<volatile __int64*>(&ref), static_cast<__int64>(value), static_cast<__int64>(expected));
            if (original == static_cast<__int64>(expected)) return true;
            expected = static_cast<T>(original);
            return false;
        }
        else {
            if (ref == expected) {
                ref = value;
                return true;
            }
            expected = ref;
            return false;
        }
#else
        return __atomic_compare_exchange_n(&ref, &expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
    }

    auto fetch_add(T arg) const noexcept -> T {
#if defined(_MSC_VER)
        if constexpr (sizeof(T) == 4)
            return static_cast<T>(_InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&ref), static_cast<long>(arg)));
        else if constexpr (sizeof(T) == 8)
            return static_cast<T>(_InterlockedExchangeAdd64(reinterpret_cast<volatile __int64*>(&ref), static_cast<__int64>(arg)));
        else {
            T old = ref;
            ref += arg;
            return old;
        }
#else
        return __atomic_fetch_add(&ref, arg, __ATOMIC_SEQ_CST);
#endif
    }

    auto fetch_sub(T arg) const noexcept -> T {
#if defined(_MSC_VER)
        if constexpr (sizeof(T) == 4)
            return static_cast<T>(_InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&ref), -static_cast<long>(arg)));
        else if constexpr (sizeof(T) == 8)
            return static_cast<T>(_InterlockedExchangeAdd64(reinterpret_cast<volatile __int64*>(&ref), -static_cast<__int64>(arg)));
        else {
            T old = ref;
            ref -= arg;
            return old;
        }
#else
        return __atomic_fetch_sub(&ref, arg, __ATOMIC_SEQ_CST);
#endif
    }

private:
    static_assert(std::is_integral_v<T> && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8), "T must be integral of supported size");

    T& ref;
};
} // end namespace
#endif
