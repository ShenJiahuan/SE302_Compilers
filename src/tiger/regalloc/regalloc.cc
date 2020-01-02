#include <tiger/liveness/liveness.h>
#include <algorithm>
#include <set>
#include <map>
#include <stack>
#include <sstream>
#include "tiger/frame/x64frame.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/regalloc/regalloc.h"

namespace RA {

static LIVE::LiveGraph liveness;
static std::set<G::Node<TEMP::Temp> *> spillWorkList, freezeWorkList, simplifyWorkList;
static std::map<G::Node<TEMP::Temp> *, LIVE::MoveList *> moveList;
static LIVE::MoveList *workListMoves = nullptr, *activeMoves = nullptr, *coalescedMoves = nullptr, *constrainedMoves = nullptr, *frozenMoves = nullptr;
static std::vector<G::Node<TEMP::Temp> *> selectStack;
static std::map<G::Node<TEMP::Temp> *, int> degree;
static std::set<std::pair<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *>> adjSet;
static std::map<G::Node<TEMP::Temp> *, std::set<G::Node<TEMP::Temp> *>> adjList;
static std::map<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp>* > alias;
static std::set<G::Node<TEMP::Temp> *> coalescedNodes, spilledNodes, coloredNodes;
static std::set<TEMP::Temp *> noSpillTemp;
static TEMP::Map *coloring = TEMP::Map::Empty();

bool precolored(TEMP::Temp *temp) {
  std::vector<TEMP::Temp *> regs = F::X64Frame::registers();
  return std::find(regs.begin(), regs.end(), temp) != regs.end();
}

void AddEdge(G::Node<TEMP::Temp> *u, G::Node<TEMP::Temp> *v) {
  if (adjSet.find(std::make_pair(u, v)) == adjSet.end() && u != v) {
    adjSet.insert(std::make_pair(u, v));
    adjSet.insert(std::make_pair(v, u));
    if (!precolored(u->NodeInfo())) {
      adjList[u].insert(v);
      degree[u]++;
    }
    if (!precolored(v->NodeInfo())) {
      adjList[v].insert(u);
      degree[v]++;
    }
  }
}

void Build() {
  for (LIVE::MoveList *p = liveness.moves; p; p = p->tail) {
    G::Node<TEMP::Temp> *srcNode = p->src;
    G::Node<TEMP::Temp> *dstNode = p->dst;
    moveList[srcNode] = new LIVE::MoveList(srcNode, dstNode, moveList[srcNode]);
    moveList[dstNode] = new LIVE::MoveList(srcNode, dstNode, moveList[dstNode]);
  }
  workListMoves = liveness.moves;
  for (G::NodeList<TEMP::Temp> *p = liveness.graph->Nodes(); p; p = p->tail) {
    for (G::NodeList<TEMP::Temp> *q = p->head->Adj(); q; q = q->tail)
      AddEdge(p->head, q->head);
  }

  auto registers = F::X64Frame::registers();
  auto colors = F::X64Frame::colors();
  for (int i = 0; i < F::X64Frame::K; i++) {
    std::string *color = new std::string;
    *color = colors[i];
    coloring->Enter(registers[i], color);
  }
  std::string *rspColor = new std::string("%rsp");
  coloring->Enter(F::X64Frame::RSP(), rspColor);
}

LIVE::MoveList* NodeMoves(G::Node<TEMP::Temp> *node) {
  return LIVE::MoveList::Intersect(moveList[node], LIVE::MoveList::Union(activeMoves, workListMoves));
}

bool MoveRelated(G::Node<TEMP::Temp> *node) {
  return NodeMoves(node) != nullptr;
}

void MakeWorkList() {
  for (G::NodeList<TEMP::Temp> *p = liveness.graph->Nodes(); p; p = p->tail) {
    G::Node<TEMP::Temp> *tempNode = p->head;
    TEMP::Temp *temp = tempNode->NodeInfo();
    if (precolored(temp)) {
      continue;
    }
    if (degree[tempNode] >= F::X64Frame::K) {
      spillWorkList.insert(tempNode);
    } else if (MoveRelated(tempNode)) {
      freezeWorkList.insert(tempNode);
    } else {
      simplifyWorkList.insert(tempNode);
    }
  }
}

void EnableMove(G::NodeList<TEMP::Temp> *nodes) {
  for (G::NodeList<TEMP::Temp> *p = nodes; p; p = p->tail) {
    for (LIVE::MoveList *m = NodeMoves(p->head); m; m = m->tail) {
      if (activeMoves && activeMoves->contains(m->src, m->dst)) {
        activeMoves = LIVE::MoveList::Subtract(activeMoves, new LIVE::MoveList(m->src, m->dst, nullptr));
        workListMoves = LIVE::MoveList::Union(workListMoves, new LIVE::MoveList(m->src, m->dst, nullptr));
      }
    }
  }
}

G::NodeList<TEMP::Temp> *Adjacent(G::Node<TEMP::Temp> *node) {
  G::NodeList<TEMP::Temp> *result = nullptr;
  for (G::Node<TEMP::Temp> *adj : adjList[node]) {
    if (std::find(selectStack.begin(), selectStack.end(), adj) == selectStack.end() && coalescedNodes.find(adj) == coalescedNodes.end()) {
      result = new G::NodeList<TEMP::Temp>(adj, result);
    }
  }
  return result;
}

void DecrementDegree(G::Node<TEMP::Temp> *node) {
  if (precolored(node->NodeInfo())) {
    return;
  }
  int d = degree[node];
  degree[node] = d - 1;
  if (d == F::X64Frame::K) {
    EnableMove(G::NodeList<TEMP::Temp>::CatList(
            new G::NodeList<TEMP::Temp>(node, nullptr),
            Adjacent(node)));
    spillWorkList.erase(node);
    if (MoveRelated(node)) {
      freezeWorkList.insert(node);
    } else {
      simplifyWorkList.insert(node);
    }
  }
}

void Simplify() {
  G::Node<TEMP::Temp> *tempNode = *(simplifyWorkList.begin());
//  printf("Simplify: %d\n", tempNode->NodeInfo()->Int());
  simplifyWorkList.erase(tempNode);
  selectStack.push_back(tempNode);
  for (G::NodeList<TEMP::Temp> *p = tempNode->Adj(); p; p = p->tail) {
    DecrementDegree(p->head);
  }
}

G::Node<TEMP::Temp> *GetAlias(G::Node<TEMP::Temp> *node) {
  if (coalescedNodes.find(node) != coalescedNodes.end()) {
    return GetAlias(alias[node]);
  } else {
    return node;
  }
}

void AddWorkList(G::Node<TEMP::Temp> *node) {
  if (!precolored(node->NodeInfo()) && !MoveRelated(node) && degree[node] < F::X64Frame::K) {
    freezeWorkList.erase(node);
    simplifyWorkList.insert(node);
  }
}

bool OK(G::Node<TEMP::Temp> *t, G::Node<TEMP::Temp> *r) {
  return degree[t] < F::X64Frame::K
    || precolored(t->NodeInfo())
    || adjSet.find(std::make_pair(t, r)) != adjSet.end();
}

bool OK_forAll(G::NodeList<TEMP::Temp> *nodes, G::Node<TEMP::Temp> *r) {
  for (G::NodeList<TEMP::Temp> *p = nodes; p; p = p->tail) {
    if (!OK(p->head, r)) {
      return false;
    }
  }
  return true;
}

bool Conservative(G::NodeList<TEMP::Temp> *nodes) {
  int k = 0;
  for (auto node = nodes; node; node = node->tail) {
    if (precolored(node->head->NodeInfo()) || degree[node->head] >= F::X64Frame::K) {
      k++;
    }
  }
  return k < F::X64Frame::K;
}

void Combine(G::Node<TEMP::Temp> *u, G::Node<TEMP::Temp> *v) {
//  printf("Combine: %d and %d\n", u->NodeInfo()->Int(), v->NodeInfo()->Int());
  if (freezeWorkList.find(v) != freezeWorkList.end()) {
    freezeWorkList.erase(v);
  } else {
    spillWorkList.erase(v);
  }
  coalescedNodes.insert(v);
  alias[v] = u;
  moveList[u] = LIVE::MoveList::Union(moveList[u], moveList[v]);
  EnableMove(new G::NodeList<TEMP::Temp>(v, nullptr));

  for (auto p = Adjacent(v); p; p = p->tail) {
    AddEdge(p->head, u);
    DecrementDegree(p->head);
  }

  if (degree[u] >= F::X64Frame::K && freezeWorkList.find(u) != freezeWorkList.end()) {
    freezeWorkList.erase(u);
    spillWorkList.insert(u);
  }
}

void Coalesce() {
  G::Node<TEMP::Temp> *x, *y;
  G::Node<TEMP::Temp> *u, *v;
  x = workListMoves->src;
  y = workListMoves->dst;

  x = GetAlias(x);
  y = GetAlias(y);

  if (precolored(y->NodeInfo())) {
    u = y;
    v = x;
  } else {
    u = x;
    v = y;
  }

  workListMoves = workListMoves->tail;
  if (u == v) {
    coalescedMoves = LIVE::MoveList::Union(coalescedMoves, new LIVE::MoveList(x, y, nullptr));
    AddWorkList(u);
  } else if (precolored(v->NodeInfo()) || adjSet.find(std::make_pair(u, v)) != adjSet.end()) {
    constrainedMoves = LIVE::MoveList::Union(constrainedMoves, new LIVE::MoveList(x, y, nullptr));
    AddWorkList(u);
    AddWorkList(v);
  } else if ((precolored(u->NodeInfo()) && OK_forAll(Adjacent(v), u))
    || (!precolored(u->NodeInfo()) && Conservative(G::NodeList<TEMP::Temp>::CatList(Adjacent(u), Adjacent(v))))) {
    coalescedMoves = LIVE::MoveList::Union(coalescedMoves, new LIVE::MoveList(x, y, nullptr));
    Combine(u, v);
    AddWorkList(u);
  } else {
    activeMoves = LIVE::MoveList::Union(activeMoves, new LIVE::MoveList(x, y, nullptr));
  }
}

void FreezeMoves(G::Node<TEMP::Temp> *u) {
  for (auto p = NodeMoves(u); p; p = p->tail) {
    G::Node<TEMP::Temp> *v;
    auto x = p->src;
    auto y = p->dst;
    if (GetAlias(y) == GetAlias(u)) {
      v = GetAlias(x);
    } else {
      v = GetAlias(y);
    }
    activeMoves = LIVE::MoveList::Subtract(activeMoves, new LIVE::MoveList(x, y, nullptr));
    frozenMoves = LIVE::MoveList::Union(frozenMoves, new LIVE::MoveList(x, y, nullptr));

    if (!precolored(v->NodeInfo()) && !NodeMoves(v) && degree[v] < F::X64Frame::K) {
      freezeWorkList.erase(v);
      simplifyWorkList.insert(v);
    }
  }
}

void Freeze() {
  auto node = *(freezeWorkList.begin());
  freezeWorkList.erase(node);
  simplifyWorkList.insert(node);
  FreezeMoves(node);
}

void SelectSpill() {
  G::Node<TEMP::Temp> *chosen = nullptr;
  double chosen_priority = 1E20;
  for (auto node : spillWorkList) {
    if (noSpillTemp.find(node->NodeInfo()) != noSpillTemp.end()) {
      continue;
    }
    if (liveness.priority[node->NodeInfo()] < chosen_priority) {
      chosen = node;
      chosen_priority = liveness.priority[node->NodeInfo()];
    }
  }
  if (!chosen) {
    chosen = *(spilledNodes.begin());
  }
  spillWorkList.erase(chosen);
  simplifyWorkList.insert(chosen);
  FreezeMoves(chosen);
}

void AssignColors() {
  while (!selectStack.empty()) {
    auto n = selectStack[selectStack.size() - 1];
    selectStack.pop_back();
    std::set<std::string> okColors;
    auto colorVec = F::X64Frame::colors();
    okColors.insert(colorVec.begin(), colorVec.end());
    for (auto w : adjList[n]) {
      if (coloredNodes.find(GetAlias(w)) != coloredNodes.end() || precolored(GetAlias(w)->NodeInfo())) {
        okColors.erase(*(coloring->Look(GetAlias(w)->NodeInfo())));
      }
    }
    if (okColors.empty()) {
      spilledNodes.insert(n);
    } else {
      coloredNodes.insert(n);
      std::string *color = new std::string;
      *color = *(okColors.begin());
      coloring->Enter(n->NodeInfo(), color);
    }
  }
  for (auto n : coalescedNodes) {
    coloring->Enter(n->NodeInfo(), coloring->Look(GetAlias(n)->NodeInfo()));
  }
}

AS::InstrList *RewriteProgram(F::Frame *f, AS::InstrList *il) {
  for (auto node : spilledNodes) {
    TEMP::Temp *spilledTemp = node->NodeInfo();
    f->offset -= F::X64Frame::wordSize;
    AS::InstrList *newIl = nullptr;
    for (AS::InstrList *p = il; p; p = p->tail) {
      TEMP::TempList *src, *dst;
      switch (p->head->kind) {
        case AS::Instr::LABEL: {
          src = nullptr;
          dst = nullptr;
          break;
        }
        case AS::Instr::MOVE: {
          auto moveInstr = (AS::MoveInstr *) p->head;
          src = moveInstr->src;
          dst = moveInstr->dst;
          break;
        }
        case AS::Instr::OPER: {
          auto moveInstr = (AS::OperInstr *) p->head;
          src = moveInstr->src;
          dst = moveInstr->dst;
          break;
        }
      }
      if (src && src->contains(spilledTemp) && dst && dst->contains(spilledTemp)) {
        TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
        noSpillTemp.insert(newTemp);
        src->replace(spilledTemp, newTemp);
        dst->replace(spilledTemp, newTemp);
        std::stringstream stream;
        stream << "movq (" << f->name->Name() << "_framesize" << f->offset << ")(`s0), `d0";
        std::string assem = stream.str();
        newIl = AS::InstrList::Splice(newIl, new AS::InstrList(new AS::OperInstr(assem, new TEMP::TempList(newTemp, nullptr), new TEMP::TempList(F::X64Frame::RSP(), nullptr), nullptr), nullptr));
        newIl = AS::InstrList::Splice(newIl, new AS::InstrList(p->head, nullptr));
        stream.str(0);
        stream << "movq `s0, (" << f->name->Name() << "_framesize" << f->offset << ")(`s1)";
        assem = stream.str();
        newIl = AS::InstrList::Splice(newIl, new AS::InstrList(new AS::OperInstr(assem, nullptr, new TEMP::TempList(newTemp, new TEMP::TempList(F::X64Frame::RSP(), nullptr)), nullptr), nullptr));
      } else if (src && src->contains(spilledTemp)) {
        TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
        noSpillTemp.insert(newTemp);
        src->replace(spilledTemp, newTemp);
        std::stringstream stream;
        stream << "movq (" << f->name->Name() << "_framesize" << f->offset << ")(`s0), `d0";
        std::string assem = stream.str();
        newIl = AS::InstrList::Splice(newIl, new AS::InstrList(new AS::OperInstr(assem, new TEMP::TempList(newTemp, nullptr), new TEMP::TempList(F::X64Frame::RSP(), nullptr), nullptr), nullptr));
        newIl = AS::InstrList::Splice(newIl, new AS::InstrList(p->head, nullptr));
      } else if (dst && dst->contains(spilledTemp)) {
        TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
        noSpillTemp.insert(newTemp);
        dst->replace(spilledTemp, newTemp);
        std::stringstream stream;
        newIl = AS::InstrList::Splice(newIl, new AS::InstrList(p->head, nullptr));
        stream << "movq `s0, (" << f->name->Name() << "_framesize" << f->offset << ")(`s1)";
        std::string assem = stream.str();
        newIl = AS::InstrList::Splice(newIl, new AS::InstrList(new AS::OperInstr(assem, nullptr, new TEMP::TempList(newTemp, new TEMP::TempList(F::X64Frame::RSP(), nullptr)), nullptr), nullptr));
      } else {
        newIl = AS::InstrList::Splice(newIl, new AS::InstrList(p->head, nullptr));
      }
    }
    il = newIl;
  }
  spilledNodes.clear();
  coloredNodes.clear();
  coalescedNodes.clear();
  return il;
}

AS::InstrList *removeUnnecessary(AS::InstrList *il) {
 AS::InstrList *head = nullptr, *newp = nullptr;
 for (AS::InstrList *p = il; p; p = p->tail) {
   AS::Instr *instr = p->head;
   if (instr->kind == AS::Instr::MOVE) {
     auto *moveInstr = (AS::MoveInstr *) p->head;
     TEMP::Temp *src = moveInstr->src->head, *dst = moveInstr->dst->head;
     if (coloring->Look(src) == coloring->Look(dst)) {
       continue;
     }
   }
   if (head) {
     newp->tail = new AS::InstrList(instr, nullptr);
     newp = newp->tail;
   } else {
     head = newp = new AS::InstrList(instr, nullptr);
   }
 }
 return head;
//   return il;
}

Result RegAlloc(F::Frame* f, AS::InstrList* il) {
  G::Graph<AS::Instr> *cfg = FG::AssemFlowGraph(il, f);
  liveness = LIVE::Liveness(cfg);
  Build();
  MakeWorkList();
  do {
      if (!simplifyWorkList.empty()) {
        Simplify();
      } else if (workListMoves) {
        Coalesce();
      } else if (!freezeWorkList.empty()) {
        Freeze();
      } else if (!spillWorkList.empty()) {
        SelectSpill();
      }
  } while (!simplifyWorkList.empty() || workListMoves || !freezeWorkList.empty() || !spillWorkList.empty());
  AssignColors();
  if (!spilledNodes.empty()) {
    il = RewriteProgram(f, il);
    return RegAlloc(f, il);
  } else {
    Result result;
    result.coloring = coloring;
    result.il = removeUnnecessary(il);
    return result;
  }
}

}  // namespace RA