#pragma once

#include "context.h"
#include "error.h"
#include "symbol.h"
#include "value.h"

#include <stdlib.h>

typedef struct SmArgPatternArg {
    SmSymbol id;
    bool eval;
} SmArgPatternArg;

typedef struct SmArgPattern {
    SmString name;
    SmArgPatternArg const* args;
    size_t count;

    struct {
        SmSymbol id;
        bool eval;
        bool use;
    } rest;
} SmArgPattern;

SmError sm_arg_pattern_validate_spec(SmContext* ctx, SmValue spec);

SmArgPattern sm_arg_pattern_from_spec(SmString name, SmValue spec);

inline void sm_arg_pattern_drop(SmArgPattern* pattern) {
    if (pattern->args)
        free((void*) pattern->args);

    pattern->args = NULL;
    pattern->count = 0;
}

SmError sm_arg_pattern_eval(SmArgPattern const* pattern, SmContext* ctx, SmValue args, SmValue* ret);
SmError sm_arg_pattern_unpack(SmArgPattern const* pattern, SmContext* ctx, SmScope* scope, SmValue args);
