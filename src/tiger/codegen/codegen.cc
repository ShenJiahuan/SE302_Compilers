#include <sstream>
#include <tiger/frame/x64frame.h>
#include "tiger/codegen/codegen.h"

namespace CG {

void munchStm(T::Stm *s);

TEMP::TempList *caller_save() {
  return L(F::X64Frame::RAX(),
      L(F::X64Frame::RDI(),
         L(F::X64Frame::RSI(),
           L(F::X64Frame::RDX(),
             L(F::X64Frame::RCX(),
               L(F::X64Frame::R8(),
                 L(F::X64Frame::R9(),
                   L(F::X64Frame::R10(),
                     L(F::X64Frame::R11(), nullptr)))))))));
}

static std::string frameName;

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList) {
  // TODO: Put your codes here (lab6).
  frameName = f->name->Name();
  AS::InstrList *list = nullptr;
  for (T::StmList *s1 = stmList; s1; s1 = s1->tail) {
    munchStm(s1->head);
  }
  list = iList;
  iList = last = nullptr;
  return list;
}

void emit(AS::Instr *instr) {
  if (last) {
    last = last->tail = new AS::InstrList(instr, nullptr);
  } else {
    last = iList = new AS::InstrList(instr, nullptr);
  }
}

TEMP::TempList *L(TEMP::Temp *head, TEMP::TempList *tail) {
  return new TEMP::TempList(head, tail);
}

void munchArgs(T::ExpList *list) {
    int i = 0;
    for (T::ExpList *listp = list; listp; listp = listp->tail) {
      TEMP::Temp *arg = munchExp(listp->head);
      switch (i) {
        case 0: {
          if (arg == F::X64Frame::FP()) {
            std::stringstream stream;
            stream << "leaq " << frameName << "_framesize(`s0), `d0";
            std::string assem = stream.str();
            emit(new AS::MoveInstr(assem, L(F::X64Frame::RDI(), nullptr), L(F::X64Frame::RSP(), nullptr)));
          } else {
            emit(new AS::MoveInstr("movq `s0, `d0", L(F::X64Frame::RDI(), nullptr), L(arg, nullptr)));
          }
          
          break;
        }
        case 1: {
          assert(arg != F::X64Frame::FP());
          emit(new AS::MoveInstr("movq `s0, `d0", L(F::X64Frame::RSI(), nullptr), L(arg, nullptr)));
          break;
        }
        case 2: {
          assert(arg != F::X64Frame::FP());
          emit(new AS::MoveInstr("movq `s0, `d0", L(F::X64Frame::RDX(), nullptr), L(arg, nullptr)));
          break;
        }
        case 3: {
          assert(arg != F::X64Frame::FP());
          emit(new AS::MoveInstr("movq `s0, `d0", L(F::X64Frame::RCX(), nullptr), L(arg, nullptr)));
          break;
        }
        case 4: {
          assert(arg != F::X64Frame::FP());
          emit(new AS::MoveInstr("movq `s0, `d0", L(F::X64Frame::R8(), nullptr), L(arg, nullptr)));
          break;
        }
        case 5: {
          assert(arg != F::X64Frame::FP());
          emit(new AS::MoveInstr("movq `s0, `d0", L(F::X64Frame::R9(), nullptr), L(arg, nullptr)));
          break;
        }
        default: {
          assert(arg != F::X64Frame::FP());
          emit(new AS::OperInstr("pushq `s0", nullptr, L(arg, nullptr), nullptr));
        }
      }
      i++;
    }
  }

