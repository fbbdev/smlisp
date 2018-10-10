#include "args.h"
#include "eval.h"

#include <stdio.h>
#include <stdlib.h>

// Inlines
extern inline void sm_arg_pattern_drop(SmArgPattern* pattern);

// Error message buffer
static sm_thread_local char err_buf[1024];

// Argument matching functions
SmError sm_arg_pattern_validate_spec(SmContext* ctx, SmValue spec) {
    if (!sm_value_is_symbol(spec) && !sm_value_is_list(spec)) {
        return sm_error(ctx, SmErrorInvalidArgument, "argument pattern must be a symbol or unquoted list");
    } else if (sm_value_is_symbol(spec)) {
        if (spec.quotes > 1)
            return sm_error(ctx, SmErrorInvalidArgument, "argument pattern quoted more than once");
    } else if (sm_value_is_quoted(spec)) {
        return sm_error(ctx, SmErrorInvalidArgument, "argument pattern cannot be a quoted list");
    } else {
        for (SmCons* arg = spec.data.cons; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_symbol(arg->car) ||
                    (!sm_value_is_symbol(arg->cdr) &&
                        (!sm_value_is_list(arg->cdr) || sm_value_is_quoted(arg->cdr))))
            {
                return sm_error(ctx, SmErrorInvalidArgument, "argument pattern lists may only contain symbols");
            } else if (arg->car.quotes > 1 || (sm_value_is_symbol(arg->cdr) && arg->cdr.quotes > 1)) {
                return sm_error(ctx, SmErrorInvalidArgument, "symbol in argument pattern list quoted more than once");
            }
        }
    }

    return sm_ok;
}

SmArgPattern sm_arg_pattern_from_spec(SmString name, SmValue spec) {
    // Spec *must* be valid, otherwise this is UB. See sm_arg_pattern_validate_spec
    SmArgPattern pattern = { name, NULL, 0, { NULL, false, false } };

    if (sm_value_is_symbol(spec)) {
        pattern.rest.id = spec.data.symbol;
        pattern.rest.eval = !sm_value_is_quoted(spec);
        pattern.rest.use = true;
        return pattern;
    }

    pattern.count = sm_list_size(spec.data.cons);

    SmArgPatternArg* args = (SmArgPatternArg*) sm_aligned_alloc(
        sm_alignof(SmArgPatternArg), pattern.count*sizeof(SmArgPatternArg));

    pattern.args = args;

    for (SmCons* arg = spec.data.cons; arg; ++args, arg = sm_list_next(arg)) {
        args->id = arg->car.data.symbol;
        args->eval = !sm_value_is_quoted(arg->car);

        if (sm_value_is_symbol(arg->cdr)) {
            pattern.rest.id = arg->cdr.data.symbol;
            pattern.rest.eval = !sm_value_is_quoted(arg->cdr);
            pattern.rest.use = true;
        }
    }

    return pattern;
}

