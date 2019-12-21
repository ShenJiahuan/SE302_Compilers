#include "tiger/frame/frame.h"
#include "tiger/frame/x64frame.h"

#include <string>

namespace F {

constexpr const int F::X64Frame::wordSize = 8;

Access *X64Frame::allocLocal(bool escape) {
  F::Access *access;
  if (escape) {
    access = new InFrameAccess(offset);
    offset -= wordSize;
  } else {
    access = new InRegAccess(TEMP::Temp::NewTemp());
  }
  F::AccessList *ptr = locals;
  if (!ptr) {
    locals = new AccessList(access, nullptr);
  } else {
    while (ptr->tail) {
      ptr = ptr->tail;
    }
    ptr->tail = new AccessList(access, nullptr);
  }
  return access;
}

Frame *X64Frame::newFrame(TEMP::Label *name, U::BoolList *escapes) {
  Frame *frame = new X64Frame();
  frame->name = name;
  frame->offset = -wordSize;
  frame->formals = frame->locals = nullptr;
  U::BoolList *escape_ptr = escapes;
  AccessList *formals_ptr = nullptr;
  while (escape_ptr) {
    F::Access *access;
    if (escape_ptr->head) {
      access = new InFrameAccess(frame->offset);
      frame->offset -= X64Frame::wordSize;
    } else {
      access = new InRegAccess(TEMP::Temp::NewTemp());
    }
    if (!frame->formals) {
      frame->formals = new AccessList(access, nullptr);
      formals_ptr = frame->formals;
    } else {
      formals_ptr->tail = new AccessList(access, nullptr);
      formals_ptr = formals_ptr->tail;
    }
    escape_ptr = escape_ptr->tail;
  }
  frame->viewShift = nullptr;
  T::StmList *viewShiftPtr = nullptr;
  int i = 0;
  for (AccessList *accessPtr = frame->formals; accessPtr; accessPtr = accessPtr->tail) {
    Access *access = accessPtr->head;
    if (access->kind == Access::INFRAME) {
      T::Exp *dstExp = new T::MemExp(new T::BinopExp(T::PLUS_OP, new T::TempExp(F::X64Frame::RBP()), new T::ConstExp(((F::InFrameAccess *) access)->offset)));
      T::Stm *stm;
      switch (i) {
        case 0: {
          stm = new T::MoveStm(dstExp, new T::TempExp(F::X64Frame::RDI()));
          break;
        }
        case 1: {
          stm = new T::MoveStm(dstExp, new T::TempExp(F::X64Frame::RSI()));
          break;
        }
        case 2: {
          stm = new T::MoveStm(dstExp, new T::TempExp(F::X64Frame::RDX()));
          break;
        }
        case 3: {
          stm = new T::MoveStm(dstExp, new T::TempExp(F::X64Frame::RCX()));
          break;
        }
        case 4: {
          stm = new T::MoveStm(dstExp, new T::TempExp(F::X64Frame::R8()));
          break;
        }
        case 5: {
          stm = new T::MoveStm(dstExp, new T::TempExp(F::X64Frame::R9()));
          break;
        }
        default: {
          stm = new T::MoveStm(dstExp, new T::MemExp(new T::BinopExp(T::BinOp::PLUS_OP, new T::TempExp(F::X64Frame::RBP()), new T::ConstExp((i + 1 - 6) * F::X64Frame::wordSize))));
          break;
        }
      }
//      frame->offset -= wordSize;
      if (frame->viewShift == nullptr) {
        frame->viewShift = viewShiftPtr = new T::StmList(stm, nullptr);
      } else {
        viewShiftPtr->tail = new T::StmList(stm, nullptr);
        viewShiftPtr = viewShiftPtr->tail;
      }
      i++;
    } else {
      // TODO: handle in reg access
    }
  }
  return frame;
}

static TEMP::Temp *rbp = nullptr;

static TEMP::Temp *rsp = nullptr;

static TEMP::Temp *rax = nullptr;

static TEMP::Temp *rdi = nullptr;

static TEMP::Temp *rsi = nullptr;

static TEMP::Temp *rdx = nullptr;

static TEMP::Temp *rcx = nullptr;

static TEMP::Temp *r8 = nullptr;

static TEMP::Temp *r9 = nullptr;

static TEMP::Temp *r10 = nullptr;

static TEMP::Temp *r11 = nullptr;

static TEMP::Temp *r12 = nullptr;

static TEMP::Temp *r13 = nullptr;

static TEMP::Temp *r14 = nullptr;

static TEMP::Temp *r15 = nullptr;

static TEMP::Temp *rbx = nullptr;

TEMP::Temp *X64Frame::RSP() {
  if (!rsp) {
    rsp = TEMP::Temp::NewTemp();
  }
  return rsp;
}

TEMP::Temp *X64Frame::RAX() {
  if (!rax) {
    rax = TEMP::Temp::NewTemp();
  }
  return rax;
}

TEMP::Temp *X64Frame::RDI() {
  if (!rdi) {
    rdi = TEMP::Temp::NewTemp();
  }
  return rdi;
}

TEMP::Temp *X64Frame::RSI() {
  if (!rsi) {
    rsi = TEMP::Temp::NewTemp();
  }
  return rsi;
}

TEMP::Temp *X64Frame::RBP() {
  if (!rbp) {
    rbp = TEMP::Temp::NewTemp();
  }
  return rbp;
}

TEMP::Temp *X64Frame::RDX() {
  if (!rdx) {
    rdx = TEMP::Temp::NewTemp();
  }
  return rdx;
}

TEMP::Temp *X64Frame::RCX() {
  if (!rcx) {
    rcx = TEMP::Temp::NewTemp();
  }
  return rcx;
}

TEMP::Temp *X64Frame::R8() {
  if (!r8) {
    r8 = TEMP::Temp::NewTemp();
  }
  return r8;
}

TEMP::Temp *X64Frame::R9() {
  if (!r9) {
    r9 = TEMP::Temp::NewTemp();
  }
  return r9;
}

TEMP::Temp *X64Frame::R10() {
  if (!r10) {
    r10 = TEMP::Temp::NewTemp();
  }
  return r10;
}

TEMP::Temp *X64Frame::R11() {
  if (!r11) {
    r11 = TEMP::Temp::NewTemp();
  }
  return r11;
}

TEMP::Temp *X64Frame::R12() {
  if (!r12) {
    r12 = TEMP::Temp::NewTemp();
  }
  return r12;
}

TEMP::Temp *X64Frame::R13() {
  if (!r13) {
    r13 = TEMP::Temp::NewTemp();
  }
  return r13;
}

TEMP::Temp *X64Frame::R14() {
  if (!r14) {
    r14 = TEMP::Temp::NewTemp();
  }
  return r14;
}

TEMP::Temp *X64Frame::R15() {
  if (!r15) {
    r15 = TEMP::Temp::NewTemp();
  }
  return r15;
}

TEMP::Temp *X64Frame::RBX() {
  if (!rbx) {
    rbx = TEMP::Temp::NewTemp();
  }
  return rbx;
}

T::Exp *X64Frame::exp(F::Access *access, T::Exp *framePtr) {
  if (access->kind == F::Access::INFRAME) {
    return new T::MemExp(new T::BinopExp(T::PLUS_OP, framePtr, new T::ConstExp(((InFrameAccess *) access)->offset)));
  } else {
    return new T::TempExp(((F::InRegAccess *) access)->reg);
  }
}

T::Exp *X64Frame::externalCall(const std::string &s, T::ExpList *args) {
  return new T::CallExp(new T::NameExp(TEMP::NamedLabel(s)), args);
}

const std::map<TEMP::Temp *, std::string> X64Frame::regs = {
        {RAX(), "%rax"}, {RBX(), "%rbx"}, {RCX(), "%rcx"}, {RDX(), "%rdx"},
        {RSI(), "%rsi"}, {RDI(), "%rdi"}, {RBP(), "%rbp"}, /* {RSP(), "%rsp"}, */
        {R8(), "%r8"}, {R9(), "%r9"}, {R10(), "%r10"}, {R11(), "%r11"},
        {R12(), "%r12"}, {R13(), "%r13"}, {R14(), "%r14"}, {R15(), "%r15"}
};

std::vector<TEMP::Temp *> X64Frame::registers() {
  std::vector<TEMP::Temp *> result;
  for (const auto &each : regs) {
    result.push_back(each.first);
  }
  return result;
}

std::vector<std::string> X64Frame::colors() {
  std::vector<std::string> result;
  for (const auto &each : regs) {
    result.push_back(each.second);
  }
  return result;
}

T::Stm *F_procEntryExit1(F::Frame *frame, T::Stm *stm) {
  T::Stm *result = stm;
  for (T::StmList *viewShiftPtr = frame->viewShift; viewShiftPtr; viewShiftPtr = viewShiftPtr->tail) {
    result = new T::SeqStm(viewShiftPtr->head, result);
  }
  return result;
}

AS::InstrList *F_procEntryExit2(AS::InstrList *body) {
  return AS::InstrList::Splice(
    body, 
    new AS::InstrList(
      new AS::OperInstr("", new TEMP::TempList(F::X64Frame::RAX(), new TEMP::TempList(F::X64Frame::RSP(), nullptr)), nullptr, nullptr), nullptr));
}

AS::Proc *F_procEntryExit3(F::Frame *frame, AS::InstrList *body) {
  std::string prologue = frame->name->Name() + ":\n";
  int extraStack = std::max(frame->maxArgs - 6, 0) * F::X64Frame::wordSize;
  prologue += ".set " + frame->name->Name() + "_framesize, " + std::to_string(-frame->offset + extraStack) + "\n";
  prologue += "subq $" + std::to_string(-frame->offset + extraStack) + ", %rsp\n";
  std::string epilog = "addq $" + std::to_string(-frame->offset + extraStack) + ", %rsp\nretq\n";
  return new AS::Proc(prologue, body, epilog);
}

}  // namespace F