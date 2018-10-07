#include "args.h"
#include "builtins.h"
#include "eval.h"
#include "number.h"

#include <stdio.h>
#include <string.h>

// Private helpers
static inline SmError exit_frame_pass_error(SmContext* ctx, SmError err) {
    sm_context_exit_frame(ctx);
    return err;
}

#define return_value(value) return ((*ret = (value)), (sm_ok))
#define return_value_exit_frame(ctx, value) return ((*ret = (value)), (sm_context_exit_frame(ctx)), (sm_ok))
#define return_nil(err) return ((*ret = sm_value_nil()), (err))
#define return_nil_exit_frame(ctx, err) return ((*ret = sm_value_nil()), exit_frame_pass_error((ctx), (err)))

// Builtin registration
void sm_register_builtins(SmContext* ctx) {
    #define REGISTER_BUILTIN_OP(symbol, id) \
        sm_context_register_function(\
            ctx, sm_symbol(&ctx->symbols, sm_string_from_cstring(#id)), SM_BUILTIN_SYMBOL(symbol));
    #define REGISTER_BUILTIN(id) REGISTER_BUILTIN_OP(id, id)
    #define REGISTER_BUILTIN_VAR(id) \
        sm_context_register_variable(\
            ctx, sm_symbol(&ctx->symbols, sm_string_from_cstring(#id)), SM_BUILTIN_SYMBOL(id));

    SM_BUILTIN_TABLE(REGISTER_BUILTIN, REGISTER_BUILTIN_OP, REGISTER_BUILTIN_VAR)

    #undef REGISTER_BUILTIN_OP
    #undef REGISTER_BUILTIN
    #undef REGISTER_BUILTIN_VAR
}

// Builtins
SmError SM_BUILTIN_SYMBOL(eval)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (!sm_value_is_list(args) || sm_value_is_quoted(args))
        return sm_error(ctx, SmErrorInvalidArgument, "eval cannot accept a dotted argument list");
    else if (sm_value_is_nil(args))
        return sm_error(ctx, SmErrorMissingArguments, "eval requires exactly 1 argument");
    else if (!sm_value_is_list(args.data.cons->cdr) || sm_value_is_quoted(args.data.cons->cdr))
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
    if (!sm_value_is_list(args) || sm_value_is_quoted(args))
        return sm_error(ctx, SmErrorInvalidArgument, "print cannot accept a dotted argument list");
    else if (sm_value_is_nil(args))
        return sm_error(ctx, SmErrorMissingArguments, "print requires exactly 1 argument");
    else if (!sm_value_is_list(args.data.cons->cdr) || sm_value_is_quoted(args.data.cons->cdr))
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


SmError SM_BUILTIN_SYMBOL(set)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Two required arguments, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true}, { NULL, true} };
    SmArgPattern pattern = { pargs, 2, { NULL, false, false } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("set"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    if (!sm_value_is_symbol(ret->data.cons->car) || sm_value_is_quoted(ret->data.cons->car))
        return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "first argument to set must evaluate to an unquoted symbol"));

    SmString str = sm_symbol_str(ret->data.cons->car.data.symbol);
    if (str.length && str.data[0] == ':')
        return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "keysymbol cannot be used as variable name"));

    if (str.length == 3 && strncmp(str.data, "nil", 3) == 0)
        return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "nil constant cannot be used as variable name"));

    SmVariable* var = sm_scope_lookup(&ctx->frame->scope, ret->data.cons->car.data.symbol);
    if (var) {
        var->value = ret->data.cons->cdr.data.cons->car;
    } else {
        sm_scope_set(&ctx->main.scope,
            ret->data.cons->car.data.symbol,
            ret->data.cons->cdr.data.cons->car);
    }

    return_value_exit_frame(ctx, ret->data.cons->cdr.data.cons->car);
}

