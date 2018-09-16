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

    // Conses
    SmTypeCons,
    SmTypeCall,       // Unquoted list

    // Count of types
    SmTypeCount
} SmType;

typedef struct SmValue {
    SmType type;

    union {
        SmNumber number;
        SmWord word;
        struct SmCons* cons;
    } data;
} SmValue;

typedef struct SmCons {
    SmValue car;
    SmValue cdr;
} SmCons;

// Value functions
inline SmValue sm_value_nil() {
    return (SmValue){ SmTypeNil, { .cons = NULL } };
}

inline SmValue sm_value_number(SmNumber number) {
    return (SmValue){ SmTypeNumber, { .number = number } };
}

inline SmValue sm_value_word(SmWord word, bool quoted) {
    return (SmValue){ quoted ? SmTypeWord : SmTypeReference, { .word = word } };
}

inline SmValue sm_value_cons(SmCons* cons, bool quoted) {
    return (SmValue){ quoted ? SmTypeCons : SmTypeCall, { .cons = cons } };
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

inline bool sm_value_is_cons(SmValue value) {
    return value.type == SmTypeCons || value.type == SmTypeCall;
}

inline bool sm_value_is_unquoted(SmValue value) {
    return value.type == SmTypeCall || value.type == SmTypeReference;
}

inline bool sm_value_is_quoted(SmValue value) {
    return value.type != SmTypeCall && value.type != SmTypeReference;
}
