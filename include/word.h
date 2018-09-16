#pragma once

#include "rbtree.h"
#include "util.h"

#include <stdint.h>

// Types
typedef void const* SmWord;
typedef SmRBTree SmWordSet;

// Word set functions
#define sm_word_set sm_string_rbtree
#define sm_word_set_size sm_rbtree_size
#define sm_word_set_first sm_rbtree_first
#define sm_word_set_next sm_rbtree_next

void sm_word_set_drop(SmWordSet* set);

// Word functions
SmWord sm_word(SmWordSet* set, SmString str);

inline SmString sm_word_str(SmWord word) {
    return *(SmString const*) word;
}

#define sm_word_key sm_ptr_key
