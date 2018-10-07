#include "parser.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

// Inlines
extern inline SmParser sm_parser(SmString name, SmString source);

// Error message buffer
static sm_thread_local char err_buf[1024];

// Lexer (private)
typedef enum TokenType {
    End = 0,
    Integer,
    Float,
    Symbol,
    String,
    LParen,
    RParen,
    Quote,
    Dot,
    Truncated,
    Invalid
} TokenType;

typedef struct Token {
    TokenType type;
    SmString source;
    SmSourceLoc location;
} Token;

static inline bool utf8_seq_start(uint8_t b) {
    return (b & 0x80) == 0x00 || (b & 0xe0) == 0xc0 ||
           (b & 0xf0) == 0xe0 || (b & 0xf8) == 0xf0;
}

static inline size_t utf8_next(SmString str) {
    for (size_t i = 1; i < str.length; ++i) {
        if (utf8_seq_start((uint8_t) str.data[i]))
            return i;
    }

    return str.length;
}

static inline size_t utf8_incr(SmString* str) {
    size_t next = utf8_next(*str);
    str->data += next;
    str->length -= next;
    return next;
}

static size_t consume(SmParser* parser, size_t amount) {
    size_t consumed = 0;

    for (; parser->source.length > 0 && amount > 0; --amount) {
        bool nl = (*parser->source.data == '\n');
        size_t bytes = utf8_incr(&parser->source);

        parser->location.index += bytes;
        parser->location.line += nl;
        parser->location.col = nl ? 1 : parser->location.col + 1;

        consumed += bytes;
    }

    return consumed;
}

static size_t consume_whitespace(SmParser* parser) {
    // Also consumes comments
    size_t consumed = 0;

    bool comment = (*parser->source.data == ';');

    while (parser->source.length > 0 && (isspace(*parser->source.data) || comment)) {
        bool nl = (*parser->source.data == '\n');

        ++parser->source.data;
        --parser->source.length;

        ++parser->location.index;
        parser->location.line += nl;
        parser->location.col = nl ? 1 : parser->location.col + 1;

        ++consumed;

        comment = (comment && !nl) || (parser->source.length > 0 && *parser->source.data == ';');
    }

    return consumed;
}

static inline bool token_boundary(char c) {
    return isspace(c) || c == '\'' || c == '"' || c == '(' || c == ')';
}

static bool valid_integer(SmString str) {
    // Check if string is a valid integer: /^[+-]?[0-9]+$/

    if (str.length == 0)
        return false;

    if (*str.data == '+' || *str.data == '-')
        utf8_incr(&str);

    if (str.length == 0)
        return false;

    for (; str.length; utf8_incr(&str))
        if (!isdigit(*str.data))
            return false;

    return true;
}

static bool valid_float(SmString str) {
    // Check if string is a valid float:
    // /^[+-]?([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?$/

    if (str.length == 0)
        return false;

    if (*str.data == '+' || *str.data == '-')
        utf8_incr(&str);

    if (str.length == 0 || (!isdigit(*str.data) && *str.data != '.'))
        return false;

    bool initial_digit = isdigit(*str.data);

    while (str.length > 0 && isdigit(*str.data))
        utf8_incr(&str);

    if (str.length == 0)
        return true;

    if (*str.data == '.') {
        utf8_incr(&str);

        if (!initial_digit && (str.length == 0 || !isdigit(*str.data)))
            return false;

        while (str.length > 0 && isdigit(*str.data))
            utf8_incr(&str);

        if (str.length == 0)
            return true;
    }

    if (*str.data != 'e' && *str.data != 'E')
        return false;

    utf8_incr(&str);

    return valid_integer(str);
}