TEMP::Temp *munchExp(T::Exp *e) {
  switch (e->kind) {
    case T::Exp::Kind::MEM: {
      T::MemExp *memExp = (T::MemExp *) e;
      TEMP::Temp *r = TEMP::Temp::NewTemp();
      if (memExp->exp->kind == T::Exp::Kind::BINOP 
        && ((T::BinopExp *)memExp->exp)->op == T::PLUS_OP 
        && ((T::BinopExp *)memExp->exp)->right->kind == T::Exp::Kind::CONST) {
        T::Exp *e1 = ((T::BinopExp *) memExp->exp)->left;
        /** MEM(BINOP(PLUS, e1, CONST(i))) */
        TEMP::Temp *e1temp = munchExp(e1);
        std::stringstream stream;
        if (e1temp == F::X64Frame::FP()) {
          stream << "movq (" << frameName << "_framesize" << ((T::ConstExp *) ((T::BinopExp *) memExp->exp)->right)->consti << ")(`s0), `d0";
          std::string assem = stream.str();
          emit(new AS::OperInstr(assem, L(r, nullptr), L(F::X64Frame::RSP(), nullptr), nullptr));
        } else {
          stream << "movq " << ((T::ConstExp *) ((T::BinopExp *) memExp->exp)->right)->consti << "(`s0), `d0";
          std::string assem = stream.str();
          emit(new AS::OperInstr(assem, L(r, nullptr), L(e1temp, nullptr), nullptr));
        }
      } else if (memExp->exp->kind == T::Exp::Kind::BINOP
        && ((T::BinopExp *)memExp->exp)->op == T::PLUS_OP 
        && ((T::BinopExp *)memExp->exp)->left->kind == T::Exp::Kind::CONST) {
        T::Exp *e1 = ((T::BinopExp *)memExp->exp)->right;
        /** MEM(BINOP(PLUS, CONST(i), e1)) */
        TEMP::Temp *e1temp = munchExp(e1);
        std::stringstream stream;
        if (e1temp == F::X64Frame::FP()) {
          stream << "movq (" << frameName << "_framesize" << ((T::ConstExp *) ((T::BinopExp *) memExp->exp)->left)->consti << ")(`s0), `d0";
          std::string assem = stream.str();
          emit(new AS::OperInstr(assem, L(r, nullptr), L(F::X64Frame::RSP(), nullptr), nullptr));
        } else {
          stream << "movq " << ((T::ConstExp *) ((T::BinopExp *) memExp->exp)->left)->consti << "(`s0), `d0";
          std::string assem = stream.str();
          emit(new AS::OperInstr(assem, L(r, nullptr), L(e1temp, nullptr), nullptr));
        }
      } else if (memExp->exp->kind == T::Exp::Kind::CONST) {
        /** MEM(CONST(i)) */
        emit(new AS::OperInstr("movq111", L(r, nullptr), nullptr, nullptr));
      } else {
        T::Exp *e1 = memExp->exp;
        /** MEM(e1) */
        TEMP::Temp *e1temp = munchExp(e1);
        assert(e1temp != F::X64Frame::FP());
        emit(new AS::OperInstr("movq (`s0), `d0", L(r, nullptr), L(e1temp, nullptr), nullptr));
      }
      return r;
    }
    case T::Exp::Kind::BINOP: {
      TEMP::Temp *r = TEMP::Temp::NewTemp();
      T::BinopExp *binopExp = (T::BinopExp *) e;
      switch (binopExp->op) {
        case T::PLUS_OP: {
          T::Exp *e1 = binopExp->left;
          T::Exp *e2 = binopExp->right;
          TEMP::Temp *e1temp = munchExp(e1);
          TEMP::Temp *e2temp = munchExp(e2);
          if (e1temp == F::X64Frame::FP()) {
            std::stringstream stream;
            stream << "leaq " << frameName << "_framesize(`s0), `d0";
            std::string assem = stream.str();
            emit(new AS::MoveInstr(assem, L(r, nullptr), L(F::X64Frame::RSP(), nullptr)));
          } else {
            emit(new AS::MoveInstr("movq `s0, `d0", L(r, nullptr), L(e1temp, nullptr)));
          }
          emit(new AS::OperInstr("addq `s0, `d0", L(r, nullptr), L(e2temp, L(r, nullptr)), nullptr));
          return r;
        }
        case T::MINUS_OP: {
          T::Exp *e1 = binopExp->left;
          T::Exp *e2 = binopExp->right;
          TEMP::Temp *e1temp = munchExp(e1);
          TEMP::Temp *e2temp = munchExp(e2);
          if (e1temp == F::X64Frame::FP()) {
            std::stringstream stream;
            stream << "leaq " << frameName << "_framesize(`s0), `d0";
            std::string assem = stream.str();
            emit(new AS::MoveInstr(assem, L(r, nullptr), L(F::X64Frame::RSP(), nullptr)));
          } else {
            emit(new AS::MoveInstr("movq `s0, `d0", L(r, nullptr), L(e1temp, nullptr)));
          }
          emit(new AS::OperInstr("subq `s0, `d0", L(r, nullptr), L(e2temp, L(r, nullptr)), nullptr));
          return r;
        }
        case T::MUL_OP: {
          T::Exp *e1 = binopExp->left;
          T::Exp *e2 = binopExp->right;
          TEMP::Temp *e1temp = munchExp(e1);
          TEMP::Temp *e2temp = munchExp(e2);
          assert(e1temp != F::X64Frame::FP());
          assert(e2temp != F::X64Frame::FP());
          emit(new AS::MoveInstr("movq `s0, `d0", L(r, nullptr), L(e1temp, nullptr)));
          emit(new AS::OperInstr("imulq `s0, `d0", L(r, nullptr), L(e2temp, L(r, nullptr)), nullptr));
          return r;
        }
        case T::DIV_OP: {
          T::Exp *e1 = binopExp->left;
          T::Exp *e2 = binopExp->right;
          TEMP::Temp *e1temp = munchExp(e1);
          TEMP::Temp *e2temp = munchExp(e2);
          assert(e1temp != F::X64Frame::FP());
          assert(e2temp != F::X64Frame::FP());
          emit(new AS::MoveInstr("movq `s0, `d0", L(F::X64Frame::RAX(), nullptr), L(e1temp, nullptr)));
          emit(new AS::OperInstr("cqo", nullptr, nullptr, nullptr));
          emit(new AS::OperInstr("idivq `s0", L(F::X64Frame::RAX(), nullptr), L(e2temp, nullptr), nullptr));
          emit(new AS::MoveInstr("movq `s0, `d0", L(r, nullptr), L(F::X64Frame::RAX(), nullptr)));
          return r;
        }
      }
      return r;
    }
    case T::Exp::Kind::CONST: {
      TEMP::Temp *r = TEMP::Temp::NewTemp();
      std::stringstream stream;
      stream << "movq $" << ((T::ConstExp *) e)->consti << ", `d0";

      std::string assem = stream.str();
      emit(new AS::OperInstr(assem, L(r, nullptr), nullptr, nullptr));
      return r;
    }
    case T::Exp::Kind::TEMP: {
      T::TempExp *temp = (T::TempExp *) e;
      return temp->temp;
    }
    case T::Exp::Kind::NAME: {
      TEMP::Temp *r = TEMP::Temp::NewTemp();
      std::stringstream stream;
      stream << "leaq " << TEMP::LabelString(((T::NameExp *) e)->name) << "(%rip), `d0";
      std::string assem = stream.str();
      emit(new AS::OperInstr(assem, L(r, nullptr), nullptr, nullptr));
      return r;
    }
    case T::Exp::Kind::CALL: {
      TEMP::Temp *r = TEMP::Temp::NewTemp();
      std::string label = TEMP::LabelString(((T::NameExp *) ((T::CallExp *) e)->fun)->name);
      munchArgs(((T::CallExp *) e)->args);
      std::string assem = std::string("callq ") + std::string(label);
      emit(new AS::OperInstr(assem, nullptr /* TODO: change to caller_save() in lab6 */, nullptr, nullptr));
      emit(new AS::MoveInstr("movq `s0, `d0", L(r, nullptr), L(F::X64Frame::RAX(), nullptr)));
      return r;
    }
  }
}

