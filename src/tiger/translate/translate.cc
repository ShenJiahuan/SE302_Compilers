#include "tiger/translate/translate.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/temp.h"
#include "tiger/semant/semant.h"
#include "tiger/semant/types.h"
#include "tiger/util/util.h"

extern EM::ErrorMsg errormsg;

namespace {
  static TY::TyList *make_formal_tylist(S::Table<TY::Ty> * tenv, A::FieldList *params) {
    if (params == nullptr) {
      return nullptr;
    }

    TY::Ty *ty = tenv->Look(params->head->typ);
    if (ty == nullptr) {
      errormsg.Error(params->head->pos, "undefined type %s",
                     params->head->typ->Name().c_str());
    }

    return new TY::TyList(ty->ActualTy(), make_formal_tylist(tenv, params->tail));
  }

  static TY::FieldList *make_fieldlist(int pos, S::Table<TY::Ty> * tenv, A::FieldList *fields) {
    if (fields == nullptr) {
      return nullptr;
    }

    TY::Ty *ty = tenv->Look(fields->head->typ);
    if (ty == nullptr) {
      ty = TY::IntTy::Instance();
      errormsg.Error(pos, "undefined type %s", fields->head->typ->Name().c_str());
    }
    return new TY::FieldList(new TY::Field(fields->head->name, ty),
                             make_fieldlist(pos, tenv, fields->tail));
  }

}  // namespace

namespace TR {

static F::FragList *frags = nullptr;

class Access {
 public:
  Level *level;
  F::Access *access;

  Access(Level *level, F::Access *access) : level(level), access(access) {}
  static Access *AllocLocal(Level *level, bool escape) { return nullptr; }
};

class AccessList {
 public:
  Access *head;
  AccessList *tail;

  AccessList(Access *head, AccessList *tail) : head(head), tail(tail) {}
};

class Level {
 public:
  F::Frame *frame;
  Level *parent;

  Level(F::Frame *frame, Level *parent) : frame(frame), parent(parent) {}
  AccessList *Formals();

  static TR::Level *NewLevel(Level *parent, TEMP::Label *name,
                         U::BoolList *formals);
};

AccessList *Level::Formals() {
  F::AccessList *fAccessList = frame->getFormals();
  TR::AccessList *trAccessList = nullptr;
  TR::AccessList *trPtr = nullptr;
  for (F::AccessList *fPtr = fAccessList; fPtr; fPtr = fPtr->tail) {
    TR::AccessList *access = new TR::AccessList(new TR::Access(this, fPtr->head), nullptr);
    if (trAccessList == nullptr) {
      trAccessList = trPtr = access;
    } else {
      trPtr->tail = access;
      trPtr =  access;
    }
  }
  return trAccessList;
}

TR::Level *Level::NewLevel(TR::Level *parent, TEMP::Label *name, U::BoolList *formals) {
    F::Frame *frame = F::X64Frame::newFrame(name, new U::BoolList(true, formals));
    TR::Level *level = new TR::Level(frame, parent);
    return level;
  }

class PatchList {
public:
TEMP::Label **head;
PatchList *tail;

PatchList(TEMP::Label **head, PatchList *tail) : head(head), tail(tail) {}
};

class Cx {
 public:
  PatchList *trues;
  PatchList *falses;
  T::Stm *stm;

  Cx(PatchList *trues, PatchList *falses, T::Stm *stm)
      : trues(trues), falses(falses), stm(stm) {}
};

class Exp {
 public:
  enum Kind { EX, NX, CX };

  Kind kind;

  Exp(Kind kind) : kind(kind) {}

  virtual T::Exp *UnEx() const = 0;
  virtual T::Stm *UnNx() const = 0;
  virtual Cx UnCx() const = 0;
};

class ExpAndTy {
 public:
  TR::Exp *exp;
  TY::Ty *ty;

  ExpAndTy(TR::Exp *exp, TY::Ty *ty) : exp(exp), ty(ty) {}
};

class ExExp : public Exp {
 public:
  T::Exp *exp;

  ExExp(T::Exp *exp) : Exp(EX), exp(exp) {}

  T::Exp *UnEx() const override {
    return exp;
  }
  T::Stm *UnNx() const override {
    return new T::ExpStm(exp);
  }
  Cx UnCx() const override {
    TEMP::Label *t = TEMP::NewLabel(), *f = TEMP::NewLabel();

    T::CjumpStm *stm = new T::CjumpStm(T::NE_OP, exp, new T::ConstExp(0), t, f);
    PatchList *trues = new PatchList(&stm->true_label, nullptr);
    PatchList *falses = new PatchList(&stm->false_label, nullptr);
    return Cx(trues, falses, stm);
  }
};

class NxExp : public Exp {
 public:
  T::Stm *stm;

  NxExp(T::Stm *stm) : Exp(NX), stm(stm) {}

