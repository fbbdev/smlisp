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

static Object* object_new(Type type, size_t size) {
    size += offsetof(Object, data);
    if (size < sizeof(Object))
        size = sizeof(Object);

    Object* obj = sm_aligned_alloc(sm_alignof(Object), size);
    *obj = (Object){
        NULL, NULL, NULL,
        false, false, type, 1, size,
        { .string = '\0' }
    };

    switch (type) {
        case Cons:
            obj->data.cons = (SmCons){ sm_value_nil(), sm_value_nil() };
            break;
        case Scope:
            obj->data.scope = sm_scope(NULL);
            break;
        case Function:
            obj->data.function = (SmFunction){
                false, { { NULL, 0 }, NULL, 0, { NULL, false, false} },
                NULL, NULL
            };
            break;
        default:
            break;
    }

    return obj;
}

static void object_drop(Object* obj) {
    if (obj->type == Scope)
        sm_scope_drop(&obj->data.scope);
    else if (obj->type == Function)
        sm_function_drop(&obj->data.function);

    free(obj);
}

static inline Object** object_slot(Object** root, Object* obj) {
    return (!obj->parent) ? root :
        (obj == obj->parent->left) ? &obj->parent->left : &obj->parent->right;
}

static Object* object_rotate_left(Object* obj) {
    Object* r = obj->right;
    Object* p = obj->parent;

    sm_assert(r != NULL);

    obj->right = r->left;
    r->left = obj;
    obj->parent = r;
    r->parent = p;

    if (obj->right)
        obj->right->parent = obj;

    // Update height
    size_t lh = obj->left ? obj->left->height : 0;
    size_t rh = obj->right ? obj->right->height : 0;
    obj->height = 1 + ((lh < rh) ? rh : lh);

    rh = r->right ? r->right->height : 0;
    r->height = 1 + ((obj->height < rh) ? rh : obj->height);

    return r;
}

static Object* object_rotate_right(Object* obj) {
    Object* l = obj->left;
    Object* p = obj->parent;

    sm_assert(l != NULL);

    obj->left = l->right;
    l->right = obj;
    obj->parent = l;
    l->parent = p;

    if (obj->left)
        obj->left->parent = obj;

    // Update height
    size_t lh = obj->left ? obj->left->height : 0;
    size_t rh = obj->right ? obj->right->height : 0;
    obj->height = 1 + ((lh < rh) ? rh : lh);

    lh = l->left ? l->left->height : 0;
    l->height = 1 + ((lh < obj->height) ? obj->height : lh);

    return l;
}

static void object_insert(Object** root, Object* obj) {
    Object** insp = root;
    obj->parent = NULL;

    // Find insertion point (assume node is not in tree already)
    while (*insp) {
        obj->parent = *insp;
        insp = (obj < *insp) ? &(*insp)->left : &(*insp)->right;
    }

    // Insert node
    *insp = obj;

    // Rebalance AVL tree
    for (Object* p = obj->parent; p; p = p->parent) {
        const size_t lh = p->left ? p->left->height : 0;
        const size_t rh = p->right ? p->right->height : 0;
        p->height = 1 + ((lh < rh) ? rh : lh);

        const intptr_t balance = ((intptr_t)rh) - ((intptr_t)lh);

        if (balance >= -1 && balance <= 1)
            continue;

        Object** slot = object_slot(root, p);

        if (balance < -1) { // Left cases
            if (obj < p->left) { // Left left
                *slot = object_rotate_right(p);
            } else { // Left right
                p->left = object_rotate_left(p->left);
                *slot = object_rotate_right(p);
            }
        } else { // Right cases
            if (obj > p->right) { // Right right
                *slot = object_rotate_left(p);
            } else { // Right left
                p->right = object_rotate_right(p->right);
                *slot = object_rotate_left(p);
            }
        }

        // Balancing one node is enough on insertion,
        // its height will be unchanged
        break;
    }
}

