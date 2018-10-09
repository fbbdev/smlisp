#include "context.h"
#include "heap.h"
#include "private/heap.h"

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

static inline void object_drop(Object* obj) {
    if (obj->type == Scope)
        sm_scope_drop(&obj->data.scope);
    else if (obj->type == Function)
        sm_function_drop(&obj->data.function);

    free(obj);
}

static inline Object* object_from_pointer(Type type, void const* ptr) {
    const size_t offset =
        (type == Cons) ? offsetof(union Data, cons)
      : (type == Scope) ? offsetof(union Data, scope)
      : (type == String) ? offsetof(union Data, string)
      : offsetof(union Data, function);

    return (Object*) (((uint8_t*) ptr) - offsetof(Object, data) - offset);
}

static inline Root* root_from_pointer(Type type, void const* ptr) {
    const size_t offset =
        (type == Value) ? offsetof(union Ptr, value)
      : (type == Scope) ? offsetof(union Ptr, scope)
      : 0;

    return (Root*) (((uint8_t*) ptr) - offsetof(Root, ptr) - offset);
}

static void gc_mark(Object* obj);

static inline void gc_mark_value(SmValue value) {
    if (sm_value_is_cons(value) && value.data.cons)
        gc_mark(object_from_pointer(Cons, value.data.cons));
    else if (sm_value_is_string(value) && value.data.string.buffer)
        gc_mark(object_from_pointer(String, value.data.string.buffer));
    else if (sm_value_is_function(value) && value.data.function)
        gc_mark(object_from_pointer(Function, value.data.function));
}

static void gc_mark(Object* obj) {
    while (!obj->marked) {
        obj->marked = true;

        if (obj->type == Cons) {
            gc_mark_value(obj->data.cons.car);

            if (sm_value_is_cons(obj->data.cons.cdr) && obj->data.cons.cdr.data.cons)
                obj = object_from_pointer(Cons, obj->data.cons.cdr.data.cons);
            else
                gc_mark_value(obj->data.cons.cdr);
        } else if (obj->type == Scope) {
            for (SmVariable* var = sm_scope_first(&obj->data.scope); var; var = sm_scope_next(&obj->data.scope, var))
                gc_mark_value(var->value);

            if (obj->data.scope.parent)
                gc_mark(object_from_pointer(Scope, obj->data.scope.parent));
        } else if (obj->type == Function) {
            if (obj->data.function.capture)
                gc_mark(object_from_pointer(Scope, obj->data.function.capture));

            if (obj->data.function.progn)
                obj = object_from_pointer(Cons, obj->data.function.progn);
        }
    }
}

// Heap functions
void sm_heap_drop(SmHeap* heap) {
    for (Object *obj = heap->objects, *next; obj; obj = next) {
        next = obj->next;
        object_drop(obj);
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

SmCons* sm_heap_alloc_cons(SmHeap* heap, SmContext const* ctx) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, ctx);

    Object* obj = object_new(heap->objects, Cons, 0);
    heap->objects = obj;

    ++heap->gc.object_count;

    return &obj->data.cons;
}

SmScope* sm_heap_alloc_scope(SmHeap* heap, SmContext const* ctx, SmScope* parent) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, ctx);

    Object* obj = object_new(heap->objects, Scope, 0);
    heap->objects = obj;

    ++heap->gc.object_count;

    obj->data.scope = sm_scope(parent);
    return &obj->data.scope;
}

char* sm_heap_alloc_string(SmHeap* heap, SmContext const* ctx, size_t length) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, ctx);

    Object* obj = object_new(heap->objects, String, sizeof(char)*length);
    heap->objects = obj;

    ++heap->gc.object_count;

    obj->data.string = '\0';
    return &obj->data.string;
}

SmFunction* sm_heap_alloc_function(SmHeap* heap, SmContext const* ctx) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, ctx);

    Object* obj = object_new(heap->objects, Function, 0);
    heap->objects = obj;

    ++heap->gc.object_count;

    obj->data.function = (SmFunction){
        false, { { NULL, 0 }, NULL, 0, { NULL, false, false} }, NULL, NULL
    };
    return &obj->data.function;
}


SmValue* sm_heap_root_value(SmHeap* heap) {
    Root* r = sm_aligned_alloc(sm_alignof(Root), sizeof(Root));

    *r = (Root){ heap->roots, NULL, Value, { .value = sm_value_nil() } };

    if (heap->roots)
        heap->roots->prev = r;

    heap->roots = r;

    return &r->ptr.value;
}

SmScope** sm_heap_root_scope(SmHeap* heap) {
    Root* r = sm_aligned_alloc(sm_alignof(Root), sizeof(Root));

    *r = (Root){ heap->roots, NULL, Scope, { .scope = NULL } };

    if (heap->roots)
        heap->roots->prev = r;

    heap->roots = r;

    return &r->ptr.scope;
}

void sm_heap_root_value_drop(SmHeap* heap, SmContext const* ctx, SmValue* root) {
    Root* r = root_from_pointer(Value, root);

    if (r->prev)
        r->prev->next = r->next;
    else
        heap->roots = r->next;

    if (r->next)
        r->next->prev = r->prev;

    if (sm_value_is_cons(r->ptr.value))
        ++heap->gc.unref_count;

    free(r);

    if (should_collect(&heap->gc))
        sm_heap_gc(heap, ctx);
}

void sm_heap_root_scope_drop(SmHeap* heap, SmContext const* ctx, SmScope** root) {
    Root* r = root_from_pointer(Scope, root);

    if (r->prev)
        r->prev->next = r->next;
    else
        heap->roots = r->next;

    if (r->next)
        r->next->prev = r->prev;

    if (r->ptr.scope)
        heap->gc.unref_count += 1 + sm_scope_size(r->ptr.scope);

    free(r);

    if (should_collect(&heap->gc))
        sm_heap_gc(heap, ctx);
}

void sm_heap_unref(SmHeap* heap, SmContext const* ctx, uint8_t count) {
    heap->gc.unref_count += count;

    if (should_collect(&heap->gc))
        sm_heap_gc(heap, ctx);
}

void sm_heap_gc(SmHeap* heap, SmContext const* ctx) {
    // Mark phase

    // Mark roots
    for (Root* r = heap->roots; r; r = r->next) {
        if (r->type == Value)
            gc_mark_value(r->ptr.value);
        else if (r->ptr.scope)
            gc_mark(object_from_pointer(Scope, r->ptr.scope));
    }

    if (ctx) {
        // Ensure current and global scope are marked
        if (ctx->scope)
            gc_mark(object_from_pointer(Scope, ctx->scope));

        if (ctx->main.saved_scope)
            gc_mark(object_from_pointer(Scope, ctx->main.saved_scope));

        // Walk stack and mark live scopes
        for (SmStackFrame* frame = ctx->frame; frame; frame = frame->parent)
            if (frame->saved_scope)
                gc_mark(object_from_pointer(Scope, frame->saved_scope));
    }

    // Sweep phase
    Object** objp = &heap->objects;

    while (*objp) {
        Object* obj = *objp;

        if (!obj->marked) {
            *objp = obj->next; // Update list
            --heap->gc.object_count;

            object_drop(obj);
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
