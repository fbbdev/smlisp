#pragma once

#include "context.h"
#include "util.h"

typedef struct SmParser {
    SmString name;
    SmString source;

    struct SmSourcePos {
        size_t line;
        size_t col;
    } pos;
} SmParser;

inline SmParser sm_parser(SmString name, SmString source) {
    return (SmParser){ name, source, { 1, 1 } };
}

SmError sm_parse_form(SmParser* parser, SmValue* form);
SmError sm_parse_all(SmParser* parser, SmValue* list);
