#pragma once

#include "heap.h"

typedef struct SmHeapObject {
    struct SmHeapObject* next;
    bool marked : 1;
    bool owned : 1;

    SmCons cons;
} Object;
