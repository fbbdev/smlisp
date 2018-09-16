#include "rbtree.h"
#include "rbtree_p.h"
#include "util.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

bool is_leaf(Node const* node) {
    return node->parent == NULL && node->left == NULL && node->right == NULL;
}

typedef struct TreeStats {
    size_t size;
    size_t leaf_nodes;
    size_t black_height;
} TreeStats;

size_t count_trailing_zeros(uintptr_t v) {
    size_t c = sizeof(v)*CHAR_BIT;
    if (v) {
        v = (v ^ (v - 1)) >> 1;  // Set v's trailing 0s to 1s and zero rest
        for (c = 0; v; ++c)
            v >>= 1;
    }

    return c;
}

bool validate_tree(Node const* node, Node const* parent,
                   SmRBTree const* tree, size_t element_alignment,
                   TreeStats* stats) {
    bool result = true;

    TreeStats root_stats = {0};
    if (!stats)
        stats = &root_stats;

    // Verify root color
    if (!parent) {
        fprintf(stderr, "Validating tree. Root node: %p\n", (void*) node);
        if (node->color != Black) {
            fprintf(stderr, "node: %p - root node is not black\n", (void*) node);
            result = false;
        }
    }

    // Verify leaf color
    if (is_leaf(node)) {
        if (node->color != Black) {
            fprintf(stderr, "node: %p - leaf node is not black\n", (void*) node);
            result = false;
        }

        ++stats->leaf_nodes;
        return result;
    }

    // Verify parent link
    if (node->parent != parent) {
        fprintf(stderr,
            "node: %p - parent field does not point to actual parent\n"
            "--- node->parent: %p; parent: %p\n",
            (void*) node, (void*) node->parent, (void*) parent);
        result = false;
    }

    // Verify red node children color
    if (node->color == Red && node->left->color != Black) {
        fprintf(stderr,
            "node: %p - left child (%p) of red node is not black\n",
            (void*) node, (void*) node->left);
        result = false;
    }

    if (node->color == Red && node->right->color != Black) {
        fprintf(stderr,
            "node: %p - right child (%p) of red node is not black\n",
            (void*) node, (void*) node->right);
        result = false;
    }

    // Verify memory alignment
    if (((uintptr_t) node) % sm_alignof(Node)) {
        fprintf(stderr,
            "node: %p - node pointer misaligned\n"
            "--- alignment: %zu; expected: %zu\n",
            (void*) node,
            ((size_t) 1) << count_trailing_zeros((uintptr_t) node),
            sm_alignof(Node));
        result = false;
    }

    void const* element = node->data + tree->node_padding;
    if (((uintptr_t) element) % element_alignment) {
        fprintf(stderr,
            "node: %p - element pointer (%p) misaligned\n"
            "--- alignment: %zu; expected: %zu\n",
            (void*) node, (void*) element,
            ((size_t) 1) << count_trailing_zeros((uintptr_t) element),
            element_alignment);
        result = false;
    }

    // Verify key ordering
    SmKey key = tree->key(element);

    if (!is_leaf(node->left) && tree->compare(tree->key(node->left->data + tree->node_padding), key) >= 0) {
        fprintf(stderr,
            "node: %p - left child (%p) has higher or equal key\n",
            (void*) node, (void*) node->left);
        result = false;
    }

    if (!is_leaf(node->right) && tree->compare(tree->key(node->right->data + tree->node_padding), key) <= 0) {
        fprintf(stderr,
            "node: %p - right child (%p) has lower or equal key\n",
            (void*) node, (void*) node->right);
        result = false;
    }

    // Verify lower branches
    TreeStats left_stats = { 0 };
    bool left_result = validate_tree(node->left, node, tree, element_alignment, &left_stats);

    TreeStats right_stats = { 0 };
    bool right_result = validate_tree(node->right, node, tree, element_alignment, &right_stats);

    // Verify black height
    if (left_stats.black_height != right_stats.black_height) {
        fprintf(stderr,
            "node: %p - left and right branches have different black height\n"
            "--- left: %zu; right: %zu\n",
            (void*) node, left_stats.black_height, right_stats.black_height);
        result = false;
    }

    // Report errors in branches
    if (!left_result) {
        fprintf(stderr,
            "node: %p - error in left branch (%p)\n",
            (void*) node, (void*) node->left);
        result = false;
    }

    if (!right_result) {
        fprintf(stderr,
            "node: %p - error in right branch (%p)\n",
            (void*) node, (void*) node->right);
        result = false;
    }

    stats->size += 1 + left_stats.size + right_stats.size;
    stats->leaf_nodes += left_stats.leaf_nodes + right_stats.leaf_nodes;
    stats->black_height = (node->color == Black) +
        ((left_stats.black_height ) ? left_stats.black_height : right_stats.black_height);

    if (!parent) {
        fprintf(stderr, "Tree at (%p) is%s valid\n", (void*) node, result ? "" : " not");
        fprintf(stderr, "--- size: %zu nodes; leaf_nodes: %zu; black_height: %zu\n",
            stats->size, stats->leaf_nodes, stats->black_height);
    }

    return result;
}

