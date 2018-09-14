#include "value.h"

// Inlines
extern inline SmValue sm_value_nil();
extern inline SmValue sm_value_number(SmNumber number);
extern inline SmValue sm_value_word(SmWord word, bool quoted);
extern inline SmValue sm_value_list(SmList* list, bool quoted);
extern inline SmValue sm_value_dup(SmValue value);
extern inline SmValue sm_value_clone(SmValue value);
extern inline void sm_value_drop(SmValue value);
extern inline bool sm_value_is_nil(SmValue value);
extern inline bool sm_value_is_number(SmValue value);
extern inline bool sm_value_is_word(SmValue value);
extern inline bool sm_value_is_list(SmValue value);
extern inline bool sm_value_is_unquoted(SmValue value);
extern inline bool sm_value_is_quoted(SmValue value);
