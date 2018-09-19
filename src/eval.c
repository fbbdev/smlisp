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
    // Loop to avoid recursion
    while (1) {
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
                        SmBuildCar, sm_value_word(form.data.word),
                        SmBuildCdr, sm_value_word(sm_word(&ctx->words, sm_string_from_cstring("args"))),
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
            sm_throw(ctx, SmErrorUndefinedVariable, err_buf, false);
        }

        // Return anything but function calls
        if (!sm_value_is_cons(form)) {
            *ret = form;
            return sm_ok;
        }

        // Lists are interpreted as function/macro calls
        SmCons* call = form.data.cons;

        if (!sm_value_is_word(call->car) || sm_value_is_quoted(call->car)) {
            sm_throw(ctx, SmErrorInvalidArgument, "function name must be an unquoted word", false);
        }

        SmString fn_name = sm_word_str(call->car.data.word);

        if (fn_name.length && fn_name.data[0] == ':') {
            snprintf(err_buf, sizeof(err_buf), "keyword %.*s is not a function name", (int) fn_name.length, fn_name.data);
            sm_throw(ctx, SmErrorInvalidArgument, err_buf, false);
        }

        // If eval is invoked, check arguments, replace form and recurse
        if (fn_name.length == 4 && !strncmp(fn_name.data, "eval", 4)) {
            if (sm_value_is_nil(call->cdr)) {
                sm_throw(ctx, SmErrorMissingArguments, "eval requires exactly 1 parameter", false);
            } else if (!sm_value_is_cons(call->cdr) || !sm_value_is_list(sm_list_next(call)->cdr)) {
                sm_throw(ctx, SmErrorInvalidArgument, "eval cannot accept a dotted parameter list", false);
            } else if (!sm_value_is_nil(sm_list_next(call)->cdr)) {
                sm_throw(ctx, SmErrorExcessArguments, "eval requires exactly 1 parameter", false);
            }

            form = sm_list_next(call)->car;
            continue;
        }

        // Lookup external function
        SmExternalFunction ext_fn = sm_context_lookup_function(ctx, call->car.data.word);
        if (ext_fn)
            // Call without evaluating arguments
            return ext_fn(ctx, sm_list_next(call), ret);

        SmValue *fn = sm_heap_root(&ctx->heap);

        // Lookup function as external variable
        SmExternalVariable ext_var = sm_context_lookup_variable(ctx, call->car.data.word);
        if (ext_var) {
            ext_var(ctx, fn);
        } else {
            // Lookup function as variable in scope
            SmVariable* scope_var = scope_lookup(ctx->frame, form.data.word);
            if (!scope_var) {
                snprintf(err_buf, sizeof(err_buf), "function not found: %.*s", (int) fn_name.length, fn_name.data);
                sm_throw(ctx, SmErrorUndefinedVariable, err_buf, false);
            }

            *fn = scope_var->value;
        }

        if (!sm_value_is_cons(*fn) || sm_value_is_quoted(*fn) ||
            !sm_value_is_word(fn->data.cons->car) || sm_value_is_quoted(fn->data.cons->car) ||
            sm_word_str(fn->data.cons->car.data.word).length != 6 ||
            strncmp(sm_word_str(fn->data.cons->car.data.word).data, "lambda", 6) != 0)
        {
            snprintf(err_buf, sizeof(err_buf), "%.*s is not a function", (int) fn_name.length, fn_name.data);
            sm_throw(ctx, SmErrorInvalidArgument, err_buf, false);
        }

        SmError err = sm_validate_lambda(ctx, sm_list_next(fn->data.cons));
        if (err.code != SmErrorOk) {
            snprintf(err_buf, sizeof(err_buf), "%.*s is not a function: %.*s",
                     (int) fn_name.length, fn_name.data, (int) err.message.length, err.message.data);
            sm_throw(ctx, SmErrorInvalidArgument, err_buf, false);
        }

        // Call lambda function
        SmStackFrame frame;
        sm_context_enter_frame(ctx, &frame, fn_name, *fn);
        err = sm_invoke_lambda(ctx, sm_list_next(fn->data.cons), sm_list_next(call), ret);
        sm_context_exit_frame(ctx);

        sm_heap_root_drop(&ctx->heap, fn);

        return err;
    }
}

SmError sm_validate_lambda(SmContext* ctx, SmCons* params) {
    if (!params) {
        sm_throw(ctx, SmErrorMissingArguments, "lambda requires at least 1 parameter", false);
    } else if (!sm_value_is_word(params->car) && (!sm_value_is_list(params->car) || sm_value_is_quoted(params->car))) {
        sm_throw(ctx, SmErrorInvalidArgument, "lambda requires a word or an unquoted argument list as first parameter", false);
    } else if (sm_value_is_list(params->car)) {
        for (SmCons* arg = params->car.data.cons; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_word(arg->car)
                    || (!sm_value_is_word(arg->cdr)
                        && (!sm_value_is_list(arg->cdr) || sm_value_is_quoted(arg->cdr))))
            {
                sm_throw(ctx, SmErrorInvalidArgument, "lambda argument lists may only contain words", false);
            }
        }
    }

    for (SmCons* code = params; code; code = sm_list_next(code)) {
        if (!sm_value_is_list(code->cdr) || sm_value_is_quoted(code->cdr))
            sm_throw(ctx, SmErrorInvalidArgument, "lambda code is a dotted list", false);
    }

    return sm_ok;
}

static void eval_param_list(SmContext* ctx, SmValue* ret, SmCons* params) {
    *ret = sm_value_cons(sm_heap_alloc(&ctx->heap, ctx->frame));
    SmCons* param = ret->data.cons;

    sm_eval(ctx, params->car, &param->car);

    if (!sm_value_is_list(params->cdr))
        sm_eval(ctx, params->cdr, &param->cdr);

    for (params = sm_list_next(params); params; params = sm_list_next(params)) {
        param->cdr = sm_value_cons(sm_heap_alloc(&ctx->heap, ctx->frame));
        param = sm_list_next(param);

        sm_eval(ctx, params->car, &param->car);

        if (!sm_value_is_list(params->cdr))
            sm_eval(ctx, params->cdr, &param->cdr);
    }
}

SmError sm_invoke_lambda(SmContext* ctx, SmCons* lambda, SmCons* params, SmValue* ret) {
    if (sm_value_is_word(lambda->car)) {
        // If argument list is a single word, pass down everything as rest

        if (!params) {
            sm_scope_set(&ctx->frame->scope, (SmVariable){ lambda->car.data.word, sm_value_nil() });
        } if (sm_value_is_quoted(lambda->car)) {
            // If the word is quoted, do not evaluate params
            sm_scope_set(&ctx->frame->scope, (SmVariable){ lambda->car.data.word, sm_value_cons(params) });
        } else {
            // Otherwise, evaluate params and build new list
            SmVariable* args = sm_scope_set(&ctx->frame->scope, (SmVariable){ lambda->car.data.word, sm_value_nil() });
            eval_param_list(ctx, &args->value, params);
        }
    } else {
        // We have a detailed argument list, walk it and validate params


    }

    sm_unused(ret);
    return sm_ok;
}
