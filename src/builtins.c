#include "args.h"
#include "builtins.h"
#include "eval.h"
#include "number.h"

#include <stdio.h>

// Builtin registration
void sm_register_builtins(SmContext* ctx) {
    #define REGISTER_BUILTIN_OP(symbol, id) \
        sm_context_register_function(\
            ctx, sm_word(&ctx->words, sm_string_from_cstring(#id)), SM_BUILTIN_SYMBOL(symbol));
    #define REGISTER_BUILTIN(id) REGISTER_BUILTIN_OP(id, id)

    SM_BUILTIN_TABLE(REGISTER_BUILTIN, REGISTER_BUILTIN_OP)

    #undef REGISTER_BUILTIN_OP
    #undef REGISTER_BUILTIN
}

// Builtins

#define builtin_return(value) return ((*ret = (value)), (sm_ok))

SmError SM_BUILTIN_SYMBOL(eval)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (sm_value_is_nil(args))
        return sm_error(ctx, SmErrorMissingArguments, "eval requires exactly 1 argument");
    else if (!sm_value_is_cons(args) || !sm_value_is_list(args.data.cons->cdr))
        return sm_error(ctx, SmErrorInvalidArgument, "eval cannot accept a dotted argument list");
    else if (!sm_value_is_nil(args.data.cons->cdr))
        return sm_error(ctx, SmErrorExcessArguments, "eval requires exactly 1 argument");

    SmValue* form = sm_heap_root(&ctx->heap);
    SmError err = sm_eval(ctx, args.data.cons->car, form);
    if (sm_is_ok(err))
        err = sm_eval(ctx, *form, ret);

    sm_heap_root_drop(&ctx->heap, ctx->frame, form);
    return err;
}

SmError SM_BUILTIN_SYMBOL(print)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (sm_value_is_nil(args))
        return sm_error(ctx, SmErrorMissingArguments, "print requires exactly 1 argument");
    else if (!sm_value_is_cons(args) || !sm_value_is_list(args.data.cons->cdr))
        return sm_error(ctx, SmErrorInvalidArgument, "print cannot accept a dotted argument list");
    else if (!sm_value_is_nil(args.data.cons->cdr))
        return sm_error(ctx, SmErrorExcessArguments, "print requires exactly 1 argument");

    SmError err = sm_eval(ctx, args.data.cons->car, ret);

    if (sm_is_ok(err)) {
        sm_print_value(stdout, *ret);
        printf("\n");
        fflush(stdout);
    }

    return err;
}


SmError SM_BUILTIN_SYMBOL(cons)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Two required arguments, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true }, { NULL, true } };
    SmArgPattern pattern = { pargs, 2, { NULL, false, false } };

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err)) {
        *ret = sm_value_nil(); // Drop evaluated argument list
        return err;
    }

    // Second argument becomes cdr
    ret->data.cons->cdr = ret->data.cons->cdr.data.cons->car;
    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(list)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Optional argument list, evaluated
    SmArgPattern pattern = { NULL, 0, { NULL, true, true } };

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err)) {
        *ret = sm_value_nil(); // Drop evaluated argument list
        return err;
    }

    // Return evaluated argument list, precisely what list does
    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(list_dot)(SmContext* ctx, SmValue args, SmValue* ret) {
    // One required argument plus optional argument list, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true } };
    SmArgPattern pattern = { pargs, 1, { NULL, true, true } };

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err)) {
        *ret = sm_value_nil(); // Drop evaluated argument list
        return err;
    }

    SmCons* arg = ret->data.cons;

    // If only one argument, return it
    if (!sm_list_next(arg))
        builtin_return(ret->data.cons->car);

    // Stop one cons before the last one
    while (sm_list_next(sm_list_next(arg)))
        arg = sm_list_next(arg);

    // Use last argument as final cdr
    arg->cdr = arg->cdr.data.cons->car;

    // Return evaluated argument list
    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(lambda)(SmContext* ctx, SmValue args, SmValue* ret) {
    SmError err = sm_validate_lambda(ctx, args);
    if (!sm_is_ok(err))
        return err;

    SmCons* lambda = sm_heap_alloc(&ctx->heap, ctx->frame);
    *ret = sm_value_cons(lambda);

    lambda->car = sm_value_word(sm_word(&ctx->words, sm_string_from_cstring("lambda")));
    lambda->cdr = args;

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(quote)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (sm_value_is_nil(args))
        return sm_error(ctx, SmErrorMissingArguments, "quote requires exactly 1 argument");
    else if (!sm_value_is_cons(args) || !sm_value_is_list(args.data.cons->cdr))
        return sm_error(ctx, SmErrorInvalidArgument, "quote cannot accept a dotted argument list");
    else if (!sm_value_is_nil(args.data.cons->cdr))
        return sm_error(ctx, SmErrorExcessArguments, "quote requires exactly 1 argument");

    // This should work like a macro, that is: its result should be evaluated
    // again. So we return the argument without quoting it (== quote + eval).
    builtin_return(args.data.cons->car);
}


