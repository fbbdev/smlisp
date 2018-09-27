#include "heap.h"
#include "heap_p.h"

#include "util.h"

#include <stdint.h>

// Inlines
extern inline SmHeap sm_heap(SmGCConfig gc);
extern inline size_t sm_heap_size(SmHeap const* heap);

// Private helpers
static inline bool should_collect(struct SmGCStatus const* gc) {
    return (gc->object_count >= gc->object_threshold) ||
           (gc->unref_count >= gc->config.unref_threshold);
}

static inline Object* object_new(Object* next) {
    Object* obj = sm_aligned_alloc(sm_alignof(Object), sizeof(Object));

    *obj = (Object){ next, false, { sm_value_nil(), sm_value_nil() } };

    return obj;
}

static inline Object* object_from_cons(SmCons* cons) {
    return (Object*) (((uint8_t*) cons) - offsetof(Object, cons));
}

static inline Root* root_from_value(SmValue* value) {
    return (Root*) (((uint8_t*) value) - offsetof(Root, value));
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

    for (Root *r = heap->roots, *next; r; r = next) {
        next = r->next;
        free(r);
    }

    heap->objects = NULL;
    heap->roots = NULL;

    // Reset gc status
    heap->gc.object_count = heap->gc.unref_count = 0;
    heap->gc.object_threshold = heap->gc.config.object_threshold;
}

SmCons* sm_heap_alloc(SmHeap* heap, SmStackFrame const* frame) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, frame);

    Object* obj = object_new(heap->objects);
    heap->objects = obj;

    ++heap->gc.object_count;

    return &obj->cons;
}

SmValue* sm_heap_root(SmHeap* heap) {
    Root* r = sm_aligned_alloc(sm_alignof(Root), sizeof(Root));

    *r = (Root){ heap->roots, NULL, sm_value_nil() };

    if (heap->roots)
        heap->roots->prev = r;

    heap->roots = r;

    return &r->value;
}

void sm_heap_root_drop(SmHeap* heap, SmStackFrame const* frame, SmValue* root) {
    Root* r = root_from_value(root);

    if (r->prev)
        r->prev->next = r->next;
    else
        heap->roots = r->next;

    if (r->next)
        r->next->prev = r->prev;

    if (sm_value_is_cons(r->value))
        ++heap->gc.unref_count;

    free(r);

    if (should_collect(&heap->gc))
        sm_heap_gc(heap, frame);
}

void sm_heap_unref(SmHeap* heap, SmStackFrame const* frame, uint8_t count) {
    heap->gc.unref_count += count;

    if (should_collect(&heap->gc))
        sm_heap_gc(heap, frame);
}

void sm_heap_gc(SmHeap* heap, SmStackFrame const* frame) {
    // Mark phase

    // Mark roots
    for (Root* r = heap->roots; r; r = r->next) {
        if (sm_value_is_cons(r->value) && r->value.data.cons)
            gc_mark(object_from_cons(r->value.data.cons));
    }

    // Walk stack and mark live objects
    for (; frame; frame = frame->parent) {
        if (sm_value_is_cons(frame->fn) && frame->fn.data.cons)
            gc_mark(object_from_cons(frame->fn.data.cons));

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
