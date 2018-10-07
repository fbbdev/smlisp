#pragma once

#include "scope.h"
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

SmCons* sm_heap_alloc_cons(SmHeap* heap, struct SmContext const* ctx);
SmScope* sm_heap_alloc_scope(SmHeap* heap, struct SmContext const* ctx, SmScope* parent);
char* sm_heap_alloc_string(SmHeap* heap, struct SmContext const* ctx, size_t length);

SmValue* sm_heap_root_value(SmHeap* heap);
SmScope** sm_heap_root_scope(SmHeap* heap);
void sm_heap_root_value_drop(SmHeap* heap, struct SmContext const* ctx, SmValue* root);
void sm_heap_root_scope_drop(SmHeap* heap, struct SmContext const* ctx, SmScope** root);

void sm_heap_unref(SmHeap* heap, struct SmContext const* ctx, uint8_t count);

void sm_heap_gc(SmHeap* heap, struct SmContext const* ctx);
