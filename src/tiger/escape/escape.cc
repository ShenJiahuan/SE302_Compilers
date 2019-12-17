#include "tiger/escape/escape.h"

namespace ESC {

class EscapeEntry {
 public:
  int depth;
  bool* escape;

  EscapeEntry(int depth, bool* escape) : depth(depth), escape(escape) {}
};

void TraverseDec(S_table *env, int depth, A::Dec *d) {
  switch (d->kind) {
    case A::Dec::VAR: {
      auto varDec = (A::VarDec *) d;
      varDec->escape = false;
      env->Enter(varDec->var, new EscapeEntry(depth, &(varDec->escape)));
      TraverseExp(env, depth, varDec->init);
      break;
    }
    case A::Dec::TYPE: {
      break;
    }
    case A::Dec::FUNCTION: {
      auto functionDec = (A::FunctionDec *) d;
      for (auto function = functionDec->functions; function; function = function->tail) {
        env->BeginScope();
        for (auto formal = function->head->params; formal; formal = formal->tail) {
          formal->head->escape = false;
          EscapeEntry *entry = new EscapeEntry(depth + 1, &formal->head->escape);
          env->Enter(formal->head->name, entry);
        }
        TraverseExp(env, depth + 1, function->head->body);
        env->EndScope();
      }
      break;
    }
    default: {
      assert(false);
    }
  }
}

void TraverseVar(S_table *env, int depth, A::Var *v) {
  switch (v->kind) {
    case A::Var::SIMPLE: {
      auto simpleVar = (A::SimpleVar *) v;
      EscapeEntry* entry = env->Look(simpleVar->sym);
      if (depth > entry->depth) {
        *(entry->escape) = true;
      }
      break;
    }
    case A::Var::FIELD: {
      auto fieldVar = (A::FieldVar *) v;
      TraverseVar(env, depth, fieldVar->var);
      break;
    }
    case A::Var::SUBSCRIPT: {
      auto subscriptVar = (A::SubscriptVar *) v;
      TraverseVar(env, depth, subscriptVar->var);
      TraverseExp(env, depth, subscriptVar->subscript);
      break;
    }
    default: {
      assert(false);
    }
  }
}

void TraverseExp(S_table *env, int depth, A::Exp *e) {
  switch (e->kind) {
    case A::Exp::LET: {
      for (auto dec = ((A::LetExp *) e)->decs; dec; dec = dec->tail) {
        TraverseDec(env, depth, dec->head);
      }
      TraverseExp(env, depth, ((A::LetExp *) e)->body);
      break;
    }
    case A::Exp::SEQ: {
      auto seqExp = (A::SeqExp *) e;
      for (auto exp = seqExp->seq; exp; exp = exp->tail) {
        TraverseExp(env, depth, exp->head);
      }
      break;
    }
    case A::Exp::CALL: {
      auto callExp = (A::CallExp *) e;
      for (auto exp = callExp->args; exp; exp = exp->tail) {
        TraverseExp(env, depth, exp->head);
      }
      break;
    }
    case A::Exp::VAR: {
      auto varExp = (A::VarExp *) e;
      TraverseVar(env, depth, varExp->var);
      break;
    }
    case A::Exp::OP: {
      auto opExp = (A::OpExp *) e;
      TraverseExp(env, depth, opExp->left);
      TraverseExp(env, depth, opExp->right);
      break;
    }
    case A::Exp::IF: {
      auto ifExp = (A::IfExp *) e;
      TraverseExp(env, depth, ifExp->test);
      TraverseExp(env, depth, ifExp->then);
      if (ifExp->elsee) {
        TraverseExp(env, depth, ifExp->elsee);
      }
      break;
    }
    case A::Exp::FOR: {
      auto forExp = (A::ForExp *) e;
      forExp->escape = false;
      env->Enter(forExp->var, new EscapeEntry(depth, &(forExp->escape)));
      TraverseExp(env, depth, forExp->lo);
      TraverseExp(env, depth, forExp->hi);
      TraverseExp(env, depth, forExp->body);
      break;
    }
    case A::Exp::RECORD: {
      auto recordExp = (A::RecordExp *) e;
      for (auto field = recordExp->fields; field; field = field->tail) {
        TraverseExp(env, depth, field->head->exp);
      }
      break;
    }
    case A::Exp::WHILE: {
      auto whileExp = (A::WhileExp *) e;
      TraverseExp(env, depth, whileExp->test);
      TraverseExp(env, depth, whileExp->body);
      break;
    }
    case A::Exp::ASSIGN: {
      auto assignExp = (A::AssignExp *) e;
      TraverseVar(env, depth, assignExp->var);
      TraverseExp(env, depth, assignExp->exp);
      break;
    }
    case A::Exp::ARRAY: {
      auto arrayExp = (A::ArrayExp *) e;
      TraverseExp(env, depth, arrayExp->size);
      TraverseExp(env, depth, arrayExp->init);
      break;
    }
    case A::Exp::INT:
    case A::Exp::STRING:
    case A::Exp::BREAK:
    case A::Exp::NIL:
      break;
    default: {
      assert(false);
    }
  }
}

void FindEscape(A::Exp* exp) {
  S_table *env = new S_table();
  TraverseExp(env, 1, exp);
}

}  // namespace ESC