static Token lexer_next(SmParser* parser) {
    consume_whitespace(parser);

    if (parser->source.length == 0)
        return (Token){ End, { NULL, 0 }, parser->location };

    Token tok = { Invalid, { parser->source.data, 0, }, parser->location };

    switch (*parser->source.data) {
        case '(':
            tok.type = LParen;
            tok.source.length = consume(parser, 1);
            break;

        case ')':
            tok.type = RParen;
            tok.source.length = consume(parser, 1);
            break;

        case '\'':
            tok.type = Quote;
            tok.source.length = consume(parser, 1);
            break;

        case '"':
            tok.type = Truncated;
            tok.source.length += consume(parser, 1);

            while (parser->source.length > 0 && *parser->source.data != '"') {
                tok.source.length += consume(parser, 1);
                if (*parser->source.data == '\\')
                    // Do not check source.length, this will return 0 at end of input
                    tok.source.length += consume(parser, 1);
            }

            if (parser->source.length > 0) {
                tok.type = String;
                tok.source.length += consume(parser, 1);
            }
            break;

        case '.':
            if (parser->source.length == 1 || token_boundary(parser->source.data[1])) {
                tok.type = Dot;
                tok.source.length = consume(parser, 1);
                break;
            }

        default: // Enlarge token until next boundary, decide type later
            while (parser->source.length > 0 && !token_boundary(*parser->source.data)) {
                if (*parser->source.data == '|') {
                    tok.type = Truncated;
                    tok.source.length += consume(parser, 1);

                    // Everything between pipes is escaped and becomes a symbol
                    while (parser->source.length > 0 && *parser->source.data != '|')
                        tok.source.length += consume(parser, 1);

                    if (parser->source.length > 0)
                        tok.type = Symbol; // Opening pipe has been matched
                }

                tok.source.length += consume(parser, 1);
            }
            break;
    }

    if (tok.type == Invalid) {
        if (valid_integer(tok.source))
            tok.type = Integer;
        else if (valid_float(tok.source))
            tok.type = Float;
        else
            tok.type = Symbol;
    }

    return tok;
}

static inline Token lexer_peek(SmParser const* parser) {
    SmParser copy = *parser;
    return lexer_next(&copy);
}

static SmError parser_error(SmParser const* parser, Token tok, SmContext const* ctx,
                            SmErrorCode err, char const* msg)
{
    snprintf(err_buf, sizeof(err_buf), "at %.*s:%zu:%zu: %s",
        (int) parser->name.length, parser->name.data,
        tok.location.line, tok.location.col, msg);
    return sm_error(ctx, err, err_buf);
}

static SmError parse_integer(SmParser const* parser, SmContext const* ctx, Token tok, int64_t* ret) {
    // Expects a valid token (see valid_integer)
    SmString str = tok.source;
    bool sign = *str.data == '-';

    if (*str.data == '+' || sign)
        utf8_incr(&str);

    for (*ret = 0; str.length > 0; utf8_incr(&str)) {
        if (INT64_MAX/10 < *ret)
            return parser_error(parser, tok, ctx, SmErrorInvalidLiteral, "integer literal overflow");

        *ret *= 10;

        if (INT64_MAX - *ret < *str.data - '0')
            return parser_error(parser, tok, ctx, SmErrorInvalidLiteral, "integer literal overflow");

        *ret += *str.data - '0';
    }

    if (sign)
        *ret = -*ret;

    return sm_ok;
}

static SmError parse_float(SmParser const* parser, SmContext const* ctx, Token tok, double* ret) {
    // Expects a valid token (see valid_float)
    SmString str = tok.source;
    bool sign = *str.data == '-';
    bool dot = false;
    int64_t exp = 0;

    if (*str.data == '+' || sign)
        utf8_incr(&str);

    for (*ret = 0.0; str.length > 0 && (isdigit(*str.data) || *str.data == '.'); utf8_incr(&str)) {
        if (*str.data == '.') {
            dot = true;
            continue;
        }

        *ret *= 10.0;
        *ret += *str.data - '0';
        exp -= dot;

        if (isinf(*ret))
            return parser_error(parser, tok, ctx, SmErrorInvalidLiteral, "float significand overflow");
    }

    if (str.length > 0 && *ret != 0.0) {
        // We have an exponent (do not check, token is assumed to be valid)
        utf8_incr(&str); // Skip [eE] char

        int64_t e = 0;
        tok.source = str;
        if (!sm_is_ok(parse_integer(parser, ctx, tok, &e)))
            return parser_error(parser, tok, ctx, SmErrorInvalidLiteral, "float exponent overflow");

        exp += e;
    }

    if (*ret != 0.0) {
        // Apply exponent
        double fac = exp < 0 ? 0.1 : 10.0;
        exp = exp < 0 ? -exp : exp;

        while (exp--)
            *ret *= fac;

        if (isinf(*ret) || *ret == 0.0)
            return parser_error(parser, tok, ctx, SmErrorInvalidLiteral, "float exponent overflow");
    }

    if (sign)
        *ret = -*ret;

    return sm_ok;
}

