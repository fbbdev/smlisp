#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// General purpose macros
#define sm_unused(value) ((void) (value))

#if (__STDC_VERSION__ >= 201112L)
    // C11
    #define sm_alignof _Alignof
    #define sm_noreturn _Noreturn
    #define sm_thread_local _Thread_local

#elif defined(_MSC_VER)
    // MSVC
    #define sm_alignof __alignof
    #define sm_noreturn __declspec(noreturn)
    #define sm_thread_local __declspec(thread)

#elif defined(__INTEL_COMPILER)
    // Intel C Compiler
    #define sm_alignof __alignof
    #define sm_noreturn __attribute__((noreturn))
    #define sm_thread_local __thread

#elif defined(__GNUC__) || defined(__clang__)
    // GCC and clang
    #define sm_alignof __alignof__
    #define sm_noreturn __attribute__((noreturn))
    #define sm_thread_local __thread

#else
    // C99 polyfills
    #define sm_alignof(type) offsetof(struct { char c; type d; }, d)
    #define sm_noreturn

    #ifndef sm_thread_local
        #error Please define sm_thread_local as an attribute supported by your compiler
    #endif

#endif

// Error handling
#define sm_panic(...) {sm_handle_panic(__func__, __FILE__, __LINE__, __VA_ARGS__);}
#define sm_guard(cond, ...) {if (!(cond)) sm_panic(__VA_ARGS__);}

#ifdef NDEBUG
    #define sm_assert(cond)
#else
    #define sm_assert(cond) sm_guard(cond, "assertion failed: " #cond)
#endif

sm_noreturn void sm_handle_panic(char const* fn, char const* file,
                                 unsigned int line, char const* message);

// Incapsulate a reference to arbitrary contiguous data (for map/set keys)
typedef struct SmKey {
    void const* data;
    size_t size;
} SmKey;

typedef SmKey (*SmKeyFunction)(void const* element);
typedef intptr_t (*SmKeyComparisonFunction)(SmKey lhs, SmKey rhs);

intptr_t sm_key_compare_data(SmKey lhs, SmKey rhs);

inline intptr_t sm_key_compare_ptr(SmKey lhs, SmKey rhs) {
    return ((intptr_t) lhs.data) - ((intptr_t) rhs.data);
}

inline intptr_t sm_key_compare_size(SmKey lhs, SmKey rhs) {
    return ((intptr_t) lhs.size) - ((intptr_t) rhs.size);
}

inline SmKey sm_ptr_key(void const* element) {
    return (SmKey){ *(void const* const*) element, 0 };
}

// Encapsulate a reference to a string
typedef struct SmString {
    char const* data;
    size_t length;
} SmString;

inline SmString sm_string_from_cstring(char const* str) {
    return (SmString){ str, strlen(str)*sizeof(char) };
}

inline SmKey sm_string_key(void const* element) {
    SmString const* str = (SmString const*) element;
    return (SmKey){ str->data, str->length };
}

// Memory management
size_t sm_common_alignment(size_t a1, size_t a2);

inline void* sm_aligned_alloc(size_t alignment, size_t size) {
    #if (__STDC_VERSION__ >= 201112L)
        void* ptr = aligned_alloc(alignment, size);
    #else
        sm_unused(alignment);
        void* ptr = malloc(size);
    #endif
    sm_guard(ptr != NULL, "allocation failed");
    return ptr;
}

// Testing
typedef struct SmTestContext {
    size_t pass;
    size_t fail;
    int argc;
    char** argv;
} SmTestContext;

inline SmTestContext sm_test_context(int argc, char** argv) {
    return (SmTestContext){ 0, 0, argc, argv };
}

bool sm_test(SmTestContext* ctx, char const* desc, bool result);
bool sm_test_report(SmTestContext const* ctx);