void munchMoveStm(T::MoveStm *s) {
  T::Exp *dst = s->dst, *src = s->src;
  if (dst->kind == T::Exp::Kind::MEM) {
    T::MemExp *memDst = (T::MemExp *) dst;
    if (memDst->exp->kind == T::Exp::Kind::BINOP
      && ((T::BinopExp *) memDst->exp)->op == T::PLUS_OP
      && ((T::BinopExp *) memDst->exp)->right->kind == T::Exp::Kind::CONST) {
      T::Exp *e1 = ((T::BinopExp *) memDst->exp)->left, *e2 = src;
      /** MOVE(MEM(BINOP(PLUS, e1, CONST(i)), e2) */
      TEMP::Temp *e1temp = munchExp(e1);
      TEMP::Temp *e2temp = munchExp(e2);
      std::stringstream stream;
      if (e1temp == F::X64Frame::FP()) {
        stream << "movq `s0, (" << frameName << "_framesize" << ((T::ConstExp *) ((T::BinopExp *) memDst->exp)->right)->consti << ")(`s1)";
        std::string assem = stream.str();
        emit(new AS::OperInstr(assem, nullptr, L(e2temp, L(F::X64Frame::RSP(), nullptr)), nullptr));
      } else {
        stream << "movq `s0, " << ((T::ConstExp *) ((T::BinopExp *) memDst->exp)->right)->consti << "(`s1)";
        std::string assem = stream.str();
        emit(new AS::OperInstr(assem, nullptr, L(e2temp, L(e1temp, nullptr)), nullptr));
      }
    } else if (memDst->exp->kind == T::Exp::Kind::BINOP
               && ((T::BinopExp *) memDst->exp)->op == T::PLUS_OP
               && ((T::BinopExp *) memDst->exp)->left->kind == T::Exp::Kind::CONST) {
      T::Exp *e1 = ((T::BinopExp *) memDst->exp)->right, *e2 = src;
      /** MOVE(MEM(BINOP(PLUS, CONST(i), e1), e2) */
      TEMP::Temp *e1temp = munchExp(e1);
      TEMP::Temp *e2temp = munchExp(e2);
      std::stringstream stream;
      stream << "movq `s0, " << ((T::ConstExp *) ((T::BinopExp *) memDst->exp)->left)->consti << "(`s1)";
      std::string assem = stream.str();
      assert(e1temp != F::X64Frame::FP());
      assert(e2temp != F::X64Frame::FP());
      emit(new AS::OperInstr(assem, nullptr, L(e2temp, L(e1temp, nullptr)), nullptr));
    } else if (src->kind == T::Exp::Kind::MEM) {
      T::Exp *e1 = memDst->exp, *e2 = ((T::MemExp *) src)->exp;
      /** MOVE(MEM(e1), MEM(e2)) */
      TEMP::Temp *t = TEMP::Temp::NewTemp();
      TEMP::Temp *e1temp = munchExp(e1);
      TEMP::Temp *e2temp = munchExp(e2);
      assert(e1temp != F::X64Frame::FP());
      assert(e2temp != F::X64Frame::FP());
      emit(new AS::OperInstr("movq (`s0), `d0", L(t, nullptr), L(e2temp, nullptr), nullptr));
      emit(new AS::OperInstr("movq `s0, (`s1)", nullptr, L(t, L(e1temp, nullptr)), nullptr));
    } else if (memDst->kind == T::Exp::Kind::CONST) {
      T::Exp *e2 = src;
      /** MOVE(MEM(CONST(i)), e2) */
      TEMP::Temp *e2temp = munchExp(e2);
      assert(e2temp != F::X64Frame::FP());
      emit(new AS::OperInstr("movq (some const), `s0", nullptr, L(e2temp, nullptr), nullptr));
    } else {
      T::Exp *e1 = memDst->exp, *e2 = src;
      /** MOVE(MEM(e1), e2) */
      TEMP::Temp *e1temp = munchExp(e1);
      TEMP::Temp *e2temp = munchExp(e2);
      assert(e1temp != F::X64Frame::FP());
      assert(e2temp != F::X64Frame::FP());
      emit(new AS::OperInstr("movq `s0, (`s1)", nullptr, L(e2temp, L(e1temp, nullptr)), nullptr));
    }
  } else if (dst->kind == T::Exp::Kind::TEMP) {
    T::Exp *e2 = src;
    /** MOVE(TEMP(i), e2) */
    TEMP::Temp *e2temp = munchExp(e2);
    if (e2temp == F::X64Frame::FP()) {
      std::stringstream stream;
      stream << "movq " << frameName << "_framesize(`s0), `d0";
      std::string assem = stream.str();
      emit(new AS::MoveInstr(assem, L(((T::TempExp *) dst)->temp, nullptr), L(F::X64Frame::RSP(), nullptr)));
    } else {
      emit(new AS::MoveInstr("movq `s0, `d0", L(((T::TempExp *) dst)->temp, nullptr), L(e2temp, nullptr)));
    }
  }
}

