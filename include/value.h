#pragma once

#include "number.h"
#include "util.h"
#include "word.h"

#include <stdbool.h>

struct SmContext;

typedef enum SmType {
    SmTypeNil = 0,
    SmTypeNumber,
    SmTypeWord,
    SmTypeCons,

    SmTypeCount
} SmType;

typedef enum SmBuildOp {
    SmBuildEnd = 0,
    SmBuildCar,
    SmBuildCdr,
    SmBuildList,

    SmBuildOpCount
} SmBuildOp;

typedef struct SmValue {
    SmType type;
    uint8_t quotes;

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
    return (SmValue){ SmTypeNil, 0, { .cons = NULL } };
}

inline SmValue sm_value_number(SmNumber number) {
    return (SmValue){ SmTypeNumber, 0, { .number = number } };
}

inline SmValue sm_value_word(SmWord word) {
    return (SmValue){ SmTypeWord, 0, { .word = word } };
}

inline SmValue sm_value_cons(SmCons* cons) {
    sm_assert(cons != NULL);
    return (SmValue){ SmTypeCons, 0, { .cons = cons } };
}

inline bool sm_value_is_nil(SmValue value) {
    return value.type == SmTypeNil;
}

inline bool sm_value_is_number(SmValue value) {
    return value.type == SmTypeNumber;
}

inline bool sm_value_is_word(SmValue value) {
    return value.type == SmTypeWord;
}

inline bool sm_value_is_cons(SmValue value) {
    return value.type == SmTypeCons;
}

inline bool sm_value_is_unquoted(SmValue value) {
    return value.quotes == 0;
}

inline bool sm_value_is_quoted(SmValue value) {
    return value.quotes != 0;
}

inline SmValue sm_value_quote(SmValue value, uint8_t quotes) {
    value.quotes += quotes;
    return value;
}

inline SmValue sm_value_unquote(SmValue value, uint8_t unquotes) {
    // Clamp to 0
    value.quotes -= (unquotes < value.quotes) ? unquotes : value.quotes;
    return value;
}

// List functions
inline bool sm_value_is_list(SmValue value) {
    return value.type == SmTypeCons || value.type == SmTypeNil;
}

inline SmCons* sm_list_next(SmCons* cons) {
    return (cons && cons->cdr.type == SmTypeCons && cons->cdr.quotes == 0) ?
        cons->cdr.data.cons : NULL;
}

inline size_t sm_list_size(SmCons* cons) {
    size_t size = 0;

    for (; cons; cons = sm_list_next(cons))
        ++size;

    return size;
}

inline bool sm_list_is_dotted(SmCons* cons) {
    for (; cons; cons = sm_list_next(cons))
        if (!sm_value_is_list(cons->cdr) || sm_value_is_quoted(cons->cdr))
            return true;

    return false;
}

void sm_build_list(struct SmContext* ctx, SmValue* ret, ...);
