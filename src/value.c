#include "context.h"
#include "value.h"
#include "util.h"

#include <stdarg.h>

// Inlines
extern inline SmValue sm_value_nil();
extern inline SmValue sm_value_number(SmNumber number);
extern inline SmValue sm_value_word(SmWord word);
extern inline SmValue sm_value_cons(SmCons* cons);
extern inline bool sm_value_is_nil(SmValue value);
extern inline bool sm_value_is_number(SmValue value);
extern inline bool sm_value_is_word(SmValue value);
extern inline bool sm_value_is_cons(SmValue value);
extern inline bool sm_value_is_unquoted(SmValue value);
extern inline bool sm_value_is_quoted(SmValue value);
extern inline SmValue sm_value_quote(SmValue value, uint8_t quotes);
extern inline SmValue sm_value_unquote(SmValue value, uint8_t unquotes);

extern inline bool sm_value_is_list(SmValue value);
extern inline SmCons* sm_list_next(SmCons* cons);

SmValue build_list_v(SmContext* ctx, va_list* args) {
    SmBuildOp op = va_arg(*args, SmBuildOp);

    if (op == SmBuildCdr)
        return va_arg(*args, SmValue);
    else if (op == SmBuildEnd)
        return sm_value_nil();

    SmCons* cons = sm_heap_alloc_owned(&ctx->heap, ctx->frame);
    SmValue ret = sm_value_cons(cons); // Save head

    if (op == SmBuildCar) {
        cons->car = va_arg(*args, SmValue);
    } else if (op == SmBuildList) {
        cons->car = build_list_v(ctx, args);
        sm_heap_disown_value(&ctx->heap, ctx->frame, cons->car);
    } else {
        sm_panic("invalid operation in sm_build_list arguments");
    }

    for (op = va_arg(*args, SmBuildOp); op != SmBuildCdr && op != SmBuildEnd; op = va_arg(*args, SmBuildOp))
    {
        // Continue list
        cons->cdr = sm_value_cons(sm_heap_alloc(&ctx->heap, ctx->frame));
        cons = cons->cdr.data.cons;

        if (op == SmBuildCar) {
            cons->car = va_arg(*args, SmValue);
        } else if (op == SmBuildList) {
            cons->car = build_list_v(ctx, args);
            sm_heap_disown_value(&ctx->heap, ctx->frame, cons->car);
        } else {
            sm_panic("invalid operation in sm_build_list arguments");
        }
    }

    cons->cdr = (op == SmBuildCdr) ? va_arg(*args, SmValue) : sm_value_nil();

    return ret;
}

SmValue sm_build_list(SmContext* ctx, ...) {
    va_list args;

    va_start(args, ctx);
    SmValue ret = build_list_v(ctx, &args);
    va_end(args);

    return ret;
}
