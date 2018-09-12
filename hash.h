#pragma once

#include "util.h"

#include <stdint.h>

uint32_t sm_hash(void const* data, size_t size, uint32_t seed);

inline uint32_t sm_hash_str(SmString str, uint32_t seed) {
    return sm_hash(str.data, str.length, seed);
}
