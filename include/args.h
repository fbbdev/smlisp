#pragma once

#include "error.h"
#include "context.h"
#include "value.h"
#include "word.h"

#include <stdlib.h>

typedef struct SmArgPatternArg {
    SmWord id;
    bool eval;
} SmArgPatternArg;

typedef struct SmArgPattern {
    SmArgPatternArg const* args;
    size_t count;

    struct {
        SmWord id;
        bool eval;
        bool use;
    } rest;
} SmArgPattern;

SmError sm_arg_pattern_validate_spec(SmContext* ctx, SmValue spec);

SmArgPattern sm_arg_pattern_from_spec(SmValue spec);

inline void sm_arg_pattern_drop(SmArgPattern* pattern) {
    if (pattern->args)
        free((void*) pattern->args);

    pattern->args = NULL;
    pattern->count = 0;
}

SmError sm_arg_pattern_eval(SmArgPattern const* pattern, SmContext* ctx, SmValue args, SmValue* ret);
SmError sm_arg_pattern_unpack(SmArgPattern const* pattern, SmContext* ctx, SmValue args);
