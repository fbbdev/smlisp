#include "rbtree.h"
#include "private/rbtree.h"

#include <stdbool.h>
#include <stdint.h>

// Inlines
extern inline SmRBTree sm_ptr_rbtree();
extern inline SmRBTree sm_string_rbtree();
extern inline bool sm_rbtree_empty(SmRBTree const* tree);

// Leaf node
static Node LEAF_v = { NULL, NULL, NULL, Black };
static Node* const LEAF = &LEAF_v;

// Private helpers
static inline Node* node_grandparent(Node* n) {
    return n->parent ? n->parent->parent : NULL;
}

static inline Node* node_sibling(Node* n) {
    return n->parent ?
      ((n == n->parent->left) ? n->parent->right : n->parent->left) : NULL;
}

static inline Node* node_uncle(Node* n) {
    return n->parent ? node_sibling(n->parent) : NULL;
}

static inline size_t node_size(SmRBTree const* tree) {
    return offsetof(Node, data) + tree->node_padding + tree->element_size;
}

static inline Node* node_new(SmRBTree const* tree, Node* parent, void const* element) {
    Node* node = sm_aligned_alloc(tree->node_alignment, node_size(tree));

    node->parent = parent;
    node->left = node->right = LEAF;
    node->color = Red;

    memcpy(node->data + tree->node_padding, element, tree->element_size);

    return node;
}

