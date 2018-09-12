#include "util.h"

// Inlines
extern inline SmString sm_string_from_cstring(char const* str);
extern inline SmKey sm_string_key(void const* element);
extern inline void* sm_aligned_alloc(size_t alignment, size_t size);
extern inline bool sm_test(char const* desc, bool result);

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