static void object_erase(Object** root, Object* obj) {
    // If node has two children, swap it with predecessor
    if (obj->left && obj->right) {
        #define SWAP(tp, a, b) { tp tmp = (a); (a) = (b); (b) = tmp; }

        Object* pred = obj->left;
        while (pred->right)
            pred = pred->right;

        SWAP(Object*, obj->parent, pred->parent);
        SWAP(Object*, obj->left, pred->left);
        SWAP(Object*, obj->right, pred->right);
        SWAP(size_t, obj->height, pred->height);

        if (pred->left == pred)
            pred->left = obj;
        else
            obj->parent->right = obj;

        pred->left->parent = pred;
        pred->right->parent = pred;

        if (!pred->parent)
            *root = pred;

        #undef SWAP
    }

    // Replace node with (possibly non-NULL) child
    Object* child = obj->left ? obj->left : obj->right;

    if (child)
        child->parent = obj->parent;

    *object_slot(root, obj) = child;

    // Rebalance AVL tree
    for (Object* p = obj->parent; p; p = p->parent) {
        const size_t lh = p->left ? p->left->height : 0;
        const size_t rh = p->right ? p->right->height : 0;
        p->height = 1 + ((lh < rh) ? rh : lh);

        const intptr_t balance = ((intptr_t)rh) - ((intptr_t)lh);

        if (balance >= -1 && balance <= 1)
            continue;

        Object** slot = object_slot(root, p);

        if (balance < -1) { // Left cases
            if (!p->right || p->left->height >= p->right->height) { // Left left
                *slot = object_rotate_right(p);
            } else { // Left right
                p->left = object_rotate_left(p->left);
                *slot = object_rotate_right(p);
            }
        } else { // Right cases
            if (!p->left || p->right->height >= p->left->height) { // Right right
                *slot = object_rotate_left(p);
            } else { // Right left
                p->right = object_rotate_right(p->right);
                *slot = object_rotate_left(p);
            }
        }

        // On deletion, we go up the tree and rebalance all nodes
        p = *slot;
    }
}

static Object* object_from_pointer(Object* root, void const* ptr) {
    Object* obj = root;
    const uintptr_t key = (uintptr_t) ptr;

    if (!ptr)
        return NULL;

    // Tree lookup for an object containing this pointer
    while (obj) {
        if (obj->all_marked)
            return NULL;

        const uintptr_t start = (uintptr_t) obj;
        const uintptr_t end = start + obj->size;

        if (key < start)
            obj = obj->left;
        else if (key < end)
            return obj;
        else
            obj = obj->right;
    }

    return obj;
}

static Object* object_first(Object* root) {
    if (!root)
        return NULL;

    while (root->left)
        root = root->left;

    return root;
}

// Traverse tree depth first
// XXX: NOT IN ORDER, SUITABLE FOR DROPPING THE WHOLE TREE
static Object* object_next(Object* obj) {
    if (!obj || !obj->parent)
        return NULL;

    if (obj == obj->parent->left && obj->parent->right)
        return object_first(obj->parent->right);

    return obj->parent;
}

// Traverse tree in order
// XXX: IN SORTED ORDER, SUITABLE FOR DROPPING SINGLE NODES
static Object* object_succ(Object* obj) {
    if (!obj)
        return NULL;

    if (obj->right)
        return object_first(obj->right);

    while (obj->parent && obj == obj->parent->right)
        obj = obj->parent;

    return obj->parent;
}

static inline Root* root_from_pointer(Type type, void const* ptr) {
    const size_t offset =
        (type == Value) ? offsetof(union Ptr, value)
      : (type == Scope) ? offsetof(union Ptr, scope)
      : 0;

    return (Root*) (((uint8_t*) ptr) - offsetof(Root, ptr) - offset);
}

static void gc_mark(Object* root, Object* obj);

static inline void gc_mark_value(Object* root, SmValue value) {
    switch (value.type) {
        case SmTypeSymbol:
            gc_mark(root, object_from_pointer(root, value.data.symbol));
            break;
        case SmTypeString:
            gc_mark(root, object_from_pointer(root, value.data.string.view.data));
            break;
        case SmTypeCons:
            gc_mark(root, object_from_pointer(root, value.data.cons));
            break;
        case SmTypeFunction:
            gc_mark(root, object_from_pointer(root, value.data.function));
            break;
        default:
            break;
    }
}