SmError sm_arg_pattern_eval(SmArgPattern const* pattern, SmContext* ctx, SmValue args, SmValue* ret) {
    SmCons* arg = (sm_value_is_cons(args) && !sm_value_is_quoted(args)) ? args.data.cons : NULL;
    size_t available = sm_list_size(arg);

    SmValue dot = arg ? sm_list_dot(arg) : args;

    // Eval dot part if we need more arguments or rest says so
    bool eval_dot = available < pattern->count || (pattern->rest.use && pattern->rest.eval);

    // ret is a gc root so we use it for temporary storage
    if (!eval_dot) {
        *ret = dot;
    } else if (!sm_value_is_symbol(dot) || sm_value_is_quoted(dot)) {
        *ret = sm_value_unquote(dot, 1);
        eval_dot = false; // We don't need a root later so set eval_dot to false
    } else {
        SmError err = sm_eval(ctx, dot, ret);
        if (!sm_is_ok(err))
            return err;
    }

    SmValue final_cdr = *ret;

    if (sm_value_is_list(*ret) && !sm_value_is_quoted(*ret)) {
        available += sm_list_size(ret->data.cons);
        final_cdr = sm_list_dot(ret->data.cons);
    }

    // Reject invalid argument lists
    if (available < pattern->count) {
        *ret = sm_value_nil();
        snprintf(err_buf, sizeof(err_buf), "%.*s: expected %s%zu arguments, %zu given",
            (int) pattern->name.length, pattern->name.data,
            pattern->rest.use ? "at least " : "", pattern->count, available);
        return sm_error(ctx, SmErrorMissingArguments, err_buf);
    } else if (available > pattern->count && !pattern->rest.use) {
        *ret = sm_value_nil();
        snprintf(err_buf, sizeof(err_buf), "%.*s: expected %zu arguments, %zu given",
            (int) pattern->name.length, pattern->name.data,
            pattern->count, available);
        return sm_error(ctx, SmErrorExcessArguments, err_buf);
    } else if (!pattern->rest.use && (!sm_value_is_nil(final_cdr) || sm_value_is_quoted(final_cdr))) {
        *ret = sm_value_nil();
        snprintf(err_buf, sizeof(err_buf), "%.*s: cannot accept dotted argument list",
            (int) pattern->name.length, pattern->name.data);
        return sm_error(ctx, SmErrorInvalidArgument, err_buf);
    }

    // At this point we know input matches the pattern so we can get away
    // with minimal checks

    // Fast path for some rest only cases
    if (!pattern->count) {
        if (!pattern->rest.use || !pattern->rest.eval) {
            // If no evaluation is needed, we can just return args; if rest is not used, args is nil anyway
            *ret = args;
            return sm_ok;
        } else if (!arg) {
            // If evaluation is needed but we only have the dot part,
            // we can just return the evaluated dot part (already stored in ret)
            return sm_ok;
        }
    }

    // Save evaluated dot part, create root if needed
    SmValue* dot_root = eval_dot ? sm_heap_root_value(&ctx->heap) : &dot;
    *dot_root = *ret;

    // Init output list
    *ret = sm_value_cons(sm_heap_alloc_cons(&ctx->heap, ctx));
    SmCons* out = ret->data.cons;

    SmError err = sm_ok;
    bool into_dot = false;

    if (!arg && sm_value_is_cons(*dot_root) && !sm_value_is_quoted(*dot_root)) {
        arg = dot_root->data.cons;
        into_dot = true;
    }

    for (size_t i = 0; arg; ++i) {
        if (!into_dot && ((i < pattern->count) ? pattern->args[i].eval : pattern->rest.eval)) {
            err = sm_eval(ctx, arg->car, &out->car);
            if (!sm_is_ok(err))
                break;
        } else {
            out->car = arg->car;
        }

        if (!(arg = sm_list_next(arg)) && !into_dot &&
                sm_value_is_cons(*dot_root) && !sm_value_is_quoted(*dot_root))
        {
            // If possible, continue taking arguments from dot part
            arg = dot_root->data.cons;
            into_dot = true;
        }

        if (arg) {
            out->cdr = sm_value_cons(sm_heap_alloc_cons(&ctx->heap, ctx));
            out = out->cdr.data.cons;
        }
    }

    out->cdr = final_cdr;

    if (!sm_is_ok(err))
        *ret = sm_value_nil(); // In case of error drop output list

    if (eval_dot)
        sm_heap_root_value_drop(&ctx->heap, ctx, dot_root);

    return err;
}