  T::Exp *UnEx() const override {
    return new T::EseqExp(stm, new T::ConstExp(0));
  }
  T::Stm *UnNx() const override {
    return stm;
  }
  Cx UnCx() const override {
    assert(false);
    return Cx(nullptr, nullptr, nullptr);
  }
};

class CxExp : public Exp {
 public:
  Cx cx;

  CxExp(struct Cx cx) : Exp(CX), cx(cx) {}
  CxExp(PatchList *trues, PatchList *falses, T::Stm *stm)
      : Exp(CX), cx(trues, falses, stm) {}

  T::Exp *UnEx() const override {
    TEMP::Temp *r = TEMP::Temp::NewTemp();
    TEMP::Label *t = TEMP::NewLabel(), *f = TEMP::NewLabel();
    do_patch(cx.trues, t);
    do_patch(cx.falses, f);

    return new T::EseqExp(new T::MoveStm(new T::TempExp(r), new T::ConstExp(1)),
             new T::EseqExp(cx.stm,
               new T::EseqExp(new T::LabelStm(f),
                 new T::EseqExp(new T::MoveStm(new T::TempExp(r), new T::ConstExp(0)),
                   new T::EseqExp(new T::LabelStm(t), new T::TempExp(r))))));
  }
  T::Stm *UnNx() const override {
    return new T::ExpStm(UnEx());
  }
  Cx UnCx() const override {
    return cx;
  }
};

void do_patch(PatchList *tList, TEMP::Label *label) {
  for (; tList; tList = tList->tail) *(tList->head) = label;
}

PatchList *join_patch(PatchList *first, PatchList *second) {
  if (!first) return second;
  for (; first->tail; first = first->tail)
    ;
  first->tail = second;
  return first;
}

Level *Outermost() {
  static Level *lv = nullptr;
  if (lv != nullptr) return lv;
  F::Frame *frame = F::X64Frame::newFrame(TEMP::NamedLabel("tigermain"), nullptr);
  lv = new Level(frame, nullptr);
  return lv;
}

F::FragList *TranslateProgram(A::Exp *root) {
  // TODO: Put your codes here (lab5).
  TR::Level *level = Outermost();
  TEMP::Label *mainLabel = TEMP::NewLabel();
  TR::ExpAndTy root_expty = root->Translate(E::BaseVEnv(), E::BaseTEnv(), level, mainLabel);
  TR::functionDec(root_expty.exp, level);
  return frags;
}

TR::Access *allocLocal(TR::Level *level, bool escape) {
  F::Access *fAccess = level->frame->allocLocal(escape);
  TR::Access *access = new TR::Access(level, fAccess);
  return access;
}

TR::Exp *simpleVar(TR::Access *access, TR::Level *level) {
  T::Exp *frame = new T::TempExp(F::X64Frame::RBP());
  while (level != access->level) {
    frame = new T::MemExp(new T::BinopExp(T::PLUS_OP, frame, new T::ConstExp(-F::X64Frame::wordSize)));
    level = level->parent;
  }
  frame = F::X64Frame::exp(access->access, frame);
  return new TR::ExExp(frame);
}

TR::Exp *subscriptVar(TR::Exp *var, TR::Exp *subscript) {
  return new TR::ExExp(
           new T::MemExp(
             new T::BinopExp(
               T::PLUS_OP,
               var->UnEx(),
               new T::BinopExp(T::MUL_OP, subscript->UnEx(), new T::ConstExp(F::X64Frame::wordSize))
             )
           )
         );
}

TR::Exp *array(TR::Exp *size, TR::Exp *init) {
  T::ExpList *argList = new T::ExpList(size->UnEx(),
                      new T::ExpList(init->UnEx(), nullptr));
  return new TR::ExExp(F::X64Frame::externalCall("initArray", argList));
}

TR::Exp *calc(A::Oper op, TR::Exp *lhs, TR::Exp *rhs) {
  if (op == A::PLUS_OP) {
    return new TR::ExExp(new T::BinopExp(T::PLUS_OP, lhs->UnEx(), rhs->UnEx()));
  } else if (op == A::MINUS_OP) {
    return new TR::ExExp(new T::BinopExp(T::MINUS_OP, lhs->UnEx(), rhs->UnEx()));
  } else if (op == A::TIMES_OP) {
    return new TR::ExExp(new T::BinopExp(T::MUL_OP, lhs->UnEx(), rhs->UnEx()));
  } else if (op == A::DIVIDE_OP) {
    return new TR::ExExp(new T::BinopExp(T::DIV_OP, lhs->UnEx(), rhs->UnEx()));
  }
}

TR::Exp *cmp(A::Oper op, TR::Exp *lhs, TR::Exp *rhs) {
  T::CjumpStm *stm;
  if (op == A::EQ_OP) {
    stm = new T::CjumpStm(T::EQ_OP, lhs->UnEx(), rhs->UnEx(), nullptr, nullptr);
  } else if (op == A::NEQ_OP) {
    stm = new T::CjumpStm(T::NE_OP, lhs->UnEx(), rhs->UnEx(), nullptr, nullptr);
  } else if (op == A::LT_OP) {
    stm = new T::CjumpStm(T::LT_OP, lhs->UnEx(), rhs->UnEx(), nullptr, nullptr);
  } else if (op == A::LE_OP) {
    stm = new T::CjumpStm(T::LE_OP, lhs->UnEx(), rhs->UnEx(), nullptr, nullptr);
  } else if (op == A::GE_OP) {
    stm = new T::CjumpStm(T::GE_OP, lhs->UnEx(), rhs->UnEx(), nullptr, nullptr);
  } else if (op == A::GT_OP) {
    stm = new T::CjumpStm(T::GT_OP, lhs->UnEx(), rhs->UnEx(), nullptr, nullptr);
  }

  PatchList *trues = new PatchList(&stm->true_label, nullptr);
  PatchList *falses = new PatchList(&stm->false_label, nullptr);
  return new CxExp(trues, falses, stm);
}

TR::Exp *assign(TR::Exp *lhs, TR::Exp *rhs) {
  return new TR::NxExp(new T::MoveStm(lhs->UnEx(), rhs->UnEx()));
}

TR::Exp *call(TEMP::Label *label, std::vector<TR::Exp *> expList, TR::Level *caller, TR::Level *callee) {
  T::Exp *staticLink = new T::TempExp(F::X64Frame::RBP());
  TR::Level *levelp = caller;
  while (levelp != callee->parent) {
    staticLink = new T::MemExp(new T::BinopExp(T::MINUS_OP, staticLink, new T::ConstExp(F::X64Frame::wordSize)));
    levelp = levelp->parent;
  }

  T::ExpList *list = nullptr;
  T::ExpList *listPtr = nullptr;
  for (const auto& exp : expList) {
    T::ExpList *texp = new T::ExpList(exp->UnEx(), nullptr);
    if (list) {
      listPtr->tail = texp;
      listPtr = texp;
    } else {
      list = listPtr = texp;
    }
  }
  if (callee->parent) {
    list = new T::ExpList(staticLink, list);
    caller->frame->maxArgs = std::max(caller->frame->maxArgs, (int) expList.size() + 1);
    return new TR::ExExp(new T::CallExp(new T::NameExp(label), list));
  } else {
    caller->frame->maxArgs = std::max(caller->frame->maxArgs, (int) expList.size());
    return new TR::ExExp(F::X64Frame::externalCall(TEMP::LabelString(label), list));
  }
}

TR::Exp *string(const std::string &str) {
  TEMP::Label *label = TEMP::NewLabel();
  frags = new F::FragList(new F::StringFrag(label, str), frags);
  return new TR::ExExp(new T::NameExp(label));
}

TR::Exp *if_(TR::Exp *test, TR::Exp *then, TR::Exp *elsee) {
  Cx cx = test->UnCx();
  TEMP::Temp *r = TEMP::Temp::NewTemp();
  TEMP::Label *t = TEMP::NewLabel(), *f = TEMP::NewLabel(), *final = TEMP::NewLabel();
  do_patch(cx.trues, t);
  do_patch(cx.falses, f);
  if (elsee) {
    return new ExExp(
        new T::EseqExp(cx.stm,
         new T::EseqExp(new T::LabelStm(t),
          new T::EseqExp(new T::MoveStm(new T::TempExp(r), then->UnEx()),
           new T::EseqExp(new T::JumpStm(new T::NameExp(final), new TEMP::LabelList(final, nullptr)),
            new T::EseqExp(new T::LabelStm(f),
             new T::EseqExp(new T::MoveStm(new T::TempExp(r), elsee->UnEx()),
              new T::EseqExp(new T::JumpStm(new T::NameExp(final), new TEMP::LabelList(final, nullptr)),
               new T::EseqExp(new T::LabelStm(final), new T::TempExp(r))))))))));
  } else {
    return new NxExp(
        new T::SeqStm(cx.stm,
        new T::SeqStm(new T::LabelStm(t),
          new T::SeqStm(then->UnNx(), new T::LabelStm(f)))));
  }
}

TR::Exp *seq(TR::Exp *lhs, TR::Exp *rhs) {
  if (rhs) {
    return new TR::ExExp(new T::EseqExp(lhs->UnNx(), rhs->UnEx()));
  } else {
    return new TR::ExExp(new T::EseqExp(lhs->UnNx(), new T::ConstExp(0)));
  }
}

TR::Exp *nil() {
  return new TR::ExExp(new T::ConstExp(0));
}

TR::Exp *while_(TR::Exp *test, TR::Exp *body, TEMP::Label *doneLabel) {
  Cx testCx = test->UnCx();
  TEMP::Label *bodyLabel = TEMP::NewLabel(), *testLabel = TEMP::NewLabel();
  do_patch(testCx.trues, bodyLabel);
  do_patch(testCx.falses, doneLabel);
  return new TR::NxExp(
      new T::SeqStm(new T::LabelStm(testLabel),
      new T::SeqStm(testCx.stm,
        new T::SeqStm(new T::LabelStm(bodyLabel),
         new T::SeqStm(body->UnNx(),
          new T::SeqStm(new T::JumpStm(new T::NameExp(testLabel), new TEMP::LabelList(testLabel, nullptr)),
           new T::LabelStm(doneLabel)))))));
}

TR::Exp *for_(F::Access *access, TR::Exp *lo, TR::Exp *hi, TR::Exp *body, TEMP::Label *doneLabel) {
  TEMP::Label *bodyLabel = TEMP::NewLabel(), *testLabel = TEMP::NewLabel();
  T::Exp *i = F::X64Frame::exp(access, new T::TempExp(F::X64Frame::RBP()));
  return new TR::NxExp(
      new T::SeqStm(new T::MoveStm(i, lo->UnEx()),
      new T::SeqStm(new T::LabelStm(testLabel),
       new T::SeqStm(new T::CjumpStm(T::LE_OP, i, hi->UnEx(), bodyLabel, doneLabel),
        new T::SeqStm(new T::LabelStm(bodyLabel),
         new T::SeqStm(body->UnNx(),
          new T::SeqStm(new T::MoveStm(i, new T::BinopExp(T::PLUS_OP, i, new T::ConstExp(1))),
           new T::SeqStm(new T::JumpStm(new T::NameExp(testLabel), new TEMP::LabelList(testLabel, nullptr)),
            new T::LabelStm(doneLabel)))))))));
}

TR::Exp *break_(TEMP::Label *done) {
  return new TR::NxExp(new T::JumpStm(new T::NameExp(done), new TEMP::LabelList(done, nullptr)));
}

T::Stm *make_record(const std::vector<TR::Exp *> &expList, TEMP::Temp *r, int offset) {
  if (expList.size() == 1) {
    return new T::MoveStm(new T::MemExp(new T::BinopExp(T::PLUS_OP, new T::TempExp(r), new T::ConstExp(offset * F::X64Frame::wordSize))), expList[offset]->UnEx());
  } else if (offset == expList.size() - 2) {
    return new T::SeqStm(
            new T::MoveStm(new T::MemExp(new T::BinopExp(T::PLUS_OP, new T::TempExp(r), new T::ConstExp(offset * F::X64Frame::wordSize))), expList[offset]->UnEx()),
            new T::MoveStm(new T::MemExp(new T::BinopExp(T::PLUS_OP, new T::TempExp(r), new T::ConstExp((offset + 1) * F::X64Frame::wordSize))), expList[offset + 1]->UnEx()));
  } else {
    return new T::SeqStm(
            new T::MoveStm(new T::MemExp(new T::BinopExp(T::PLUS_OP, new T::TempExp(r), new T::ConstExp(offset * F::X64Frame::wordSize))), expList[offset]->UnEx()),
            TR::make_record(expList, r, offset + 1));
  }
}

TR::Exp *recordExp(std::vector<TR::Exp *> expList) {
  int count = expList.size();
  TEMP::Temp *r = TEMP::Temp::NewTemp();
  T::Stm *stm = new T::MoveStm(
          new T::TempExp(r),
          F::X64Frame::externalCall(
                  "allocRecord",
                  new T::ExpList(new T::ConstExp(F::X64Frame::wordSize * count),nullptr)));

  stm = new T::SeqStm(stm, TR::make_record(expList, r, 0));
  return new TR::ExExp(new T::EseqExp(stm, new T::TempExp(r)));
}

TR::Exp *fieldVar(TR::Exp *exp, int offset) {
  return new TR::ExExp(new T::MemExp(new T::BinopExp(T::PLUS_OP, exp->UnEx(), new T::ConstExp(offset * F::X64Frame::wordSize))));
}

TR::Exp *stringEqual(TR::Exp *lhs, TR::Exp *rhs) {
  return new TR::ExExp(
          F::X64Frame::externalCall("stringEqual", new T::ExpList(lhs->UnEx(), new T::ExpList(rhs->UnEx(),nullptr))));
}

void functionDec(TR::Exp *body, TR::Level *level) {
  T::Stm *stm = new T::MoveStm(new T::TempExp(F::X64Frame::RAX()), body->UnEx());
  stm = F::F_procEntryExit1(level->frame, stm);
  frags = new F::FragList(new F::ProcFrag(stm, level->frame), frags);
}

}  // namespace TR

