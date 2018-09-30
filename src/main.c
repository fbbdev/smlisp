#include "smlisp.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int interactive(char const* progname) {
    fprintf(stderr, "%s: <stdin>: not implemented yet\n", progname);
    return -1;
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

    sm_heap_root_drop(&ctx->heap, ctx->frame, forms);
    sm_heap_root_drop(&ctx->heap, ctx->frame, res);

    sm_context_drop(ctx);

    return exit_code;
}
