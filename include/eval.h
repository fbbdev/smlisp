#pragma once

#include "error.h"
#include "context.h"
#include "value.h"

SmError sm_eval(SmContext* ctx, SmValue form, SmValue* ret);
