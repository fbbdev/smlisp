#include "args.h"
#include "eval.h"

#include <stdio.h>
#include <string.h>

// Error message buffer
static sm_thread_local char err_buf[1024];

// Private helpers
static SmVariable* scope_lookup(SmStackFrame const* frame, SmWord id) {
    for (; frame; frame = frame->parent) {
        SmVariable* var = sm_scope_get(&frame->scope, id);
        if (var)
            return var;
    }

    return NULL;
}

// Eval functions
SmError sm_eval(SmContext* ctx, SmValue form, SmValue* ret) {
    // If quoted, just unquote
    if (sm_value_is_quoted(form)) {
        *ret = sm_value_unquote(form, 1);
        return sm_ok;
    }

    // Unquoted words trigger external/variable lookup
    if (sm_value_is_word(form)) {
        SmString var_name = sm_word_str(form.data.word);

        // Keywords represent themselves
        if (var_name.length && var_name.data[0] == ':') {
            *ret = form;
            return sm_ok;
        }

        // Lookup external variable
        SmExternalVariable ext_var = sm_context_lookup_variable(ctx, form.data.word);
        if (ext_var)
            return ext_var(ctx, ret);

        // Lookup external function
        if (sm_context_lookup_function(ctx, form.data.word)) {
            // Return lambda wrapping the external function
            sm_build_list(ctx, ret,
                SmBuildCar, sm_value_word(sm_word(&ctx->words, sm_string_from_cstring("lambda"))),
                SmBuildCar, sm_value_quote(sm_value_word(sm_word(&ctx->words, sm_string_from_cstring("args"))), 1),
                SmBuildList,
                    SmBuildCar, sm_value_word(sm_word(&ctx->words, sm_string_from_cstring("eval"))),
                    SmBuildList,
                        SmBuildCar, sm_value_word(sm_word(&ctx->words, sm_string_from_cstring("list"))),
                        SmBuildCar, sm_value_quote(sm_value_word(form.data.word), 1),
                        SmBuildCdr, sm_value_word(sm_word(&ctx->words, sm_string_from_cstring("args"))),
                    SmBuildEnd,
                SmBuildEnd);
            return sm_ok;
        }

        // Lookup variable in scope
        SmVariable* scope_var = scope_lookup(ctx->frame, form.data.word);
        if (scope_var) {
            *ret = scope_var->value;
            return sm_ok;
        }

        snprintf(err_buf, sizeof(err_buf), "variable not found: %.*s", (int) var_name.length, var_name.data);
        return sm_error(ctx, SmErrorUndefinedVariable, err_buf);
    }

    // Return anything but function calls
    if (!sm_value_is_cons(form)) {
        *ret = form;
        return sm_ok;
    }

    // Lists are interpreted as function/macro calls
    SmCons* call = form.data.cons;

    if (!sm_value_is_word(call->car) || sm_value_is_quoted(call->car)) {
        return sm_error(ctx, SmErrorInvalidArgument, "function name must be an unquoted word");
    }

    SmString fn_name = sm_word_str(call->car.data.word);

    if (fn_name.length && fn_name.data[0] == ':') {
        snprintf(err_buf, sizeof(err_buf), "keyword %.*s is not a function name", (int) fn_name.length, fn_name.data);
        return sm_error(ctx, SmErrorInvalidArgument, err_buf);
    }

    // Lookup external function
    SmExternalFunction ext_fn = sm_context_lookup_function(ctx, call->car.data.word);
    if (ext_fn)
        // Call without evaluating arguments
        return ext_fn(ctx, call->cdr, ret);

    SmValue* fn = sm_heap_root(&ctx->heap);

    // Lookup function as external variable
    SmExternalVariable ext_var = sm_context_lookup_variable(ctx, call->car.data.word);
    if (ext_var) {
        SmError err = ext_var(ctx, fn);
        if (err.code != SmErrorOk)
            return err;
    } else {
        // Lookup function as variable in scope
        SmVariable* scope_var = scope_lookup(ctx->frame, form.data.word);
        if (!scope_var) {
            snprintf(err_buf, sizeof(err_buf), "function not found: %.*s", (int) fn_name.length, fn_name.data);
            return sm_error(ctx, SmErrorUndefinedVariable, err_buf);
        }

        *fn = scope_var->value;
    }

    if (!sm_value_is_cons(*fn) || sm_value_is_quoted(*fn) ||
        !sm_value_is_word(fn->data.cons->car) || sm_value_is_quoted(fn->data.cons->car) ||
        sm_word_str(fn->data.cons->car.data.word).length != 6 ||
        strncmp(sm_word_str(fn->data.cons->car.data.word).data, "lambda", 6) != 0)
    {
        snprintf(err_buf, sizeof(err_buf), "%.*s is not a function", (int) fn_name.length, fn_name.data);
        return sm_error(ctx, SmErrorInvalidArgument, err_buf);
    }

    SmError err = sm_validate_lambda(ctx, fn->data.cons->cdr);
    if (err.code != SmErrorOk) {
        snprintf(err_buf, sizeof(err_buf), "%.*s is not a function: %.*s",
                 (int) fn_name.length, fn_name.data, (int) err.message.length, err.message.data);
        return sm_error(ctx, SmErrorInvalidArgument, err_buf);
    }

    // Call lambda function
    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, fn_name, *fn);
    sm_heap_root_drop(&ctx->heap, fn);

    err = sm_invoke_lambda(ctx, sm_list_next(fn->data.cons), call->cdr, ret);
    sm_context_exit_frame(ctx);

    return err;
}

// Lambda expression handling
SmError sm_validate_lambda(SmContext* ctx, SmValue args) {
    if (sm_value_is_nil(args)) {
        return sm_error(ctx, SmErrorMissingArguments, "lambda requires at least 1 argument");
    } else if (!sm_value_is_cons(args) || sm_list_is_dotted(args.data.cons)) {
        return sm_error(ctx, SmErrorInvalidArgument, "lambda cannot accept a dotted argument list");
    }

    return sm_arg_pattern_validate_spec(ctx, args.data.cons->car);
}

SmError sm_invoke_lambda(SmContext* ctx, SmCons* lambda, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(lambda); sm_unused(args); sm_unused(ret);
    return sm_ok;
}