namespace A {

TR::ExpAndTy SimpleVar::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  E::EnvEntry *x = venv->Look(this->sym);
  if (x && x->kind == E::EnvEntry::VAR) {
    return TR::ExpAndTy(TR::simpleVar(((E::VarEntry *) x)->access, level), ((E::VarEntry *) x)->ty);
  } else {
    errormsg.Error(this->pos, "111111");
    return TR::ExpAndTy(nullptr, TY::IntTy::Instance());
  }
}

TR::ExpAndTy FieldVar::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  TR::ExpAndTy var = this->var->Translate(venv, tenv, level, label);
  TY::Ty* varTy = var.ty->ActualTy();

  if (varTy->kind != TY::Ty::Kind::RECORD) {
    errormsg.Error(this->pos, "not a record type");
    return TR::ExpAndTy(nullptr, TY::IntTy::Instance());
  }

  TY::FieldList *list = ((TY::RecordTy *) varTy)->fields;
  int offset = 0;
  while (list) {
    TY::Field *field = list->head;
    if (field->name == this->sym) {
      return TR::ExpAndTy(TR::fieldVar(var.exp, offset), field->ty);
    }
    list = list->tail;
    offset++;
  }
  errormsg.Error(this->pos, "field %s doesn't exist", this->sym->Name().c_str());
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy SubscriptVar::Translate(S::Table<E::EnvEntry> *venv,
                                     S::Table<TY::Ty> *tenv, TR::Level *level,
                                     TEMP::Label *label) const {
  TR::ExpAndTy var = this->var->Translate(venv, tenv, level, label);
  TR::ExpAndTy subscript = this->subscript->Translate(venv, tenv, level, label);
  TY::Ty *ty = var.ty->ActualTy();
  if (ty->kind != TY::Ty::Kind::ARRAY) {
    errormsg.Error(this->pos, "array type required");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  return TR::ExpAndTy(TR::subscriptVar(var.exp, subscript.exp), ((TY::ArrayTy *) ty)->ty->ActualTy());
}

TR::ExpAndTy VarExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  if (this->var->kind == Var::SIMPLE) {
    if (!venv->Look(((SimpleVar *) this->var)->sym)) {
      errormsg.Error(this->pos, "undefined variable %s", ((SimpleVar *) this->var)->sym->Name().c_str());
      return TR::ExpAndTy(nullptr, TY::NilTy::Instance());
    }
    return this->var->Translate(venv, tenv, level, label);
  } else if (this->var->kind == Var::SUBSCRIPT) {
    return this->var->Translate(venv, tenv, level, label);
  } else if (this->var->kind == Var::FIELD) {
    return this->var->Translate(venv, tenv, level, label);
  }
}

