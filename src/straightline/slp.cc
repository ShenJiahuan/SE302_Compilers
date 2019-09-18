#include <iostream>
#include "straightline/slp.h"

namespace A {
int A::CompoundStm::MaxArgs() const {
  return stm1->MaxArgs() > stm2->MaxArgs() ? stm1->MaxArgs() : stm2->MaxArgs();
}

Table *A::CompoundStm::Interp(Table *t) const {
  t = stm1->Interp(t);
  t = stm2->Interp(t);
  return t;
}

int A::AssignStm::MaxArgs() const {
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
    IntAndTable *intAndTable = exp->Interp(t);
    t = intAndTable->t;
    t = t->Update(id, intAndTable->i);
    return t;
}

Table *A::PrintStm::Interp(Table *t) const {
    IntAndTable *intAndTable = exps->InterpAndPrint(t);
    t = intAndTable->t;
    return t;
}

IntAndTable *A::IdExp::Interp(Table *t) const {
    int num = t->Lookup(id);
    return new IntAndTable(num, t);
}

IntAndTable *A::IdExp::InterpAndPrint(Table *t) const {
    int num = t->Lookup(id);
    std::cout << num << " ";
    return new IntAndTable(num, t);
}

IntAndTable *A::NumExp::Interp(Table *t) const {
    return new IntAndTable(num, t);
}

IntAndTable *A::NumExp::InterpAndPrint(Table *t) const {
    std::cout << num << " ";
    return new IntAndTable(num, t);
}

IntAndTable *A::OpExp::Interp(Table *t) const {
    int value = 0;
    switch (oper) {
        case PLUS:
            value = left->Interp(t)->i + right->Interp(t)->i;
            break;
        case MINUS:
            value = left->Interp(t)->i - right->Interp(t)->i;
            break;
        case TIMES:
            value = left->Interp(t)->i * right->Interp(t)->i;
            break;
        case DIV:
            value = left->Interp(t)->i / right->Interp(t)->i;
            break;
    }
    return new IntAndTable(value, t);
}

IntAndTable *A::OpExp::InterpAndPrint(Table *t) const {
    int value = 0;
    switch (oper) {
        case PLUS:
            value = left->Interp(t)->i + right->Interp(t)->i;
            break;
        case MINUS:
            value = left->Interp(t)->i - right->Interp(t)->i;
            break;
        case TIMES:
            value = left->Interp(t)->i * right->Interp(t)->i;
            break;
        case DIV:
            value = left->Interp(t)->i / right->Interp(t)->i;
            break;
    }
    std::cout << value << " ";
    return new IntAndTable(value, t);
}

IntAndTable *A::EseqExp::Interp(Table *t) const {
    t = stm->Interp(t);
    return exp->Interp(t);
}

IntAndTable *A::EseqExp::InterpAndPrint(Table *t) const {
    t = stm->Interp(t);
    return exp->InterpAndPrint(t);
}

IntAndTable *A::PairExpList::Interp(Table *t) const {
    t = head->Interp(t)->t;
    return tail->Interp(t);
}

IntAndTable *A::PairExpList::InterpAndPrint(Table *t) const {
    IntAndTable *intAndTable = head->Interp(t);
    std::cout << intAndTable->i << " ";
    t = intAndTable->t;
    return tail->InterpAndPrint(t);
}

IntAndTable *A::LastExpList::Interp(Table *t) const {
    return last->Interp(t);
}

IntAndTable *A::LastExpList::InterpAndPrint(Table *t) const {
    IntAndTable *intAndTable = last->Interp(t);
    std::cout << intAndTable->i << std::endl;
    return intAndTable;
}

int A::PrintStm::MaxArgs() const {
  return exps->MaxArgs() > 2 ? exps->MaxArgs() : 2;
}

int A::IdExp::MaxArgs() const {
  return 1;
}

int A::NumExp::MaxArgs() const {
    return 1;
}

int A::OpExp::MaxArgs() const {
    return left->MaxArgs() > right->MaxArgs() ? left->MaxArgs() : right->MaxArgs();
}

int A::EseqExp::MaxArgs() const {
    return stm->MaxArgs() > exp->MaxArgs() ? stm->MaxArgs() : exp->MaxArgs();
}

int A::PairExpList::MaxArgs() const {
    return head->MaxArgs() + tail->MaxArgs();
}

int A::LastExpList::MaxArgs() const {
    return last->MaxArgs();
}

int Table::Lookup(std::string key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(std::string key, int value) const {
  return new Table(key, value, this);
}
}  // namespace A
