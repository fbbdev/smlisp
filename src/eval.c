#include "eval.h"

#include <stdio.h>

sm_thread_local static char err_buf[1024];

SmVariable* scope_lookup(SmStackFrame const* frame, SmWord id) {
    for (; frame; frame = frame->parent) {
        SmVariable* var = sm_scope_get(&frame->scope, id);
        if (var)
            return var;
    }

    return NULL;
}

SmError sm_eval(SmContext* ctx, SmValue form, SmValue* ret) {
    // If quoted, just unquote
    if (sm_value_quoted(form)) {
        *ret = sm_value_unquote(form, 1);
        return sm_ok;
    }

    // Words trigger external/variable lookup
    if (sm_value_is_word(form)) {
        SmString str = sm_word_str(form.data.word);

        // Keywords represent themselves
        if (str.length && str.data[0] == ':') {
            *ret = form;
            return sm_ok;
        }

        // Lookup external variable
        SmExternalVariable ext_var = sm_context_lookup_variable(ctx, form.data.word);
        if (ext_var)
            return ext_var(ctx, ret);

        // Lookup external function
        if (sm_context_lookup_function(ctx, form.data.word)) {
            // Return lambda wrapping the external function
            *ret = sm_value_cons(sm_build_list(ctx, 3,
                sm_value_word(sm_word(&ctx->word, sm_string_from_cstring("lambda"))), false,
                sm_value_word(sm_word(&ctx->word, sm_string_from_cstring("args"))), false,
                sm_build_list(ctx, 2,
                    sm_value_word(form.data.word), false,
                    sm_value_word(sm_word(&ctx->word, sm_string_from_cstring("args"))), false
                    sm_value_nil(), false
                ), true,
                sm_value_nil(), false
            ));
            return sm_ok;
        }

        // Lookup variable in scope
        SmVariable* scope_var = scope_lookup(ctx->frame, form.data.word);
        if (scope_var) {
            *ret = scope_var->value;
            return sm_ok;
        }

        snprintf(err_buf, sizeof(err_buf), "undefined variable '%.*s'", str.data, str.length);
        sm_throw(ctx, SmErrorUndefinedVariable, err_buf, false);
    }

    // Return anything but function calls
    if (!sm_value_is_cons(form)) {
        *ret = form;
        return sm_ok;
    }

    return sm_ok;
}
