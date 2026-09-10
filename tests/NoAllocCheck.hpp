#pragma once

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <execinfo.h>

namespace no_alloc {
inline std::atomic<bool>& zone_flag() {
    static std::atomic<bool> flag{false};
    return flag;
}

inline void report_and_abort(const char* what, std::size_t size, std::size_t align = 0) {
    fprintf(stderr, "\nALLOCATION IN NO-ALLOC ZONE: %s(size=%zu%s)\n", what, size, align ? ", aligned alloc" : "");

    void* frames[64];
    int n = backtrace(frames, 64);
    char** syms = backtrace_symbols(frames, n);
    fprintf(stderr, "backtrace (%d frames)\n", n);
    for (int i = 0; i < n; ++i) {
        fprintf(stderr, "  %s\n", syms ? syms[i] : "?");
    }
    fflush(stderr);
    std::abort();
}


inline void enter_no_alloc_zone() {
    no_alloc::zone_flag().store(true, std::memory_order_relaxed);
}

inline void exit_no_alloc_zone() {
    no_alloc::zone_flag().store(false, std::memory_order_relaxed);
}

inline bool in_no_alloc_zone() {
    return no_alloc::zone_flag().load(std::memory_order_relaxed);
}
}

void* operator new(std::size_t size) {
    if (no_alloc::in_no_alloc_zone()) no_alloc::report_and_abort("new", size);
    void* p = std::malloc(size ? size : 1);
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t size) {
    if (no_alloc::in_no_alloc_zone()) no_alloc::report_and_abort("new[]", size);
    void* p = std::malloc(size ? size : 1);
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new(std::size_t size, std::align_val_t align) {
    if (no_alloc::in_no_alloc_zone()) {
        no_alloc::report_and_abort("new(aligned)", size, static_cast<std::size_t>(align));
    }
    void* p = nullptr;
    if (posix_memalign(&p, static_cast<std::size_t>(align), size ? size : 1) != 0) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t size, std::align_val_t align) {
    if (no_alloc::in_no_alloc_zone()) {
        no_alloc::report_and_abort("new[](aligned)", size, static_cast<std::size_t>(align));
    }
    void* p = nullptr;
    if (posix_memalign(&p, static_cast<std::size_t>(align), size ? size : 1) != 0) {
        throw std::bad_alloc();
    }
    return p;
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }

#pragma GCC diagnostic pop
