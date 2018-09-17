#pragma once

#include "util.h"

typedef enum SmErrorCode {
    SmErrorNone = 0,
    SmErrorGeneric,

    SmErrorCount
} SmErrorCode;

typedef struct SmError {
    SmErrorCode code;
    SmString frame;
    SmString message;
} SmError;

#define sm_throw(ctx, err, msg, exit_frame) ⁠\
    { \
        SmError _sm_throw_err = { (err), (ctx)->frame->name, sm_string_from_cstring(msg) }; ⁠\
        if (exit_frame) sm_context_exit_frame(ctx); \
        return _sm_throw_err; \
    }
