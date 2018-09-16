#pragma once

#include "heap.h"
#include "stack.h"
#include "util.h"
#include "value.h"
#include "word.h"

typedef struct SmContext {
    SmWordSet words;

    SmStackFrame main;
    SmStackFrame* frame;

    SmHeap heap;
} SmContext;

inline void sm_context_init(SmContext* ctx, SmGCConfig gc) {
    *ctx = (SmContext){
        sm_word_set(),
        sm_stack_frame(NULL, sm_string_from_cstring("<main>")),
        &ctx->main,
        sm_heap(gc)
    };
}

void sm_context_drop(SmContext* ctx);

inline void sm_context_enter_frame(SmContext* ctx, SmStackFrame* frame, SmString name) {
    *frame = sm_stack_frame(ctx->frame, name);
    ctx->frame = frame;
}

void sm_context_exit_frame(SmContext* ctx);
