#include "context.h"
#include "function.h"
#include "parser.h"
#include "util.h"
#include "value.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

// Inlines
extern inline SmValue sm_value_nil();
extern inline SmValue sm_value_number(SmNumber number);
extern inline SmValue sm_value_symbol(SmSymbol symbol);
extern inline SmValue sm_value_string(SmString view);
extern inline SmValue sm_value_cons(SmCons* cons);
extern inline SmValue sm_value_function(SmFunction* function);
extern inline bool sm_value_is_nil(SmValue value);
extern inline bool sm_value_is_number(SmValue value);
extern inline bool sm_value_is_symbol(SmValue value);
extern inline bool sm_value_is_string(SmValue value);
extern inline bool sm_value_is_cons(SmValue value);
extern inline bool sm_value_is_function(SmValue value);
extern inline bool sm_value_is_quoted(SmValue value);
extern inline SmValue sm_value_quote(SmValue value, uint8_t quotes);
extern inline SmValue sm_value_unquote(SmValue value, uint8_t unquotes);

extern inline bool sm_value_is_list(SmValue value);
extern inline SmCons* sm_list_next(SmCons* cons);
extern inline SmValue sm_list_dot(SmCons* cons);
extern inline bool sm_list_is_dotted(SmCons* cons);
extern inline size_t sm_list_size(SmCons* cons);

// Private helpers
static bool is_piped_symbol(SmString str) {
    if (str.length == 0)
        return true;

    if (str.data[0] == '.' && str.length == 1)
        return true;

    if (sm_can_parse_float(str))
        return true;

    for (size_t i = 0; i < str.length; ++i) {
        if (isspace(str.data[i]) || str.data[i] == '(' ||
            str.data[i] == ')' || str.data[i] == '\'' || str.data[i] == '"')
        {
            return true;
        }
    }

    return false;
}

static inline char const* escape_char(char chr) {
    switch (chr) {
        case '\\':
            return "\\\\";
        case '\"':
            return "\\\"";
        case '\n':
            return "\\n";
        case '\r':
            return "\\r";
        case '\b':
            return "\\b";
        case '\t':
            return "\\t";
        case '\f':
            return "\\f";
        case '\a':
            return "\\a";
        case '\v':
            return "\\v";
        default:
            return NULL;
    }
}

// Debug helper
void sm_print_value(FILE* f, SmValue value) {
    for (uint8_t i = 0; i < value.quotes; ++i)
        fprintf(f, "\'");

    SmString str;

    switch (value.type) {
        case SmTypeNil:
            printf("nil");
            break;

        case SmTypeNumber:
            if (sm_number_is_int(value.data.number))
                fprintf(f, "%jd", (intmax_t) value.data.number.value.i);
            else
                fprintf(f, "%f", value.data.number.value.f);
            break;

        case SmTypeSymbol:
            str = sm_symbol_str(value.data.symbol);

            bool piped = is_piped_symbol(str);
            if (piped)
                fprintf(f, "|");

            fwrite(str.data, str.length, sizeof(char), f);

            if (piped)
                fprintf(f, "|");
            break;

        case SmTypeString:
            fprintf(f, "\"");
            for (char const *p = value.data.string.data, *end = p + value.data.string.length;
                 p != end; ++p)
            {
                char const* esc = escape_char(*p);
                if (esc)
                    fprintf(f, "%s", esc);
                else
                    fprintf(f, "%c", *p);
            }
            fprintf(f, "\"");
            break;

        case SmTypeCons:
            fprintf(f, "(");

            sm_print_value(f, value.data.cons->car);
            if (!sm_value_is_list(value.data.cons->cdr) || sm_value_is_quoted(value.data.cons->cdr)) {
                fprintf(f, " . ");
                sm_print_value(f, value.data.cons->cdr);
            }

            for (SmCons* cons = sm_list_next(value.data.cons); cons; cons = sm_list_next(cons)) {
                fprintf(f, " ");
                sm_print_value(f, cons->car);

                if (!sm_value_is_list(cons->cdr) || sm_value_is_quoted(cons->cdr)) {
                    fprintf(f, " . ");
                    sm_print_value(f, cons->cdr);
                }
            }

            fprintf(f, ")");
            break;

        case SmTypeFunction:
            fprintf(f, "(%s ", value.data.function->macro ? "macro" : "lambda");
            if (value.data.function->args.count > 0 || !value.data.function->args.rest.use)
                fprintf(f, "(");

            if (value.data.function->args.count > 0) {
                sm_print_value(f, sm_value_quote(
                    sm_value_symbol(value.data.function->args.args[0].id),
                    !value.data.function->args.args[0].eval));

                for (size_t i = 1; i < value.data.function->args.count; ++i) {
                    fprintf(f, " ");
                    sm_print_value(f, sm_value_quote(
                        sm_value_symbol(value.data.function->args.args[i].id),
                        !value.data.function->args.args[i].eval));
                }

                if (value.data.function->args.rest.use)
                    fprintf(f, " . ");
            }

            if (value.data.function->args.rest.use)
                sm_print_value(f, sm_value_quote(
                    sm_value_symbol(value.data.function->args.rest.id),
                    !value.data.function->args.rest.eval));

            if (value.data.function->args.count > 0 || !value.data.function->args.rest.use)
                fprintf(f, ")");

            for (SmCons* cons = value.data.function->progn; cons; cons = sm_list_next(cons)) {
                fprintf(f, " ");
                sm_print_value(f, cons->car);
            }

            fprintf(f, ")");
            break;

        default:
            break;
    }
}

