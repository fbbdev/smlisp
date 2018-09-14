#pragma once

#include "rbtree.h"
#include "util.h"

#include <stdint.h>

// Types
typedef uintptr_t SmWord;
typedef SmWordSet SmRBTree;

// Word set functions
#define sm_word_set sm_string_rbtree
#define sm_word_set_size sm_rbtree_size

void sm_word_set_drop(SmWordSet* set);

// Word functions
SmWord sm_word(SmWordSet* set, SmString str);

inline SmString sm_word_str(SmWord word) {
    return *(SmString*) word;
}
