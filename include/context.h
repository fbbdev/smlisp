#pragma once

#include "error.h"
#include "heap.h"
#include "stack.h"
#include "util.h"
#include "value.h"
#include "symbol.h"

typedef struct SmContext {
    SmSymbolSet symbols;

    SmRBTree externals;

    SmStackFrame main;
    SmStackFrame* frame;

    SmHeap heap;
} SmContext;

typedef SmError (*SmExternalFunction)(SmContext* ctx, SmValue args, SmValue* ret);
typedef SmError (*SmExternalVariable)(SmContext* ctx, SmValue* ret);

// Context functions
SmContext* sm_context(SmGCConfig gc);
void sm_context_drop(SmContext* ctx);

inline void sm_context_enter_frame(SmContext* ctx, SmStackFrame* frame, SmString name, SmValue fn) {
    *frame = sm_stack_frame(ctx->frame, name, fn);
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
void sm_context_register_function(SmContext* ctx, SmSymbol id, SmExternalFunction fn);
void sm_context_register_variable(SmContext* ctx, SmSymbol id, SmExternalVariable var);

void sm_context_unregister_external(SmContext* ctx, SmSymbol id);

SmExternalFunction sm_context_lookup_function(SmContext* ctx, SmSymbol id);
SmExternalVariable sm_context_lookup_variable(SmContext* ctx, SmSymbol id);
