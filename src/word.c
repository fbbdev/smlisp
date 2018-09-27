#include "word.h"

#include <ctype.h>
#include <string.h>

// Inlines
extern inline SmString sm_word_str(SmWord word);

void sm_word_set_drop(SmWordSet* set) {
    for (SmString* str = (SmString*)  sm_rbtree_first(set); str; str = (SmString*) sm_rbtree_next(set, str))
        free((char*) str->data);

    sm_rbtree_drop(set);
}

SmWord sm_word(SmWordSet* set, SmString str) {
    SmWord word = (SmWord) sm_rbtree_find(set, &str);

    if (!word) {
        char* buf = sm_aligned_alloc(sm_alignof(char), str.length*sizeof(char));

        // Copy string and transform to lowercase
        strncpy(buf, str.data, str.length);
        for (char *p = buf, *end = buf + str.length; p != end; ++p)
            *p = tolower(*p);

        str.data = buf;

        word = (SmWord) sm_rbtree_insert(set, &str);
    }

    return word;
}
