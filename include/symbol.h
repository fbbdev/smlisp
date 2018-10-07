#pragma once

#include "rbtree.h"
#include "util.h"

#include <stdint.h>

// Types
typedef void const* SmSymbol;
typedef SmRBTree SmSymbolSet;

// Symbol set functions
#define sm_symbol_set sm_string_rbtree
#define sm_symbol_set_size sm_rbtree_size
#define sm_symbol_set_first sm_rbtree_first
#define sm_symbol_set_next sm_rbtree_next

void sm_symbol_set_drop(SmSymbolSet* set);

// Symbol functions
SmSymbol sm_symbol(SmSymbolSet* set, SmString str);

inline SmString sm_symbol_str(SmSymbol symbol) {
    return *(SmString const*) symbol;
}

#define sm_symbol_key sm_ptr_key
