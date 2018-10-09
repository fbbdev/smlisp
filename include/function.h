#pragma once

#include "args.h"
#include "error.h"
#include "scope.h"
#include "value.h"

typedef struct SmFunction {
    bool macro;
    SmArgPattern args;
    SmScope* capture;
    SmCons* progn;
} SmFunction;

// Expects a valid lambda expression (see sm_validate_lambda)
inline SmFunction sm_function(SmString name, SmScope* capture, SmCons* lambda) {
    return (SmFunction){
        false,
        sm_arg_pattern_from_spec(name, lambda->car),
        capture,
        sm_list_next(lambda)
    };
}

// Expects a valid macro expression (see sm_validate_lambda)
inline SmFunction sm_macro(SmString name, SmScope* capture, SmCons* lambda) {
    return (SmFunction){
        true,
        sm_arg_pattern_from_spec(name, lambda->car),
        capture,
        sm_list_next(lambda)
    };
}

inline void sm_function_drop(SmFunction* function) {
    sm_arg_pattern_drop(&function->args);
}

SmError sm_function_invoke(SmFunction const* function, struct SmContext* ctx, SmValue args, SmValue* ret);

// Lambda expression handling
SmError sm_validate_lambda(struct SmContext* ctx, SmValue expr);
