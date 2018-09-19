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

SmError SM_BUILTIN_SYMBOL(print)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(cons)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(lambda)(SmContext* ctx, SmCons* params, SmValue* ret) {
    SmError err = sm_validate_lambda(ctx, params);
    if (err.code != SmErrorOk)
        return err;

    SmCons* lambda = sm_heap_alloc(&ctx->heap, ctx->frame);
    *ret = sm_value_cons(lambda);

    lambda->car = sm_value_word(sm_word(&ctx->words, sm_string_from_cstring("lambda")));
    lambda->cdr = sm_value_cons(params);

    return sm_ok;
}

SmError SM_BUILTIN_SYMBOL(quote)(SmContext* ctx, SmCons* params, SmValue* ret) {
    if (!params) {
        sm_throw(ctx, SmErrorMissingArguments, "quote requires exactly 1 parameter", false);
    } else if (sm_list_next(params)) {
        sm_throw(ctx, SmErrorExcessArguments, "quote requires exactly 1 parameter", false);
    } else if (!sm_value_is_nil(params->cdr)) {
        sm_throw(ctx, SmErrorInvalidArgument, "quote cannot accept a dotted parameter list", false);
    }

    sm_return(sm_value_quote(params->car, 1));
}


SmError SM_BUILTIN_SYMBOL(car)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(caar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(caaar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caadr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdaar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caddr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdadr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdddr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(caaaar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caaadr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caadar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadaar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdaaar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caaddr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(caddar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddaar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadadr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdadar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdaadr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cadddr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cdddar)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(cddddr)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}


SmError SM_BUILTIN_SYMBOL(add)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(sub)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(mul)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}

SmError SM_BUILTIN_SYMBOL(div)(SmContext* ctx, SmCons* params, SmValue* ret) {
    sm_unused(ctx); sm_unused(params);
    sm_return(sm_value_nil());
}
