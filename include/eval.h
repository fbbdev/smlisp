#pragma once

#include "context.h"
#include "error.h"
#include "value.h"

SmError sm_eval(SmContext* ctx, SmValue form, SmValue* ret);
