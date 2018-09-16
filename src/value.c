#include "value.h"

// Inlines
extern inline SmValue sm_value_nil();
extern inline SmValue sm_value_number(SmNumber number);
extern inline SmValue sm_value_word(SmWord word, bool quoted);
extern inline SmValue sm_value_cons(SmCons* cons, bool quoted);
extern inline bool sm_value_is_nil(SmValue value);
extern inline bool sm_value_is_number(SmValue value);
extern inline bool sm_value_is_word(SmValue value);
extern inline bool sm_value_is_cons(SmValue value);
extern inline bool sm_value_is_unquoted(SmValue value);
extern inline bool sm_value_is_quoted(SmValue value);
