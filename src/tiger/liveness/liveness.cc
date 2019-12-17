#include <map>
#include <set>
#include <vector>
#include <tiger/frame/x64frame.h>
#include "tiger/liveness/liveness.h"

namespace LIVE {

static std::map<TEMP::Temp *, G::Node<TEMP::Temp> *> temp2Node;

bool contains(TEMP::TempList *list, TEMP::Temp *temp) {
  for (TEMP::TempList *p = list; p; p = p->tail) {
    if (p->head == temp) {
      return true;
    }
  }
  return false;
}

TEMP::TempList *Union(TEMP::TempList *lhs, TEMP::TempList *rhs) {
  TEMP::TempList *result = nullptr;
  for (TEMP::TempList *p = lhs; p; p = p->tail) {
    if (!contains(result, p->head)) {
      result = new TEMP::TempList(p->head, result);
    }
  }

  for (TEMP::TempList *p = rhs; p; p = p->tail) {
    if (!contains(result, p->head)) {
      result = new TEMP::TempList(p->head, result);
    }
  }

  return result;
}

TEMP::TempList *Subtract(TEMP::TempList *lhs, TEMP::TempList *rhs) {
  TEMP::TempList *result = nullptr;
  for (TEMP::TempList *p = lhs; p; p = p->tail) {
    if (!contains(rhs, p->head)) {
      result = new TEMP::TempList(p->head, result);
    }
  }
  return result;
}

bool equal(TEMP::TempList *lhs, TEMP::TempList *rhs) {
  std::set<int> lhsIds, rhsIds;
  for (TEMP::TempList *p = lhs; p; p = p->tail) {
    lhsIds.insert(p->head->Int());
  }

  for (TEMP::TempList *p = rhs; p; p = p->tail) {
    rhsIds.insert(p->head->Int());
  }

  return lhsIds == rhsIds;
}

bool equal(std::map<G::Node<AS::Instr> *, TEMP::TempList *> lhs, std::map<G::Node<AS::Instr> *, TEMP::TempList *> rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (const auto &item : lhs) {
    if (rhs.find(item.first) == rhs.end()) {
      return false;
    }
    if (!equal(item.second, rhs[item.first])) {
      return false;
    }
  }
  return true;
}

G::Node<TEMP::Temp> *GetNode(G::Graph<TEMP::Temp> *graph, TEMP::Temp *temp) {
  if (temp2Node.find(temp) == temp2Node.end()) {
    temp2Node[temp] = graph->NewNode(temp);
  }
  return temp2Node[temp];
}

void showTemp(TEMP::Temp *temp) {
  printf("%d", temp->Int());
}

void addPrecoloredConflict(G::Graph<TEMP::Temp>* graph) {
  for (TEMP::Temp *temp1 : F::X64Frame::registers()) {
    for (TEMP::Temp *temp2 : F::X64Frame::registers()) {
      G::Node<TEMP::Temp> *temp1Node = GetNode(graph, temp1);
      G::Node<TEMP::Temp> *temp2Node = GetNode(graph, temp2);
      if (temp1Node != temp2Node) {
        G::Graph<TEMP::Temp>::AddEdge(temp1Node, temp2Node);
        G::Graph<TEMP::Temp>::AddEdge(temp2Node, temp1Node);
      }
    }
  }
}

LiveGraph Liveness(G::Graph<AS::Instr>* flowgraph) {
  LiveGraph liveGraph;
  liveGraph.graph = new G::Graph<TEMP::Temp>();
  liveGraph.moves = nullptr;
  temp2Node.clear();

  std::map<G::Node<AS::Instr> *, TEMP::TempList *> in, out;

  while (true) {
    std::map<G::Node<AS::Instr> *, TEMP::TempList *> lastIn = in, lastOut = out;
    for (G::NodeList<AS::Instr> *p = flowgraph->Nodes(); p; p = p->tail) {
      TEMP::TempList *defs = FG::Def(p->head);
      TEMP::TempList *uses = FG::Use(p->head);
      in[p->head] = Union(uses, Subtract(out[p->head], defs));
      out[p->head] = nullptr;
      for (G::NodeList<AS::Instr> *q = p->head->Succ(); q; q = q->tail) {
        out[p->head] = Union(out[p->head], in[q->head]);
      }
    }
    if (equal(in, lastIn) && equal(out, lastOut)) {
      break;
    }
  }

  addPrecoloredConflict(liveGraph.graph);

  for (G::NodeList<AS::Instr> *p = flowgraph->Nodes(); p; p = p->tail) {
    TEMP::TempList *defs = FG::Def(p->head);
    TEMP::TempList *uses = FG::Use(p->head);
    if (FG::IsMove(p->head) && defs && uses) {
      G::Node<TEMP::Temp> *srcNode = GetNode(liveGraph.graph, uses->head);
      G::Node<TEMP::Temp> *dstNode = GetNode(liveGraph.graph, defs->head);
      liveGraph.moves = new MoveList(srcNode, dstNode, liveGraph.moves);
      for (TEMP::TempList *q = out[p->head]; q; q = q->tail) {
        if (q->head == uses->head) {
          continue;
        }
        G::Node<TEMP::Temp> *outNode = GetNode(liveGraph.graph, q->head);
        if (dstNode != outNode) {
          G::Graph<TEMP::Temp>::AddEdge(dstNode, outNode);
          G::Graph<TEMP::Temp>::AddEdge(outNode, dstNode);
        }
      }
    } else {
      for (TEMP::TempList *def = defs; def; def = def->tail) {
        for (TEMP::TempList *q = out[p->head]; q; q = q->tail) {
          G::Node<TEMP::Temp> *dstNode = GetNode(liveGraph.graph, def->head);
          G::Node<TEMP::Temp> *outNode = GetNode(liveGraph.graph, q->head);
          if (dstNode != outNode) {
            G::Graph<TEMP::Temp>::AddEdge(dstNode, outNode);
            G::Graph<TEMP::Temp>::AddEdge(outNode, dstNode);
          }
        }
      }
    }
  }

  for (G::NodeList<AS::Instr> *p = flowgraph->Nodes(); p; p = p->tail) {
    TEMP::TempList *defs = FG::Def(p->head);
    TEMP::TempList *uses = FG::Use(p->head);
    for (TEMP::TempList *q = defs; q; q = q->tail) {
      liveGraph.priority[q->head]++;
    }
    for (TEMP::TempList *q = uses; q; q = q->tail) {
      liveGraph.priority[q->head]++;
    }
  }
  for (G::NodeList<TEMP::Temp> *p = liveGraph.graph->Nodes(); p; p = p->tail) {
    liveGraph.priority[p->head->NodeInfo()] /= p->head->Degree();
  }
  return liveGraph;
}

MoveList *MoveList::Union(MoveList *lhs, MoveList *rhs) {
  MoveList *result = nullptr;
  for (MoveList *p = lhs; p; p = p->tail) {
    if (result && result->contains(p->src, p->dst)) {
      continue;
    }
    result = new MoveList(p->src, p->dst, result);
  }

  for (MoveList *p = rhs; p; p = p->tail) {
    if (result && result->contains(p->src, p->dst)) {
      continue;
    }
    result = new MoveList(p->src, p->dst, result);
  }

  return result;
}

MoveList *MoveList::Subtract(MoveList *lhs, MoveList *rhs) {
  MoveList *result = nullptr;
  for (MoveList *p = lhs; p; p = p->tail) {
    if (rhs && rhs->contains(p->src, p->dst)) {
      continue;
    }
    result = new MoveList(p->src, p->dst, result);
  }
  return result;
}

MoveList *MoveList::Intersect(MoveList *lhs, MoveList *rhs) {
  MoveList *result = nullptr;
  for (MoveList *p = lhs; p; p = p->tail) {
    if (!rhs || !rhs->contains(p->src, p->dst)) {
      continue;
    }
    result = new MoveList(p->src, p->dst, result);
  }
  return result;
}

bool MoveList::contains(G::Node<TEMP::Temp> *src, G::Node<TEMP::Temp> *dst) {
  if (tail) {
    return (this->src == src && this->dst == dst) || (this->src == dst && this->dst == src) || tail->contains(src, dst);
  } else {
    return (this->src == src && this->dst == dst) || (this->src == dst && this->dst == src);
  }
}
}  // namespace LIVE