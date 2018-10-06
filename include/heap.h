#pragma once

#include "stack.h"
#include "util.h"
#include "value.h"

#include <stdint.h>

typedef struct SmGCConfig {
    size_t object_threshold;
    uint8_t object_threshold_factor;
    uint8_t unref_threshold;
} SmGCConfig;

typedef struct SmHeap {
    struct SmHeapObject* objects;
    struct SmHeapRoot* roots;

    struct SmGCStatus {
        SmGCConfig config;
        size_t object_count;
        size_t object_threshold;
        uint16_t unref_count;
    } gc;
} SmHeap;

inline SmHeap sm_heap(SmGCConfig gc) {
    return (SmHeap){ NULL, NULL, { gc, 0, gc.object_threshold, 0 } };
}

void sm_heap_drop(SmHeap* heap);

inline size_t sm_heap_size(SmHeap const* heap) {
    return heap->gc.object_count;
}

SmCons* sm_heap_alloc_cons(SmHeap* heap, SmStackFrame const* frame);
SmScope* sm_heap_alloc_scope(SmHeap* heap, SmStackFrame const* frame);
char* sm_heap_alloc_string(SmHeap* heap, SmStackFrame const* frame, size_t length);

SmValue* sm_heap_root(SmHeap* heap);
void sm_heap_root_drop(SmHeap* heap, SmStackFrame const* frame, SmValue* root);

void sm_heap_unref(SmHeap* heap, SmStackFrame const* frame, uint8_t count);

void sm_heap_gc(SmHeap* heap, SmStackFrame const* frame);
