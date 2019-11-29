#ifndef TIGER_CODEGEN_CODEGEN_H_
#define TIGER_CODEGEN_CODEGEN_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/translate/tree.h"

namespace CG {

static AS::InstrList *iList = nullptr, *last = nullptr;

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList);

void munchStm(T::Stm *s);

void munchArgs(T::ExpList *list);

TEMP::TempList *L(TEMP::Temp *head, TEMP::TempList *tail);

TEMP::Temp *munchExp(T::Exp *e);
}
#endif