#include "value.h"

// Inlines
extern inline SmValue sm_value_nil();
extern inline SmValue sm_value_number(SmNumber number);
extern inline SmValue sm_value_word(SmWord word);
extern inline SmValue sm_value_cons(SmCons* cons);
extern inline bool sm_value_is_nil(SmValue value);
extern inline bool sm_value_is_number(SmValue value);
extern inline bool sm_value_is_word(SmValue value);
extern inline bool sm_value_is_cons(SmValue value);
extern inline bool sm_value_is_unquoted(SmValue value);
extern inline bool sm_value_is_quoted(SmValue value);
extern inline SmValue sm_value_quote(SmValue value, uint8_t quotes);
extern inline SmValue sm_value_unquote(SmValue value, uint8_t unquotes);
