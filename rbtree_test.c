#include "rbtree.h"
#include "util.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

// Private types
typedef enum Color {
    Red = 0,
    Black
} Color;

typedef struct SmRBTreeNode {
    Color color;

    struct SmRBTreeNode* parent;
    struct SmRBTreeNode* left;
    struct SmRBTreeNode* right;

    uint8_t data[];
} Node;

typedef struct TreeStats {
    size_t size;
    size_t leaf_nodes;
    size_t black_height;
} TreeStats;

bool is_leaf(Node const* node) {
    return node->parent == NULL && node->left == NULL && node->right == NULL;
}

bool validate_tree(Node const* node, Node const* parent,
                   SmRBTree const* tree, TreeStats* stats) {
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

    // Verify key ordering
    SmKey key = tree->key(node->data + tree->node_padding);

    if (!is_leaf(node->left) && sm_key_compare(tree->key(node->left->data + tree->node_padding), key) >= 0) {
        fprintf(stderr,
            "node: %p - left child (%p) has higher or equal key\n",
            (void*) node, (void*) node->left);
        result = false;
    }

    if (!is_leaf(node->right) && sm_key_compare(tree->key(node->right->data + tree->node_padding), key) <= 0) {
        fprintf(stderr,
            "node: %p - right child (%p) has lower or equal key\n",
            (void*) node, (void*) node->right);
        result = false;
    }

    // Verify lower branches
    TreeStats left_stats = { 0 };
    bool left_result = validate_tree(node->left, node, tree, &left_stats);

    TreeStats right_stats = { 0 };
    bool right_result = validate_tree(node->right, node, tree, &right_stats);

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
        fprintf(stderr, "Tree at (%p) is%s valid\n", (void*) node, result ? "" : "not");
        fprintf(stderr, "--- size: %zu nodes; leaf_nodes: %zu; black_height: %zu\n",
            stats->size, stats->leaf_nodes, stats->black_height);
    }

    return result;
}

int main() {
    bool status = true;

    SmRBTree tree = sm_string_rbtree();
    void* element = NULL;

    SmString str[] = {
        sm_string_from_cstring("hello"),
        sm_string_from_cstring("how"),
        sm_string_from_cstring("are"),
        sm_string_from_cstring("you"),
        sm_string_from_cstring("fine"),
        sm_string_from_cstring("thanks")
    };

    status &= sm_test("sm_rbtree_empty should return true for empty tree",
        sm_rbtree_empty(&tree) == true);

    status &= sm_test("sm_rbtree_find should return NULL for empty tree",
        sm_rbtree_find(&tree, &str[0]) == NULL);
    status &= sm_test("sm_rbtree_find_by_key should return NULL for empty tree",
        sm_rbtree_find_by_key(&tree, sm_string_key(&str[0])) == NULL);

    element = sm_rbtree_insert(&tree, &str[0]);
    fprintf(stderr, "Inserted element in empty tree: %p\n", element);

    status &= sm_test("sm_rbtree_insert should succeed on empty tree",
        element != NULL && element != &str[0]);
    status &= sm_test("sm_rbtree_empty should return false after first insertion",
        sm_rbtree_empty(&tree) == false);
    status &= sm_test("tree should be valid after first insertion",
        validate_tree(tree.root, NULL, &tree, NULL));

    status &= sm_test("sm_rbtree_find should return NULL for absent key",
        sm_rbtree_find(&tree, &str[1]) == NULL);
    status &= sm_test("sm_rbtree_find_by_key should return NULL for absent key",
        sm_rbtree_find_by_key(&tree, sm_string_key(&str[1])) == NULL);

    status &= sm_test("sm_rbtree_find should return element for present key",
        sm_rbtree_find(&tree, &str[0]) == element);
    status &= sm_test("sm_rbtree_find_by_key should return element for present key",
        sm_rbtree_find_by_key(&tree, sm_string_key(&str[0])) == element);

    sm_rbtree_erase(&tree, element);
    status &= sm_test("sm_rbtree_empty should return true after sm_rbtree_erase on last element",
        sm_rbtree_empty(&tree) == true);

    sm_rbtree_drop(&tree);
    status &= sm_test("sm_rbtree_empty should return true after sm_rbtree_drop",
        sm_rbtree_empty(&tree) == true);

    return status ? 0 : -1;
}
