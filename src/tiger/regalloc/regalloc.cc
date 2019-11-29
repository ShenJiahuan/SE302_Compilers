#include "tiger/frame/x64frame.h"
#include <map>
#include <tiger/codegen/codegen.h>
#include <vector>
#include <sstream>
#include "tiger/regalloc/regalloc.h"

namespace RA {

static AS::InstrList *iList = nullptr, *last = nullptr;

static std::map<F::Frame *, std::map<TEMP::Temp *, F::Access *>> access;

static bool noLoadNext = false;

static std::vector<std::pair<std::string *, TEMP::Temp *>> reg_candidate = {
        std::make_pair(new std::string("%r10"), F::X64Frame::R10()),
        std::make_pair(new std::string("%r11"), F::X64Frame::R11()),
        std::make_pair(new std::string("%rdi"), F::X64Frame::RDI()),
        std::make_pair(new std::string("%rsi"), F::X64Frame::RSI()),
        std::make_pair(new std::string("%rdx"), F::X64Frame::RDX()),
        std::make_pair(new std::string("%rcx"), F::X64Frame::RCX()),
        std::make_pair(new std::string("%rax"), F::X64Frame::RAX())
};

static void place(AS::Instr *instr) {
  noLoadNext = (instr->kind == AS::Instr::OPER && ((AS::OperInstr *) instr)->assem.find("callq") == 0)
               || (instr->kind == AS::Instr::MOVE && ((AS::MoveInstr *) instr)->assem.find("callq") == 0);

  if (last) {
    last = last->tail = new AS::InstrList(instr, nullptr);
  } else {
    last = iList = new AS::InstrList(instr, nullptr);
  }
}

bool precolored(TEMP::Temp *t) {
  return t == F::X64Frame::RAX() || t == F::X64Frame::RBX() || t == F::X64Frame::RCX() || t == F::X64Frame::RDX()
         || t == F::X64Frame::RSI() || t == F::X64Frame::RDI() || t == F::X64Frame::RBP() || t == F::X64Frame::R10()
         || t == F::X64Frame::R11() || t == F::X64Frame::RSP();
}

static void load(F::Frame* f, TEMP::Temp *src, int i, int offset) {
  if (src == F::X64Frame::RSP()) {
    return;
  } else {
    std::stringstream stream;
    stream << "movq " << -(f->offset - offset) << "(`s0), `d0";
//    stream << "movq " << offset << "(`s0), `d0";
    std::string assem = stream.str();
    if (precolored(src)) {
      place(new AS::MoveInstr(assem, CG::L(src, nullptr), CG::L(F::X64Frame::RSP(), nullptr)));
    } else {
      assert(false);
    }
    printf("%d\n", i);
  }
}

static void store(F::Frame* f, TEMP::Temp *dst, int i, int offset) {
  if (dst == F::X64Frame::RSP()) {
    return;
  } else {
    std::stringstream stream;
    stream << "movq `s0, " << -(f->offset - offset) << "(`s1)";
//    stream << "movq `s0, " << offset << "(`s1)";
    std::string assem = stream.str();
    if (precolored((dst))) {
      place(new AS::MoveInstr(assem, nullptr, CG::L(dst, CG::L(F::X64Frame::RSP(), nullptr))));
    } else {
      assert(false);
    }
    printf("%d\n", i);
  }
}

void spill(F::Frame *f, TEMP::Temp *t) {
  if (access[f].find(t) == access[f].end()) {
    access[f][t] = f->allocLocal(true);
  }
}

void spillAll(F::Frame *f, AS::InstrList *il) {
  for (AS::InstrList *il_ptr = il; il_ptr; il_ptr = il_ptr->tail) {
    AS::Instr *instr = il_ptr->head;
    if (instr->kind == AS::Instr::OPER) {
      AS::OperInstr *operInstr = (AS::OperInstr *) instr;
      for (TEMP::TempList *list = operInstr->src; list; list = list->tail) {
        spill(f, list->head);
      }
      for (TEMP::TempList *list = operInstr->dst; list; list = list->tail) {
        spill(f, list->head);
      }
    } else if (instr->kind == AS::Instr::MOVE) {
      AS::MoveInstr *moveInstr = (AS::MoveInstr *) instr;
      for (TEMP::TempList *list = moveInstr->src; list; list = list->tail) {
        spill(f, list->head);
      }
      for (TEMP::TempList *list = moveInstr->dst; list; list = list->tail) {
        spill(f, list->head);
      }
    }
  }
}

TEMP::Map *initColoring() {
  TEMP::Map *coloring = TEMP::Map::Empty();
  coloring->Enter(F::X64Frame::RAX(), new std::string("%rax"));
  coloring->Enter(F::X64Frame::RBX(), new std::string("%rdi"));
  coloring->Enter(F::X64Frame::RCX(), new std::string("%rcx"));
  coloring->Enter(F::X64Frame::RDX(), new std::string("%rdx"));
  coloring->Enter(F::X64Frame::RSI(), new std::string("%rsi"));
  coloring->Enter(F::X64Frame::RDI(), new std::string("%rdi"));
  coloring->Enter(F::X64Frame::RBP(), new std::string("%rbp"));
  coloring->Enter(F::X64Frame::R10(), new std::string("%r10"));
  coloring->Enter(F::X64Frame::R11(), new std::string("%r11"));
  coloring->Enter(F::X64Frame::RSP(), new std::string("%rsp"));
  return coloring;
}

bool instr_share_src_dst(AS::Instr *instr) {
  return instr->kind == AS::Instr::OPER
      && (((AS::OperInstr *) instr)->assem.find("addq") == 0
        || ((AS::OperInstr *) instr)->assem.find("subq") == 0
        || ((AS::OperInstr *) instr)->assem.find("imulq") == 0);
}

Result RegAlloc(F::Frame* f, AS::InstrList* il) {
  // TODO: Put your codes here (lab6).
  iList = last = nullptr;
  spillAll(f, il);
  Result result;
  result.coloring = initColoring();
  int viewShiftCount = 0;
  int instCount = 0;
  for (F::AccessList *accessPtr = f->formals; accessPtr; accessPtr = accessPtr->tail) {
    viewShiftCount++;
  }
  for (AS::InstrList *il_ptr = il; il_ptr; il_ptr = il_ptr->tail) {
    if (il_ptr->head->kind == AS::Instr::LABEL) {
      printf("%s\n", ((AS::LabelInstr *)il_ptr->head)->assem.c_str());
      place(il_ptr->head);
    } else {
      TEMP::TempList *src, *dst;
      if (il_ptr->head->kind == AS::Instr::MOVE) {
        printf("%s\n", ((AS::MoveInstr *)il_ptr->head)->assem.c_str());
        src = ((AS::MoveInstr *) il_ptr->head)->src, dst = ((AS::MoveInstr *) il_ptr->head)->dst;
      } else {
        printf("%s\n", ((AS::OperInstr *)il_ptr->head)->assem.c_str());
        src = ((AS::OperInstr *) il_ptr->head)->src, dst = ((AS::OperInstr *) il_ptr->head)->dst;
      }

      std::vector<int> offset;

      int i = 0;
      if (src) {
        for (TEMP::TempList *src_ptr = src; src_ptr; src_ptr = src_ptr->tail) {
          offset.push_back(((F::InFrameAccess *) access[f][src_ptr->head])->offset);
          if (!precolored(src_ptr->head)) {
            src_ptr->head = reg_candidate[i].second;
//            result.coloring->Enter(src_ptr->head, reg_candidate[i].first);
          }
          i++;
        }
      }
      if (dst) {
        for (TEMP::TempList *dst_ptr = dst; dst_ptr; dst_ptr = dst_ptr->tail) {
          offset.push_back(((F::InFrameAccess *) access[f][dst_ptr->head])->offset);
          if (instr_share_src_dst(il_ptr->head)) {
            dst_ptr->head = src->tail->head;
          } else if (!precolored(dst_ptr->head)) {
            dst_ptr->head = reg_candidate[i].second;
//            result.coloring->Enter(dst_ptr->head, reg_candidate[i].first);
          }
          i++;
        }
      }

      i = 0;
      if (src) {
        for (TEMP::TempList *src_ptr = src; src_ptr; src_ptr = src_ptr->tail) {
          if (instCount >= viewShiftCount && !noLoadNext) {
            load(f, src_ptr->head, i, offset[i]);
          }
          i++;
        }
      }
      place(il_ptr->head);
      if (dst) {
        for (TEMP::TempList *dst_ptr = dst; dst_ptr; dst_ptr = dst_ptr->tail) {
          store(f, dst_ptr->head, i, offset[i]);
          i++;
        }
      }
      instCount++;
    }
  }
  result.il = iList;
  return result;
}

}  // namespace RA