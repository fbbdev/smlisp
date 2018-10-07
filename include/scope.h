#pragma once

#include "rbtree.h"
#include "symbol.h"
#include "util.h"
#include "value.h"

#include <stdbool.h>

typedef struct SmScope {
    struct SmScope* parent;
    SmRBTree vars;
} SmScope;

typedef struct SmVariable {
    SmSymbol id;
    SmValue value;
} SmVariable;

inline SmScope sm_scope(SmScope* parent) {
    return (SmScope) {
        parent,
        sm_rbtree(sizeof(SmVariable), sm_alignof(SmVariable), sm_symbol_key, sm_key_compare_ptr)
    };
}

inline void sm_scope_drop(SmScope* scope) {
    scope->parent = NULL;
    sm_rbtree_drop(&scope->vars);
}

inline size_t sm_scope_size(SmScope const* scope) {
    return sm_rbtree_size(&scope->vars);
}

inline SmVariable* sm_scope_get(SmScope const* scope, SmSymbol id) {
    return (SmVariable*) sm_rbtree_find_by_key(&scope->vars, sm_symbol_key(&id));
}

inline SmVariable* sm_scope_set(SmScope* scope, SmSymbol id, SmValue value) {
    SmVariable var = { id, value };
    return (SmVariable*) sm_rbtree_insert(&scope->vars, &var);
}

inline void sm_scope_delete(SmScope* scope, SmSymbol id) {
    sm_rbtree_erase(&scope->vars, sm_rbtree_find_by_key(&scope->vars, sm_symbol_key(&id)));
}

inline bool sm_scope_is_set(SmScope const* scope, SmSymbol id) {
    return sm_scope_get(scope, id) != NULL;
}

inline SmVariable* sm_scope_first(SmScope const* scope) {
    return (SmVariable*) sm_rbtree_first(&scope->vars);
}

inline SmVariable* sm_scope_next(SmScope const* scope, SmVariable* var) {
    return (SmVariable*) sm_rbtree_next(&scope->vars, var);
}

inline SmVariable* sm_scope_lookup(SmScope const* scope, SmSymbol id) {
    SmVariable* var = NULL;

    for (; scope; scope = scope->parent) {
        if ((var = sm_scope_get(scope, id)))
            break;
    }

    return var;
}
