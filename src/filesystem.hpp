// Copyright (C) 2022 Tycho Softworks.
// This code is licensed under MIT license.

#ifndef TYCHO_FILESYSTEM_HPP_
#define TYCHO_FILESYSTEM_HPP_

#include <filesystem>
#include <type_traits>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdlib>

#include <sys/types.h>
#include <fcntl.h>

#ifndef _MSC_VER
#include <unistd.h>
#else
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#endif

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) || defined(WIN32)
#if _WIN32_WINNT < 0x0600 && !defined(_MSC_VER)
#undef  _WIN32_WINNT
#define _WIN32_WINNT    0x0600  // NOLINT
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include <io.h>
#else
#include <sys/mman.h>
#endif

#if defined(__OpenBSD__)
#define stat64 stat     // NOLINT
#define fstat64 fstat   // NOLINT
#endif

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) || defined(WIN32)
inline auto getline_w32(char **lp, std::size_t *size, FILE *fp) -> ssize_t {
    std::size_t pos{0};
    int c{EOF};

    if(lp == nullptr || fp == nullptr || size == nullptr) return -1;
    c = getc(fp);   // FlawFinder: ignore
    if(c == EOF) return -1;

    if(*lp == nullptr) {
        *lp = static_cast<char *>(std::malloc(128)); // NOLINT
        if(*lp == nullptr) return -1;
        *size = 128;
    }

    pos = 0;
    while(c != EOF) {
        if (pos + 1 >= *size) {
            auto new_size = *size + (*size >> 2);
            if(new_size < 128) {
                new_size = 128;
            }
            auto new_ptr = static_cast<char *>(realloc(*lp, new_size)); // NOLINT
            if(new_ptr == nullptr) return -1;
            *size = new_size;
            *lp = new_ptr;
        }

        (reinterpret_cast<unsigned char *>(*lp))[pos ++] = c;
        if(c == '\n') break;
        c = getc(fp);   // FlawFinder: ignore
    }
    (*lp)[pos] = '\0';
    return static_cast<ssize_t>(pos);
}
#endif

namespace tycho::fsys {
using namespace std::filesystem;

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) || defined(WIN32)
enum mode {rd = _O_RDONLY, wr = _O_WRONLY, app = _O_APPEND, rw = _O_RDWR, make = _O_CREAT, empty = _O_TRUNC};

template <typename T>
inline auto read(int fd, T& data) noexcept {    // FlawFinder: ignore
    static_assert(std::is_trivial_v<T>, "T must be Trivial type");
    return _read(fd, &data, sizeof(data));
}

template <typename T>
inline auto write(int fd, const T& data) noexcept {
    static_assert(std::is_trivial_v<T>, "T must be Trivial type");
    return _write(fd, &data, sizeof(data));
}

inline auto seek(int fd, off_t pos) noexcept {
    return _lseek(fd, pos, SEEK_SET);
}

inline auto tell(int fd) noexcept {
    return _lseek(fd, 0, SEEK_CUR);
}

inline auto append(int fd) noexcept {
    return _lseek(fd, 0, SEEK_END);
}

inline auto open(const fsys::path& path, mode flags = mode::rw) noexcept {  // FlawFinder: ignore
    const auto filename = path.string();
    return _open(filename.c_str(), flags | _O_BINARY, 0664);
}

inline auto close(int fd) noexcept {
    return _close(fd);
}

inline auto read(int fd, void *buf, std::size_t len) {   // FlawFinder: ignore
    return _read(fd, buf, len);
}

inline auto write(int fd, const void *buf, std::size_t len) {
    return _write(fd, buf, len);
}

inline auto read_at(int fd, void *buf, std::size_t len, off_t pos) { // FlawFinder: ignore
    auto handle = (HANDLE)_get_osfhandle(fd);
    if(handle == INVALID_HANDLE_VALUE) return (DWORD)-1;
    OVERLAPPED overlapped{0};
    overlapped.Offset = static_cast<DWORD>(pos);
    overlapped.OffsetHigh = 0;
    DWORD bytesRead{0};
    if(ReadFile(handle, buf, static_cast<DWORD>(len), &bytesRead, &overlapped)) return bytesRead;
    return (DWORD)-1;
}

inline auto write_at(int fd, const void *buf, std::size_t len, off_t pos) {
    auto handle = (HANDLE)_get_osfhandle(fd);
    if(handle == INVALID_HANDLE_VALUE) return (DWORD)-1;
    OVERLAPPED overlapped{0};
    overlapped.Offset = static_cast<DWORD>(pos);
    overlapped.OffsetHigh = 0;
    DWORD bytesWritten{0};
    if(WriteFile(handle, buf, static_cast<DWORD>(len), &bytesWritten, &overlapped)) return bytesWritten;
    return (DWORD)-1;
}