SmError SM_BUILTIN_SYMBOL(car)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(caar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(caaar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caadr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdaar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caddr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdadr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdddr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(caaaar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caaadr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caadar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadaar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdaaar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caaddr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caddar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddaar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadadr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdadar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdaadr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadddr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdddar)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddddr)(SmContext* ctx, SmValue args, SmValue* ret) {
    sm_unused(ctx); sm_unused(args);
    builtin_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(add)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Optional argument list, evaluated
    SmArgPattern pattern = { NULL, 0, { NULL, true, true } };

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err)) {
        *ret = sm_value_nil(); // Drop evaluated argument list
        return err;
    }

    SmNumber res = sm_number_int(0);
    SmCons* arg = (sm_value_is_cons(*ret) ? ret->data.cons : NULL);

    // Sum integers
    for (; arg; arg = sm_list_next(arg)) {
        if (!sm_value_is_number(arg->car)) {
            *ret = sm_value_nil(); // Drop evaluated argument list
            return sm_error(ctx, SmErrorInvalidArgument, "+ takes only numbers as arguments");
        }

        if (!sm_number_is_int(arg->car.data.number))
            break;

        res.value.i += arg->car.data.number.value.i;
    }

    // Sum floats
    if (arg) {
        res = sm_number_as_float(res);

        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car)) {
                *ret = sm_value_nil(); // Drop evaluated argument list
                return sm_error(ctx, SmErrorInvalidArgument, "+ takes only numbers as arguments");
            }

            res.value.f += sm_number_as_float(arg->car.data.number).value.f;
        }
    }

    builtin_return(sm_value_number(res));
}

SmError SM_BUILTIN_SYMBOL(sub)(SmContext* ctx, SmValue args, SmValue* ret) {
    // One required argument plus optional argument list, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true } };
    SmArgPattern pattern = { pargs, 1, { NULL, true, true } };

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err)) {
        *ret = sm_value_nil(); // Drop evaluated argument list
        return err;
    }

    // Argument list is not empty so skip type check
    SmCons* arg = ret->data.cons;

    if (!sm_value_is_number(arg->car)) {
        *ret = sm_value_nil(); // Drop evaluated argument list
        return sm_error(ctx, SmErrorInvalidArgument, "- takes only numbers as arguments");
    }

    // If there is a single argument, invert sign
    if (!sm_list_next(arg)) {
        if (sm_number_is_int(arg->car.data.number))
            builtin_return(sm_value_number(sm_number_int(- arg->car.data.number.value.i)));
        else
            builtin_return(sm_value_number(sm_number_float(- arg->car.data.number.value.f)));
    }

    SmNumber res = arg->car.data.number;
    arg = sm_list_next(arg);

    // Subtract integers
    if (sm_number_is_int(res)) {
        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car)) {
                *ret = sm_value_nil(); // Drop evaluated argument list
                return sm_error(ctx, SmErrorInvalidArgument, "- takes only numbers as arguments");
            }

            if (!sm_number_is_int(arg->car.data.number))
                break;

            res.value.i -= arg->car.data.number.value.i;
        }
    }

    // Subtract floats
    if (arg) {
        res = sm_number_as_float(res);

        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car)) {
                *ret = sm_value_nil(); // Drop evaluated argument list
                return sm_error(ctx, SmErrorInvalidArgument, "- takes only numbers as arguments");
            }

            res.value.f += sm_number_as_float(arg->car.data.number).value.f;
        }
    }

    builtin_return(sm_value_number(res));
}

