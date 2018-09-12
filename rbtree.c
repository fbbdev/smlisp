#include "rbtree.h"

#include <stdint.h>

// Inlines
extern inline SmRBTree sm_string_rbtree();
extern inline bool sm_rbtree_empty(SmRBTree const* tree);

// Private types
enum Color {
    Red = 0,
    Black
};

typedef struct SmRBTreeNode {
    struct SmRBTreeNode* parent;
    struct SmRBTreeNode* left;
    struct SmRBTreeNode* right;

    Color color;

    uint8_t data[];
} Node;

static Node LEAF[1] = { { NULL, NULL, NULL, Black } };

// Private methods
static inline Node* node_grandparent(Node* n) {
    return n->parent ? n->parent->parent : NULL;
}

static inline Node* node_sibling(Node* n) {
    return n->parent ?
      ((n == n->parent->left) ? n->parent->right : n->parent->left) : NULL;
}

static inline Node* node_uncle(Node* n) {
    return n->parent ? sibling(n->parent) : NULL;
}

static inline size_t node_size(SmRBTree const* tree) {
    return offsetof(Node, element) + tree->node_padding + tree->element_size;
}

static inline Node* node(SmRBTree const* tree, Node* parent, void const* element) {
    Node* node = sm_aligned_alloc(tree->node_alignment, node_size(tree));

    node->parent = parent;
    node->left = node->right = LEAF;
    node->color = Red;

    memcpy(node->data + tree->node_padding, element, tree->element_size);

    return node;
}

static Node* tree_clone(SmRBTree const* tree, Node const* root) {
    if (!root || root == LEAF)
        return root;

    Node* clone = sm_aligned_alloc(tree->node_alignment, node_size(tree));

    clone->parent = NULL;

    clone->left = tree_clone(tree, root->left);
    if (clone->left != LEAF)
        clone->left->parent = clone;

    clone->right = tree_clone(tree, root->right);
    if (clone->right != LEAF)
        clone->right->parent = clone;

    clone->color = root->color;

    memcpy(clone->data + tree->node_padding, root->data + tree->node_padding, tree->element_size);

    return clone;
}

static void tree_drop(Node* root) {
    if (!root || root == LEAF)
        return;

    Node* left = root->left;
    Node* right = root->right;

    free(root);

    tree_drop(left);
    tree_drop(right);
}

// Repair functions (private)
static void rotate_left(Node* n) {
    Node* nnew = n->right;
    Node* p = n->parent;

    assert(nnew != LEAF);

    n->right = nnew->left;
    nnew->left = n;
    n->parent = nnew;

    if (n->right != LEAF)
        n->right->parent = n;

    if (p) {
        if (n == p->left)
            p->left = nnew;
        else
            p->right = nnew;
    }

    nnew->parent = p;
}

static void rotate_right(Node* n) {
    Node* nnew = n->left;
    Node* p = n->parent;

    assert(nnew != LEAF);

    n->left = nnew->right;
    nnew->right = n;
    n->parent = nnew;

    if (n->left != LEAF)
        n->left->parent = n;

    if (p) {
        if (n == p->left)
            p->left = nnew;
        else
            p->right = nnew;
    }

    nnew->parent = p;
}

static void repair_after_insert(SmRBTree* tree, Node* n) {
    if (!n->parent) {
        n->color = Black;
    } else if (n->parent->color != Black) { // Implies parent is not root
        Node* g = node_grandparent(n);
        Node* u = node_uncle(n);

        if (u->color == Red) {
            n->parent->color = Black;
            u->color = Black;
            g->color = Red;
            return repair_after_insert(g); // Tail call
        } else {
            // Bring n on the outside of g
            if (n == g->left->right) {
                rotate_left(n->parent);
                n = n->left;
            } else if (n == g->right->left) {
                rotate_right(n->parent);
                n = n->right;
            }

            // Rotate g
            if (n == n->parent->left)
                rotate_right(g);
            else
                rotate_left(g);

            g->parent->color = Black;
            g->color = Red;

            // Update root
            if (!g->parent->parent)
                tree->root = g->parent;
        }
    }
}

// Lifetime management
SmRBTree sm_rbtree(size_t element_size, size_t element_alignment, SmKeyFunction key) {
    size_t padding = element_alignment - (offsetof(Node, element) % element_alignment);
    size_t alignment = (element_alignment > alignof(Node)) ? element_alignment : alignof(Node);

    return (SmRBTree){
        element_size,
        padding,
        alignment,
        key,
        NULL
    };
}

SmRBTree sm_rbtree_clone(SmRBTree const* tree) {
    SmRBTree clone = *tree;
    clone.root = tree_clone(tree->root);
    return clone;
}

void sm_rbtree_drop(SmRBTree* tree) {
    tree_drop(tree->root);
    tree->root = NULL;
}

// Modifiers
void* sm_rbtree_insert(SmRBTree* tree, void const* element) {
    // If empty, create root directly
    if (!tree->root) {
        tree->root = node(tree, NULL, element);
        tree->root->color = Black;
        return tree->root->data + tree->node_padding;
    }

    SmKey key = tree->key(element);

    // Find insertion point
    Node* node = tree->root;
    void* node_element = node->data + tree->node_padding;
    int cmp = sm_key_compare(key, tree->key(node_element));

    while ((cmp < 0 && node->left != LEAF) || (cmp > 0 && node->right != LEAF)) {
        node = (cmp < 0) ? node->left : node->right;

        node_element = node->data + tree->node_padding;
        cmp = sm_key_compare(key, tree->key(node_element));
    }

    // Element found, return node
    if (cmp == 0)
        return node_element;

    // Insert new node
    node = node(tree, node, element);
    node_element = node->data + tree->node_padding;

    if (cmp < 0)
        node->parent->left = node;
    else
        node->parent->right = node;

    // Repair tree
    repair_after_insert(root, node);

    return node_element;
}

void sm_rbtree_erase(SmRBTree* tree, void* element) {
    // Find node from element
    Node* node = (Node*) (((uint8_t*) element) + tree->element_size - tree->node_size);

    if (node->left != LEAF && node->right != LEAF) {
        // If node has two non-leaf children, swap it with predecessor
        Node* predecessor = node->left;
        while (predecessor->right != LEAF)
            predecessor = predecessor->right;

        Node* tmpn = predecessor->parent;
        predecessor->parent = node->parent;
        node->parent = (tmpn != node) ? tmpn : predecessor;

        tmpn = predecessor->left;
        predecessor->left = (node->left != predecessor) ? node->left : node;
        node->left = tmpn;

        tmpn = predecessor->right;
        predecessor->right = node->right;
        node->right = tmpn;

        Color tmpc = predecessor->color;
        predecessor->color = node->color;
        node->color = tmpc;

        // Update root if necessary
        if (!predecessor->parent)
            tree->root = predecessor;
    }

    Node* child = (node->left != LEAF) ? node->left : node->right;
    

    if (node->color == Red) {
        node->parent->
    }
}

// Lookup
void* sm_rbtree_find(SmRBTree const* tree, void const* element) {
    return sm_rbtree_find_by_key(tree, tree->key(element));
}

void* sm_rbtree_find_by_key(SmRBTree const* tree, SmRBTreeKey key) {
    if (!tree->root)
        return NULL;

    Node* node = tree->root;

    while (node != LEAF) {
        void* element = node->data + tree->node_padding;
        int cmp = sm_key_compare(key, tree->key(element));

        if (cmp == 0)
            return element;
        else if (cmp < 0)
            node = node->left;
        else
            node = node->right;
    }

    return NULL;
}