SmError SM_BUILTIN_SYMBOL(setq)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (!sm_value_is_list(args) || sm_value_is_quoted(args))
        return sm_error(ctx, SmErrorInvalidArgument, "setq cannot accept a dotted argument list");
    else if (sm_value_is_nil(args))
        return sm_error(ctx, SmErrorMissingArguments, "setq requires at least 2 arguments");

    for (SmCons* cons = args.data.cons; cons; cons = sm_list_next(cons)) {
        if (!sm_value_is_list(cons->cdr) || sm_value_is_quoted(cons->cdr))
            return sm_error(ctx, SmErrorInvalidArgument, "setq cannot accept a dotted argument list");
        else if (sm_value_is_nil(cons->cdr))
            return sm_error(ctx, SmErrorMissingArguments, "odd argument count given to setq");
        else if (!sm_value_is_symbol(cons->car) || sm_value_is_quoted(cons->car))
            return sm_error(ctx, SmErrorInvalidArgument, "odd arguments to setq must be unquoted symbols");

        SmString str = sm_symbol_str(cons->car.data.symbol);
        if (str.length && str.data[0] == ':')
            return_nil(sm_error(ctx, SmErrorInvalidArgument, "keysymbol cannot be used as variable name"));

        if (str.length == 3 && strncmp(str.data, "nil", 3) == 0)
            return_nil(sm_error(ctx, SmErrorInvalidArgument, "nil constant cannot be used as variable name"));

        SmError err = sm_eval(ctx, cons->cdr.data.cons->car, ret);
        if (!sm_is_ok(err))
            return_nil(err);

        SmVariable* var = sm_scope_lookup(&ctx->frame->scope, cons->car.data.symbol);
        if (var)
            var->value = *ret;
        else
            sm_scope_set(&ctx->main.scope, cons->car.data.symbol, *ret);

        cons = sm_list_next(cons);
        if (!sm_value_is_list(cons->cdr) || sm_value_is_quoted(cons->cdr))
            return_nil(sm_error(ctx, SmErrorInvalidArgument, "setq cannot accept a dotted argument list"));
        else if (!sm_value_is_nil(cons->cdr))
            *ret = sm_value_nil();
    }

    return sm_ok;
}


