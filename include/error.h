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

#define sm_ok ((SmError){ SmErrorOk, { NULL, 0 }, { NULL, 0 } })

#define sm_error(ctx, err, msg) \
    ((SmError){ \
        (err), \
        ctx->frame->name, \
        sm_string_from_cstring(msg) \
    })