int main(int argc, char* argv[]) {
    SmTestContext ctx = sm_test_context(argc, argv);

    SmRBTree tree = sm_string_rbtree();
    SmString* element = NULL;

    SmString str[] = {
        sm_string_from_cstring("hello"),
        sm_string_from_cstring("how"),
        sm_string_from_cstring("are"),
        sm_string_from_cstring("you"),
        sm_string_from_cstring("fine"),
        sm_string_from_cstring("thanks")
    };
    const size_t str_count = sizeof(str)/sizeof(SmString);

    char buf[32] = "hello";
    SmString buf_str = sm_string_from_cstring(buf);

    sm_test(&ctx, "sm_rbtree_empty should return true for empty tree",
        sm_rbtree_empty(&tree) == true);
    sm_test(&ctx, "sm_rbtree_size should return 0 for empty tree",
        sm_rbtree_size(&tree) == 0);

    sm_test(&ctx, "sm_rbtree_find should return NULL for empty tree",
        sm_rbtree_find(&tree, &str[0]) == NULL);
    sm_test(&ctx, "sm_rbtree_find_by_key should return NULL for empty tree",
        sm_rbtree_find_by_key(&tree, sm_string_key(&str[0])) == NULL);

    element = (SmString*) sm_rbtree_insert(&tree, &str[0]);

    sm_test(&ctx, "sm_rbtree_insert should succeed on empty tree",
        element != NULL);
    sm_test(&ctx, "tree should be valid after first insertion",
        validate_tree(tree.root, NULL, &tree, sm_alignof(SmString), NULL));
    sm_test(&ctx, "tree element should be a copy of the inserted value",
        element->data == str[0].data && element->length == str[0].length);
    sm_test(&ctx, "sm_rbtree_empty should return false after first insertion",
        sm_rbtree_empty(&tree) == false);
    sm_test(&ctx, "sm_rbtree_size should return 1 after first insertion",
        sm_rbtree_size(&tree) == 1);

    sm_test(&ctx, "sm_rbtree_find should return NULL for absent key",
        sm_rbtree_find(&tree, &str[1]) == NULL);
    sm_test(&ctx, "sm_rbtree_find_by_key should return NULL for absent key",
        sm_rbtree_find_by_key(&tree, sm_string_key(&str[1])) == NULL);

    sm_test(&ctx, "sm_rbtree_find should return element for unique present key",
        sm_rbtree_find(&tree, &str[0]) == element);
    sm_test(&ctx, "sm_rbtree_find_by_key should return element for unique present key",
        sm_rbtree_find_by_key(&tree, sm_string_key(&str[0])) == element);
    sm_test(&ctx, "sm_rbtree_find should return element for unique present key when querying with different pointer",
        sm_rbtree_find(&tree, &buf_str) == element);
    sm_test(&ctx, "sm_rbtree_find_by_key should return element for unique present key when querying with different pointer",
        sm_rbtree_find_by_key(&tree, sm_string_key(&buf_str)) == element);

    sm_rbtree_erase(&tree, element);
    sm_test(&ctx, "sm_rbtree_empty should return true after sm_rbtree_erase on last element",
        sm_rbtree_empty(&tree) == true);
    sm_test(&ctx, "sm_rbtree_size should return 0 after sm_rbtree_erase on last element",
        sm_rbtree_size(&tree) == 0);

    bool insertion_ok = true, valid = true, copy_ok = true, size_ok = true;
    for (size_t i = 0; i < str_count; ++i) {
        element = (SmString*) sm_rbtree_insert(&tree, &str[i]);
        valid = validate_tree(tree.root, NULL, &tree, sm_alignof(SmString), NULL) && valid;
        insertion_ok = (element != NULL) && insertion_ok;
        copy_ok = (element->data == str[i].data && element->length == str[i].length) && copy_ok;
        size_ok = (sm_rbtree_size(&tree) == i + 1) && size_ok;
    }

    sm_test(&ctx, "sm_rbtree_insert should always succeed on non-empty trees", insertion_ok);
    sm_test(&ctx, "tree should be valid after any number of insertions", valid);
    sm_test(&ctx, "tree element should be a copy of the inserted value for any insertion", copy_ok);
    sm_test(&ctx, "sm_rbtree_empty should return false after any number of insertions",
        sm_rbtree_empty(&tree) == false);
    sm_test(&ctx, "sm_rbtree_size should return correct size after any number of insertions", size_ok);

    bool find_found = true, find_ok = true, find_bk_found = true, find_bk_ok = true;
    for (size_t i = 0; i < str_count; ++i) {
        element = (SmString*) sm_rbtree_find(&tree, &str[i]);
        find_found = (element != NULL) && find_found;
        find_ok = (element->data == str[i].data && element->length == str[i].length) && find_ok;

        element = (SmString*) sm_rbtree_find_by_key(&tree, sm_string_key(&str[i]));
        find_bk_found = (element != NULL) && find_bk_found;
        find_bk_ok = (element->data == str[i].data && element->length == str[i].length) && find_bk_ok;
    }

    sm_test(&ctx, "sm_rbtree_find should not return NULL for any present key", find_found);
    sm_test(&ctx, "sm_rbtree_find should return element for any present key", find_ok);
    sm_test(&ctx, "sm_rbtree_find_by_key should not return NULL for any present key", find_bk_found);
    sm_test(&ctx, "sm_rbtree_find_by_key should return element for any present key", find_bk_ok);

    find_found = true; find_ok = true; find_bk_found = true; find_bk_ok = true;
    for (size_t i = 0; i < str_count; ++i) {
        strncpy(buf, str[i].data, str[i].length);
        buf_str.length = str[i].length;

        element = (SmString*) sm_rbtree_find(&tree, &buf_str);
        find_found = (element != NULL) && find_found;
        find_ok = (element->data == str[i].data && element->length == str[i].length) && find_ok;

        element = (SmString*) sm_rbtree_find_by_key(&tree, sm_string_key(&buf_str));
        find_bk_found = (element != NULL) && find_bk_found;
        find_bk_ok = (element->data == str[i].data && element->length == str[i].length) && find_bk_ok;
    }

    sm_test(&ctx, "sm_rbtree_find should not return NULL for any present key when querying with different pointer", find_found);
    sm_test(&ctx, "sm_rbtree_find should return element for any present key when querying with different pointer", find_ok);
    sm_test(&ctx, "sm_rbtree_find_by_key should not return NULL for any present key when querying with different pointer", find_bk_found);
    sm_test(&ctx, "sm_rbtree_find_by_key should return element for any present key when querying with different pointer", find_bk_ok);

    // Erase half of elements
    bool erase_ok = true; valid = true; size_ok = true;
    for (size_t i = 0; i < str_count; i += 2) {
        element = (SmString*) sm_rbtree_find(&tree, &str[i]);
        sm_rbtree_erase(&tree, element);
        valid = validate_tree(tree.root, NULL, &tree, sm_alignof(SmString), NULL) && valid;
        erase_ok = (sm_rbtree_find(&tree, &str[i]) == NULL) && erase_ok;
        size_ok = (sm_rbtree_size(&tree) == str_count - ((i/2) + 1)) && size_ok;
    }

    sm_test(&ctx, "tree should be valid after any number of removals", valid);
    sm_test(&ctx, "sm_rbtree_find should return NULL after removal of queried key", erase_ok);
    sm_test(&ctx, "sm_rbtree_size should return correct size after any number of removals", size_ok);

    sm_rbtree_clear(&tree);
    sm_test(&ctx, "sm_rbtree_empty should return true after sm_rbtree_clear",
        sm_rbtree_empty(&tree) == true);
    sm_test(&ctx, "sm_rbtree_size should return 0 after sm_rbtree_clear",
        sm_rbtree_size(&tree) == 0);

    sm_rbtree_drop(&tree);

    return !sm_test_report(&ctx);
}