SmError SM_BUILTIN_SYMBOL(do)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (!sm_value_is_list(args) || sm_value_is_quoted(args))
        return sm_error(ctx, SmErrorMissingArguments, "do cannot accept a dotted code list");

    SmError err = sm_ok;

    // Return nil when code list is empty
    *ret = sm_value_nil();

    // Run each form in code list, return result of last one
    for (SmCons* code = args.data.cons; code; code = sm_list_next(code)) {
        if (!sm_value_is_list(code->cdr) || sm_value_is_quoted(code->cdr))
            return_nil(sm_error(ctx, SmErrorInvalidArgument, "do cannot accept a dotted code list"));

        *ret = sm_value_nil();
        err = sm_eval(ctx, code->car, ret);
        if (!sm_is_ok(err))
            return_nil(err);
    }

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(let)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (!sm_value_is_list(args) || sm_value_is_quoted(args))
        return sm_error(ctx, SmErrorMissingArguments, "let cannot accept a dotted argument list");
    else if (sm_value_is_nil(args))
        return sm_error(ctx, SmErrorMissingArguments, "let requires at least 1 argument");
    else if (!sm_value_is_list(args.data.cons->cdr) || sm_value_is_quoted(args.data.cons->cdr))
        return sm_error(ctx, SmErrorMissingArguments, "let cannot accept a dotted code list");
    else if (!sm_value_is_list(args.data.cons->car) || sm_value_is_quoted(args.data.cons->car))
        return sm_error(ctx, SmErrorInvalidArgument, "first argument to let must be an unquoted binding list");

    // FIXME: This implementation is wrong, works like (let*)
    // Fix must wait for proper scope separation
    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("let"), sm_value_nil());

    for (SmCons* cons = args.data.cons->car.data.cons; cons; cons = sm_list_next(cons)) {
        if (!sm_value_is_list(cons->cdr) || sm_value_is_quoted(cons->cdr))
            return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "let cannot accept a dotted binding list"));
        else if ((!sm_value_is_symbol(cons->car) && !sm_value_is_cons(cons->car)) || sm_value_is_quoted(cons->car))
            return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "let binding lists may contain only unquoted 2-element lists or symbols"));
        else if (sm_value_is_cons(cons->car)) {
            if (!sm_value_is_cons(cons->car.data.cons->cdr) || sm_value_is_quoted(cons->car.data.cons->cdr))
                return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "let binding lists may contain only unquoted 2-element lists or symbols"));
            else if (!sm_value_is_nil(cons->car.data.cons->cdr.data.cons->cdr) || sm_value_is_quoted(cons->car.data.cons->cdr.data.cons->cdr))
                return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "let binding lists may contain only unquoted 2-element lists or symbols"));
            else if (!sm_value_is_symbol(cons->car.data.cons->car) || sm_value_is_quoted(cons->car.data.cons->car))
                return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "list elements in let binding lists must be (symbol expression) pairs"));
        }

        SmSymbol id = sm_value_is_symbol(cons->car) ? cons->car.data.symbol : cons->car.data.cons->car.data.symbol;
        SmString str = sm_symbol_str(id);
        if (str.length && str.data[0] == ':')
            return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "keysymbol cannot be used as variable name"));

        if (str.length == 3 && strncmp(str.data, "nil", 3) == 0)
            return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "nil constant cannot be used as variable name"));

        SmVariable* var = sm_scope_set(&frame.scope, id, sm_value_nil());

        if (sm_value_is_cons(cons->car)) {
            SmError err = sm_eval(ctx, cons->car.data.cons->cdr.data.cons->car, &var->value);
            if (!sm_is_ok(err))
                return_nil_exit_frame(ctx, err);
        }
    }

    SmError err = sm_ok;

    // Return nil when code list is empty
    *ret = sm_value_nil();

    // Run each form in code list, return result of last one
    for (SmCons* code = sm_list_next(args.data.cons); code; code = sm_list_next(code)) {
        if (!sm_value_is_list(code->cdr) || sm_value_is_quoted(code->cdr))
            return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "let cannot accept a dotted code list"));

        *ret = sm_value_nil();
        err = sm_eval(ctx, code->car, ret);
        if (!sm_is_ok(err))
            return_nil_exit_frame(ctx, err);
    }

    sm_context_exit_frame(ctx);
    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(if)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (!sm_value_is_list(args) || sm_value_is_quoted(args) || !sm_value_is_nil(sm_list_dot(args.data.cons)))
        return sm_error(ctx, SmErrorMissingArguments, "if cannot accept a dotted argument list");

    size_t argc = sm_list_size(args.data.cons);
    if (argc < 2 || argc > 3)
        return sm_error(ctx, (argc < 2) ? SmErrorMissingArguments : SmErrorExcessArguments, "if requires 2 or 3 arguments");

    SmError err = sm_eval(ctx, args.data.cons->car, ret);
    if (!sm_is_ok(err))
        return_nil(err);

    if (!sm_value_is_nil(*ret)) {
        return sm_eval(ctx, args.data.cons->cdr.data.cons->car, ret);
    } else if (argc == 3) {
        return sm_eval(ctx, args.data.cons->cdr.data.cons->cdr.data.cons->car, ret);
    }

    return_nil(sm_ok);
}


