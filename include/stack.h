#pragma once

#include "rbtree.h"
#include "util.h"
#include "value.h"
#include "word.h"

#include <stdbool.h>

typedef SmRBTree SmScope;

typedef struct SmVariable {
    SmWord id;
    SmValue value;
} SmVariable;

typedef struct SmStackFrame {
    struct SmStackFrame* parent;
    SmString name;

    SmScope scope;
} SmStackFrame;

// Scope functions
#define sm_scope_drop sm_rbtree_drop
#define sm_scope_size sm_rbtree_size

inline SmScope sm_scope() {
    return sm_rbtree(sizeof(SmVariable), sm_alignof(SmVariable),
                     sm_word_key, sm_key_compare_ptr);
}

inline SmVariable* sm_scope_get(SmScope const* scope, SmWord id) {
    return (SmVariable*) sm_rbtree_find_by_key(scope, sm_word_key(&id));
}

inline void sm_scope_set(SmScope* scope, SmVariable var) {
    sm_rbtree_insert(scope, &var);
}

inline void sm_scope_delete(SmScope* scope, SmWord id) {
    sm_rbtree_erase(scope, sm_rbtree_find_by_key(scope, sm_word_key(&id)));
}

inline bool sm_scope_is_set(SmScope const* scope, SmWord id) {
    return sm_scope_get(scope, id) != NULL;
}

inline SmVariable* sm_scope_first(SmScope const* scope) {
    return (SmVariable*) sm_rbtree_first(scope);
}

inline SmVariable* sm_scope_next(SmScope const* scope, SmVariable* var) {
    return (SmVariable*) sm_rbtree_next(scope, var);
}

// Frame functions
inline SmStackFrame sm_stack_frame(SmStackFrame* parent, SmString name) {
    return (SmStackFrame){
        parent,
        name,
        sm_scope()
    };
}

inline void sm_stack_frame_drop(SmStackFrame* frame) {
    sm_scope_drop(&frame->scope);
}
