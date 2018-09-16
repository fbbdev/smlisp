#include "context.h"

// Inlines
inline void sm_context_init(SmContext* ctx, SmGCConfig gc);
inline void sm_context_enter_frame(SmContext* ctx, SmStackFrame* frame, SmString name);

void sm_context_drop(SmContext* ctx) {
    sm_word_set_drop(&ctx->words);
    sm_stack_frame_drop(&ctx->main);
    sm_heap_drop(&ctx->heap);
}

void sm_context_exit_frame(SmContext* ctx) {
    SmStackFrame* frame = ctx->frame;

    if (frame->parent) { // Never exit main frame
        ctx->frame = frame->parent;

        sm_heap_unref(&ctx->heap, ctx->frame, sm_scope_size(&frame->scope));
        sm_stack_frame_drop(frame);
    }
}
