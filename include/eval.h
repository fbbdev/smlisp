#pragma once

#include "context.h"
#include "error.h"
#include "value.h"

SmError sm_eval(SmContext* ctx, SmValue form, SmValue* ret);

// Lambda expression handling
SmError sm_validate_lambda(SmContext* ctx, SmValue args);
SmError sm_invoke_lambda(SmContext* ctx, SmCons* lambda, SmValue args, SmValue* ret);
