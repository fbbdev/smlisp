#pragma once

#include "heap.h"
#include "scope.h"

#include <stdint.h>

typedef enum Type {
    Cons,
    Scope,
    String
} Type;

typedef struct SmHeapObject {
    struct SmHeapObject* next;
    unsigned int type : 2;
    bool marked : 1;

    union Data {
        SmCons cons;
        SmScope scope;
        char string;
    } data;
    uint8_t mem[];
} Object;

typedef struct SmHeapRoot {
    struct SmHeapRoot* next;
    struct SmHeapRoot* prev;
    SmValue value;
} Root;
