#include "builtins.h"
#include "eval.h"

void sm_register_builtins(SmContext* ctx) {
    #define REGISTER_BUILTIN_OP(symbol, id) \
        sm_context_register_function(\
            ctx, sm_word(&ctx->words, sm_string_from_cstring(#id)), SM_BUILTIN_SYMBOL(symbol));
    #define REGISTER_BUILTIN(id) REGISTER_BUILTIN_OP(id, id)

    SM_BUILTIN_TABLE(REGISTER_BUILTIN, REGISTER_BUILTIN_OP)

    #undef REGISTER_BUILTIN_OP
    #undef REGISTER_BUILTIN
}

#define sm_return(value) { *ret = (value); return sm_ok; }

SmError SM_BUILTIN_SYMBOL(eval)(SmContext* ctx, SmValue params, SmValue* ret) {
    if (sm_value_is_nil(params))
        return sm_error(ctx, SmErrorMissingArguments, "eval requires exactly 1 parameter");
    else if (!sm_value_is_cons(params) || !sm_value_is_list(params.data.cons->cdr))
        return sm_error(ctx, SmErrorInvalidArgument, "eval cannot accept a dotted parameter list");
    else if (!sm_value_is_nil(params.data.cons->cdr))
        return sm_error(ctx, SmErrorExcessArguments, "eval requires exactly 1 parameter");

    SmValue* form = sm_heap_root(&ctx->heap);
    SmError err = sm_eval(ctx, params.data.cons->car, form);
    if (err.code == SmErrorOk)
        err = sm_eval(ctx, *form, ret);

    sm_heap_root_drop(&ctx->heap, form);
    return err;
}

SmError SM_BUILTIN_SYMBOL(print)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(cons)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(lambda)(SmContext* ctx, SmValue params, SmValue* ret) {
    SmError err = sm_validate_lambda(ctx, params);
    if (err.code != SmErrorOk)
        return err;

    SmCons* lambda = sm_heap_alloc(&ctx->heap, ctx->frame);
    *ret = sm_value_cons(lambda);

    lambda->car = sm_value_word(sm_word(&ctx->words, sm_string_from_cstring("lambda")));
    lambda->cdr = params;

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(quote)(SmContext* ctx, SmValue params, SmValue* ret) {
    if (sm_value_is_nil(params))
        return sm_error(ctx, SmErrorMissingArguments, "quote requires exactly 1 parameter");
    else if (!sm_value_is_cons(params) || !sm_value_is_list(params.data.cons->cdr))
        return sm_error(ctx, SmErrorInvalidArgument, "quote cannot accept a dotted parameter list");
    else if (!sm_value_is_nil(params.data.cons->cdr))
        return sm_error(ctx, SmErrorExcessArguments, "quote requires exactly 1 parameter");

    sm_return(sm_value_quote(params.data.cons->car, 1));
}


SmError SM_BUILTIN_SYMBOL(car)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(caar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(caaar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caadr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdaar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caddr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdadr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdddr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(caaaar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caaadr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caadar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadaar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdaaar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caaddr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caddar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddaar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadadr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdadar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdaadr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadddr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdddar)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddddr)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(add)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(sub)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(mul)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(div)(SmContext* ctx, SmValue params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}