TR::ExpAndTy NilExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::NilTy::Instance());
}

TR::ExpAndTy IntExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(this->i)), TY::IntTy::Instance());
}

TR::ExpAndTy StringExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  if (this->kind != A::Exp::Kind::STRING) {
    errormsg.Error(this->pos, "666666");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  return TR::ExpAndTy(TR::string(this->s), TY::StringTy::Instance());
}

TR::ExpAndTy CallExp::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  E::EnvEntry *x = venv->Look(this->func);
  if (!x || x->kind != E::EnvEntry::Kind::FUN) {
    errormsg.Error(this->pos, "undefined function %s", this->func->Name().c_str());
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }

  TY::TyList *formals = ((E::FunEntry *) x)->formals;
  TY::Ty *result = ((E::FunEntry *)x)->result;
  ExpList *args;
  std::vector<TR::Exp *> expList;
  for (args = this->args; args && formals; args = args->tail, formals = formals->tail) {
    TR::ExpAndTy arg = args->head->Translate(venv, tenv, level, label);
    TY::Ty *argTy = arg.ty->ActualTy();
    expList.push_back(arg.exp);
    if (formals->head != argTy && argTy->kind != TY::Ty::Kind::NIL) {
      errormsg.Error(this->pos, "para type mismatch");
    }
  }
  if (args) {
    errormsg.Error(this->pos, "too many params in function %s", this->func->Name().c_str());
  }
  return TR::ExpAndTy(TR::call(this->func, expList, level, ((E::FunEntry *) x)->level), result);
}

