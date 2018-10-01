#include "stack.h"

// Inlines
extern inline SmStackFrame sm_stack_frame(SmStackFrame* parent, SmString name, SmValue fn);
extern inline void sm_stack_frame_drop(SmStackFrame* frame);