template <typename T>
inline auto read_at(int fd, T& data, off_t pos) noexcept {
    static_assert(std::is_trivial_v<T>, "T must be Trivial type");
    return fsys::read_at(fd, &data, sizeof(data), pos);
}

template <typename T>
inline auto write_at(int fd, const T& data, off_t pos) noexcept {
    static_assert(std::is_trivial_v<T>, "T must be Trivial type");
    return fsys::write_at(fd, &data, sizeof(data), pos);
}

inline auto load(int fd, std::size_t size, bool rw = false) -> void* {
    auto handle = (HANDLE)_get_osfhandle(fd);
    if(handle == INVALID_HANDLE_VALUE) return nullptr;

    auto map = CreateFileMapping(handle, nullptr, rw ? PAGE_READWRITE : PAGE_READONLY, 0, size, nullptr);
    if(map == nullptr) return nullptr;

    auto addr = MapViewOfFile(map, rw ? FILE_MAP_READ | FILE_MAP_WRITE : FILE_MAP_READ, 0, 0, size);
    CloseHandle(map);
    return addr;
}

inline void unload(void *addr, [[maybe_unused]]std::size_t size) {
    if(addr == nullptr) return;
    UnmapViewOfFile(addr);
}

inline auto sync(int fd) {
    auto handle = (HANDLE)_get_osfhandle(fd);
    if(handle == INVALID_HANDLE_VALUE) return -1;
    if(FlushFileBuffers(handle))
        return 0;
    return -1;
}

