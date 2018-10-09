#pragma once

#include "../../include/heap.h"
#include "../../include/scope.h"
#include "../../include/function.h"

#include <stdint.h>

#if (SIZE_MAX == 0xFFFF)
    #define SIZE_BITS 12
#elif (SIZE_MAX == 0xFFFFFFFF)
    #define SIZE_BITS 28
#elif (SIZE_MAX == 0xFFFFFFFFFFFFFFFF)
    #define SIZE_BITS 60
#else
    #error Cannot determine size_t bits
#endif

typedef enum Type {
    Symbol = 0,
    Cons,
    Scope,
    Function,
    String
} Type;

// Objects implement an AVL augmented tree
typedef struct SmHeapObject {
    struct SmHeapObject* parent;
    struct SmHeapObject* left;
    struct SmHeapObject* right;

    bool marked : 1;
    bool all_marked : 1;
    unsigned int type : 2;
    size_t height : SIZE_BITS;

    size_t size;

    union Data {
        SmString symbol;
        SmCons cons;
        SmScope scope;
        SmFunction function;
        char string;
    } data;

    uint8_t mem[];
} Object;

typedef struct SmHeapRoot {
    struct SmHeapRoot* next;
    struct SmHeapRoot* prev;

    bool value : 1;

    union Ref {
        SmValue value;
        void* any;
    } ref;
} Root;