TR::ExpAndTy OpExp::Translate(S::Table<E::EnvEntry> *venv,
                              S::Table<TY::Ty> *tenv, TR::Level *level,
                              TEMP::Label *label) const {
  TR::ExpAndTy left = this->left->Translate(venv, tenv, level, label);
  TR::ExpAndTy right = this->right->Translate(venv, tenv, level, label);
  TY::Ty *leftTy = left.ty->ActualTy();
  TY::Ty *rightTy = right.ty->ActualTy();

  if (this->oper == PLUS_OP || this->oper == MINUS_OP || this->oper == TIMES_OP || this->oper == DIVIDE_OP) {
    if (leftTy->kind != TY::Ty::Kind::INT && leftTy->kind != TY::Ty::Kind::NIL) {
      errormsg.Error(this->left->pos, "integer required");
      return TR::ExpAndTy(nullptr, TY::IntTy::Instance());
    }

    if (rightTy->kind != TY::Ty::Kind::INT && rightTy->kind != TY::Ty::Kind::NIL) {
      errormsg.Error(this->right->pos, "integer required");
      return TR::ExpAndTy(nullptr, TY::IntTy::Instance());
    }
  } else {
    if (!leftTy->IsSameType(rightTy) && leftTy->kind != TY::Ty::Kind::VOID && rightTy->kind != TY::Ty::Kind::VOID) {
      errormsg.Error(this->pos, "same type required");
    }
  }

  if (this->oper == A::PLUS_OP || this->oper == A::MINUS_OP || this->oper == A::TIMES_OP || this->oper == A::DIVIDE_OP) {
    return TR::ExpAndTy(TR::calc(this->oper, left.exp, right.exp), TY::IntTy::Instance());
  } else if (this->oper == A::EQ_OP && leftTy->kind == TY::Ty::Kind::STRING && rightTy->kind == TY::Ty::Kind::STRING) {
    return TR::ExpAndTy(TR::stringEqual(left.exp, right.exp), TY::IntTy::Instance());
  } else {
    return TR::ExpAndTy(TR::cmp(this->oper, left.exp, right.exp), TY::IntTy::Instance());
  }
}