SmError SM_BUILTIN_SYMBOL(cons)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Two required arguments, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true }, { NULL, true } };
    SmArgPattern pattern = { pargs, 2, { NULL, false, false } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("cons"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    // Second argument becomes cdr
    ret->data.cons->cdr = ret->data.cons->cdr.data.cons->car;

    sm_context_exit_frame(ctx);
    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(list)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Optional argument list, evaluated
    SmArgPattern pattern = { NULL, 0, { NULL, true, true } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("list"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    // Return evaluated argument list, precisely what list does
    sm_context_exit_frame(ctx);
    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(list_dot)(SmContext* ctx, SmValue args, SmValue* ret) {
    // One required argument plus optional argument list, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true } };
    SmArgPattern pattern = { pargs, 1, { NULL, true, true } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("list*"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    SmCons* arg = ret->data.cons;

    // If only one argument, return it
    if (!sm_list_next(arg))
        return_value_exit_frame(ctx, ret->data.cons->car);

    // Stop one cons before the last one
    while (sm_list_next(sm_list_next(arg)))
        arg = sm_list_next(arg);

    // Use last argument as final cdr
    arg->cdr = arg->cdr.data.cons->car;

    // Return evaluated argument list
    sm_context_exit_frame(ctx);
    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(lambda)(SmContext* ctx, SmValue args, SmValue* ret) {
    SmError err = sm_validate_lambda(ctx, args);
    if (!sm_is_ok(err))
        return err;

    SmCons* lambda = sm_heap_alloc_cons(&ctx->heap, ctx->frame);
    *ret = sm_value_cons(lambda);

    lambda->car = sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda")));
    lambda->cdr = args;

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(quote)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (!sm_value_is_list(args) || sm_value_is_quoted(args))
        return sm_error(ctx, SmErrorInvalidArgument, "quote cannot accept a dotted argument list");
    else if (sm_value_is_nil(args))
        return sm_error(ctx, SmErrorMissingArguments, "quote requires exactly 1 argument");
    else if (!sm_value_is_list(args.data.cons->cdr) || sm_value_is_quoted(args.data.cons->cdr))
        return sm_error(ctx, SmErrorInvalidArgument, "quote cannot accept a dotted argument list");
    else if (!sm_value_is_nil(args.data.cons->cdr))
        return sm_error(ctx, SmErrorExcessArguments, "quote requires exactly 1 argument");

    // This should work like a macro, that is: its result should be evaluated
    // again. So we return the argument without quoting it (== quote + eval).
    return_value(args.data.cons->car);
}


SmError SM_BUILTIN_SYMBOL(car)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (!sm_value_is_list(args) || sm_value_is_quoted(args))
        return sm_error(ctx, SmErrorInvalidArgument, "car cannot accept a dotted argument list");
    else if (sm_value_is_nil(args))
        return sm_error(ctx, SmErrorMissingArguments, "car requires exactly 1 argument");
    else if (!sm_value_is_list(args.data.cons->cdr) || sm_value_is_quoted(args.data.cons->cdr))
        return sm_error(ctx, SmErrorInvalidArgument, "car cannot accept a dotted argument list");
    else if (!sm_value_is_nil(args.data.cons->cdr))
        return sm_error(ctx, SmErrorExcessArguments, "car requires exactly 1 argument");

    SmError err = sm_eval(ctx, args.data.cons->car, ret);
    if (!sm_is_ok(err))
        return_nil(err);

    if (sm_value_is_quoted(*ret))
        return_value(sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("quote"))));

    if (!sm_value_is_list(*ret))
        return_nil(sm_error(ctx, SmErrorInvalidArgument, "car argument must be a list"));

    if (!sm_value_is_nil(*ret))
        *ret = ret->data.cons->car;

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cdr)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (!sm_value_is_list(args) || sm_value_is_quoted(args))
        return sm_error(ctx, SmErrorInvalidArgument, "cdr cannot accept a dotted argument list");
    else if (sm_value_is_nil(args))
        return sm_error(ctx, SmErrorMissingArguments, "cdr requires exactly 1 argument");
    else if (!sm_value_is_list(args.data.cons->cdr) || sm_value_is_quoted(args.data.cons->cdr))
        return sm_error(ctx, SmErrorInvalidArgument, "cdr cannot accept a dotted argument list");
    else if (!sm_value_is_nil(args.data.cons->cdr))
        return sm_error(ctx, SmErrorExcessArguments, "cdr requires exactly 1 argument");

    SmError err = sm_eval(ctx, args.data.cons->car, ret);
    if (!sm_is_ok(err))
        return_nil(err);

    if (sm_value_is_quoted(*ret)) {
        SmCons* cons = sm_heap_alloc_cons(&ctx->heap, ctx->frame);
        cons->car = sm_value_unquote(*ret, 1);
        return_value(sm_value_cons(cons));
    }

    if (!sm_value_is_list(*ret))
        return_nil(sm_error(ctx, SmErrorInvalidArgument, "cdr argument must be a list"));

    if (!sm_value_is_nil(*ret))
        *ret = ret->data.cons->cdr;

    return sm_ok;
}


SmError SM_BUILTIN_SYMBOL(caar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildCar, sm_value_symbol(lst),
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cadr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildCar, sm_value_symbol(lst),
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cdar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildCar, sm_value_symbol(lst),
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cddr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildCar, sm_value_symbol(lst),
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}


SmError SM_BUILTIN_SYMBOL(caaar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildCar, sm_value_symbol(lst),
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(caadr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildCar, sm_value_symbol(lst),
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cadar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildCar, sm_value_symbol(lst),
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cdaar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildCar, sm_value_symbol(lst),
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(caddr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildCar, sm_value_symbol(lst),
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cddar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildCar, sm_value_symbol(lst),
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cdadr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildCar, sm_value_symbol(lst),
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cdddr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildCar, sm_value_symbol(lst),
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}


SmError SM_BUILTIN_SYMBOL(caaaar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(car),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(caaadr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(cdr),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(caadar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(car),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cadaar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(car),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cdaaar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(car),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(caaddr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(cdr),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(caddar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(car),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cddaar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(car),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cadadr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(cdr),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cdadar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(car),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cdaadr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(cdr),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cadddr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(car),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(cdr),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cdaddr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(car),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(cdr),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cddadr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(car),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(cdr),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cdddar)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol car = sm_symbol(&ctx->symbols, sm_string_from_cstring("car"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(car),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(cddddr)(SmContext* ctx, SmValue* ret) {
    SmSymbol lst = sm_symbol(&ctx->symbols, sm_string_from_cstring("lst"));
    SmSymbol cdr = sm_symbol(&ctx->symbols, sm_string_from_cstring("cdr"));

    sm_build_list(ctx, ret,
        SmBuildCar, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring("lambda"))),
        SmBuildList,
            SmBuildCar, sm_value_symbol(lst),
            SmBuildEnd,
        SmBuildList,
            SmBuildCar, sm_value_symbol(cdr),
            SmBuildList,
                SmBuildCar, sm_value_symbol(cdr),
                SmBuildList,
                    SmBuildCar, sm_value_symbol(cdr),
                    SmBuildList,
                        SmBuildCar, sm_value_symbol(cdr),
                        SmBuildCar, sm_value_symbol(lst),
                        SmBuildEnd,
                    SmBuildEnd,
                SmBuildEnd,
            SmBuildEnd,
        SmBuildEnd);

    return sm_ok;
}


SmError SM_BUILTIN_SYMBOL(add)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Optional argument list, evaluated
    SmArgPattern pattern = { NULL, 0, { NULL, true, true } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("+"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    SmNumber res = sm_number_int(0);
    SmCons* arg = (sm_value_is_cons(*ret) ? ret->data.cons : NULL);

    // Sum integers
    for (; arg; arg = sm_list_next(arg)) {
        if (!sm_value_is_number(arg->car))
            return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "+ arguments must be numbers"));

        if (!sm_number_is_int(arg->car.data.number))
            break;

        res.value.i += arg->car.data.number.value.i;
    }

    // Sum floats
    if (arg) {
        res = sm_number_as_float(res);

        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car))
                return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "+ arguments must be numbers"));

            res.value.f += sm_number_as_float(arg->car.data.number).value.f;
        }
    }

    return_value_exit_frame(ctx, sm_value_number(res));
}

SmError SM_BUILTIN_SYMBOL(sub)(SmContext* ctx, SmValue args, SmValue* ret) {
    // One required argument plus optional argument list, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true } };
    SmArgPattern pattern = { pargs, 1, { NULL, true, true } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("-"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    // Argument list is not empty so skip type check
    SmCons* arg = ret->data.cons;

    if (!sm_value_is_number(arg->car))
        return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "- arguments must be numbers"));

    // If there is a single argument, invert sign
    if (!sm_list_next(arg)) {
        if (sm_number_is_int(arg->car.data.number))
            return_value_exit_frame(ctx, sm_value_number(sm_number_int(- arg->car.data.number.value.i)));
        else
            return_value_exit_frame(ctx, sm_value_number(sm_number_float(- arg->car.data.number.value.f)));
    }

    SmNumber res = arg->car.data.number;
    arg = sm_list_next(arg);

    // Subtract integers
    if (sm_number_is_int(res)) {
        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car))
                return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "- arguments must be numbers"));

            if (!sm_number_is_int(arg->car.data.number))
                break;

            res.value.i -= arg->car.data.number.value.i;
        }
    }

    // Subtract floats
    if (arg) {
        res = sm_number_as_float(res);

        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car))
                return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "- arguments must be numbers"));

            res.value.f -= sm_number_as_float(arg->car.data.number).value.f;
        }
    }

    return_value_exit_frame(ctx, sm_value_number(res));
}

