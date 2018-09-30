#pragma once

#include "util.h"

#include <stdio.h>

struct SmContext;

typedef enum SmErrorCode {
    SmErrorOk = 0,
    SmErrorMissingArguments,
    SmErrorExcessArguments,
    SmErrorInvalidArgument,
    SmErrorUndefinedVariable,
    SmErrorSyntaxError,
    SmErrorLexicalError,
    SmErrorGeneric,

    SmErrorCount
} SmErrorCode;

typedef struct SmError {
    SmErrorCode code;
    SmString frame;
    SmString message;
} SmError;

SmString sm_error_code_string(SmErrorCode code);

#define sm_ok ((SmError){ SmErrorOk, { NULL, 0 }, { NULL, 0 } })

inline bool sm_is_ok(SmError err) {
    return err.code == SmErrorOk;
}

SmError sm_error(struct SmContext const* ctx, SmErrorCode err, char const* msg);

inline void sm_report_error(FILE* f, SmError err) {
    SmString code_string = sm_error_code_string(err.code);
    fprintf(f, "ERROR: %.*s\nin frame: %.*s\n--- %.*s\n",
        (int) code_string.length, code_string.data,
        (int) err.frame.length, err.frame.data,
        (int) err.message.length, err.message.data);
}
