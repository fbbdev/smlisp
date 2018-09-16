#pragma once

#include "stack.h"
#include "value.h"

#include <stdint.h>

typedef struct SmGCConfig {
    size_t object_threshold;
    uint8_t object_threshold_factor;
    uint8_t unref_threshold;
} SmGCConfig;

typedef struct SmHeap {
    struct SmHeapObject* objects;
    struct SmHeapObject* owned;

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

SmCons* sm_heap_alloc(SmHeap* heap, SmStackFrame const* frame);
SmCons* sm_heap_alloc_owned(SmHeap* heap, SmStackFrame const* frame);

void sm_heap_unref(SmHeap* heap, SmStackFrame const* frame, uint8_t count);
void sm_heap_disown(SmHeap* heap, SmStackFrame const* frame, SmCons* obj);

void sm_heap_gc(SmHeap* heap, SmStackFrame const* frame);