SmError SM_BUILTIN_SYMBOL(mul)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Optional argument list, evaluated
    SmArgPattern pattern = { NULL, 0, { NULL, true, true } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("*"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    SmNumber res = sm_number_int(1);
    SmCons* arg = (sm_value_is_cons(*ret) ? ret->data.cons : NULL);

    // Multiply integers
    for (; arg; arg = sm_list_next(arg)) {
        if (!sm_value_is_number(arg->car))
            return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "* arguments must be numbers"));

        if (!sm_number_is_int(arg->car.data.number))
            break;

        res.value.i *= arg->car.data.number.value.i;
    }

    // Multiply floats
    if (arg) {
        res = sm_number_as_float(res);

        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car))
                return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "* arguments must be numbers"));

            res.value.f *= sm_number_as_float(arg->car.data.number).value.f;
        }
    }

    return_value_exit_frame(ctx, sm_value_number(res));
}

SmError SM_BUILTIN_SYMBOL(div)(SmContext* ctx, SmValue args, SmValue* ret) {
    // One required argument plus optional argument list, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true} };
    SmArgPattern pattern = { pargs, 1, { NULL, true, true } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("/"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    // Argument list is not empty so skip type check
    SmCons* arg = ret->data.cons;

    if (!sm_value_is_number(arg->car))
        return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "- arguments must be numbers"));

    // If there is a single argument, return inverse
    if (!sm_list_next(arg)) {
        if (sm_number_is_int(arg->car.data.number))
            return_value_exit_frame(ctx, sm_value_number(sm_number_int(1/arg->car.data.number.value.i)));
        else
            return_value_exit_frame(ctx, sm_value_number(sm_number_float(1.0/arg->car.data.number.value.f)));
    }

    SmNumber res = arg->car.data.number;
    arg = sm_list_next(arg);

    // Divide integers
    if (sm_number_is_int(res)) {
        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car))
                return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "/ arguments must be numbers"));

            if (!sm_number_is_int(arg->car.data.number))
                break;

            res.value.i /= arg->car.data.number.value.i;
        }
    }

    // Divide floats
    if (arg) {
        res = sm_number_as_float(res);

        for (; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_number(arg->car))
                return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "/ arguments must be numbers"));

            res.value.f /= sm_number_as_float(arg->car.data.number).value.f;
        }
    }

    return_value_exit_frame(ctx, sm_value_number(res));
}