static void gc_mark(Object* root, Object* obj) {
    while (obj && !obj->marked) {
        // Mark object and ancestors if needed
        obj->marked = true;
        obj->all_marked = (!obj->left || obj->left->all_marked) &&
                          (!obj->right || obj->right->all_marked);

        if (obj->all_marked) {
            for (Object* p = obj->parent; p; p = p->parent) {
                p->all_marked = p->marked &&
                                (!p->left || p->left->all_marked) &&
                                (!p->right || p->right->all_marked);
                if (!p->all_marked)
                    break;
            }
        }

        if (obj->type == Cons) {
            gc_mark_value(root, obj->data.cons.car);

            if (sm_value_is_cons(obj->data.cons.cdr) && obj->data.cons.cdr.data.cons)
                obj = object_from_pointer(root, obj->data.cons.cdr.data.cons);
            else
                gc_mark_value(root, obj->data.cons.cdr);
        } else if (obj->type == Scope) {
            for (SmVariable* var = sm_scope_first(&obj->data.scope); var; var = sm_scope_next(&obj->data.scope, var))
                gc_mark_value(root, var->value);

            if (obj->data.scope.parent)
                gc_mark(root, object_from_pointer(root, obj->data.scope.parent));
        } else if (obj->type == Function) {
            if (obj->data.function.capture)
                gc_mark(root, object_from_pointer(root, obj->data.function.capture));

            if (obj->data.function.progn)
                obj = object_from_pointer(root, obj->data.function.progn);
        }
    }
}

// Heap functions
void sm_heap_drop(SmHeap* heap) {
    for (Object *obj = object_first(heap->objects), *next; obj; obj = next) {
        next = object_next(obj);
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

    Object* obj = object_new(Cons, 0);
    object_insert(&heap->objects, obj);

    ++heap->gc.object_count;

    return &obj->data.cons;
}

SmScope* sm_heap_alloc_scope(SmHeap* heap, SmContext const* ctx) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, ctx);

    Object* obj = object_new(Scope, 0);
    object_insert(&heap->objects, obj);

    ++heap->gc.object_count;

    return &obj->data.scope;
}

SmFunction* sm_heap_alloc_function(SmHeap* heap, SmContext const* ctx) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, ctx);

    Object* obj = object_new(Function, 0);
    object_insert(&heap->objects, obj);

    ++heap->gc.object_count;

    return &obj->data.function;
}

char* sm_heap_alloc_string(SmHeap* heap, SmContext const* ctx, size_t length) {
    if (should_collect(&heap->gc))
        sm_heap_gc(heap, ctx);

    Object* obj = object_new(String, sizeof(char)*length);
    object_insert(&heap->objects, obj);

    ++heap->gc.object_count;

    return &obj->data.string;
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
            gc_mark_value(heap->objects, r->ptr.value);
        else if (r->ptr.scope)
            gc_mark(heap->objects, object_from_pointer(heap->objects, r->ptr.scope));
    }

    if (ctx) {
        // Ensure current and global scope are marked
        if (ctx->scope)
            gc_mark(heap->objects, object_from_pointer(heap->objects, ctx->scope));

        for (SmVariable* var = sm_scope_first(&ctx->globals); var; var = sm_scope_next(&ctx->globals, var))
            gc_mark_value(heap->objects, var->value);

        // Walk stack and mark live scopes
        for (SmStackFrame* frame = ctx->frame; frame; frame = frame->parent)
            if (frame->saved_scope)
                gc_mark(heap->objects, object_from_pointer(heap->objects, frame->saved_scope));
    }

    // Sweep phase
    for (Object *obj = object_first(heap->objects), *next; obj; obj = next) {
        next = object_succ(obj);

        if (!obj->marked) {
            object_erase(&heap->objects, obj);
            object_drop(obj);
            --heap->gc.object_count;
        } else {
            obj->marked = false;
            obj->all_marked = false;
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
