#pragma once

#include "context.h"
#include "util.h"
#include "value.h"

#include <stdbool.h>

typedef struct SmSourceLoc {
    size_t index;
    size_t line;
    size_t col;
} SmSourceLoc;

typedef struct SmParser {
    SmString name;
    SmString source;

    SmSourceLoc location;
} SmParser;

inline SmParser sm_parser(SmString name, SmString source) {
    return (SmParser){ name, source, { 0, 1, 1 } };
}

bool sm_parser_finished(SmParser const* parser);
SmError sm_parser_parse_form(SmParser* parser, SmContext* ctx, SmValue* form);
SmError sm_parser_parse_all(SmParser* parser, SmContext* ctx, SmValue* list);

bool sm_can_parse_float(SmString str);
