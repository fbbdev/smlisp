#pragma once

#include "../../include/context.h"

typedef enum Type {
    Function,
    Variable
} Type;

typedef struct External {
    SmSymbol id;
    Type type;
    union {
        SmExternalFunction function;
        SmExternalVariable variable;
    } fn;
} External;