static SmError parse_symbol(SmParser const* parser, SmContext* ctx, Token tok, SmSymbol* ret) {
    sm_unused(parser);

    char* buf = sm_aligned_alloc(16, tok.source.length*sizeof(char));
    char* end = buf + tok.source.length;

    strncpy(buf, tok.source.data, tok.source.length);

    for (char* p = buf; p < end; ++p) {
        if (*p == '|') {
            memmove(p, p + 1, end - p - 1);
            --p;
            --end;
        }
    }

    *ret = sm_symbol(&ctx->symbols, (SmString){ buf, end - buf });
    free(buf);

    return sm_ok;
}

static SmError parse_string(SmParser const* parser, SmContext* ctx, Token tok, SmString* ret) {
    *ret = (SmString){ NULL, 0 };

    if (tok.source.length <= 2)
        return sm_ok;

    char* buf = sm_aligned_alloc(16, (tok.source.length - 2)*sizeof(char));
    char* end = buf + tok.source.length - 2;

    strncpy(buf, tok.source.data + 1, tok.source.length - 2);

    for (char* p = buf; p < end; ++p) {
        if (*p == '\\') {
            memmove(p, p + 1, end - p - 1);
            --end;

            switch (*p) {
                case '\\':
                    *p = '\\';
                    break;
                case '\"':
                    *p = '\"';
                    break;
                case 'n':
                    *p = '\n';
                    break;
                case 'r':
                    *p = '\r';
                    break;
                case 'b':
                    *p = '\b';
                    break;
                case 't':
                    *p = '\t';
                    break;
                case 'f':
                    *p = '\f';
                    break;
                case 'a':
                    *p = '\a';
                    break;
                case 'v':
                    *p = '\v';
                    break;
                default:
                    free(buf);
                    return parser_error(parser, tok, ctx, SmErrorInvalidLiteral, "invalid escape sequence in string literal");
            }
        }
    }

    *ret = sm_symbol_str(sm_symbol(&ctx->symbols, (SmString){ buf, end - buf }));
    free(buf);

    return sm_ok;
}

// Paser functions
bool sm_parser_finished(SmParser* parser) {
    consume_whitespace(parser);
    return parser->source.length == 0;
}

