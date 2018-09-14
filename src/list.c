#include "value.h"

// Inlines
extern inline void sm_list_ref(SmList* lst);

SmList* sm_list_cons(SmValue car, SmValue cdr) {
    SmList* lst = sm_aligned_alloc(sm_alignof(SmList), sizeof(SmList));

    lst->car = sm_value_dup(car);
    lst->cdr = sm_value_dup(cdr);

    lst->ref_count = 0;

    return lst;
}

// XXX: DO NOT USE with circular lists!!
SmList* sm_list_clone(SmList* lst) {
    SmList* clone = sm_aligned_alloc(sm_alignof(SmList), sizeof(SmList));

    clone->car = sm_value_dup(lst->car);
    clone->cdr = sm_value_clone(lst->cdr);

    clone->ref_count = 0;

    return clone;
}

void sm_list_drop(SmList* lst) {
    sm_value_drop(lst->car);
    sm_value_drop(lst->cdr);

    free(lst);
}

void sm_list_unref(SmList* lst) {
    if (lst->ref_count-- <= 1)
        sm_list_drop(lst);
}
