#include "context.h"
#include "context_p.h"

// Inlines
extern inline void sm_context_enter_frame(SmContext* ctx, SmStackFrame* frame, SmString name, SmValue fn);
extern inline void sm_context_exit_frame(SmContext* ctx);

void sm_context_init(SmContext* ctx, SmGCConfig gc) {
    *ctx = (SmContext){
        sm_word_set(),
        sm_rbtree(sizeof(External), sm_alignof(External), sm_word_key, sm_key_compare_ptr),
        sm_stack_frame(NULL, sm_string_from_cstring("<main>"), sm_value_nil()),
        &ctx->main,
        sm_heap(gc)
    };
}

void sm_context_drop(SmContext* ctx) {
    sm_word_set_drop(&ctx->words);
    sm_stack_frame_drop(&ctx->main);
    sm_heap_drop(&ctx->heap);
}

// External management
void sm_context_register_function(SmContext* ctx, SmWord id, SmExternalFunction fn) {
    External b = { id, Function, { .function = fn } };
    sm_rbtree_insert(&ctx->externals, &b);
}

void sm_context_register_variable(SmContext* ctx, SmWord id, SmExternalVariable var) {
    External b = { id, Variable, { .variable = var } };
    sm_rbtree_insert(&ctx->externals, &b);
}

void sm_context_unregister_external(SmContext* ctx, SmWord id) {
    sm_rbtree_erase(&ctx->externals, sm_rbtree_find_by_key(&ctx->externals, sm_word_key(&id)));
}

SmExternalFunction sm_context_lookup_function(SmContext* ctx, SmWord id) {
    External* b = (External*) sm_rbtree_find_by_key(&ctx->externals, sm_word_key(&id));
    return (b && b->type == Function) ? b->fn.function : NULL;
}

SmExternalVariable sm_context_lookup_variable(SmContext* ctx, SmWord id) {
    External* b = (External*) sm_rbtree_find_by_key(&ctx->externals, sm_word_key(&id));
    return (b && b->type == Variable) ? b->fn.variable : NULL;
}
