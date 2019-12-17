#ifndef TIGER_LIVENESS_LIVENESS_H_
#define TIGER_LIVENESS_LIVENESS_H_

#include <map>
#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/util/graph.h"

namespace LIVE {

class MoveList {
 public:
  G::Node<TEMP::Temp>*src, *dst;
  MoveList* tail;

  MoveList(G::Node<TEMP::Temp>* src, G::Node<TEMP::Temp>* dst, MoveList* tail)
      : src(src), dst(dst), tail(tail) {}

  static MoveList *Union(MoveList *lhs, MoveList *rhs);

  static MoveList *Subtract(MoveList *lhs, MoveList *rhs);

  static MoveList *Intersect(MoveList *lhs, MoveList *rhs);

  bool contains(G::Node<TEMP::Temp> *src, G::Node<TEMP::Temp> *dst);
};

class LiveGraph {
 public:
  G::Graph<TEMP::Temp>* graph;
  MoveList* moves;
  std::map<TEMP::Temp *, double> priority;
};

LiveGraph Liveness(G::Graph<AS::Instr>* flowgraph);

}  // namespace LIVE

#endif