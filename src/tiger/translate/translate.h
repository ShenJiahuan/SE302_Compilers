#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

#include "tiger/absyn/absyn.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/x64frame.h"

/* Forward Declarations */
namespace A {
class Exp;
}  // namespace A

namespace TR {

class Access;
class Exp;
class ExpAndTy;
class Level;
class PatchList;

Level* Outermost();

F::FragList* TranslateProgram(A::Exp*);

void do_patch(PatchList *tList, TEMP::Label *label);

TR::Access *allocLocal(TR::Level *level, bool escape);

TR::Exp *simpleVar(TR::Access *access, TR::Level *level);

void functionDec(TR::Exp *body, TR::Level *level);
}  // namespace TR

#endif