// List functions
void sm_list_copy(SmContext* ctx, SmCons* cons, SmValue* ret) {
    if (!cons) {
        *ret = sm_value_nil();
        return;
    }

    SmCons* copy = sm_heap_alloc_cons(&ctx->heap, ctx);
    *ret = sm_value_cons(copy);

    copy->car = cons->car;
    if (!sm_value_is_list(cons->cdr) || sm_value_is_quoted(cons->cdr))
        copy->cdr = cons->cdr;

    for (cons = sm_list_next(cons); cons; cons = sm_list_next(cons)) {
        copy->cdr = sm_value_cons(sm_heap_alloc_cons(&ctx->heap, ctx));
        copy = copy->cdr.data.cons;

        copy->car = cons->car;
        if (!sm_value_is_list(cons->cdr) || sm_value_is_quoted(cons->cdr))
            copy->cdr = cons->cdr;
    }
}

// List building
void build_list_v(SmContext* ctx, SmValue* ret, va_list* args) {
    SmBuildOp op = va_arg(*args, SmBuildOp);

    if (op == SmBuildCdr) {
        *ret = va_arg(*args, SmValue);
        return;
    } else if (op == SmBuildEnd) {
        *ret = sm_value_nil();
        return;
    }

    SmCons* cons = sm_heap_alloc_cons(&ctx->heap, ctx);
    *ret = sm_value_cons(cons); // Save head

    if (op == SmBuildCar)
        cons->car = va_arg(*args, SmValue);
    else if (op == SmBuildList)
        build_list_v(ctx, &cons->car, args);
    else
        sm_panic("invalid operation in sm_build_list arguments");

    for (op = va_arg(*args, SmBuildOp); op != SmBuildCdr && op != SmBuildEnd; op = va_arg(*args, SmBuildOp))
    {
        // Continue list
        cons->cdr = sm_value_cons(sm_heap_alloc_cons(&ctx->heap, ctx));
        cons = sm_list_next(cons);

        if (op == SmBuildCar)
            cons->car = va_arg(*args, SmValue);
        else if (op == SmBuildList)
            build_list_v(ctx, &cons->car, args);
        else
            sm_panic("invalid operation in sm_build_list arguments");
    }

    cons->cdr = (op == SmBuildCdr) ? va_arg(*args, SmValue) : sm_value_nil();
}

void sm_build_list(SmContext* ctx, SmValue* ret, ...) {
    va_list args;

    va_start(args, ret);
    build_list_v(ctx, ret, &args);
    va_end(args);
}