SmError SM_BUILTIN_SYMBOL(mul)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Optional argument list, evaluated
    SmArgPattern pattern = { NULL, 0, { NULL, true, true } };

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err)) {
        *ret = sm_value_nil(); // Drop evaluated argument list
        return err;
    }

    SmNumber res = sm_number_int(1);
    SmCons* arg = (sm_value_is_cons(*ret) ? ret->data.cons : NULL);

    // Multiply integers
    for (; arg; arg = sm_list_next(arg)) {
        if (!sm_value_is_number(arg->car)) {
            *ret = sm_value_nil(); // Drop evaluated argument list
            return sm_error(ctx, SmErrorInvalidArgument, "* takes only numbers as arguments");
        }

        if (!sm_number_is_int(arg->car.data.number))
            break;

        res.value.i *= arg->car.data.number.value.i;
    }

    // Multiply floats
    if (arg) {
        res = sm_number_as_float(res);

        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car)) {
                *ret = sm_value_nil(); // Drop evaluated argument list
                return sm_error(ctx, SmErrorInvalidArgument, "* takes only numbers as arguments");
            }

            res.value.f *= sm_number_as_float(arg->car.data.number).value.f;
        }
    }

    builtin_return(sm_value_number(res));
}

SmError SM_BUILTIN_SYMBOL(div)(SmContext* ctx, SmValue args, SmValue* ret) {
    // One required argument plus optional argument list, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true} };
    SmArgPattern pattern = { pargs, 1, { NULL, true, true } };

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err)) {
        *ret = sm_value_nil(); // Drop evaluated argument list
        return err;
    }

    // Argument list is not empty so skip type check
    SmCons* arg = ret->data.cons;

    if (!sm_value_is_number(arg->car)) {
        *ret = sm_value_nil(); // Drop evaluated argument list
        return sm_error(ctx, SmErrorInvalidArgument, "- takes only numbers as arguments");
    }

    // If there is a single argument, return inverse
    if (!sm_list_next(arg)) {
        if (sm_number_is_int(arg->car.data.number))
            builtin_return(sm_value_number(sm_number_int(1/arg->car.data.number.value.i)));
        else
            builtin_return(sm_value_number(sm_number_float(1.0/arg->car.data.number.value.f)));
    }

    SmNumber res = arg->car.data.number;
    arg = sm_list_next(arg);

    // Divide integers
    if (sm_number_is_int(res)) {
        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car)) {
                *ret = sm_value_nil(); // Drop evaluated argument list
                return sm_error(ctx, SmErrorInvalidArgument, "/ takes only numbers as arguments");
            }

            if (!sm_number_is_int(arg->car.data.number))
                break;

            res.value.i /= arg->car.data.number.value.i;
        }
    }

    // Divide floats
    if (arg) {
        res = sm_number_as_float(res);

        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car)) {
                *ret = sm_value_nil(); // Drop evaluated argument list
                return sm_error(ctx, SmErrorInvalidArgument, "/ takes only numbers as arguments");
            }

            res.value.f /= sm_number_as_float(arg->car.data.number).value.f;
        }
    }

    builtin_return(sm_value_number(res));
}
