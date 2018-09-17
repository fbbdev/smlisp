#include "stack.h"

// Inlines
extern inline SmScope sm_scope();
extern inline SmVariable* sm_scope_get(SmScope const* scope, SmWord id);
extern inline void sm_scope_set(SmScope* scope, SmVariable var);
extern inline void sm_scope_delete(SmScope* scope, SmWord id);
extern inline bool sm_scope_is_set(SmScope const* scope, SmWord id);
extern inline SmVariable* sm_scope_first(SmScope const* scope);
extern inline SmVariable* sm_scope_next(SmScope const* scope, SmVariable* var);
extern inline SmStackFrame sm_stack_frame(SmStackFrame* parent, SmString name);
extern inline void sm_stack_frame_drop(SmStackFrame* frame);
