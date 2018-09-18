#include "heap.h"
#include "heap_p.h"

#include "util.h"

#include <stdint.h>

// Inlines
extern inline SmHeap sm_heap(SmGCConfig gc);
extern inline size_t sm_heap_size(SmHeap const* heap);
extern inline void sm_heap_disown_value(SmHeap* heap, SmStackFrame const* frame, SmValue v);

// Private helpers
static inline bool should_collect(struct SmGCStatus const* gc) {
    return (gc->object_count >= gc->object_threshold) ||
           (gc->unref_count >= gc->config.unref_threshold);
}

static inline Object* object_new(Object* next) {
    Object* obj = sm_aligned_alloc(sm_alignof(Object), sizeof(Object));

    *obj = (Object){
        next, false, false,
        { sm_value_nil(), sm_value_nil() }
    };

    return obj;
}

static inline Object* object_from_cons(SmCons* cons) {
    return (Object*) (((uint8_t*) cons) - offsetof(Object, cons));
}

static void gc_mark(Object* obj) {
    while (!obj->marked) {
        obj->marked = true;

        if (sm_value_is_cons(obj->cons.car) && obj->cons.car.data.cons)
            gc_mark(object_from_cons(obj->cons.car.data.cons));

        if (sm_value_is_cons(obj->cons.cdr) && obj->cons.cdr.data.cons)
            obj = object_from_cons(obj->cons.cdr.data.cons);
    }
}

// Heap functions
void sm_heap_drop(SmHeap* heap) {
    for (Object *obj = heap->objects, *next; obj; obj = next) {
        next = obj->next;
        free(obj);
    }

    for (Object *obj = heap->owned, *next; obj; obj = next) {
        next = obj->next;
        free(obj);
    }

    heap->objects = heap->owned = NULL;

    // Reset gc status
    heap->gc.object_count = heap->gc.unref_count = 0;
    heap->gc.object_threshold = heap->gc.config.object_threshold;
}

SmCons* sm_heap_alloc(SmHeap* heap, SmStackFrame const* frame) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, frame);

    Object* obj = object_new(heap->objects);
    heap->objects = obj;

    return &obj->cons;
}

SmCons* sm_heap_alloc_owned(SmHeap* heap, SmStackFrame const* frame) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, frame);

    Object* obj = object_new(heap->owned);
    obj->owned = true;

    heap->owned = obj;

    return &obj->cons;
}

void sm_heap_unref(SmHeap* heap, SmStackFrame const* frame, uint8_t count) {
    heap->gc.unref_count += count;

    if (should_collect(&heap->gc))
        sm_heap_gc(heap, frame);
}

void sm_heap_disown(SmHeap* heap, SmStackFrame const* frame, SmCons* cons) {
    Object* obj = object_from_cons(cons);

    if (obj->owned) {
        obj->owned = false;
        ++heap->gc.unref_count;

        if (should_collect(&heap->gc))
            sm_heap_gc(heap, frame);
    }
}

void sm_heap_gc(SmHeap* heap, SmStackFrame const* frame) {
    // Mark phase

    // Mark owned objects
    for (Object *obj = heap->owned, *next; obj; obj = next) {
        next = obj->next;

        if (obj->owned) {
            gc_mark(obj);
        } else {
            // Disowned, move to normal objects
            obj->next = heap->objects;
            heap->objects = obj;
        }
    }

    // Walk stack and mark live objects
    for (; frame; frame = frame->parent) {
        for (SmVariable* var = sm_scope_first(&frame->scope); var; var = sm_scope_next(&frame->scope, var)) {
            if (sm_value_is_cons(var->value) && var->value.data.cons)
                gc_mark(object_from_cons(var->value.data.cons));
        }
    }

    // Sweep phase
    Object** objp = &heap->objects;

    while (*objp) {
        Object* obj = *objp;

        if (!obj->marked) {
            *objp = obj->next; // Update list
            --heap->gc.object_count;
            free(obj);
        } else {
            obj->marked = false;
            objp = &obj->next;
        }
    }

    // Clear marked flag for owned objects
    for (Object* obj = heap->owned; obj; obj = obj->next)
        obj->marked = false;

    // Update gc status
    if (heap->gc.object_count >= heap->gc.object_threshold)
    {
        heap->gc.object_threshold *= heap->gc.config.object_threshold_factor;
    }
    else if (heap->gc.object_threshold > heap->gc.config.object_threshold &&
             heap->gc.object_count < (heap->gc.object_threshold/heap->gc.config.object_threshold_factor))
    {
        heap->gc.object_threshold /= heap->gc.config.object_threshold_factor;
    }

    heap->gc.unref_count = 0;
}
