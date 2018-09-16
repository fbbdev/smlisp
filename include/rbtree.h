#pragma once

#include "util.h"

#include <stdbool.h>

typedef struct SmRBTree {
    size_t element_size;
    size_t node_padding;
    size_t node_alignment;
    SmKeyFunction key;
    SmKeyComparisonFunction compare;

    struct SmRBTreeNode* root;
} SmRBTree;

// Lifetime management
SmRBTree sm_rbtree(size_t element_size, size_t element_alignment,
                   SmKeyFunction key, SmKeyComparisonFunction compare);

inline SmRBTree sm_ptr_rbtree() {
    return sm_rbtree(sizeof(void*), sm_alignof(void*), sm_ptr_key, sm_key_compare_ptr);
}

inline SmRBTree sm_string_rbtree() {
    return sm_rbtree(sizeof(SmString), sm_alignof(SmString), sm_string_key, sm_key_compare_data);
}

SmRBTree sm_rbtree_clone(SmRBTree const* tree);
void sm_rbtree_drop(SmRBTree* tree);

// Capacity
inline bool sm_rbtree_empty(SmRBTree const* tree) {
    return tree->root == NULL;
}

size_t sm_rbtree_size(SmRBTree const* tree);

// Modifiers
#define sm_rbtree_clear sm_rbtree_drop

void* sm_rbtree_insert(SmRBTree* tree, void const* element);
void sm_rbtree_erase(SmRBTree* tree, void* element); // XXX: sm_rbtree_erase wants element pointers FROM THE TREE!!

// Iteration
void* sm_rbtree_first(SmRBTree const* tree);
void* sm_rbtree_next(SmRBTree const* tree, void* element);

// Lookup
void* sm_rbtree_find(SmRBTree const* tree, void const* element);
void* sm_rbtree_find_by_key(SmRBTree const* tree, SmKey key);
