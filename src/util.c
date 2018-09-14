#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Inlines
extern inline SmString sm_string_from_cstring(char const* str);
extern inline SmKey sm_string_key(void const* element);
extern inline void* sm_aligned_alloc(size_t alignment, size_t size);
extern inline SmTestContext sm_test_context(int argc, char** argv);

// Private helpers
static inline size_t gcd_size_t(size_t m, size_t n) {
    return (n == 0) ? m : (m % n == 0) ? n : gcd_size_t(n, m % n);
}

// Error handling
sm_noreturn void sm_handle_panic(char const* fn, char const* file,
                                 unsigned int line, char const* message) {
    fprintf(stderr,
        "PANIC in function %s at %s:%u: %s\n",
        fn, file, line, message);

    abort();
}

// Key functions
int sm_key_compare(SmKey lhs, SmKey rhs) {
    uint8_t const* ldata = (uint8_t const*) lhs.data;
    uint8_t const* lend = ldata + lhs.size;

    uint8_t const* rdata = (uint8_t const*) rhs.data;
    uint8_t const* rend = rdata + rhs.size;

    while (ldata != lend && rdata != rend && *ldata == *rdata)
        ++ldata, ++rdata;

    if (ldata == lend && rdata == rend)
        return 0;

    return (ldata == lend) ? -*rdata : ((rdata == rend) ? *ldata : *ldata - *rdata);
}

// Memory management
size_t sm_common_alignment(size_t a1, size_t a2) {
    return (a1*a2)/gcd_size_t(a1, a2);
}

// Testing
bool sm_test(SmTestContext* ctx, char const* desc, bool result) {
    ctx->pass += result;
    ctx->fail += !result;

    printf("[%s] %s\n", result ? " OK " : "FAIL", desc);
    fflush(stdout);
    return result;
}

bool sm_test_report(SmTestContext const* ctx) {
    char const* test_name = "";

    if (ctx->argc) {
#ifdef _WIN32
        char const pathsep = '\\';
#else
        char const pathsep = '/';
#endif
        if (!(test_name = strrchr(ctx->argv[0], pathsep)))
            test_name = ctx->argv[0];
        else
            ++test_name;
    }

    printf("[%s] %s%stests passed: %zu; failed: %zu; total: %zu\n",
        ctx->fail ? "FAIL" : " OK ",
        test_name,
        ctx->argc ? ": " : "",
        ctx->pass, ctx->fail,
        ctx->pass + ctx->fail);
    return !ctx->fail;
}
