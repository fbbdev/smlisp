#pragma once

#include "rbtree.h"
#include "util.h"
#include "value.h"
#include "symbol.h"

typedef SmRBTree SmScope;

typedef struct SmVariable {
    SmSymbol id;
    SmValue value;
} SmVariable;

#define sm_scope_drop sm_rbtree_drop
#define sm_scope_size sm_rbtree_size

inline SmScope sm_scope() {
    return sm_rbtree(sizeof(SmVariable), sm_alignof(SmVariable),
                     sm_symbol_key, sm_key_compare_ptr);
}

inline SmVariable* sm_scope_get(SmScope const* scope, SmSymbol id) {
    return (SmVariable*) sm_rbtree_find_by_key(scope, sm_symbol_key(&id));
}

inline SmVariable* sm_scope_set(SmScope* scope, SmSymbol id, SmValue value) {
    SmVariable var = { id, value };
    return (SmVariable*) sm_rbtree_insert(scope, &var);
}

inline void sm_scope_delete(SmScope* scope, SmSymbol id) {
    sm_rbtree_erase(scope, sm_rbtree_find_by_key(scope, sm_symbol_key(&id)));
}

inline bool sm_scope_is_set(SmScope const* scope, SmSymbol id) {
    return sm_scope_get(scope, id) != NULL;
}

inline SmVariable* sm_scope_first(SmScope const* scope) {
    return (SmVariable*) sm_rbtree_first(scope);
}

inline SmVariable* sm_scope_next(SmScope const* scope, SmVariable* var) {
    return (SmVariable*) sm_rbtree_next(scope, var);
}
