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

static inline Object* object_new(Object* next, Type type, size_t size) {
    size += offsetof(Object, data);

    Object* obj = sm_aligned_alloc(sm_alignof(Object), (size > sizeof(Object)) ? size : sizeof(Object));
    *obj = (Object){ next, type, false, { .cons = { sm_value_nil(), sm_value_nil() } } };

    return obj;
}

static inline Object* object_from_pointer(Type type, void const* ptr) {
    return (Object*) (((uint8_t*) ptr) - offsetof(Object, data) -
        ((type == Cons) ? offsetof(union Data, cons) : offsetof(union Data, string)));
}

static inline Root* root_from_value(SmValue* value) {
    return (Root*) (((uint8_t*) value) - offsetof(Root, value));
}

static void gc_mark(Object* obj) {
    while (!obj->marked) {
        obj->marked = true;

        if (obj->type == Cons) {
            if (sm_value_is_cons(obj->data.cons.car) && obj->data.cons.car.data.cons)
                gc_mark(object_from_pointer(Cons, obj->data.cons.car.data.cons));

            if (sm_value_is_cons(obj->data.cons.cdr) && obj->data.cons.cdr.data.cons)
                obj = object_from_pointer(Cons, obj->data.cons.cdr.data.cons);
        }
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

SmCons* sm_heap_alloc_cons(SmHeap* heap, SmStackFrame const* frame) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, frame);

    Object* obj = object_new(heap->objects, Cons, 0);
    heap->objects = obj;

    ++heap->gc.object_count;

    return &obj->data.cons;
}

char* sm_heap_alloc_string(SmHeap* heap, SmStackFrame const* frame, size_t length) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, frame);

    Object* obj = object_new(heap->objects, String, sizeof(char)*length);
    heap->objects = obj;

    ++heap->gc.object_count;

    obj->data.string = '\0';
    return &obj->data.string;
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
            gc_mark(object_from_pointer(Cons, r->value.data.cons));
        else if (sm_value_is_string(r->value) && r->value.data.string.data)
            gc_mark(object_from_pointer(String, r->value.data.string.data));
    }

    // Walk stack and mark live objects
    for (; frame; frame = frame->parent) {
        if (sm_value_is_cons(frame->fn) && frame->fn.data.cons)
            gc_mark(object_from_pointer(Cons, frame->fn.data.cons));
        else if (sm_value_is_string(frame->fn) && frame->fn.data.string.data)
            gc_mark(object_from_pointer(String, frame->fn.data.string.data));

        for (SmVariable* var = sm_scope_first(&frame->scope); var; var = sm_scope_next(&frame->scope, var)) {
            if (sm_value_is_cons(var->value) && var->value.data.cons)
                gc_mark(object_from_pointer(Cons, var->value.data.cons));
            if (sm_value_is_string(var->value) && var->value.data.string.data)
                gc_mark(object_from_pointer(String, var->value.data.string.data));
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