inline auto exclusive_open(const fsys::path& path, [[maybe_unused]] bool all = false) {
    auto filename = path.string();
    auto handle = CreateFile(filename.c_str(), GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(handle == INVALID_HANDLE_VALUE) return -1;
    auto fd = _open_osfhandle((intptr_t)handle, _O_RDWR);
    if(fd == -1)
        CloseHandle(handle);
    return fd;
}

inline auto shared_access(const fsys::path& path) {
    auto filename = path.string();
    auto handle = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(handle == INVALID_HANDLE_VALUE) return -1;
    auto fd = _open_osfhandle((intptr_t)handle, _O_RDONLY);
    if(fd == -1)
        CloseHandle(handle);
    return fd;
}

inline auto native_handle(int fd) {
    auto handle = _get_osfhandle(fd);
    if(handle == -1) return static_cast<void *>(nullptr);
    return reinterpret_cast<void *>(handle);
}

inline auto native_handle(std::FILE *fp) {
    return native_handle(_fileno(fp));
}
#else
enum mode {rd = O_RDONLY, wr = O_WRONLY, app = O_APPEND, rw = O_RDWR, make = O_CREAT, empty = O_TRUNC};

template <typename T>
inline auto read(int fd, T& data) noexcept {    // FlawFinder: ignore
    static_assert(std::is_trivial_v<T>, "T must be Trivial type");
    return ::read(fd, &data, sizeof(data)); // FlawFinder: ignore
}

template <typename T>
inline auto write(int fd, const T& data) noexcept {
    static_assert(std::is_trivial_v<T>, "T must be Trivial type");
    return ::write(fd, &data, sizeof(data));
}

template <typename T>
inline auto read_at(int fd, T& data, off_t pos) noexcept {    // FlawFinder: ignore
    static_assert(std::is_trivial_v<T>, "T must be Trivial type");
    return ::pread(fd, &data, sizeof(data), pos); // FlawFinder: ignore
}

template <typename T>
inline auto write_at(int fd, const T& data, off_t pos) noexcept {
    static_assert(std::is_trivial_v<T>, "T must be Trivial type");
    return ::pwrite(fd, &data, sizeof(data), pos);
}

inline auto seek(int fd, off_t pos) noexcept {
    return ::lseek(fd, pos, SEEK_SET);
}

inline auto tell(int fd) noexcept {
    return ::lseek(fd, 0, SEEK_CUR);
}

inline auto append(int fd) noexcept {
    return ::lseek(fd, 0, SEEK_END);
}

inline auto open(const fsys::path& path, mode flags = mode::rw) noexcept {   // FlawFinder: ignore
    return ::open(path.string().c_str(), flags, 0664); // FlawFinder: ignore
}

inline auto close(int fd) noexcept {
    return ::close(fd);
}

inline auto read(int fd, void *buf, std::size_t len) { // FlawFinder: ignore
    return ::read(fd, buf, len);    // FlawFinder: ignore
}

inline auto read_at(int fd, void *buf, std::size_t len, off_t pos) { // FlawFinder: ignore
    return ::pread(fd, buf, len, pos);
}

inline auto load(int fd, std::size_t size, bool rw = false) -> void * {
    void *addr = ::mmap(nullptr, size, rw ? PROT_READ | PROT_WRITE : PROT_READ, MAP_SHARED, fd, 0);
    if(addr == MAP_FAILED) return nullptr;
    return addr;
}

inline auto unload(void *addr, std::size_t size) {
    if(addr == MAP_FAILED || addr == nullptr)
        return;
    munmap(addr, size);
}

inline auto write(int fd, const void *buf, std::size_t len) {
    return ::write(fd, buf, len);
}

inline auto write_at(int fd, const void *buf, std::size_t len, off_t pos) {
    return ::pwrite(fd, buf, len, pos);
}

inline auto sync(int fd) {
    return ::fsync(fd);
}

inline auto native_handle(int fd) {
    return fd;
}

inline auto exclusive_open(const fsys::path& path, bool all = false) {
    const auto file = path.string();
    auto fd = ::open(file.c_str(), O_RDWR | O_CREAT | O_EXCL, all ? 0644 : 0640); // FlawFinder: ignore
    if(fd == -1) return -1;
    struct flock lock{};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if(fcntl(fd, F_SETLK, &lock) == -1) {
        close(fd);
        return -1;
    }
    return fd;
}

inline auto shared_access(const fsys::path& path) {
    const auto filename = path.string();
    return ::open(filename.c_str(), O_RDONLY);  // FlawFinder: ignore
}

inline auto native_handle(std::FILE *fp) {
    return fileno(fp);
}
#endif

class fd_t final {
public:
    fd_t() noexcept = default;
    fd_t(const fd_t&) = delete;
    auto operator=(const fd_t&) noexcept -> auto& = delete;

    explicit fd_t(int fd) noexcept : fd_(fd) {}

    explicit fd_t(const fsys::path& path, mode flags = mode::rw) noexcept :
    fd_(fsys::open(path, flags)) {} // FlawFinder: ignore

    fd_t(fd_t&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    ~fd_t() {
        if(fd_ != -1)
            fsys::close(fd_);
    }

    auto operator=(fd_t&& other) noexcept -> auto& {
        release();
        fd_ = other.fd_;
        return *this;
    }

    auto operator=(int fd) noexcept -> fd_t& {
        release();
        fd_ = fd;
        return *this;
    }

    auto operator*() const noexcept {
        return fd_;
    }

    operator int() const noexcept {
        return fd_;
    }

    operator bool() const noexcept {
        return fd_ != -1;
    }

    auto operator!() const noexcept {
        return fd_ == -1;
    }

    // FlawFinder: ignore
    void open(const fsys::path& path, mode flags = mode::rw) noexcept {
        release();
        fd_ = fsys::open(path, flags);  // FlawFinder: ignore
    }

    auto seek(off_t pos) const noexcept {
        return fd_ == -1 ? -EBADF : fsys::seek(fd_, pos);
    }

    auto tell() const noexcept {
        return fd_ == -1 ? -EBADF : fsys::tell(fd_);
    }

    auto append() const noexcept {
        return fd_ == -1 ? -EBADF : fsys::append(fd_);
    }

    auto read(void *buf, std::size_t len) const noexcept { // FlawFinder: ignore
        return fd_ == -1 ? -EBADF : fsys::read(fd_, buf, len);  // FlawFinder: ignore
    }

    auto write(const void *buf, std::size_t len) const noexcept {
        return fd_ == -1 ? -EBADF : fsys::write(fd_, buf, len);
    }

    auto read_at(void *buf, std::size_t len, off_t pos) const noexcept { // FlawFinder: ignore
        return fd_ == -1 ? -EBADF : fsys::read_at(fd_, buf, len, pos);  // FlawFinder: ignore
    }

    auto write_at(const void *buf, std::size_t len, off_t pos) const noexcept {
        return fd_ == -1 ? -EBADF : fsys::write_at(fd_, buf, len, pos);
    }

    auto load(std::size_t size, bool rw = false) const noexcept {
        return fd_ == -1 ? nullptr : fsys::load(fd_, size, rw);
    }

    template <typename T>
    auto read(T& data) const noexcept {       // FlawFinder: ignore
        static_assert(std::is_trivial_v<T>, "T must be Trivial type");
        return fd_ == -1 ? -EBADF : fsys::read(fd_, &data, sizeof(data));   // FlawFinder: ignore
    }

    template <typename T>
    auto write(const T& data) const noexcept {
        static_assert(std::is_trivial_v<T>, "T must be Trivial type");
        return fd_ == -1 ? -EBADF : fsys::write(fd_, &data, sizeof(data));
    }

    template <typename T>
    auto read_at(T& data, off_t pos) const noexcept {   // FlawFinder: ignore
        static_assert(std::is_trivial_v<T>, "T must be Trivial type");
        return fd_ == -1 ? -EBADF : fsys::read_at(fd_, &data, sizeof(data), pos);   // FlawFinder: ignore
    }

    template <typename T>
    auto write_at(const T& data, off_t pos) const noexcept {
        static_assert(std::is_trivial_v<T>, "T must be Trivial type");
        return fd_ == -1 ? -EBADF : fsys::write_at(fd_, &data, sizeof(data), pos);
    }

private:
    int fd_{-1};

    void release() noexcept {
        if(fd_ != -1)
            fsys::close(fd_);
        fd_ = -1;
    }
};
} // end fsys namespace

namespace tycho {
template <typename Func>
inline auto scan_stream(std::istream& input, Func proc) {
    std::string line;
    std::size_t count{0};
    while(std::getline(input, line)) {
        if (!proc(line)) break;
        ++count;
    }
    return count;
}

template <typename Func>
inline auto scan_file(const fsys::path& path, Func proc) {
    std::fstream input(path);
    std::string line;
    std::size_t count{0};
    while(std::getline(input, line)) {
        if (!proc(line)) break;
        ++count;
    }
    return count;
}

#if !defined(_MSC_VER) && !defined(__MINGW32__) && !defined(__MINGW64__) && !defined(WIN32)
template <typename Func>
inline auto scan_file(std::FILE *file, Func proc, std::size_t size = 0) {
    char *buf{nullptr};
    std::size_t count{0};
    if(size)
        buf = static_cast<char *>(std::malloc(size));    // NOLINT
    while(!feof(file)) {
        auto len = getline(&buf, &size, file);
        if(len < 0 || !proc({buf, std::size_t(len)})) break;
        ++count;
    }
    if(buf)
        std::free(buf);  // NOLINT
    return count;
}

template <typename Func>
inline auto scan_command(const std::string& cmd, Func proc, std::size_t size = 0) {
    auto file = popen(cmd.c_str(), "r");    // FlawFinder: ignore

    if(!file) return std::size_t(0);

    auto count = scan_file(file, proc, size);
    pclose(file);
    return count;
}
#else
template <typename Func>
inline auto scan_file(std::FILE *file, Func proc, std::size_t size = 0) {
    char *buf{nullptr};
    std::size_t count{0};
    if(size)
        buf = static_cast<char *>(std::malloc(size));    // NOLINT
    while(!feof(file)) {
        auto len = getline_w32(&buf, &size, file);
        if(len < 0 || !proc({buf, std::size_t(len)})) break;
        ++count;
    }
    if(buf)
        std::free(buf);  // NOLINT
    return count;
}

template <typename Func>
inline auto scan_command(const std::string& cmd, Func proc, std::size_t size = 0) {
    auto file = _popen(cmd.c_str(), "r");    // FlawFinder: ignore

    if(!file) return std::size_t(0);
    auto count = scan_file(file, proc, size);
    _pclose(file);
    return count;
}
#endif

inline auto make_input(const fsys::path& path, std::ios::openmode mode = std::ios::binary) {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    file.open(path, mode);  // FlawFinder: ignore
    return file;
}

inline auto make_output(const fsys::path& path, std::ios::openmode mode = std::ios::binary) {
    std::ofstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    file.open(path, mode);  // FlawFinder: ignore
    return file;
}

template <typename Func>
inline auto scan_directory(const fsys::path& path, Func proc) {
    auto dir = fsys::directory_iterator(path);
    return std::count_if(begin(dir), end(dir), proc);
}

template <typename Func>
inline auto scan_recursive(const fsys::path& path, Func proc) {
    auto dir = fsys::recursive_directory_iterator(path, fsys::directory_options::skip_permission_denied);
    return std::count_if(begin(dir), end(dir), proc);
}

inline auto to_string(const fsys::path& path) {
    return std::string{path.string()};
}
} // end namespace

#if defined(TYCHO_PRINT_HPP_) && (__cplusplus < 202002L)
template <>
class fmt::formatter<tycho::fsys::path> {
public:
    static constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename Context>
    constexpr auto format(const tycho::fsys::path& path, Context& ctx) const {
        return format_to(ctx.out(), "{}", std::string{path.string()});
    }
};
#elif defined(TYCHO_PRINT_HPP)
template <>
struct std::formatter<tycho::fsys::path> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const tycho::fsys::path& path, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", path.string());
    }
};
#endif

inline auto operator<<(std::ostream& out, const tycho::fsys::path& path) -> std::ostream& {
    out << path.string();
    return out;
}
#endif
