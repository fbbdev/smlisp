#pragma once

#include "util.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum SmType {
    // Atoms
    SmTypeNil = 0,
    SmTypeNumber,
    SmTypeWord,
    SmTypeReference,  // Unquoted word

    // Lists
    SmTypeList,
    SmTypeCall,       // Unquoted list

    // Number of types
    SmTypeCount
} SmType;

typedef struct SmList SmList;

typedef struct SmValue {
    SmType type;

    union {
        int64_t number;
        uintptr_t word;
        SmList* list;
    } data;
} SmValue;

struct SmList {
    SmValue car;
    SmValue cdr;

    uint32_t ref_count;
};

// Word functions
uintptr_t sm_word(SmString str);

inline SmString sm_word_str(uintptr_t word) {
    return *((SmString*) word);
}

// SmList functions
SmList* sm_list_cons(SmValue car, SmValue cdr);
SmList* sm_list_clone(SmList* lst);
void sm_list_drop(SmList* lst);

inline void sm_list_ref(SmList* lst) {
    ++lst->ref_count;
}

void sm_list_unref(SmList* lst);

// SmValue functions
inline SmValue sm_value_nil() {
    return (SmValue){ SmTypeNil, { .list = NULL } };
}

inline SmValue sm_value_number(int64_t number) {
    return (SmValue){ SmTypeNumber, { .number = number } };
}

inline SmValue sm_value_word(uintptr_t word, bool quoted) {
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
