#pragma once

#ifdef NDEBUG
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Macros
#define sm_unused(value) ((void) (value))
#define sm_alignof(type) offsetof(struct { char c; type d; }, d)

// Incapsulate a reference to arbitrary contiguous data (for map/set keys)
typedef struct SmKey {
    void const* data;
    size_t size;
} SmKey;

typedef SmKey (*SmKeyFunction)(void const* element);

int sm_key_compare(SmKey lhs, SmKey rhs);

// Encapsulate a reference to a string
typedef struct SmString {
    char const* data;
    size_t length;
} SmString;

inline SmString sm_string_from_cstring(char const* str) {
    return (SmString){ str, strlen(str) };
}

inline SmKey sm_string_key(void const* element) {
    SmString const* str = (SmString const*) element;
    return (SmKey){ str->data, str->length };
}

// Memory management
inline void* sm_aligned_alloc(size_t alignment, size_t size) {
#if (__STDC_VERSION__ >= 201112L)
    void* ptr = aligned_alloc(alignment, size);
#else
    sm_unused(alignment);
    void* ptr = malloc(size);
#endif
    assert(ptr != NULL);
    return ptr;
}

// Testing
inline bool sm_test(char const* desc, bool result) {
    printf("[%s] %s\n", result ? " OK " : "FAIL", desc);
    return result;
}
