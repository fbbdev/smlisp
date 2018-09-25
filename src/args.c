#include "args.h"
#include "eval.h"

#include <stdio.h>
#include <stdlib.h>

// Inlines
extern inline void sm_arg_pattern_drop(SmArgPattern* pattern);

// Error message buffer
static sm_thread_local char err_buf[1024];

// Argument matching functions
SmError sm_arg_pattern_validate_spec(SmContext* ctx, SmValue spec) {
    if (!sm_value_is_word(spec) && !sm_value_is_list(spec)) {
        return sm_error(ctx, SmErrorInvalidArgument, "argument pattern must be a word or unquoted list");
    } else if (sm_value_is_word(spec) && spec.quotes > 1) {
        return sm_error(ctx, SmErrorInvalidArgument, "argument pattern quoted more than once");
    } else if (sm_value_is_quoted(spec)) {
        return sm_error(ctx, SmErrorInvalidArgument, "argument pattern cannot be a quoted list");
    } else {
        for (SmCons* arg = spec.data.cons; arg; arg = sm_list_next(arg)) {
            if (!sm_value_is_word(arg->car) ||
                    (!sm_value_is_word(arg->cdr) &&
                        (!sm_value_is_list(arg->cdr) || sm_value_is_quoted(arg->cdr))))
            {
                return sm_error(ctx, SmErrorInvalidArgument, "argument pattern lists can only contain words");
            } else if (arg->car.quotes > 1 || (sm_value_is_word(arg->cdr) && arg->cdr.quotes > 1)) {
                return sm_error(ctx, SmErrorInvalidArgument, "word in argument pattern list quoted more than once");
            }
        }
    }

    return sm_ok;
}

SmArgPattern sm_arg_pattern_from_spec(SmValue spec) {
    // Spec *must* be valid, otherwise this is UB. See sm_arg_pattern_validate_spec
    SmArgPattern pattern = { NULL, 0, { NULL, false, false } };

    if (sm_value_is_word(spec)) {
        pattern.rest.id = spec.data.word;
        pattern.rest.eval = !sm_value_is_quoted(spec);
        pattern.rest.use = true;
    }

    pattern.count = sm_list_size(spec.data.cons);

    struct SmArgPatternArg* args =
        (struct SmArgPatternArg*) calloc(pattern.count, sizeof(struct SmArgPatternArg));

    pattern.args = args;

    for (SmCons* arg = spec.data.cons; arg; ++args, arg = sm_list_next(arg)) {
        args->id = arg->car.data.word;
        args->eval = !sm_value_is_quoted(arg->car);

        if (sm_value_is_word(arg->cdr)) {
            pattern.rest.id = arg->cdr.data.word;
            pattern.rest.eval = !sm_value_is_quoted(arg->cdr);
            pattern.rest.use = true;
        }
    }

    return pattern;
}

SmError sm_arg_pattern_eval(SmArgPattern const* pattern, SmContext* ctx, SmValue args, SmValue* ret) {
    SmCons* arg = NULL;

    if (sm_value_is_cons(args) && !sm_value_is_quoted(args)) {
        arg = args.data.cons;
    } else {
        SmError err = sm_eval(ctx, args, ret);
        if (err.code != SmErrorOk)
            return err;

        size_t available = sm_value_is_list(*ret) ? sm_list_size(ret->data.cons) : 0;

        if (available < pattern->count) {
            *ret = sm_value_nil();
            snprintf(err_buf, sizeof(err_buf), "expected %s%zu arguments, %zu given", pattern->rest.use ? "at least " : "", pattern->count, available);
            return sm_error(ctx, SmErrorMissingArguments, err_buf);
        } else if (available > pattern->count && !pattern->rest.use) {
            *ret = sm_value_nil();
            snprintf(err_buf, sizeof(err_buf), "expected %zu arguments, %zu given", pattern->count, available);
            return sm_error(ctx, SmErrorExcessArguments, err_buf);
        } else if ((!sm_value_is_list(*ret) || sm_list_is_dotted(ret->data.cons)) && !pattern->rest.use) {
            *ret = sm_value_nil();
            return sm_error(ctx, SmErrorInvalidArgument, "cannot accept dotted argument list");
        }

        return sm_ok;
    }

    return sm_ok;
}
