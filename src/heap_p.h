#pragma once

#include "heap.h"

typedef struct SmHeapObject {
    struct SmHeapObject* next;
    bool marked : 1;

    SmCons cons;
} Object;

typedef struct SmHeapRoot {
    struct SmHeapRoot* next;
    struct SmHeapRoot* prev;
    SmValue value;
} Root;