void munchStm(T::Stm *s) {
  switch (s->kind) {
    case T::Stm::Kind::MOVE: {
      munchMoveStm((T::MoveStm *) s);
      break;
    }
    case T::Stm::Kind::LABEL: {
      TEMP::Label *label = ((T::LabelStm *) s)->label;
      emit(new AS::LabelInstr(TEMP::LabelString(label), label));
      break;
    }
    case T::Stm::Kind::CJUMP: {
      T::CjumpStm *cjumpStm = (T::CjumpStm *) s;
      TEMP::Temp *e1temp = munchExp(cjumpStm->left);
      TEMP::Temp *e2temp = munchExp(cjumpStm->right);
      TEMP::Label *trues = cjumpStm->true_label;
      TEMP::Label *falses = cjumpStm->false_label;

      std::string op;
      if (cjumpStm->op == T::EQ_OP) { op = "je"; }
      else if (cjumpStm->op == T::NE_OP) { op = "jne"; }
      else if (cjumpStm->op == T::LT_OP) { op = "jl"; }
      else if (cjumpStm->op == T::LE_OP) { op = "jle"; }
      else if (cjumpStm->op == T::GE_OP) { op = "jge"; }
      else if (cjumpStm->op == T::GT_OP) { op = "jg"; }
      else if (cjumpStm->op == T::ULT_OP) { op = "jb"; }
      else if (cjumpStm->op == T::ULE_OP) { op = "jbe"; }
      else if (cjumpStm->op == T::UGE_OP) { op = "jae"; }
      else if (cjumpStm->op == T::UGT_OP) { op = "ja"; }

      emit(new AS::OperInstr("cmpq `s1, `s0", nullptr, L(e1temp, L(e2temp, nullptr)), nullptr));
      emit(new AS::OperInstr(op + " `j0", nullptr, nullptr, new AS::Targets(new TEMP::LabelList(trues, new TEMP::LabelList(falses,
                                                                                                                  nullptr)))));
      break;
    }
    case T::Stm::Kind::EXP: {
      T::ExpStm *exp = (T::ExpStm *) s;
      munchExp(exp->exp);
      break;
    }
    case T::Stm::Kind::JUMP: {
      TEMP::LabelList *jumps = ((T::JumpStm *) s)->jumps;
      emit(new AS::OperInstr("jmp `j0", nullptr, nullptr, new AS::Targets(jumps)));
      break;
    }
    case T::Stm::Kind::SEQ: {
      T::SeqStm *seq = (T::SeqStm *) s;
      munchStm(seq->left);
      munchStm(seq->right);
      break;
    }
  }
}

}  // namespace CG