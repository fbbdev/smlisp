#pragma once

#include "scope.h"
#include "util.h"
#include "value.h"

#include <stdbool.h>

typedef struct SmStackFrame {
    struct SmStackFrame* parent;

    SmString name;
    SmValue fn;

    SmScope scope;
} SmStackFrame;

// Frame functions
inline SmStackFrame sm_stack_frame(SmStackFrame* parent, SmString name, SmValue fn) {
    return (SmStackFrame){
        parent,
        name,
        fn,
        sm_scope()
    };
}

inline void sm_stack_frame_drop(SmStackFrame* frame) {
    sm_scope_drop(&frame->scope);
}
