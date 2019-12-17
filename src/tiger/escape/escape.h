#ifndef TIGER_ESCAPE_ESCAPE_H_
#define TIGER_ESCAPE_ESCAPE_H_

#include "tiger/absyn/absyn.h"

namespace ESC {
class EscapeEntry;

using S_table = S::Table<EscapeEntry>;

void FindEscape(A::Exp* exp);
void TraverseExp(S_table *env, int depth, A::Exp *e);
void TraverseDec(S_table *env, int depth, A::Dec *d);
void TraverseVar(S_table *env, int depth, A::Var *v);
}  // namespace ESC

#endif