#pragma once

#include "error.h"
#include "heap.h"
#include "stack.h"
#include "util.h"
#include "value.h"
#include "word.h"

typedef struct SmContext {
    SmWordSet words;

    SmRBTree externals;

    SmStackFrame main;
    SmStackFrame* frame;

    SmHeap heap;
} SmContext;

typedef SmError (*SmExternalFunction)(SmContext* ctx, SmCons* params, SmValue* ret);
typedef SmError (*SmExternalVariable)(SmContext* ctx, SmValue* ret);

// Context functions
void sm_context_init(SmContext* ctx, SmGCConfig gc);
void sm_context_drop(SmContext* ctx);

inline void sm_context_enter_frame(SmContext* ctx, SmStackFrame* frame, SmString name) {
    *frame = sm_stack_frame(ctx->frame, name);
    ctx->frame = frame;
}

inline void sm_context_exit_frame(SmContext* ctx) {
    SmStackFrame* frame = ctx->frame;

    if (frame->parent) { // Never exit main frame
        ctx->frame = frame->parent;

        sm_heap_unref(&ctx->heap, ctx->frame, sm_scope_size(&frame->scope));
        sm_stack_frame_drop(frame);
    }
}

// External management
void sm_context_register_function(SmContext* ctx, SmWord id, SmExternalFunction fn);
void sm_context_register_variable(SmContext* ctx, SmWord id, SmExternalVariable var);

void sm_context_unregister_external(SmContext* ctx, SmWord id);

SmExternalFunction sm_context_lookup_function(SmContext* ctx, SmWord id);
SmExternalVariable sm_context_lookup_variable(SmContext* ctx, SmWord id);
