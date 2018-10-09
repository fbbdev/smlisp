#include "context.h"
#include "private/context.h"

// Inlines
extern inline void sm_context_enter_frame(SmContext* ctx, SmStackFrame* frame, SmString name);
extern inline void sm_context_exit_frame(SmContext* ctx);

SmContext* sm_context(SmGCConfig gc) {
    SmContext* ctx = sm_aligned_alloc(sm_alignof(SmContext), sizeof(SmContext));

    *ctx = (SmContext){
        sm_symbol_set(),
        sm_rbtree(sizeof(External), sm_alignof(External), sm_symbol_key, sm_key_compare_ptr),

        (SmStackFrame){ NULL, sm_string_from_cstring("<main>"), &ctx->globals },
        sm_scope(NULL),

        &ctx->main,
        &ctx->globals,

        sm_heap(gc)
    };

    return ctx;
}

void sm_context_drop(SmContext* ctx) {
    sm_symbol_set_drop(&ctx->symbols);
    sm_rbtree_drop(&ctx->externals);
    sm_scope_drop(&ctx->globals);
    sm_heap_drop(&ctx->heap);
    free(ctx);
}

// External function/variable management
void sm_context_register_function(SmContext* ctx, SmSymbol id, SmExternalFunction fn) {
    External b = { id, Function, { .function = fn } };
    sm_rbtree_insert(&ctx->externals, &b);
}

void sm_context_register_variable(SmContext* ctx, SmSymbol id, SmExternalVariable var) {
    External b = { id, Variable, { .variable = var } };
    sm_rbtree_insert(&ctx->externals, &b);
}

void sm_context_unregister_external(SmContext* ctx, SmSymbol id) {
    sm_rbtree_erase(&ctx->externals, sm_rbtree_find_by_key(&ctx->externals, sm_symbol_key(&id)));
}

SmExternalFunction sm_context_lookup_function(SmContext* ctx, SmSymbol id) {
    External* b = (External*) sm_rbtree_find_by_key(&ctx->externals, sm_symbol_key(&id));
    return (b && b->type == Function) ? b->fn.function : NULL;
}

SmExternalVariable sm_context_lookup_variable(SmContext* ctx, SmSymbol id) {
    External* b = (External*) sm_rbtree_find_by_key(&ctx->externals, sm_symbol_key(&id));
    return (b && b->type == Variable) ? b->fn.variable : NULL;
}
