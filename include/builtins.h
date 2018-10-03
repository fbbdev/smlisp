#pragma once

#include "error.h"
#include "context.h"
#include "value.h"

void sm_register_builtins(SmContext* ctx);

#define SM_BUILTIN_SYMBOL(id) sm_builtin_##id

#define SM_BUILTIN_TABLE(builtin, builtin_op, builtin_var) \
    builtin(eval) \
    builtin(print) \
\
    builtin(set) \
    builtin(setq) \
\
    builtin(do) \
    builtin(let) \
    builtin(if) \
\
    builtin(cons) \
    builtin(list) \
    builtin_op(list_dot, list*) \
    builtin(lambda) \
    builtin(quote) \
\
    builtin(car) \
    builtin(cdr) \
\
    builtin_var(caar) \
    builtin_var(cadr) \
    builtin_var(cdar) \
    builtin_var(cddr) \
\
    builtin_var(caaar) \
    builtin_var(caadr) \
    builtin_var(cadar) \
    builtin_var(cdaar) \
    builtin_var(caddr) \
    builtin_var(cddar) \
    builtin_var(cdadr) \
    builtin_var(cdddr) \
\
    builtin_var(caaaar) \
    builtin_var(caaadr) \
    builtin_var(caadar) \
    builtin_var(cadaar) \
    builtin_var(cdaaar) \
    builtin_var(caaddr) \
    builtin_var(caddar) \
    builtin_var(cddaar) \
    builtin_var(cadadr) \
    builtin_var(cdadar) \
    builtin_var(cdaadr) \
    builtin_var(cadddr) \
    builtin_var(cdaddr) \
    builtin_var(cddadr) \
    builtin_var(cdddar) \
    builtin_var(cddddr) \
\
    builtin_op(add, +) \
    builtin_op(sub, -) \
    builtin_op(mul, *) \
    builtin_op(div, /) \
\
    builtin_op(eq,   =) \
    builtin_op(neq,  !=) \
    builtin_op(lt,   <) \
    builtin_op(lteq, <=) \
    builtin_op(gt,   >) \
    builtin_op(gteq, >=) \
\
    builtin(not) \
    builtin(and) \
    builtin(or)

#define SM_DECLARE_BUILTIN(id) \
    SmError SM_BUILTIN_SYMBOL(id)(SmContext* ctx, SmValue args, SmValue* ret);
#define SM_DECLARE_BUILTIN_OP(symbol, id) SM_DECLARE_BUILTIN(symbol)
#define SM_DECLARE_BUILTIN_VAR(id) \
    SmError SM_BUILTIN_SYMBOL(id)(SmContext* ctx, SmValue* ret);

SM_BUILTIN_TABLE(SM_DECLARE_BUILTIN, SM_DECLARE_BUILTIN_OP, SM_DECLARE_BUILTIN_VAR)

#undef SM_DECLARE_BUILTIN
#undef SM_DECLARE_BUILTIN_OP
#undef SM_DECLARE_BUILTIN_VAR
