#pragma once

#include "error.h"
#include "context.h"
#include "value.h"

void sm_register_builtins(SmContext* ctx);

#define SM_BUILTIN_SYMBOL(id) sm_builtin_##id

#define SM_BUILTIN_TABLE(builtin, builtin_op) \
    builtin(eval) \
    builtin(print) \
\
    builtin(cons) \
    builtin(lambda) \
    builtin(quote) \
\
    builtin(car) \
    builtin(cdr) \
\
    builtin(caar) \
    builtin(cadr) \
    builtin(cdar) \
    builtin(cddr) \
\
    builtin(caaar) \
    builtin(caadr) \
    builtin(cadar) \
    builtin(cdaar) \
    builtin(caddr) \
    builtin(cddar) \
    builtin(cdadr) \
    builtin(cdddr) \
\
    builtin(caaaar) \
    builtin(caaadr) \
    builtin(caadar) \
    builtin(cadaar) \
    builtin(cdaaar) \
    builtin(caaddr) \
    builtin(caddar) \
    builtin(cddaar) \
    builtin(cadadr) \
    builtin(cdadar) \
    builtin(cdaadr) \
    builtin(cadddr) \
    builtin(cdddar) \
    builtin(cddddr) \
\
    builtin_op(add, +) \
    builtin_op(sub, -) \
    builtin_op(mul, *) \
    builtin_op(div, /)

#define SM_DECLARE_BUILTIN(id) \
    SmError SM_BUILTIN_SYMBOL(id)(SmContext* ctx, SmCons* params, SmValue* ret);
#define SM_DECLARE_BUILTIN_OP(symbol, id) SM_DECLARE_BUILTIN(symbol)

SM_BUILTIN_TABLE(SM_DECLARE_BUILTIN, SM_DECLARE_BUILTIN_OP)

#undef SM_DECLARE_BUILTIN
#undef SM_DECLARE_BUILTIN_OP
