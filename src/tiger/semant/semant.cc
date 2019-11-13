#include "tiger/semant/semant.h"
#include "tiger/errormsg/errormsg.h"

extern EM::ErrorMsg errormsg;

using VEnvType = S::Table<E::EnvEntry> *;
using TEnvType = S::Table<TY::Ty> *;

namespace {
static TY::TyList *make_formal_tylist(TEnvType tenv, A::FieldList *params) {
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

static TY::FieldList *make_fieldlist(int pos, TEnvType tenv, A::FieldList *fields) {
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

namespace A {

TY::Ty *SimpleVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  E::EnvEntry *x = venv->Look(this->sym);
  if (x && x->kind == E::EnvEntry::VAR) {
    return ((E::VarEntry *) x)->ty;
  } else {
    errormsg.Error(this->pos, "111111");
    return TY::IntTy::Instance();
  }
}

TY::Ty *FieldVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  TY::Ty* ty = this->var->SemAnalyze(venv, tenv, labelcount)->ActualTy();

  if (ty->kind != TY::Ty::Kind::RECORD) {
    errormsg.Error(this->pos, "not a record type");
    return TY::IntTy::Instance();
  }

  TY::FieldList *list = ((TY::RecordTy *) ty)->fields;
  while (list) {
    TY::Field *field = list->head;
    if (field->name == this->sym) {
      return field->ty;
    }
    list = list->tail;
  }
  errormsg.Error(this->pos, "field %s doesn't exist", this->sym->Name().c_str());
  return TY::VoidTy::Instance();

}

TY::Ty *SubscriptVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                                 int labelcount) const {
  TY::Ty *ty = this->var->SemAnalyze(venv, tenv, labelcount)->ActualTy();
  if (ty->kind != TY::Ty::Kind::ARRAY) {
    errormsg.Error(this->pos, "array type required");
    return TY::VoidTy::Instance();
  }
  return ((TY::ArrayTy *) ty)->ty->ActualTy();
}

TY::Ty *VarExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  if (this->var->kind == Var::SIMPLE) {
    if (!venv->Look(((SimpleVar *) this->var)->sym)) {
      errormsg.Error(this->pos, "undefined variable %s", ((SimpleVar *) this->var)->sym->Name().c_str());
      return TY::NilTy::Instance();
    }
    return this->var->SemAnalyze(venv, tenv, labelcount);
  } else if (this->var->kind == Var::SUBSCRIPT) {
    return this->var->SemAnalyze(venv, tenv, labelcount);
  } else if (this->var->kind == Var::FIELD) {
    return this->var->SemAnalyze(venv, tenv, labelcount);
  }
}

TY::Ty *NilExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  return TY::NilTy::Instance();
}

TY::Ty *IntExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  return TY::IntTy::Instance();
}

TY::Ty *StringExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  if (this->kind != A::Exp::Kind::STRING) {
    errormsg.Error(this->pos, "666666");
    return TY::VoidTy::Instance();
  }
  return TY::StringTy::Instance();
}

TY::Ty *CallExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                            int labelcount) const {
  E::EnvEntry *x = venv->Look(this->func);
  if (!x || x->kind != E::EnvEntry::Kind::FUN) {
    errormsg.Error(this->pos, "undefined function %s", this->func->Name().c_str());
    return TY::VoidTy::Instance();
  }

  TY::TyList *formals = ((E::FunEntry *) x)->formals;
  TY::Ty *result = ((E::FunEntry *)x)->result;
  ExpList *args;
  for (args = this->args; args && formals; args = args->tail, formals = formals->tail) {
    TY::Ty *ty = args->head->SemAnalyze(venv, tenv, labelcount)->ActualTy();
    if (formals->head != ty && ty->kind != TY::Ty::Kind::NIL) {
      errormsg.Error(this->pos, "para type mismatch");
    }
  }
  if (args) {
    errormsg.Error(this->pos, "too many params in function %s", this->func->Name().c_str());
  }
  return result;

}

