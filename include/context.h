#pragma once

#include "error.h"
#include "heap.h"
#include "scope.h"
#include "symbol.h"
#include "util.h"
#include "value.h"

typedef struct SmStackFrame {
    struct SmStackFrame* parent;

    SmString name;
    SmScope* saved_scope;
} SmStackFrame;

typedef struct SmContext {
    SmSymbolSet symbols;

    SmRBTree externals;

    SmStackFrame main;
    SmStackFrame* frame;
    SmScope* scope;

    SmHeap heap;
} SmContext;

typedef SmError (*SmExternalFunction)(SmContext* ctx, SmValue args, SmValue* ret);
typedef SmError (*SmExternalVariable)(SmContext* ctx, SmValue* ret);

// Context functions
SmContext* sm_context(SmGCConfig gc);
void sm_context_drop(SmContext* ctx);

inline void sm_context_enter_frame(SmContext* ctx, SmStackFrame* frame, SmString name) {
    *frame = (SmStackFrame){ ctx->frame, name, ctx->scope };
    ctx->frame = frame;
}

inline void sm_context_exit_frame(SmContext* ctx) {
    if (ctx->frame->parent) { // Never exit main frame
        ctx->scope = ctx->frame->saved_scope;
        ctx->frame = ctx->frame->parent;
    }
}

// External function/variable management
void sm_context_register_function(SmContext* ctx, SmSymbol id, SmExternalFunction fn);
void sm_context_register_variable(SmContext* ctx, SmSymbol id, SmExternalVariable var);

void sm_context_unregister_external(SmContext* ctx, SmSymbol id);

SmExternalFunction sm_context_lookup_function(SmContext* ctx, SmSymbol id);
SmExternalVariable sm_context_lookup_variable(SmContext* ctx, SmSymbol id);
