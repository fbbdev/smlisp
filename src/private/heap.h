#pragma once

#include "../../include/heap.h"
#include "../../include/scope.h"
#include "../../include/function.h"

#include <stdint.h>

typedef enum Type {
    Cons,
    Scope,
    String,
    Function,

    Value = Cons
} Type;

typedef struct SmHeapObject {
    struct SmHeapObject* next;
    unsigned int type : 2;
    bool marked : 1;

    union Data {
        SmCons cons;
        SmScope scope;
        char string;
        SmFunction function;
    } data;

    uint8_t mem[];
} Object;

typedef struct SmHeapRoot {
    struct SmHeapRoot* next;
    struct SmHeapRoot* prev;

    Type type;

    union Ptr {
        SmValue value;
        SmScope* scope;
    } ptr;
} Root;