SmError sm_parser_parse_form(SmParser* parser, SmContext* ctx, SmValue* form) {
    Token tok = lexer_next(parser);

    uint8_t quotes = 0;

    while (tok.type == Quote) {
        if (quotes == UINT8_MAX)
            return parser_error(parser, tok, ctx, SmErrorGeneric, "max quote count exceeded");

        ++quotes;
        tok = lexer_next(parser);
    }

    SmError err = sm_ok;

    switch (tok.type) {
        case End:
            if (quotes > 0)
                err = parser_error(parser, tok, ctx, SmErrorSyntaxError, "form expected after quotes");
            else
                *form = sm_value_nil();
            break;

        case Integer:
            *form = sm_value_number(sm_number_int(0));
            err = parse_integer(parser, ctx, tok, &form->data.number.value.i);
            break;

        case Float:
            *form = sm_value_number(sm_number_float(0.0));
            err = parse_float(parser, ctx, tok, &form->data.number.value.f);
            break;

        case Symbol: {
            SmSymbol symbol = NULL;
            err = parse_symbol(parser, ctx, tok, &symbol);
            *form = sm_value_symbol(symbol);
            break;
        }

        case String:
            *form = sm_value_string((SmString){ NULL, 0 }, NULL);
            err = parse_string(parser, ctx, tok, &form->data.string.view);
            break;

        case LParen: {
            // Parse list
            Token start = tok;

            tok = lexer_peek(parser);
            if (tok.type == RParen) {
                lexer_next(parser);
                *form = sm_value_nil();
            } else {
                char buf[256];
                SmCons* cons = sm_heap_alloc_cons(&ctx->heap, ctx);
                *form = sm_value_cons(cons);

                while (tok.type != RParen && tok.type != End) {
                    err = sm_parser_parse_form(parser, ctx, &cons->car);
                    if (!sm_is_ok(err))
                        break;

                    tok = lexer_peek(parser);
                    if (tok.type == Dot) {
                        lexer_next(parser);

                        tok = lexer_peek(parser);
                        if (tok.type != Integer && tok.type != Float &&
                            tok.type != Symbol && tok.type != LParen && tok.type != Quote)
                        {
                            err = parser_error(parser, tok, ctx, SmErrorSyntaxError, "form expected after dot");
                            break;
                        }

                        err = sm_parser_parse_form(parser, ctx, &cons->cdr);
                        if (!sm_is_ok(err))
                            break;

                        tok = lexer_peek(parser);
                        if (tok.type != RParen) {
                            snprintf(buf, sizeof(buf), "right parenthesis expected (left at %zu:%zu)",
                                start.location.line, start.location.col);
                            err = parser_error(parser, tok, ctx, SmErrorSyntaxError, buf);
                            break;
                        }
                    } else if (tok.type != RParen && tok.type != End) {
                        cons->cdr = sm_value_cons(sm_heap_alloc_cons(&ctx->heap, ctx));
                        cons = cons->cdr.data.cons;
                    }
                }

                if (tok.type != RParen && sm_is_ok(err)) {
                    snprintf(buf, sizeof(buf), "form or right parenthesis expected (left at %zu:%zu)",
                        start.location.line, start.location.col);
                    err = parser_error(parser, tok, ctx, SmErrorSyntaxError, buf);
                }

                lexer_next(parser); // Consume RParen
            }
            break;
        }

        case RParen:
            err = parser_error(parser, tok, ctx, SmErrorSyntaxError, "unexpected token: right parenthesis ')'");
            break;

        case Dot:
            err = parser_error(parser, tok, ctx, SmErrorSyntaxError, "unexpected token: dot '.'");
            break;

        case Truncated:
            err = parser_error(parser, tok, ctx, SmErrorLexicalError, "unterminated token");
            break;

        default:
            err = parser_error(parser, tok, ctx, SmErrorLexicalError, "unexpected or invalid token");
            break;
    }

    if (!sm_is_ok(err))
        *form = sm_value_nil(); // In case of error, drop result
    else
        form->quotes = quotes;

    return err;
}

SmError sm_parser_parse_all(SmParser* parser, SmContext* ctx, SmValue* list) {
    if (sm_parser_finished(parser)) {
        *list = sm_value_nil();
        return sm_ok;
    }

    SmCons* form = sm_heap_alloc_cons(&ctx->heap, ctx);
    *list = sm_value_cons(form);

    SmError err = sm_parser_parse_form(parser, ctx, &form->car);

    while (sm_is_ok(err) && !sm_parser_finished(parser))
    {
        form->cdr = sm_value_cons(sm_heap_alloc_cons(&ctx->heap, ctx));
        form = form->cdr.data.cons;

        err = sm_parser_parse_form(parser, ctx, &form->car);
    }

    if (!sm_is_ok(err))
        *list = sm_value_nil(); // In case of error drop list

    return err;
}

bool sm_can_parse_float(SmString str) {
    return valid_float(str);
}