SmError SM_BUILTIN_SYMBOL(eq)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Two required arguments, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true}, { NULL, true} };
    SmArgPattern pattern = { pargs, 2, { NULL, false, false } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("set"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    if (!sm_value_is_number(ret->data.cons->car) || !sm_value_is_number(ret->data.cons->cdr.data.cons->car))
        return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "= arguments must be numbers"));

    SmNumber lhs = ret->data.cons->car.data.number;
    SmNumber rhs = ret->data.cons->cdr.data.cons->car.data.number;

    SmNumberType t = sm_number_common_type(lhs.type, rhs.type);
    lhs = sm_number_as_type(t, lhs);
    rhs = sm_number_as_type(t, rhs);

    if ((sm_number_is_int(lhs) && lhs.value.i == rhs.value.i) ||
        (sm_number_is_float(lhs) && lhs.value.f == rhs.value.f))
    {
        return_value_exit_frame(ctx, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring(":true"))));
    }

    return_nil_exit_frame(ctx, sm_ok);
}

SmError SM_BUILTIN_SYMBOL(neq)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Two required arguments, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true}, { NULL, true} };
    SmArgPattern pattern = { pargs, 2, { NULL, false, false } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("set"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    if (!sm_value_is_number(ret->data.cons->car) || !sm_value_is_number(ret->data.cons->cdr.data.cons->car))
        return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "= arguments must be numbers"));

    SmNumber lhs = ret->data.cons->car.data.number;
    SmNumber rhs = ret->data.cons->cdr.data.cons->car.data.number;

    SmNumberType t = sm_number_common_type(lhs.type, rhs.type);
    lhs = sm_number_as_type(t, lhs);
    rhs = sm_number_as_type(t, rhs);

    if ((sm_number_is_int(lhs) && lhs.value.i != rhs.value.i) ||
        (sm_number_is_float(lhs) && lhs.value.f != rhs.value.f))
    {
        return_value_exit_frame(ctx, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring(":true"))));
    }

    return_nil_exit_frame(ctx, sm_ok);
}

