#include "context.h"
#include "error.h"

#include <string.h>

// Inlines
extern inline bool sm_is_ok(SmError err);
extern inline void sm_report_error(FILE* f, SmError err);

// Frame name buffer
static sm_thread_local char frame_buf[1024];

// Error code strings
static char const* error_codes[SmErrorCount] = {
    "Ok",
    "MissingArguments",
    "ExcessArguments",
    "InvalidArgument",
    "UndefinedVariable",
    "SyntaxError",
    "LexicalError",
    "InvalidLiteral",
    "Generic"
};

// Error reporting functions
SmString sm_error_code_string(SmErrorCode code) {
    return sm_string_from_cstring(error_codes[code]);
}

SmError sm_error(struct SmContext const* ctx, SmErrorCode err, char const* msg) {
    // Build stack trace
    char* p = &frame_buf[1023];
    *p = '\0';

    for (SmStackFrame* frame = ctx->frame; frame; frame = frame->parent) {
        // Check if we have space for frame name and colon
        // if the frame has a parent, also keep space for an ellipsis (3 chars)
        if ((frame->name.length + (*p != '\0')) > (size_t)(p - frame_buf - (frame->parent ? 3 : 0))) {
            // No space left, insert an ellipsis and stop
            memcpy((p -= 3), "...", 3);
            break;
        }

        // Insert a colon if this is not the leaf frame
        if (*p != '\0')
            *--p = ':';

        memcpy((p -= frame->name.length), frame->name.data, frame->name.length);
    }

    return (SmError){
        err,
        sm_string_from_cstring(p),
        sm_string_from_cstring(msg)
    };
}
