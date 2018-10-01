#include "smlisp.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SmError builtin_exit(SmContext* ctx, SmValue args, SmValue* ret) {
    int exit_code = 0;

    if (!sm_value_is_nil(args)) {
        if (!sm_value_is_cons(args) || !sm_value_is_list(args.data.cons->cdr))
            return sm_error(ctx, SmErrorInvalidArgument, "exit cannot accept a dotted argument list");
        else if (!sm_value_is_nil(args.data.cons->cdr))
            return sm_error(ctx, SmErrorExcessArguments, "exit cannot accept more than 1 argument");

        SmError err = sm_eval(ctx, args.data.cons->car, ret);
        if (!sm_is_ok(err))
            return err;

        if (!sm_value_is_number(*ret))
            return sm_error(ctx, SmErrorInvalidArgument, "exit can only accept an integer argument");
        else if (!sm_number_is_int(ret->data.number))
            return sm_error(ctx, SmErrorInvalidArgument, "exit can only accept an integer argument");

        exit_code = ret->data.number.value.i;
    }

    sm_context_drop(ctx);
    exit(exit_code);

    return sm_ok;
}

static bool complete(char const* buf) {
    size_t parens = 0;

    for (; *buf; ++buf) {
        if (*buf == '(')
            ++parens;
        else if (*buf == ')' && parens > 0)
            --parens;
        else if (*buf == '|') {
            do {
                ++buf;
            } while (*buf && *buf != '|');

            if (!*buf)
                return false;
        } else if (*buf == '\'') {
            do {
                ++buf;
            } while (isspace(*buf) || *buf == '\'');

            if (!*buf)
                return false;

            --buf; // The current character must be examined
        }
    }

    return parens == 0;
}

static int interactive(char const* progname) {
    int exit_code = 0;

    SmContext* ctx = sm_context((SmGCConfig) { 64, 2, 64 });
    sm_register_builtins(ctx);
    sm_context_register_function(ctx, sm_word(&ctx->words, sm_string_from_cstring("exit")), builtin_exit);

    SmValue* forms = sm_heap_root(&ctx->heap);
    SmValue* res = sm_heap_root(&ctx->heap);

    SmParser parser = sm_parser(sm_string_from_cstring("<stdin>"), (SmString){ NULL, 0 });

    while (1) {
        // Do not use sm_aligned_alloc so we can eventually realloc later
        size_t buf_size = 256;
        char* buf = malloc(buf_size*sizeof(char));
        sm_guard(buf != NULL, "input buffer allocation failed");

        if (!fgets(buf, buf_size, stdin)) {
            if (!feof(stdin)) {
                fprintf(stderr, "%s: <stdin>: read failed: %s\n", progname, strerror(errno));
                exit_code = -1;
            }

            free(buf);

            break;
        }

        // Ensure complete lines are read and support continuation lines
        size_t length = strlen(buf);
        while (buf[length - 1] != '\n' || !complete(buf)) {
            if (length == buf_size) {
                // No more space in buffer, reallocate
                buf_size *= 2;
                buf = realloc(buf, buf_size*sizeof(char));
                sm_guard(buf != NULL, "input buffer allocation failed");
            }

            if (!fgets(buf + length, buf_size - length, stdin)) {
                if (!feof(stdin)) {
                    fprintf(stderr, "%s: <stdin>: read failed: %s\n", progname, strerror(errno));
                    exit_code = -1;
                }

                break;
            }

            length += strlen(buf + length);
        }

        if (exit_code != 0) {
            free(buf);
            break;
        }

        parser.source = (SmString){ buf, length };
        parser.location.index = 0;

        SmError err = sm_parser_parse_all(&parser, ctx, forms);
        free(buf);

        if (!sm_is_ok(err)) {
            sm_report_error(stderr, err);
            exit_code = -1;
            break;
        }

        for (SmCons* form = forms->data.cons; sm_is_ok(err) && form; form = sm_list_next(form)) {
            *res = sm_value_nil();
            err = sm_eval(ctx, form->car, res);
        }

        if (!sm_is_ok(err)) {
            sm_report_error(stderr, err);
            exit_code = -1;
            break;
        }
    }

    sm_context_drop(ctx);

    return exit_code;
}

int main(int argc, char** argv) {
    char const* progname = argv[0];
    if (!strlen(progname))
        progname = "smlisp";

    if (argc < 2)
        return interactive(progname);

    int exit_code = 0;

    SmContext* ctx = sm_context((SmGCConfig) { 64, 2, 64 });
    sm_register_builtins(ctx);
    sm_context_register_function(ctx, sm_word(&ctx->words, sm_string_from_cstring("exit")), builtin_exit);

    SmValue* forms = sm_heap_root(&ctx->heap);
    SmValue* res = sm_heap_root(&ctx->heap);

    for (int i = 1; i < argc; ++i) {
        FILE* f = fopen(argv[i], "r");
        if (!f) {
            fprintf(stderr, "%s: %s: %s\n", progname, argv[i], strerror(errno));
            exit_code = -1;
            break;
        }

        if (fseek(f, 0, SEEK_END) != 0) {
            fclose(f);
            fprintf(stderr, "%s: %s: cannot get file size: %s\n", progname, argv[i], strerror(errno));
            exit_code = -1;
            break;
        }

        long size = ftell(f);
        if (size == EOF) {
            fclose(f);
            fprintf(stderr, "%s: %s: cannot get file size: %s\n", progname, argv[i], strerror(errno));
            exit_code = -1;
            break;
        }

        rewind(f);

        if (size <= 0) {
            fclose(f);
            continue;
        }

        char* buf = sm_aligned_alloc(16, size);
        if (fread(buf, sizeof(char), size, f) < (size_t) size) {
            fclose(f);
            free(buf);
            fprintf(stderr, "%s: %s: read failed: %s\n", progname, argv[i], strerror(errno));
            exit_code = -1;
            break;
        }

        fclose(f);

        SmParser parser = sm_parser(sm_string_from_cstring(argv[i]), (SmString){ buf, size });

        SmError err = sm_parser_parse_all(&parser, ctx, forms);
        free(buf);

        if (!sm_is_ok(err)) {
            sm_report_error(stderr, err);
            exit_code = -1;
            break;
        }

        for (SmCons* form = forms->data.cons; sm_is_ok(err) && form; form = sm_list_next(form)) {
            *res = sm_value_nil();
            err = sm_eval(ctx, form->car, res);
        }

        if (!sm_is_ok(err)) {
            sm_report_error(stderr, err);
            exit_code = -1;
            break;
        }
    }

    sm_context_drop(ctx);

    return exit_code;
}