SmError sm_arg_pattern_unpack(SmArgPattern const* pattern, SmContext* ctx, SmScope* scope, SmValue args) {
    SmCons* arg = (sm_value_is_cons(args) && !sm_value_is_quoted(args)) ? args.data.cons : NULL;
    size_t available = sm_list_size(arg);

    SmValue dot = arg ? sm_list_dot(arg) : args;
    SmValue* dot_root = &dot;

    // Eval dot part if we need more arguments or rest says so
    if (available < pattern->count || (pattern->rest.use && pattern->rest.eval)) {
        if (!sm_value_is_symbol(dot) || sm_value_is_quoted(dot)) {
            dot = sm_value_unquote(dot, 1);
        } else {
            dot_root = sm_heap_root_value(&ctx->heap);
            SmError err = sm_eval(ctx, dot, dot_root);
            if (!sm_is_ok(err)) {
                sm_heap_root_value_drop(&ctx->heap, ctx, dot_root);
                return err;
            }
        }
    }

    SmValue final_cdr = *dot_root;

    if (sm_value_is_list(*dot_root) && !sm_value_is_quoted(*dot_root)) {
        available += sm_list_size(dot_root->data.cons);
        final_cdr = sm_list_dot(dot_root->data.cons);
    }

    // Reject invalid argument lists
    if (available < pattern->count) {
        if (dot_root != &dot)
            sm_heap_root_value_drop(&ctx->heap, ctx, dot_root);
        snprintf(err_buf, sizeof(err_buf), "%.*s: expected %s%zu arguments, %zu given",
            (int) pattern->name.length, pattern->name.data,
            pattern->rest.use ? "at least " : "", pattern->count, available);
        return sm_error(ctx, SmErrorMissingArguments, err_buf);
    } else if (available > pattern->count && !pattern->rest.use) {
        if (dot_root != &dot)
            sm_heap_root_value_drop(&ctx->heap, ctx, dot_root);
        snprintf(err_buf, sizeof(err_buf), "%.*s: expected %zu arguments, %zu given",
            (int) pattern->name.length, pattern->name.data,
            pattern->count, available);
        return sm_error(ctx, SmErrorExcessArguments, err_buf);
    } else if (!pattern->rest.use && (!sm_value_is_nil(final_cdr) || sm_value_is_quoted(final_cdr))) {
        if (dot_root != &dot)
            sm_heap_root_value_drop(&ctx->heap, ctx, dot_root);

        snprintf(err_buf, sizeof(err_buf), "%.*s: cannot accept dotted argument list",
            (int) pattern->name.length, pattern->name.data);
        return sm_error(ctx, SmErrorInvalidArgument, err_buf);
    }

    // At this point we know input matches the pattern so we can get away
    // with minimal checks

    // Fast path for some rest only cases
    if (!pattern->count) {
        if (!pattern->rest.use) {
            return sm_ok;
        } else if (!pattern->rest.eval) {
            // If no evaluation is needed, we can just return args
            sm_scope_set(scope, pattern->rest.id, args);
            return sm_ok;
        } else if (!arg) {
            // If evaluation is needed but we only have the dot part,
            // we can just return the evaluated dot part
            sm_scope_set(scope, pattern->rest.id, *dot_root);
            if (dot_root != &dot)
                sm_heap_root_value_drop(&ctx->heap, ctx, dot_root);
            return sm_ok;
        }
    }

    SmError err = sm_ok;
    bool into_dot = false;

    if (!arg && sm_value_is_cons(*dot_root) && !sm_value_is_quoted(*dot_root)) {
        arg = dot_root->data.cons;
        into_dot = true;
    }

    // No need to check if arg is not null since we already know it contains
    // at least pattern->count items
    for (size_t i = 0; i < pattern->count; ++i) {
        if (!into_dot && pattern->args[i].eval) {
            SmVariable* var = sm_scope_set(scope, pattern->args[i].id, sm_value_nil());
            err = sm_eval(ctx, arg->car, &var->value);
            if (!sm_is_ok(err))
                break;
        } else {
            sm_scope_set(scope, pattern->args[i].id, arg->car);
        }

        if (!(arg = sm_list_next(arg)) && !into_dot &&
                sm_value_is_cons(*dot_root) && !sm_value_is_quoted(*dot_root))
        {
            // If possible, continue taking arguments from dot part
            arg = dot_root->data.cons;
            into_dot = true;
        }
    }

    if (sm_is_ok(err) && pattern->rest.use) {
        // Rest argument takes the value of the dot part...
        SmVariable* var = sm_scope_set(scope, pattern->rest.id, final_cdr);

        // ...unless there are more args to take, in which case we build a list
        if (arg) {
            var->value = sm_value_cons(sm_heap_alloc_cons(&ctx->heap, ctx));
            SmCons* rest = var->value.data.cons;

            while (arg) {
                if (!into_dot && pattern->rest.eval) {
                    err = sm_eval(ctx, arg->car, &rest->car);
                    if (!sm_is_ok(err))
                        break;
                } else {
                    rest->car = arg->car;
                }

                if (!(arg = sm_list_next(arg)) && !into_dot &&
                        sm_value_is_cons(*dot_root) && !sm_value_is_quoted(*dot_root))
                {
                    // If possible, continue taking arguments from dot part
                    arg = dot_root->data.cons;
                    into_dot = true;
                }

                if (arg) {
                    rest->cdr = sm_value_cons(sm_heap_alloc_cons(&ctx->heap, ctx));
                    rest = rest->cdr.data.cons;
                }
            }

            rest->cdr = final_cdr;
        }
    }

    if (dot_root != &dot)
        sm_heap_root_value_drop(&ctx->heap, ctx, dot_root);

    return err;
}
