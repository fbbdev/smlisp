#include "args.h"
#include "eval.h"

#include <stdio.h>
#include <string.h>

// Error message buffer
static sm_thread_local char err_buf[1024];

// Eval functions
SmError sm_eval(SmContext* ctx, SmValue form, SmValue* ret) {
    // If quoted, just unquote
    if (sm_value_is_quoted(form)) {
        *ret = sm_value_unquote(form, 1);
        return sm_ok;
    }

    // Unquoted symbols trigger external/variable lookup
    if (sm_value_is_symbol(form)) {
        SmString var_name = sm_symbol_str(form.data.symbol);

        // Keysymbols represent themselves
        if (var_name.length && var_name.data[0] == ':') {
            *ret = form;
            return sm_ok;
        }

        // nil represents ()
        if (var_name.length == 3 && strncmp(var_name.data, "nil", 3) == 0) {
            *ret = sm_value_nil();
            return sm_ok;
        }

        // Lookup external variable
        SmExternalVariable ext_var = sm_context_lookup_variable(ctx, form.data.symbol);
        if (ext_var)
            return ext_var(ctx, ret);

        // Lookup external function
        if (sm_context_lookup_function(ctx, form.data.symbol)) {
            // Return lambda wrapping the external function
            sm_build_list(ctx, ret,
                SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
                SmBuildCar, sm_value_quote(sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("args"))), 1),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("eval"))),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("cons"))),
                        SmBuildCar, sm_value_quote(sm_value_symbol(form.data.symbol), 1),
                        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("args"))),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd);
            return sm_ok;
        }

        // Lookup variable in scope
        SmVariable* scope_var = sm_scope_lookup(ctx->scope, form.data.symbol);
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
    SmString fn_name = sm_string_from_cstring("first element to call");
    bool inline_fn = true;
    SmValue* fn = NULL;

    if (sm_value_is_symbol(call->car) && !sm_value_is_quoted(call->car)) {
        inline_fn = false;
        fn_name = sm_symbol_str(call->car.data.symbol);

        if (fn_name.length && fn_name.data[0] == ':') {
            snprintf(err_buf, sizeof(err_buf), "keysymbol %.*s is not a valid function name", (int) fn_name.length, fn_name.data);
            return sm_error(ctx, SmErrorInvalidArgument, err_buf);
        }

        if (fn_name.length == 3 && strncmp(fn_name.data, "nil", 3) == 0)
            return sm_error(ctx, SmErrorInvalidArgument, "nil is not a valid function name");

        // Lookup external function
        SmExternalFunction ext_fn = sm_context_lookup_function(ctx, call->car.data.symbol);
        if (ext_fn)
            // Call without evaluating arguments
            return ext_fn(ctx, call->cdr, ret);

        fn = sm_heap_root_value(&ctx->heap);

        // Lookup function as external variable
        SmExternalVariable ext_var = sm_context_lookup_variable(ctx, call->car.data.symbol);
        if (ext_var) {
            SmError err = ext_var(ctx, fn);
            if (!sm_is_ok(err)) {
                sm_heap_root_value_drop(&ctx->heap, ctx, fn);
                return err;
            }
        } else {
            // Lookup function as variable in scope
            SmVariable* scope_var = sm_scope_lookup(ctx->scope, call->car.data.symbol);
            if (!scope_var) {
                snprintf(err_buf, sizeof(err_buf), "function not found: %.*s", (int) fn_name.length, fn_name.data);
                sm_heap_root_value_drop(&ctx->heap, ctx, fn);
                return sm_error(ctx, SmErrorUndefinedVariable, err_buf);
            }

            *fn = scope_var->value;
        }
    } else {
        fn = sm_heap_root_value(&ctx->heap);

        SmError err = sm_eval(ctx, call->car, fn);
        if (!sm_is_ok(err)) {
            sm_heap_root_value_drop(&ctx->heap, ctx, fn);
            return err;
        }
    }

    if (!sm_value_is_cons(*fn) || sm_value_is_quoted(*fn) ||
        !sm_value_is_symbol(fn->data.cons->car) || sm_value_is_quoted(fn->data.cons->car) ||
        sm_symbol_str(fn->data.cons->car.data.symbol).length != 6 ||
        strncmp(sm_symbol_str(fn->data.cons->car.data.symbol).data, "lambda", 6) != 0)
    {
        snprintf(err_buf, sizeof(err_buf), "%.*s is not a function", (int) fn_name.length, fn_name.data);
        sm_heap_root_value_drop(&ctx->heap, ctx, fn);
        return sm_error(ctx, SmErrorInvalidArgument, err_buf);
    }

    if (inline_fn)
        fn_name = sm_string_from_cstring("<lambda>");

    SmError err = sm_validate_lambda(ctx, fn->data.cons->cdr);
    if (!sm_is_ok(err)) {
        snprintf(err_buf, sizeof(err_buf), "%.*s is not a valid function: %.*s",
                 (int) fn_name.length, fn_name.data, (int) err.message.length, err.message.data);
        sm_heap_root_value_drop(&ctx->heap, ctx, fn);
        return sm_error(ctx, SmErrorInvalidArgument, err_buf);
    }

    // Call lambda function
    err = sm_invoke_lambda(ctx, fn_name, sm_list_next(fn->data.cons), call->cdr, ret);
    sm_heap_root_value_drop(&ctx->heap, ctx, fn);

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

SmError sm_invoke_lambda(SmContext* ctx, SmString name, SmCons* lambda, SmValue args, SmValue* ret) {
    // lambda must be a valid lambda expression (with the 'lambda' symbol omitted)
    // see sm_validate_lambda

    SmArgPattern pattern = sm_arg_pattern_from_spec(name, lambda->car);

    SmScope** arg_scope = sm_heap_root_scope(&ctx->heap);
    *arg_scope = sm_heap_alloc_scope(&ctx->heap, ctx, ctx->scope);

    // Unpack arguments into scope
    SmError err = sm_arg_pattern_unpack(&pattern, ctx, *arg_scope, args);
    sm_arg_pattern_drop(&pattern);

    if (!sm_is_ok(err)) {
        sm_heap_root_scope_drop(&ctx->heap, ctx, arg_scope);
        return err;
    }

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, name);
    ctx->scope = *arg_scope;

    // Return nil when code list is empty
    *ret = sm_value_nil();

    // Run each form in code list, return result of last one
    for (SmCons* code = sm_list_next(lambda); code; code = sm_list_next(code)) {
        *ret = sm_value_nil();
        err = sm_eval(ctx, code->car, ret);
        if (!sm_is_ok(err))
            break;
    }

    sm_context_exit_frame(ctx);
    sm_heap_root_scope_drop(&ctx->heap, ctx, arg_scope);

    return err;
}
