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
    SmTypeQuotedWord,

    // Conses
    SmTypeCons,
    SmTypeQuotedCons,

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

// Cons functions
inline SmCons* sm_cons_next(SmCons* cons) {
    return (cons->cdr.type == SmTypeCons) ? cons->cdr.data.cons : NULL;
}

// Value functions
inline SmValue sm_value_nil() {
    return (SmValue){ SmTypeNil, { .cons = NULL } };
}

inline SmValue sm_value_number(SmNumber number) {
    return (SmValue){ SmTypeNumber, { .number = number } };
}

inline SmValue sm_value_word(SmWord word, bool quoted) {
    return (SmValue){ quoted ? SmTypeQuotedWord : SmTypeWord, { .word = word } };
}

inline SmValue sm_value_cons(SmCons* cons, bool quoted) {
    return (SmValue){ quoted ? SmTypeQuotedCons : SmTypeCons, { .cons = cons } };
}

inline bool sm_value_is_nil(SmValue value) {
    return value.type == SmTypeNil;
}

inline bool sm_value_is_number(SmValue value) {
    return value.type == SmTypeNumber;
}

inline bool sm_value_is_word(SmValue value) {
    return value.type == SmTypeWord || value.type == SmTypeQuotedWord;
}

inline bool sm_value_is_cons(SmValue value) {
    return value.type == SmTypeCons || value.type == SmTypeQuotedCons;
}

inline bool sm_value_is_unquoted(SmValue value) {
    return value.type == SmTypeCons || value.type == SmTypeWord;
}

inline bool sm_value_is_quoted(SmValue value) {
    return value.type != SmTypeCons && value.type != SmTypeWord;
}
