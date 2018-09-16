#pragma once

#include "rbtree.h"

typedef enum Color {
    Red = 0,
    Black
} Color;

typedef struct SmRBTreeNode {
    struct SmRBTreeNode* parent;
    struct SmRBTreeNode* left;
    struct SmRBTreeNode* right;

    Color color;

    uint8_t data[];
} Node;
