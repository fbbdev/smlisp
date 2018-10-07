#include "scope.h"

// Inlines
extern inline SmScope sm_scope();
extern inline SmVariable* sm_scope_get(SmScope const* scope, SmSymbol id);
extern inline SmVariable* sm_scope_set(SmScope* scope, SmSymbol id, SmValue value);
extern inline void sm_scope_delete(SmScope* scope, SmSymbol id);
extern inline bool sm_scope_is_set(SmScope const* scope, SmSymbol id);
extern inline SmVariable* sm_scope_first(SmScope const* scope);
extern inline SmVariable* sm_scope_next(SmScope const* scope, SmVariable* var);