TR::ExpAndTy RecordExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  TY::Ty *ty = tenv->Look(this->typ)->ActualTy();
  if (!ty) {
    errormsg.Error(this->pos, "undefined type %s", this->typ->Name().c_str());
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  std::vector<TR::Exp *> expList;
  for (A::EFieldList *efieldPtr = this->fields; efieldPtr; efieldPtr = efieldPtr->tail) {
    TR::ExpAndTy expAndTy = efieldPtr->head->exp->Translate(venv, tenv, level, label);
    expList.push_back(expAndTy.exp);
  }

  return TR::ExpAndTy(TR::recordExp(expList), ty);
}

TR::ExpAndTy SeqExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  ExpList *list = this->seq;
  TR::Exp *exp = new TR::ExExp(new T::ConstExp(0));
  TY::Ty *ty = nullptr;
  if (!list) {
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  while (list) {
    TR::ExpAndTy expAndTy = list->head->Translate(venv, tenv, level, label);
    exp = TR::seq(exp, expAndTy.exp);
    ty = expAndTy.ty;
    list = list->tail;
  }
  return TR::ExpAndTy(exp, ty);
}

//TR::ExpAndTy SeqExp::Translate(S::Table<E::EnvEntry> *venv,
//                               S::Table<TY::Ty> *tenv, TR::Level *level,
//                               TEMP::Label *label) const {
//  ExpList *list = this->seq;
//  TY::Ty *ty = TY::VoidTy::Instance();
//  T::SeqStm *header = nullptr;
//  T::SeqStm *stmPtr = nullptr;
//  while (list) {
//    TR::ExpAndTy expAndTy = list->head->Translate(venv, tenv, level, label);
//    T::SeqStm *stm = new T::SeqStm(expAndTy.exp->UnNx(), nullptr);
//    if (stmPtr == nullptr) {
//      header = stmPtr = stm;
//    } else {
//      stmPtr->right = stm;
//      stmPtr = stm;
//    }
//    ty = expAndTy.ty;
//    list = list->tail;
//  }
//  stmPtr->right = (new TR::NxExp(new T::ExpStm(new T::ConstExp(0))))->UnNx();
//  return TR::ExpAndTy(new TR::NxExp(header), ty);
//}

TR::ExpAndTy AssignExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  TR::ExpAndTy var = this->var->Translate(venv, tenv, level, label);
  TR::ExpAndTy exp = this->exp->Translate(venv, tenv, level, label);
  TY::Ty *varTy = var.ty->ActualTy();
  TY::Ty *expTy = exp.ty->ActualTy();
  if (varTy->kind != expTy->kind && varTy->kind != TY::Ty::Kind::VOID && expTy->kind != TY::Ty::Kind::VOID) {
    errormsg.Error(this->pos, "unmatched assign exp");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }

  if (this->var->kind == A::Var::SIMPLE) {
    E::EnvEntry *x = venv->Look(((SimpleVar *) this->var)->sym);
    if (x->readonly) {
      errormsg.Error(this->pos, "loop variable can't be assigned");
    }
  }
  return TR::ExpAndTy(TR::assign(var.exp, exp.exp), TY::VoidTy::Instance());
}