TY::Ty *OpExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  TY::Ty *left = this->left->SemAnalyze(venv, tenv, labelcount)->ActualTy();
  TY::Ty *right = this->right->SemAnalyze(venv, tenv, labelcount)->ActualTy();

  if (this->oper == PLUS_OP || this->oper == MINUS_OP || this->oper == TIMES_OP || this->oper == DIVIDE_OP) {
    if (left->kind != TY::Ty::Kind::INT && left->kind != TY::Ty::Kind::NIL) {
      errormsg.Error(this->left->pos, "integer required");
      return TY::IntTy::Instance();
    }

    if (right->kind != TY::Ty::Kind::INT && right->kind != TY::Ty::Kind::NIL) {
      errormsg.Error(this->right->pos, "integer required");
      return TY::IntTy::Instance();
    }
  } else {
    if (!left->IsSameType(right) && left->kind != TY::Ty::Kind::VOID && right->kind != TY::Ty::Kind::VOID) {
      errormsg.Error(this->pos, "same type required");
    }
  }

  return TY::IntTy::Instance();
}

TY::Ty *RecordExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  TY::Ty *ty = tenv->Look(this->typ);
  if (!ty) {
    errormsg.Error(this->pos, "undefined type %s", this->typ->Name().c_str());
    return TY::VoidTy::Instance();
  }
  return ty;
}

TY::Ty *SeqExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  ExpList *list = this->seq;
  TY::Ty *ty = TY::VoidTy::Instance();
  while (list) {
    ty = list->head->SemAnalyze(venv, tenv, labelcount);
    list = list->tail;
  }
  return ty;
}

TY::Ty *AssignExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  TY::Ty *var = this->var->SemAnalyze(venv, tenv, labelcount)->ActualTy();
  TY::Ty *exp = this->exp->SemAnalyze(venv, tenv, labelcount)->ActualTy();
  if (var->kind != exp->kind && var->kind != TY::Ty::Kind::VOID && exp->kind != TY::Ty::Kind::VOID) {
    errormsg.Error(this->pos, "unmatched assign exp");
    return TY::VoidTy::Instance();
  }

  if (this->var->kind == A::Var::SIMPLE) {
    E::EnvEntry *x = venv->Look(((SimpleVar *) this->var)->sym);
    if (x->readonly) {
      errormsg.Error(this->pos, "loop variable can't be assigned");
    }
  }
  return TY::VoidTy::Instance();
}

TY::Ty *IfExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  TY::Ty *test = this->test->SemAnalyze(venv, tenv, labelcount)->ActualTy();
  TY::Ty *then = this->then->SemAnalyze(venv, tenv, labelcount)->ActualTy();
  TY::Ty *elsee = nullptr;
  if (this->elsee) {
    elsee = this->elsee->SemAnalyze(venv, tenv, labelcount)->ActualTy();
  }

  if (!elsee && then->kind != TY::Ty::Kind::VOID) {
    errormsg.Error(this->pos, "if-then exp's body must produce no value");
  }

  if (elsee && then->kind != elsee->kind && then->kind != TY::Ty::Kind::NIL && elsee->kind != TY::Ty::Kind::NIL) {
    errormsg.Error(this->pos, "then exp and else exp type mismatch");
  }
  return then;
}

TY::Ty *WhileExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  TY::Ty *body = this->body->SemAnalyze(venv, tenv, labelcount);
  if (body->kind != TY::Ty::Kind::VOID) {
    errormsg.Error(this->pos, "while body must produce no value");
  }
  return body;
}

TY::Ty *ForExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  TY::Ty *lo = this->lo->SemAnalyze(venv, tenv, labelcount);
  TY::Ty *hi = this->hi->SemAnalyze(venv, tenv, labelcount);

  if (lo->kind != TY::Ty::Kind::INT) {
    errormsg.Error(this->lo->pos, "for exp's range type is not integer");
  }
  if (hi->kind != TY::Ty::Kind::INT) {
    errormsg.Error(this->hi->pos, "for exp's range type is not integer");
  }

  venv->Enter(this->var, new E::VarEntry(lo, true));

  TY::Ty *body = this->body->SemAnalyze(venv, tenv, labelcount);

  return TY::VoidTy::Instance();
}

TY::Ty *BreakExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  return TY::VoidTy::Instance();
}

