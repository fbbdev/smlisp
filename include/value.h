#pragma once

#include "number.h"
#include "util.h"
#include "word.h"

#include <stdbool.h>

typedef enum SmType {
    // Atoms
    SmTypeNil = 0,
    SmTypeNumber,
    SmTypeWord,
    SmTypeReference,  // Unquoted word

    // Lists
    SmTypeList,
    SmTypeCall,       // Unquoted list

    // Count of types
    SmTypeCount
} SmType;

typedef struct SmValue {
    SmType type;

    union {
        SmNumber number;
        SmWord word;
        struct SmList* list;
    } data;
} SmValue;

typedef struct SmList {
    SmValue car;
    SmValue cdr;
} SmList;

// Value functions
inline SmValue sm_value_nil() {
    return (SmValue){ SmTypeNil, { .list = NULL } };
}

inline SmValue sm_value_number(SmNumber number) {
    return (SmValue){ SmTypeNumber, { .number = number } };
}

inline SmValue sm_value_word(SmWord word, bool quoted) {
    return (SmValue){ quoted ? SmTypeWord : SmTypeReference, { .word = word } };
}

inline SmValue sm_value_list(SmList* list, bool quoted) {
    return (SmValue){ quoted ? SmTypeList : SmTypeCall, { .list = list } };
}

inline bool sm_value_is_nil(SmValue value) {
    return value.type == SmTypeNil;
}

inline bool sm_value_is_number(SmValue value) {
    return value.type == SmTypeNumber;
}

inline bool sm_value_is_word(SmValue value) {
    return value.type == SmTypeWord || value.type == SmTypeReference;
}

inline bool sm_value_is_list(SmValue value) {
    return value.type == SmTypeList || value.type == SmTypeCall;
}

inline bool sm_value_is_unquoted(SmValue value) {
    return value.type == SmTypeCall || value.type == SmTypeReference;
}

inline bool sm_value_is_quoted(SmValue value) {
    return value.type != SmTypeCall && value.type != SmTypeReference;
}
