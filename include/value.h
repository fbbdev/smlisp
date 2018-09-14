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

    uintptr_t ref_count;
} SmList;

// SmList functions
SmList* sm_list_cons(SmValue car, SmValue cdr);
SmList* sm_list_clone(SmList* lst); // XXX: DO NOT USE with circular lists!!
void sm_list_drop(SmList* lst);

inline void sm_list_ref(SmList* lst) {
    ++lst->ref_count;
}

void sm_list_unref(SmList* lst);

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
    sm_list_ref(list);
    return (SmValue){ quoted ? SmTypeList : SmTypeCall, { .list = list } };
}

inline SmValue sm_value_dup(SmValue value) {
    if (value.type == SmTypeList)
        sm_list_ref(value.data.list);
    return value;
}

// XXX: DO NOT USE with circular lists!!
inline SmValue sm_value_clone(SmValue value) {
    SmValue clone = value;

    if (value.type == SmTypeList) {
        clone.data.list = sm_list_clone(value.data.list);
        sm_list_ref(clone.data.list);
    }

    return clone;
}

inline void sm_value_drop(SmValue value) {
    if (value.type == SmTypeList)
        sm_list_unref(value.data.list);
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