SmError SM_BUILTIN_SYMBOL(lt)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Two required arguments, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true}, { NULL, true} };
    SmArgPattern pattern = { pargs, 2, { NULL, false, false } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("set"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    if (!sm_value_is_number(ret->data.cons->car) || !sm_value_is_number(ret->data.cons->cdr.data.cons->car))
        return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "= arguments must be numbers"));

    SmNumber lhs = ret->data.cons->car.data.number;
    SmNumber rhs = ret->data.cons->cdr.data.cons->car.data.number;

    SmNumberType t = sm_number_common_type(lhs.type, rhs.type);
    lhs = sm_number_as_type(t, lhs);
    rhs = sm_number_as_type(t, rhs);

    if ((sm_number_is_int(lhs) && lhs.value.i < rhs.value.i) ||
        (sm_number_is_float(lhs) && lhs.value.f < rhs.value.f))
    {
        return_value_exit_frame(ctx, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring(":true"))));
    }

    return_nil_exit_frame(ctx, sm_ok);
}

SmError SM_BUILTIN_SYMBOL(lteq)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Two required arguments, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true}, { NULL, true} };
    SmArgPattern pattern = { pargs, 2, { NULL, false, false } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("set"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    if (!sm_value_is_number(ret->data.cons->car) || !sm_value_is_number(ret->data.cons->cdr.data.cons->car))
        return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "= arguments must be numbers"));

    SmNumber lhs = ret->data.cons->car.data.number;
    SmNumber rhs = ret->data.cons->cdr.data.cons->car.data.number;

    SmNumberType t = sm_number_common_type(lhs.type, rhs.type);
    lhs = sm_number_as_type(t, lhs);
    rhs = sm_number_as_type(t, rhs);

    if ((sm_number_is_int(lhs) && lhs.value.i <= rhs.value.i) ||
        (sm_number_is_float(lhs) && lhs.value.f <= rhs.value.f))
    {
        return_value_exit_frame(ctx, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring(":true"))));
    }

    return_nil_exit_frame(ctx, sm_ok);
}

SmError SM_BUILTIN_SYMBOL(gt)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Two required arguments, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true}, { NULL, true} };
    SmArgPattern pattern = { pargs, 2, { NULL, false, false } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("set"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    if (!sm_value_is_number(ret->data.cons->car) || !sm_value_is_number(ret->data.cons->cdr.data.cons->car))
        return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "= arguments must be numbers"));

    SmNumber lhs = ret->data.cons->car.data.number;
    SmNumber rhs = ret->data.cons->cdr.data.cons->car.data.number;

    SmNumberType t = sm_number_common_type(lhs.type, rhs.type);
    lhs = sm_number_as_type(t, lhs);
    rhs = sm_number_as_type(t, rhs);

    if ((sm_number_is_int(lhs) && lhs.value.i > rhs.value.i) ||
        (sm_number_is_float(lhs) && lhs.value.f > rhs.value.f))
    {
        return_value_exit_frame(ctx, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring(":true"))));
    }

    return_nil_exit_frame(ctx, sm_ok);
}