TR::ExpAndTy IfExp::Translate(S::Table<E::EnvEntry> *venv,
                              S::Table<TY::Ty> *tenv, TR::Level *level,
                              TEMP::Label *label) const {
  TR::ExpAndTy test = this->test->Translate(venv, tenv, level, label);
  TR::ExpAndTy then = this->then->Translate(venv, tenv, level, label);
  TR::ExpAndTy elsee(nullptr, nullptr);
  TY::Ty *testTy = test.ty->ActualTy();
  TY::Ty *thenTy = then.ty->ActualTy();
  TY::Ty *elseeTy = nullptr;
  if (this->elsee) {
    elsee = this->elsee->Translate(venv, tenv, level, label);
    elseeTy = elsee.ty->ActualTy();
  }

  if (!elseeTy && thenTy->kind != TY::Ty::Kind::VOID) {
    errormsg.Error(this->pos, "if-then exp's body must produce no value");
  }

  if (elseeTy && thenTy->kind != elseeTy->kind && thenTy->kind != TY::Ty::Kind::NIL && elseeTy->kind != TY::Ty::Kind::NIL) {
    errormsg.Error(this->pos, "then exp and else exp type mismatch");
  }
  return TR::ExpAndTy(TR::if_(test.exp, then.exp, elsee.exp), thenTy);
}

TR::ExpAndTy WhileExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  TEMP::Label *done = TEMP::NewLabel();
  TR::ExpAndTy test = this->test->Translate(venv, tenv, level, label);
  TR::ExpAndTy body = this->body->Translate(venv, tenv, level, done);
  TY::Ty *bodyTy = body.ty;
  if (bodyTy->kind != TY::Ty::Kind::VOID) {
    errormsg.Error(this->pos, "while body must produce no value");
  }
  return TR::ExpAndTy(TR::while_(test.exp, body.exp, done), body.ty);
}

TR::ExpAndTy ForExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  venv->BeginScope();
  TEMP::Label *done = TEMP::NewLabel();
  TR::ExpAndTy lo = this->lo->Translate(venv, tenv, level, label);
  TR::ExpAndTy hi = this->hi->Translate(venv, tenv, level, label);
  TY::Ty *loTy = lo.ty;
  TY::Ty *hiTy = hi.ty;

  if (loTy->kind != TY::Ty::Kind::INT) {
    errormsg.Error(this->lo->pos, "for exp's range type is not integer");
  }
  if (hiTy->kind != TY::Ty::Kind::INT) {
    errormsg.Error(this->hi->pos, "for exp's range type is not integer");
  }

  TR::Access *access = TR::allocLocal(level, this->escape);
  venv->Enter(this->var, new E::VarEntry(access, loTy, true));

  TR::ExpAndTy body = this->body->Translate(venv, tenv, level, done);
  TY::Ty *bodyTy = body.ty;

  venv->EndScope();
  return TR::ExpAndTy(TR::for_(access->access, lo.exp, hi.exp, body.exp, done), TY::VoidTy::Instance());
}

TR::ExpAndTy BreakExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  return TR::ExpAndTy(TR::break_(label), TY::VoidTy::Instance());
}

TR::ExpAndTy LetExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  DecList *list = this->decs;
  TR::Exp *seq = nullptr;
  while (list) {
    if (seq) {
      seq = TR::seq(seq, list->head->Translate(venv, tenv, level, label));
    } else {
      seq = list->head->Translate(venv, tenv, level, label);
    }
    list = list->tail;
  }
  TR::ExpAndTy body = this->body->Translate(venv, tenv, level, label);
  if (seq) {
    seq = TR::seq(seq, body.exp);
  } else {
    seq = body.exp;
  }
  TY::Ty *bodyTy = body.ty;
  return TR::ExpAndTy(seq, bodyTy);
}

TR::ExpAndTy ArrayExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  TY::Ty *ty = tenv->Look(this->typ)->ActualTy();

  if (((TY::ArrayTy *)ty)->ty->ActualTy() != this->init->Translate(venv, tenv, level, label).ty->ActualTy()) {
    errormsg.Error(this->pos, "type mismatch");
    return TR::ExpAndTy(nullptr, ty);
  }

  TR::Exp *size = this->size->Translate(venv, tenv, level, label).exp;
  TR::Exp *init = this->init->Translate(venv, tenv, level, label).exp;
  return TR::ExpAndTy(TR::array(size, init), tenv->Look(this->typ));
}

