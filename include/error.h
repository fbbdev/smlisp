#pragma once

#include "util.h"

typedef enum SmErrorCode {
    SmErrorOk = 0,
    SmErrorMissingArguments,
    SmErrorExcessArguments,
    SmErrorInvalidArgument,
    SmErrorUndefinedVariable,
    SmErrorGeneric,

    SmErrorCount
} SmErrorCode;

typedef struct SmError {
    SmErrorCode code;
    SmString frame;
    SmString message;
} SmError;

#define sm_ok ((SmError){ SmErrorOk })

#define sm_error(ctx, err, msg) \
    ((SmError){ \
        (err), \
        ctx->frame->name, \
        sm_string_from_cstring(msg) \
    })

#define sm_throw(ctx, err, msg, exit_frame) \
    { \
        SmError _sm_throw_err = sm_error((ctx), (err), (msg)); \
        if (exit_frame) sm_context_exit_frame(ctx); \
        return _sm_throw_err; \
    }
