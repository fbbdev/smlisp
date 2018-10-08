#include "eval.h"
#include "function.h"

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

        // keywords represent themselves
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
            SmSymbol args = sm_symbol(&ctx->symbols, sm_string_from_cstring("args"));

            sm_build_list(ctx, ret,
                SmBuildCar, sm_value_quote(sm_value_symbol(args), 1),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("eval"))),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("cons"))),
                        SmBuildCar, sm_value_quote(sm_value_symbol(form.data.symbol), 1),
                        SmBuildCar, sm_value_symbol(args),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd);

            SmFunction* fn = sm_heap_alloc_function(&ctx->heap, ctx);
            *fn = sm_function(var_name, NULL, ret->data.cons);
            *ret = sm_value_function(fn);
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

    // Call external function if possible
    if (sm_value_is_symbol(call->car) && !sm_value_is_quoted(call->car)) {
        SmExternalFunction ext_fn = sm_context_lookup_function(ctx, call->car.data.symbol);
        if (ext_fn)
            return ext_fn(ctx, call->cdr, ret);
    }

    // Evaluate first element
    SmError err = sm_eval(ctx, call->car, ret);
    if (!sm_is_ok(err))
        return err;

    if (!sm_value_is_function(*ret) || sm_value_is_quoted(*ret))
        return sm_error(ctx, SmErrorInvalidArgument, "first element of function call does not evaluate to a function");

    // Call function
    SmValue* fn = sm_heap_root_value(&ctx->heap);
    *fn = *ret;

    *ret = sm_value_nil();
    err = sm_function_invoke(fn->data.function, ctx, call->cdr, ret);

    sm_heap_root_value_drop(&ctx->heap, ctx, fn);

    return err;
}
