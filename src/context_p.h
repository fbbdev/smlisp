#pragma once

#include "context.h"

typedef enum Type {
    Function,
    Variable
} Type;

typedef struct External {
    SmWord id;
    Type type;
    union {
        SmExternalFunction function;
        SmExternalVariable variable;
    } fn;
} External;