TR::ExpAndTy VoidExp::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::Exp *FunctionDec::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  FunDecList* list = this->functions;
  FunDec* head;
  S::Table<E::EnvEntry>* cur_venv = E::BaseVEnv();
  while (list) {
    head = list->head;
    if (cur_venv->Look(head->name)) {
      errormsg.Error(this->pos, "two functions have the same name");
    }
    TY::TyList *formalTyList = make_formal_tylist(tenv, head->params);

    TEMP::Label *name = TEMP::NamedLabel(head->name->Name());
    U::BoolList *args = nullptr;
    U::BoolList *argPtr = nullptr;
    for (TY::TyList *tyPtr = formalTyList; tyPtr; tyPtr = tyPtr->tail) {
      U::BoolList *arg = new U::BoolList(true, nullptr);
      if (args == nullptr) {
        args = argPtr = arg;
      } else {
        argPtr->tail = arg;
        argPtr = arg;
      }
    }

    TR::Level *newLevel = TR::Level::NewLevel(level, name, args);
    if (head->result) {
      TY::Ty* resultTy = tenv->Look(head->result);
      cur_venv->Enter(head->name, new E::FunEntry(newLevel, name, formalTyList, resultTy));
      venv->Enter(head->name, new E::FunEntry(newLevel, name, formalTyList, resultTy));
    } else {
      cur_venv->Enter(head->name, new E::FunEntry(newLevel, name, formalTyList, TY::VoidTy::Instance()));
      venv->Enter(head->name, new E::FunEntry(newLevel, name, formalTyList, TY::VoidTy::Instance()));
    }
    list = list->tail;
  }

  list = this->functions;
  while (list) {
    head = list->head;
    TY::TyList *formalTyList = make_formal_tylist(tenv, head->params);
    venv->BeginScope();
    E::EnvEntry *entry = venv->Look(head->name);
    TR::AccessList *accessList = ((E::FunEntry *) entry)->level->Formals();
    accessList = accessList->tail; // escape static link
    for (FieldList *fieldList = head->params; fieldList; fieldList = fieldList->tail, formalTyList = formalTyList->tail) {
      venv->Enter(fieldList->head->name, new E::VarEntry(accessList->head, formalTyList->head));
      accessList = accessList->tail;
    }
    TR::ExpAndTy body = head->body->Translate(venv, tenv, ((E::FunEntry *) entry)->level, label);
    TY::Ty *bodyTy = body.ty;
    if (entry->kind != E::EnvEntry::Kind::FUN || bodyTy != ((E::FunEntry *) entry)->result->ActualTy()) {
      errormsg.Error(this->pos, "procedure returns value");
    }
    venv->EndScope();
    list = list->tail;
    TR::functionDec(body.exp, ((E::FunEntry *) entry)->level);
  }
  return TR::nil();
}

TR::Exp *VarDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                           TR::Level *level, TEMP::Label *label) const {
  TR::ExpAndTy init = this->init->Translate(venv, tenv, level, label);
  TY::Ty *initTy = init.ty->ActualTy();
  if (!this->typ && initTy->kind == TY::Ty::Kind::NIL) {
    errormsg.Error(this->pos, "init should not be nil without type specified");
  } else if (this->typ && initTy->kind == TY::Ty::Kind::VOID) {
    initTy = tenv->Look(this->typ)->ActualTy();
  }
  if (this->typ && tenv->Look(this->typ)->ActualTy() != initTy && initTy->kind != TY::Ty::Kind::NIL) {
    errormsg.Error(this->pos, "type mismatch");
  }
  TR::Access *access = TR::allocLocal(level, this->escape);
  venv->Enter(this->var, new E::VarEntry(access, initTy));
  return new TR::NxExp(new T::MoveStm(TR::simpleVar(access, level)->UnEx(), init.exp->UnEx()));
}

TR::Exp *TypeDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                            TR::Level *level, TEMP::Label *label) const {
  NameAndTyList* list = this->types;
  NameAndTy* head;
  S::Table<TY::Ty> * cur_tenv = E::BaseTEnv();
  while (list) {
    head = list->head;
    if (cur_tenv->Look(head->name)) {
      errormsg.Error(this->pos, "two types have the same name");
    }
    cur_tenv->Enter(head->name, new TY::NameTy(head->name, nullptr));
    tenv->Enter(head->name, new TY::NameTy(head->name, nullptr));
    list = list->tail;
  }

  list = this->types;
  while (list) {
    head = list->head;
    TY::NameTy *ty = (TY::NameTy *)tenv->Look(head->name);
    ty->ty = head->ty->Translate(tenv);
    list = list->tail;
  }

  list = this->types;
  while (list) {
    head = list->head;
    TY::Ty* cur = tenv->Look(head->name);
    TY::Ty* ty = cur;
    while (ty->kind == TY::Ty::NAME) {
      ty = ((TY::NameTy *)ty)->ty;
      if (((TY::NameTy *)cur)->sym == ((TY::NameTy *)ty)->sym) {
        errormsg.Error(this->pos, "illegal type cycle");
        return nullptr;
      }
    }
    list = list->tail;
  }
  return TR::nil();
}

TY::Ty *NameTy::Translate(S::Table<TY::Ty> *tenv) const {
  return new TY::NameTy(name, tenv->Look(this->name));
}

TY::Ty *RecordTy::Translate(S::Table<TY::Ty> *tenv) const {
  TY::FieldList* record = make_fieldlist(this->pos, tenv, this->record);
  return new TY::RecordTy(record);
}

TY::Ty *ArrayTy::Translate(S::Table<TY::Ty> *tenv) const {
  return new TY::ArrayTy(tenv->Look(this->array));
}

}  // namespace A
