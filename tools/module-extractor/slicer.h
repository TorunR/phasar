#ifndef PHASAR_SLICER_H
#define PHASAR_SLICER_H

#include <boost/filesystem/operations.hpp>
#include <nlohmann/json.hpp>

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/ADT/StringRef.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

#include <phasar/DB/ProjectIRDB.h>
#include <phasar/PhasarLLVM/ControlFlow/LLVMBasedBackwardICFG.h>
#include <phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h>
#include <phasar/PhasarLLVM/DataFlowSolver/IfdsIde/IFDSTabulationProblem.h>
#include <phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Solver/IFDSSolver.h>
#include <phasar/PhasarLLVM/Pointer/LLVMPointsToGraph.h>
#include <phasar/PhasarLLVM/Pointer/LLVMPointsToInfo.h>
#include <phasar/PhasarLLVM/TypeHierarchy/LLVMTypeHierarchy.h>
#include <phasar/Utils/LLVMIRToSrc.h>
#include <phasar/Utils/Logger.h>

#include <chrono>
#include <deque>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace bofs = boost::filesystem;
using namespace llvm;
using namespace std;
using namespace psr;
using json = nlohmann::json;

struct Term;
struct Location {
  unsigned int line;
  unsigned int column;
  map<string, string> meta;
  Location() {}
  Location(unsigned int line, unsigned int column)
      : line(line), column(column) {}
};
// TODO Memory Leak here
Location *createLocation(unsigned int line, unsigned int column) {
  return new Location(line, column);
}

bool operator<(const Location &l1, const Location &l2) {
  return l1.line < l2.line || (l1.line == l2.line && l1.column < l2.column);
}

void to_json(json &j, const Location &l) {
  j = json{{"line", l.line}, {"column", l.column}, {"meta", l.meta}};
}
void from_json(const json &j, Location &l) {
  j.at("line").get_to(l.line);
  j.at("column").get_to(l.column);
  j.at("meta").get_to(l.meta);
}
ostream &operator<<(ostream &os, const Location &l) {
  os << "(" << l.line << "," << l.column << ")";
  return os;
}

raw_ostream &operator<<(raw_ostream &os, const Location &l) {
  os << "(" << l.line << "," << l.column << ")";
  return os;
}

struct Term {
  std::string file;
  std::string term;
  vector<Location> locations;
};
void to_json(json &j, const Term &t) {
  j = json{{"file", t.file}, {"term", t.term}, {"locations", t.locations}};
}
void from_json(const json &j, Term &t) {
  j.at("file").get_to(t.file);
  j.at("term").get_to(t.term);
  j.at("locations").get_to(t.locations);
}

ostream &operator<<(ostream &os, const Term &t) {
  os << '\'' << t.term << "' in " << t.file << " at [";
  for (auto const &l : t.locations) {
    os << l << ",";
  }
  os << "]";
  return os;
}
class SlicerFact;
namespace std {
template <> struct hash<SlicerFact> {
  size_t operator()(const SlicerFact &x) const;
};
}
class SlicerFact {
public:
  SlicerFact() = default;
  SlicerFact(const Location *l, const Value *i) : l(l), i(i) {}
  bool operator<(const SlicerFact &rhs) const { return i < rhs.i; }
  bool operator==(const SlicerFact &rhs) const {
    return i == rhs.i && l == rhs.l;
  }

  friend std::ostream &operator<<(std::ostream &os, const SlicerFact &rhs);
  friend raw_ostream &operator<<(raw_ostream &os, const SlicerFact &rhs);
  [[nodiscard]] bool isZero() const { return i == nullptr; }
  const llvm::Value *getInstruction() { return i; }
  const Location *getLocation() { return l; }

  friend size_t std::hash<SlicerFact>::operator()(const SlicerFact &x) const;
private:
  const Location *l = nullptr;
  const Value *i = nullptr;
};

std::ostream &operator<<(std::ostream &os, const SlicerFact &rhs) {
  std::string temp;
  llvm::raw_string_ostream rso(temp);
  if (!rhs.isZero()) {
    rhs.i->print(rso, true);
    os << temp << " " << *rhs.l;
  } else {
    os << "Zero";
  }
  return os;
}

raw_ostream &operator<<(raw_ostream &os, const SlicerFact &rhs) {
  std::string temp;
  llvm::raw_string_ostream rso(temp);
  if (!rhs.isZero()) {
    rhs.i->print(rso, true);
    os << temp << " " << *rhs.l;
  } else {
    os << "Zero";
  }
  return os;
}


namespace std {
 size_t hash<SlicerFact>::operator()(const SlicerFact &x) const { return llvm::hash_value(x.i); }
} // namespace std
template <typename ICFG_T> struct SlicerAnalysisDomain : public AnalysisDomain {
  using d_t = SlicerFact;
  using n_t = const llvm::Instruction *;
  using f_t = const llvm::Function *;
  using t_t = const llvm::StructType *;
  using v_t = const llvm::Value *;
  using c_t = ICFG_T;
  using i_t = ICFG_T;
};

class NormalFlowFunction : public FlowFunction<SlicerFact> {
public:
  NormalFlowFunction(const Instruction *curr, const Instruction *succ)
      : curr(curr), succ(succ) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) override;

private:
  [[maybe_unused]] const Instruction *curr;
  [[maybe_unused]] const Instruction *succ;
};

template <typename ICFG_T>
class CallFlowFunction : public FlowFunction<SlicerFact> {
public:
  CallFlowFunction(const Instruction *callStmt, const Function *destMthd,
                   const ICFG_T *ICF)
      : callStmt(callStmt), destMthd(destMthd), ICF(ICF) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) override;

private:
  [[maybe_unused]] const Instruction *callStmt;
  [[maybe_unused]] const Function *destMthd;
  [[maybe_unused]] const ICFG_T *ICF;
};

template <typename ICFG_T>
class RetFlowFunction : public FlowFunction<SlicerFact> {
public:
  RetFlowFunction(const Instruction *callSite, const Function *calleeMthd,
                  const Instruction *exitStmt, const Instruction *retSite)
      : callSite(callSite), calleeMthd(calleeMthd), exitStmt(exitStmt),
        retSite(retSite) {}
  std::set<SlicerFact> computeTargets(SlicerFact source) override;

private:
  [[maybe_unused]] const Instruction *callSite;
  [[maybe_unused]] const Function *calleeMthd;
  [[maybe_unused]] const Instruction *exitStmt;

  [[maybe_unused]] const Instruction *retSite;
};

class CallToRetFlowFunction : public FlowFunction<SlicerFact> {
public:
  CallToRetFlowFunction(const Instruction *callSite, const Instruction *retSite,
                        set<const Function *> callees)
      : callSite(callSite), retSite(retSite), callees(std::move(callees)) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) override;

private:
  [[maybe_unused]] const Instruction *callSite;
  [[maybe_unused]] const Instruction *retSite;
  [[maybe_unused]] set<const Function *> callees;
};


void compare_slice(string original, string module);

shared_ptr<set<unsigned int>> add_block(std::string file,unsigned int line);
#endif // PHASAR_SLICER_H

