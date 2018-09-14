#include "number.h"

// Inlines
extern inline SmNumber sm_number_int(int64_t value);
extern inline SmNumber sm_number_float(double value);
extern inline SmNumber sm_number_as_int(SmNumber number);
extern inline SmNumber sm_number_as_float(SmNumber number);
extern inline SmNumber sm_number_as_type(SmNumberType type, SmNumber number);
extern inline bool sm_number_is_int(SmNumber number);
extern inline bool sm_number_is_float(SmNumber number);
extern inline SmNumberType sm_number_common_type(SmNumberType t1, SmNumberType t2);
