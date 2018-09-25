#include "eval.h"

#include <stdio.h>
#include <string.h>

static sm_thread_local char err_buf[1024];

static SmVariable* scope_lookup(SmStackFrame const* frame, SmWord id) {
    for (; frame; frame = frame->parent) {
        SmVariable* var = sm_scope_get(&frame->scope, id);
        if (var)
            return var;
    }

    return NULL;
}

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

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, fn_name, sm_value_nil());

    // Lookup function as external variable
    SmExternalVariable ext_var = sm_context_lookup_variable(ctx, call->car.data.word);
    if (ext_var) {
        SmError err = ext_var(ctx, &frame.fn);
        if (err.code != SmErrorOk) {
            sm_context_exit_frame(ctx);
            return err;
        }
    } else {
        // Lookup function as variable in scope
        SmVariable* scope_var = scope_lookup(ctx->frame, form.data.word);
        if (!scope_var) {
            snprintf(err_buf, sizeof(err_buf), "function not found: %.*s", (int) fn_name.length, fn_name.data);
            sm_context_exit_frame(ctx);
            return sm_error(ctx, SmErrorUndefinedVariable, err_buf);
        }

        frame.fn = scope_var->value;
    }

    if (!sm_value_is_cons(frame.fn) || sm_value_is_quoted(frame.fn) ||
        !sm_value_is_word(frame.fn.data.cons->car) || sm_value_is_quoted(frame.fn.data.cons->car) ||
        sm_word_str(frame.fn.data.cons->car.data.word).length != 6 ||
        strncmp(sm_word_str(frame.fn.data.cons->car.data.word).data, "lambda", 6) != 0)
    {
        snprintf(err_buf, sizeof(err_buf), "%.*s is not a function", (int) fn_name.length, fn_name.data);
        sm_context_exit_frame(ctx);
        return sm_error(ctx, SmErrorInvalidArgument, err_buf);
    }

    SmError err = sm_validate_lambda(ctx, frame.fn.data.cons->cdr);
    if (err.code != SmErrorOk) {
        snprintf(err_buf, sizeof(err_buf), "%.*s is not a function: %.*s",
                 (int) fn_name.length, fn_name.data, (int) err.message.length, err.message.data);
        sm_context_exit_frame(ctx);
        return sm_error(ctx, SmErrorInvalidArgument, err_buf);
    }

    // Call lambda function
    err = sm_invoke_lambda(ctx, sm_list_next(frame.fn.data.cons), call->cdr, ret);
    sm_context_exit_frame(ctx);

    return err;
}

SmError sm_validate_lambda(SmContext* ctx, SmValue args) {
    if (sm_value_is_nil(args)) {
        return sm_error(ctx, SmErrorMissingArguments, "lambda requires at least 1 argument");
    } else if (!sm_value_is_cons(args)) {
        return sm_error(ctx, SmErrorInvalidArgument, "lambda cannot accept a dotted argument list");
    } else if (!sm_value_is_word(args.data.cons->car) && (!sm_value_is_list(args.data.cons->car) || sm_value_is_quoted(args.data.cons->car))) {
        return sm_error(ctx, SmErrorInvalidArgument, "lambda requires a word or an unquoted argument pattern as first argument");
    } else if (sm_value_is_list(args.data.cons->car)) {
        for (SmCons* arg = args.data.cons->car.data.cons; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_word(arg->car)
                    || (!sm_value_is_word(arg->cdr)
                        && (!sm_value_is_list(arg->cdr) || sm_value_is_quoted(arg->cdr))))
            {
                return sm_error(ctx, SmErrorInvalidArgument, "lambda argument patterns may only contain words");
            }
        }
    }

    for (SmCons* code = args.data.cons; code; code = sm_list_next(code)) {
        if (!sm_value_is_list(code->cdr) || sm_value_is_quoted(code->cdr))
            return sm_error(ctx, SmErrorInvalidArgument, "lambda code is a dotted list");
    }

    return sm_ok;
}

static SmError eval_param_list(SmContext* ctx, SmValue* ret, SmValue av) {
    SmValue* root = NULL;
    SmCons* args = NULL;

    if (sm_value_is_cons(av)) {
        args = av.data.cons;
    } else {
        SmError err = sm_eval(ctx, av, ret);
        if (err.code != SmErrorOk)
            return err;

        if (sm_value_is_cons(*ret)) {
            root = sm_heap_root(&ctx->heap);
            *root = *ret;
            args = root->data.cons;
        } else {
            return sm_ok;
        }
    }

    *ret = sm_value_cons(sm_heap_alloc(&ctx->heap, ctx->frame));
    SmCons* param = ret->data.cons;

    SmError err = sm_eval(ctx, args->car, &param->car);

    if (err.code == SmErrorOk && !sm_value_is_list(args->cdr))
        err = sm_eval(ctx, args->cdr, &param->cdr);

    for (args = sm_list_next(args); args && err.code == SmErrorOk; args = sm_list_next(args)) {
        param->cdr = sm_value_cons(sm_heap_alloc(&ctx->heap, ctx->frame));
        param = sm_list_next(param);

        err = sm_eval(ctx, args->car, &param->car);

        if (err.code == SmErrorOk && !sm_value_is_list(args->cdr))
            err = sm_eval(ctx, args->cdr, &param->cdr);
    }

    if (err.code != SmErrorOk)
        *ret = sm_value_nil();

    if (root)
        sm_heap_root_drop(&ctx->heap, root);

    return err;
}

SmError sm_invoke_lambda(SmContext* ctx, SmCons* lambda, SmValue args, SmValue* ret) {
    if (sm_value_is_word(lambda->car)) {
        // If argument list is a single word, pass down everything as rest

        if (sm_value_is_quoted(lambda->car)) {
            // If the word is quoted, do not evaluate args
            sm_scope_set(&ctx->frame->scope, (SmVariable){ lambda->car.data.word, args });
        } else {
            // Otherwise, evaluate args and build new list
            SmVariable* args = sm_scope_set(&ctx->frame->scope, (SmVariable){ lambda->car.data.word, sm_value_nil() });
            SmError err = eval_param_list(ctx, &args->value, args);
            if (err.code != SmErrorOk)
                return err;
        }
    } else {
        // We have a detailed argument list, walk it and validate args


    }

    sm_unused(ret);
    return sm_ok;
}
