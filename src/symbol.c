#include "symbol.h"

#include <ctype.h>
#include <string.h>

// Inlines
extern inline SmString sm_symbol_str(SmSymbol symbol);

void sm_symbol_set_drop(SmSymbolSet* set) {
    for (SmString* str = (SmString*)  sm_rbtree_first(set); str; str = (SmString*) sm_rbtree_next(set, str))
        free((char*) str->data);

    sm_rbtree_drop(set);
}

SmSymbol sm_symbol(SmSymbolSet* set, SmString str) {
    SmSymbol symbol = (SmSymbol) sm_rbtree_find(set, &str);

    if (!symbol) {
        if (str.data && str.length > 0) {
            char* buf = sm_aligned_alloc(16, str.length*sizeof(char));
            strncpy(buf, str.data, str.length);

            str.data = buf;
        } else {
            str.data = NULL;
            str.length = 0;
        }

        symbol = (SmSymbol) sm_rbtree_insert(set, &str);
    }

    return symbol;
}
