#include <map>
#include <tiger/frame/x64frame.h>
#include "tiger/liveness/flowgraph.h"

namespace FG {

TEMP::TempList *filterRSP(TEMP::TempList *list) {
  TEMP::TempList *header = nullptr, *tail = nullptr;
  for (TEMP::TempList *p = list; p; p = p->tail) {
    if (p->head == F::X64Frame::RSP()) {
      continue;
    }
    if (header == nullptr) {
      header = tail = new TEMP::TempList(p->head, nullptr);
    } else {
      tail->tail = new TEMP::TempList(p->head, nullptr);
      tail = tail->tail;
    }
  }
  return header;
}

TEMP::TempList* Def(G::Node<AS::Instr>* n) {
  AS::Instr *instr = n->NodeInfo();
  if (instr->kind == AS::Instr::Kind::LABEL) {
    return nullptr;
  } else if (instr->kind == AS::Instr::Kind::OPER) {
    return filterRSP(((AS::OperInstr *) instr)->dst);
  } else if (instr->kind == AS::Instr::Kind::MOVE) {
    return filterRSP(((AS::MoveInstr *) instr)->dst);
  }
}

TEMP::TempList* Use(G::Node<AS::Instr>* n) {
  AS::Instr *instr = n->NodeInfo();
  if (instr->kind == AS::Instr::Kind::LABEL) {
    return nullptr;
  } else if (instr->kind == AS::Instr::Kind::OPER) {
    return filterRSP(((AS::OperInstr *) instr)->src);
  } else if (instr->kind == AS::Instr::Kind::MOVE) {
    return filterRSP(((AS::MoveInstr *) instr)->src);
  }
}

bool IsMove(G::Node<AS::Instr>* n) {
  return n->NodeInfo()->kind == AS::Instr::Kind::MOVE;
}

G::Graph<AS::Instr>* AssemFlowGraph(AS::InstrList* il, F::Frame* f) {
  G::Graph<AS::Instr> *cfg = new G::Graph<AS::Instr>();
  std::map<TEMP::Label *, G::Node<AS::Instr> *> label2Node;
  std::map<AS::Instr *, G::Node<AS::Instr> *> instr2Node;

  G::Node<AS::Instr> *prev = nullptr, *cur = nullptr;
  for (AS::InstrList *p = il; p; p = p->tail) {
    cur = cfg->NewNode(p->head);
    instr2Node[p->head] = cur;

    if (prev) {
      cfg->AddEdge(prev, cur);
    }

    if (p->head->kind == AS::Instr::Kind::LABEL) {
      label2Node[((AS::LabelInstr *) p->head)->label] = cur;
    }

    if (p->head->kind == AS::Instr::Kind::OPER && ((AS::OperInstr *) p->head)->assem.find("jmp") == 0) {
      prev = nullptr;
    } else {
      prev = cur;
    }
  }

  for (AS::InstrList *p = il; p; p = p->tail) {
    if (p->head->kind == AS::Instr::Kind::OPER && ((AS::OperInstr *) p->head)->jumps) {
      for (TEMP::LabelList *labelPtr = ((AS::OperInstr *) p->head)->jumps->labels; labelPtr; labelPtr = labelPtr->tail) {
        assert(label2Node.find(labelPtr->head) != label2Node.end());
        assert(instr2Node.find(p->head) != instr2Node.end());
        G::Node<AS::Instr> *jmpNode = instr2Node[p->head];
        G::Node<AS::Instr> *labelNode = label2Node[labelPtr->head];
        cfg->AddEdge(jmpNode, labelNode);
      }
    }
  }
  return cfg;
}

}  // namespace FG