static Node* tree_clone(SmRBTree const* tree, Node const* root) {
    if (!root || root == LEAF)
        return (Node*) root;

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

static size_t tree_size(Node* root) {
    return (!root || root == LEAF) ?
        0 : 1 + tree_size(root->left) + tree_size(root->right);
}

// Repair functions (private)
static void rotate_left(Node* n) {
    Node* nnew = n->right;
    Node* p = n->parent;

    sm_assert(nnew != LEAF);

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

    sm_assert(nnew != LEAF);

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
    while (1) { // Use a loop to avoid recursion
        if (!n->parent) {
            n->color = Black;
        } else if (n->parent->color != Black) { // Implies parent is not root
            Node* g = node_grandparent(n);
            Node* u = node_uncle(n);

            if (u->color == Red) {
                n->parent->color = Black;
                u->color = Black;
                g->color = Red;
                n = g; continue; // Move up in the tree and loop around
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

                // Update root if necessary
                if (!g->parent->parent)
                    tree->root = g->parent;
            }
        }

        break; // We're done
    }
}

static void repair_after_erase(SmRBTree* tree, Node* n, Node* p) {
    // n could be a leaf node so get sibling through p (never use n->parent)
    Node* s = (n == p->left) ? p->right : p->left;

    // This function is called only when n and its previous parent were both
    // black, so n's sibling cannot be a leaf
    while (p->color == Black && s->color == Black &&
           s->left->color == Black && s->right->color == Black) {
        // Rebalance sibling
        s->color = Red;

        // If p is root, we're done
        if (!p->parent)
            return;

        // Otherwise, rebalance p's sibling (loop)
        s = (p == p->parent->left) ? p->parent->right : p->parent->left;
        p = p->parent;
        n = p;
    }

    if (s->color == Red) {
        p->color = Red;
        s->color = Black;

        if (n == p->left) {
            rotate_left(p);
            s = p->right;
        } else {
            rotate_right(p);
            s = p->left;
        }

        // Update root if necessary
        if (!p->parent->parent)
            tree->root = s;
    }

    // s is still not a leaf
    if (p->color == Red && s->color == Black &&
        s->left->color == Black && s->right->color == Black)
    {
        s->color = Red;
        p->color = Black;
        return;
    }

    if (s->color == Black) {
        if (n == p->left && s->left->color == Red && s->right->color == Black) {
            s->color = Red;
            s->left->color = Black;
            rotate_right(s);
            s = s->parent;
        } else if (n == p->right && s->right->color == Red && s->left->color == Black) {
            s->color = Red;
            s->right->color = Black;
            rotate_left(s);
            s = s->parent;
        }
    }
    // In this last case, s was not a leaf and child was red,
    // so s is still not a leaf

    s->color = p->color;
    p->color = Black;

    if (n == p->left) {
        s->right->color = Black;
        rotate_left(p);
    } else {
        s->left->color = Black;
        rotate_right(p);
    }

    // Update root if necessary
    if (!p->parent->parent)
        tree->root = p->parent;
}

// Lifetime management
SmRBTree sm_rbtree(size_t element_size, size_t element_alignment,
                   SmKeyFunction key, SmKeyComparisonFunction compare) {
    size_t padding = (element_alignment - sizeof(Node)%element_alignment) % element_alignment;

    return (SmRBTree){
        element_size,
        // Make data begin after the struct so that we can assign safely
        // See C99 standard §6.7.2.1.21
        padding + (sizeof(Node) - offsetof(Node, data)),
        sm_common_alignment(element_alignment, sm_alignof(Node)),
        key, compare,
        NULL
    };
}

SmRBTree sm_rbtree_clone(SmRBTree const* tree) {
    SmRBTree clone = *tree;
    clone.root = tree_clone(tree, tree->root);
    return clone;
}

void sm_rbtree_drop(SmRBTree* tree) {
    tree_drop(tree->root);
    tree->root = NULL;
}

// Capacity
size_t sm_rbtree_size(SmRBTree const* tree) {
    return tree_size(tree->root);
}

// Modifiers
void* sm_rbtree_insert(SmRBTree* tree, void const* element) {
    if (!element)
        return NULL;

    // If empty, create root directly
    if (!tree->root) {
        tree->root = node_new(tree, NULL, element);
        tree->root->color = Black;
        return tree->root->data + tree->node_padding;
    }

    SmKey key = tree->key(element);

    // Find insertion point
    Node* node = tree->root;
    void* node_element = node->data + tree->node_padding;
    int cmp = tree->compare(key, tree->key(node_element));

    while ((cmp < 0 && node->left != LEAF) || (cmp > 0 && node->right != LEAF)) {
        node = (cmp < 0) ? node->left : node->right;

        node_element = node->data + tree->node_padding;
        cmp = tree->compare(key, tree->key(node_element));
    }

    // Key found, copy element and return node
    if (cmp == 0) {
        memcpy(node_element, element, tree->element_size);
        return node_element;
    }

    // Insert new node
    node = node_new(tree, node, element);
    node_element = node->data + tree->node_padding;

    if (cmp < 0)
        node->parent->left = node;
    else
        node->parent->right = node;

    // Repair tree
    repair_after_insert(tree, node);

    return node_element;
}

void sm_rbtree_erase(SmRBTree* tree, void* element) {
    if (!element)
        return;

    // Find node from element
    Node* node = (Node*) (((uint8_t*) element) + tree->element_size - node_size(tree));

    if (node->left != LEAF && node->right != LEAF) {
        // If node has two non-leaf children, swap it with predecessor
        Node* predecessor = node->left;
        while (predecessor->right != LEAF)
            predecessor = predecessor->right;

        Node tmpn = *predecessor;
        *predecessor = *node;
        *node = tmpn;


        if (predecessor->left == predecessor)
            predecessor->left = node;
        else
            node->parent->right = node;

        predecessor->left->parent = predecessor;
        if (predecessor->right != LEAF)
            predecessor->right->parent = predecessor;

        // Update root if necessary
        if (!predecessor->parent)
            tree->root = predecessor;
    }

    // Replace node with (possibly) non-leaf child
    Node* child = (node->left != LEAF) ? node->left : node->right;
    bool was_black = child->color == Black;

    if (child != LEAF) {
        child->parent = node->parent;
        child->color = Black;
    }

    if (!node->parent) {
        // Node is root, update the pointer and we're done
        tree->root = (child == LEAF) ? NULL : child;
    } else {
        // Node is not root, repair the tree
        if (node == node->parent->left)
            node->parent->left = child;
        else
            node->parent->right = child;

        if (node->color == Black && was_black)
            repair_after_erase(tree, child, node->parent);
    }

    free(node);
}

// Iteration
void* sm_rbtree_first(SmRBTree const* tree) {
    if (!tree->root)
        return NULL;

    Node* first = tree->root;
    while (first->left != LEAF)
        first = first->left;

    return first->data + tree->node_padding;
}

void* sm_rbtree_next(SmRBTree const* tree, void* element) {
    if (!element)
        return NULL;

    Node* node = (Node*) (((uint8_t*) element) + tree->element_size - node_size(tree));

    // If we have a right branch, go down
    if (node->right != LEAF) {
        node = node->right;
        while (node->left != LEAF)
            node = node->left;

        return node->data + tree->node_padding;
    }

    // No right branch, backtrack until we find root or left branch
    while (node->parent && node == node->parent->right)
        node = node->parent;

    // If we still have a parent, it is our successor; if not, we are back
    // to root after exploring everything so we should return NULL anyway.
    return node->parent ? (node->parent->data + tree->node_padding) : NULL;
}

// Lookup
void* sm_rbtree_find(SmRBTree const* tree, void const* element) {
    return sm_rbtree_find_by_key(tree, tree->key(element));
}

void* sm_rbtree_find_by_key(SmRBTree const* tree, SmKey key) {
    if (!tree->root)
        return NULL;

    Node* node = tree->root;

    while (node != LEAF) {
        void* element = node->data + tree->node_padding;
        int cmp = tree->compare(key, tree->key(element));

        if (cmp == 0)
            return element;
        else if (cmp < 0)
            node = node->left;
        else
            node = node->right;
    }

    return NULL;
}