TY::Ty *LetExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  DecList *list = this->decs;
  while (list) {
    list->head->SemAnalyze(venv, tenv, labelcount);
    list = list->tail;
  }
  TY::Ty *body = this->body->SemAnalyze(venv, tenv, labelcount);
  return body;
}

TY::Ty *ArrayExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  TY::Ty *ty = tenv->Look(this->typ)->ActualTy();

  if (((TY::ArrayTy *)ty)->ty->ActualTy() != this->init->SemAnalyze(venv, tenv, labelcount)->ActualTy()) {
    errormsg.Error(this->pos, "type mismatch");
    return ty;
  }
  return tenv->Look(this->typ);
}

TY::Ty *VoidExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                            int labelcount) const {
  return TY::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  FunDecList* list = this->functions;
  FunDec* head;
  VEnvType cur_venv = E::BaseVEnv();
  while (list) {
    head = list->head;
    if (cur_venv->Look(head->name)) {
      errormsg.Error(this->pos, "two functions have the same name");
    }
    TY::TyList *formalTyList = make_formal_tylist(tenv, head->params);
    if (head->result) {
      TY::Ty* resultTy = tenv->Look(head->result);
      cur_venv->Enter(head->name, new E::FunEntry(formalTyList, resultTy));
      venv->Enter(head->name, new E::FunEntry(formalTyList, resultTy));
    } else {
      cur_venv->Enter(head->name, new E::FunEntry(formalTyList, TY::VoidTy::Instance()));
      venv->Enter(head->name, new E::FunEntry(formalTyList, TY::VoidTy::Instance()));
    }
    list = list->tail;
  }

  list = this->functions;
  while (list) {
    head = list->head;
    TY::TyList *formalTyList = make_formal_tylist(tenv, head->params);
    venv->BeginScope();
    for (FieldList *fieldList = head->params; fieldList; fieldList = fieldList->tail, formalTyList = formalTyList->tail) {
      venv->Enter(fieldList->head->name, new E::VarEntry(formalTyList->head));
    }
    TY::Ty *ty = head->body->SemAnalyze(venv, tenv, labelcount);
    E::EnvEntry *entry = venv->Look(head->name);
    if (entry->kind != E::EnvEntry::Kind::FUN || ty != ((E::FunEntry *) entry)->result->ActualTy()) {
      errormsg.Error(this->pos, "procedure returns value");
    }
    venv->EndScope();
    list = list->tail;
  }
}

void VarDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  TY::Ty *init = this->init->SemAnalyze(venv, tenv, labelcount)->ActualTy();
  if (!this->typ && init->kind == TY::Ty::Kind::NIL) {
    errormsg.Error(this->pos, "init should not be nil without type specified");
  } else if (this->typ && init->kind == TY::Ty::Kind::VOID) {
      init = tenv->Look(this->typ)->ActualTy();
  }
  if (this->typ && tenv->Look(this->typ)->ActualTy() != init && init->kind != TY::Ty::Kind::NIL) {
    errormsg.Error(this->pos, "type mismatch");
  }
  venv->Enter(this->var, new E::VarEntry(init));
}

void TypeDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  NameAndTyList* list = this->types;
  NameAndTy* head;
  TEnvType cur_tenv = E::BaseTEnv();
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
    ty->ty = head->ty->SemAnalyze(tenv);
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
        return;
      }
    }
    list = list->tail;
  }
}

TY::Ty *NameTy::SemAnalyze(TEnvType tenv) const {
  return new TY::NameTy(name, tenv->Look(this->name));
}

TY::Ty *RecordTy::SemAnalyze(TEnvType tenv) const {
  TY::FieldList* record = make_fieldlist(this->pos, tenv, this->record);
  return new TY::RecordTy(record);
}

TY::Ty *ArrayTy::SemAnalyze(TEnvType tenv) const {
  return new TY::ArrayTy(tenv->Look(this->array));
}

}  // namespace A

namespace SEM {
void SemAnalyze(A::Exp *root) {
  if (root) root->SemAnalyze(E::BaseVEnv(), E::BaseTEnv(), 0);
}

}  // namespace SEM
