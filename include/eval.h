#pragma once

#include "error.h"
#include "context.h"
#include "value.h"

SmError sm_eval(SmContext* ctx, SmValue form, SmValue* ret);

SmError sm_validate_lambda(SmContext* ctx, SmCons* params);
SmError sm_invoke_lambda(SmContext* ctx, SmCons* lambda, SmCons* params, SmValue* ret);
