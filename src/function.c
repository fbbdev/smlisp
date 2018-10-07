#include "context.h"
#include "eval.h"
#include "function.h"

// Inlines
extern inline SmFunction sm_function(SmString name, SmScope* capture, SmCons* lambda);
extern inline void sm_function_drop(SmFunction* function);

SmError sm_function_invoke(SmFunction const* function, SmContext* ctx, SmValue args, SmValue* ret) {
    SmScope** arg_scope = sm_heap_root_scope(&ctx->heap);
    *arg_scope = sm_heap_alloc_scope(&ctx->heap, ctx, function->capture);

    // Unpack arguments into scope
    SmError err = sm_arg_pattern_unpack(&function->args, ctx, *arg_scope, args);
    if (!sm_is_ok(err)) {
        sm_heap_root_scope_drop(&ctx->heap, ctx, arg_scope);
        return err;
    }

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, function->args.name);
    ctx->scope = *arg_scope;

    // Return nil when code list is empty
    *ret = sm_value_nil();

    // Run each form in code list, return result of last one
    for (SmCons* form = function->progn; form; form = sm_list_next(form)) {
        *ret = sm_value_nil();
        err = sm_eval(ctx, form->car, ret);
        if (!sm_is_ok(err))
            break;
    }

    sm_context_exit_frame(ctx);
    sm_heap_root_scope_drop(&ctx->heap, ctx, arg_scope);

    return err;
}

// Lambda expression handling
SmError sm_validate_lambda(SmContext* ctx, SmValue expr) {
    if (sm_value_is_nil(expr)) {
        return sm_error(ctx, SmErrorMissingArguments, "lambda requires at least 1 argument");
    } else if (!sm_value_is_cons(expr) || sm_list_is_dotted(expr.data.cons)) {
        return sm_error(ctx, SmErrorInvalidArgument, "lambda cannot accept a dotted argument list");
    }

    return sm_arg_pattern_validate_spec(ctx, expr.data.cons->car);
}
