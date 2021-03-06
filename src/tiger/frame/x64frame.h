#ifndef TIGER_X64FRAME_FRAME_H_
#define TIGER_X64FRAME_FRAME_H_

#include <vector>
#include <map>
#include "frame.h"

namespace F {

class X64Frame : public Frame
{
  // TODO: Put your codes here (lab6).
public:
  static const int wordSize;
  virtual Access *allocLocal(bool escape);
  static Frame *newFrame(TEMP::Label *name, U::BoolList *list);
  static TEMP::Temp *RAX();
  static TEMP::Temp *RBX();
  static TEMP::Temp *RCX();
  static TEMP::Temp *RDX();
  static TEMP::Temp *RSI();
  static TEMP::Temp *RDI();
  static TEMP::Temp *RBP();
  static TEMP::Temp *RSP();
  static TEMP::Temp *R8();
  static TEMP::Temp *R9();
  static TEMP::Temp *R10();
  static TEMP::Temp *R11();
  static TEMP::Temp *R12();
  static TEMP::Temp *R13();
  static TEMP::Temp *R14();
  static TEMP::Temp *R15();
  static T::Exp *exp(F::Access *access, T::Exp *framePtr);
  static T::Exp *externalCall(const std::string &s, T::ExpList *args);
  static std::vector<TEMP::Temp *> registers();
  static std::vector<std::string> colors();
  static const int K = 15;
  static const std::map<TEMP::Temp *, std::string> regs;
};

class InFrameAccess : public Access {
public:
  int offset;

  InFrameAccess(int offset) : Access(INFRAME), offset(offset) {}
};

class InRegAccess : public Access {
public:
  TEMP::Temp* reg;

  InRegAccess(TEMP::Temp* reg) : Access(INREG), reg(reg) {}
};

}  // namespace F

#endif