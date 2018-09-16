#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum SmNumberType {
    SmNumberTypeInt,
    SmNumberTypeFloat
} SmNumberType;

typedef union SmNumberValue {
    int64_t i;
    double f;
} SmNumberValue;

typedef struct SmNumber {
    SmNumberType type;
    SmNumberValue value;
} SmNumber;

inline SmNumber sm_number_int(int64_t value) {
    return (SmNumber){ SmNumberTypeInt, { .i = value } };
}

inline SmNumber sm_number_float(double value) {
    return (SmNumber){ SmNumberTypeFloat, { .f = value } };
}

inline SmNumber sm_number_as_int(SmNumber number) {
    return (number.type == SmNumberTypeInt) ?
        number : sm_number_int((int64_t) number.value.f);
}

inline SmNumber sm_number_as_float(SmNumber number) {
    return (number.type == SmNumberTypeFloat) ?
        number : sm_number_float(number.value.i);
}

inline SmNumber sm_number_as_type(SmNumberType type, SmNumber number) {
    return (number.type == type) ?
        number : (SmNumber){ type, (type == SmNumberTypeInt) ? (SmNumberValue){ .i = (int64_t) number.value.f }
                                                             : (SmNumberValue){ .f = (int64_t) number.value.i }
    };
}

inline bool sm_number_is_int(SmNumber number) {
    return number.type == SmNumberTypeInt;
}

inline bool sm_number_is_float(SmNumber number) {
    return number.type == SmNumberTypeFloat;
}

inline SmNumberType sm_number_common_type(SmNumberType t1, SmNumberType t2) {
    return (t1 == SmNumberTypeFloat || t2 == SmNumberTypeFloat) ? SmNumberTypeFloat : SmNumberTypeInt;
}