SmError SM_BUILTIN_SYMBOL(gteq)(SmContext* ctx, SmValue args, SmValue* ret) {
    // Two required arguments, evaluated
    const SmArgPatternArg pargs[] = { { NULL, true}, { NULL, true} };
    SmArgPattern pattern = { pargs, 2, { NULL, false, false } };

    SmStackFrame frame;
    sm_context_enter_frame(ctx, &frame, sm_string_from_cstring("set"), sm_value_nil());

    SmError err = sm_arg_pattern_eval(&pattern, ctx, args, ret);
    if (!sm_is_ok(err))
        return_nil_exit_frame(ctx, err);

    if (!sm_value_is_number(ret->data.cons->car) || !sm_value_is_number(ret->data.cons->cdr.data.cons->car))
        return_nil_exit_frame(ctx, sm_error(ctx, SmErrorInvalidArgument, "= arguments must be numbers"));

    SmNumber lhs = ret->data.cons->car.data.number;
    SmNumber rhs = ret->data.cons->cdr.data.cons->car.data.number;

    SmNumberType t = sm_number_common_type(lhs.type, rhs.type);
    lhs = sm_number_as_type(t, lhs);
    rhs = sm_number_as_type(t, rhs);

    if ((sm_number_is_int(lhs) && lhs.value.i >= rhs.value.i) ||
        (sm_number_is_float(lhs) && lhs.value.f >= rhs.value.f))
    {
        return_value_exit_frame(ctx, sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring(":true"))));
    }

    return_nil_exit_frame(ctx, sm_ok);
}


SmError SM_BUILTIN_SYMBOL(not)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (!sm_value_is_list(args) || sm_value_is_quoted(args))
        return sm_error(ctx, SmErrorInvalidArgument, "not cannot accept a dotted argument list");
    else if (sm_value_is_nil(args))
        return sm_error(ctx, SmErrorMissingArguments, "not requires exactly 1 argument");
    else if (!sm_value_is_list(args.data.cons->cdr) || sm_value_is_quoted(args.data.cons->cdr))
        return sm_error(ctx, SmErrorInvalidArgument, "not cannot accept a dotted argument list");
    else if (!sm_value_is_nil(args.data.cons->cdr))
        return sm_error(ctx, SmErrorExcessArguments, "not requires exactly 1 argument");

    SmError err = sm_eval(ctx, args.data.cons->car, ret);
    if (!sm_is_ok(err))
        return_nil(err);

    *ret = sm_value_is_nil(*ret) ?
        sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring(":true"))) : sm_value_nil();

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(and)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (!sm_value_is_list(args) || sm_value_is_quoted(args))
        return sm_error(ctx, SmErrorMissingArguments, "and cannot accept a dotted argument list");

    SmError err = sm_ok;

    // Return true when code list is empty
    *ret = sm_value_symbol(sm_symbol(&ctx->symbols, sm_string_from_cstring(":true")));

    // Run each form in code list, return result of last one unless a previous one returns nil
    for (SmCons* arg = args.data.cons; arg; arg = sm_list_next(arg)) {
        if (!sm_value_is_list(arg->cdr) || sm_value_is_quoted(arg->cdr))
            return_nil(sm_error(ctx, SmErrorInvalidArgument, "and cannot accept a dotted code list"));

        *ret = sm_value_nil();
        err = sm_eval(ctx, arg->car, ret);
        if (!sm_is_ok(err))
            return_nil(err);

        if (sm_value_is_nil(*ret))
            return sm_ok;
    }

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(or)(SmContext* ctx, SmValue args, SmValue* ret) {
    if (!sm_value_is_list(args) || sm_value_is_quoted(args))
        return sm_error(ctx, SmErrorMissingArguments, "or cannot accept a dotted argument list");

    SmError err = sm_ok;

    // Return nil when code list is empty
    *ret = sm_value_nil();

    // Run each form in code list, return first result not nil
    for (SmCons* arg = args.data.cons; arg; arg = sm_list_next(arg)) {
        if (!sm_value_is_list(arg->cdr) || sm_value_is_quoted(arg->cdr))
            return_nil(sm_error(ctx, SmErrorInvalidArgument, "or cannot accept a dotted code list"));

        *ret = sm_value_nil();
        err = sm_eval(ctx, arg->car, ret);
        if (!sm_is_ok(err))
            return_nil(err);

        if (!sm_value_is_nil(*ret))
            return sm_ok;
    }

    return sm_ok;
}
