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
#define __INTERPROCEDURAL__
#define OPERAND_PROP
//#define _DISTANCE_LIMITS_
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

bool operator==(const Location &l1, const Location &l2) {
  return l1.line == l2.line || l1.column == l2.column;
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
} // namespace std
class SlicerFact {
public:
  SlicerFact() = default;
  SlicerFact(const Location *l, const Value *i)
      : l(l), i(i)  {}
  SlicerFact(const Location *l, const Value *i,
             int8_t inter_distance)
      : l(l), i(i)
#ifdef _DISTANCE_LIMITS_
        ,inter_distance(inter_distance)
#endif
  {}
  bool operator<(const SlicerFact &rhs) const { return i < rhs.i; }
  bool operator==(const SlicerFact &rhs) const {
    return i == rhs.i && l == rhs.l;
  }

  friend std::ostream &operator<<(std::ostream &os, const SlicerFact &rhs);
  friend raw_ostream &operator<<(raw_ostream &os, const SlicerFact &rhs);
  [[nodiscard]] bool isZero() const { return i == nullptr; }
  const llvm::Value *getInstruction() { return i; }
  const Location *getLocation() { return l; }
  int8_t getInterDistance() const;

  friend size_t std::hash<SlicerFact>::operator()(const SlicerFact &x) const;

  bool is_within_limits() {
#ifdef _DISTANCE_LIMITS_
    return inter_distance < INTER_LIMIT;
#else
    return true;
#endif
  }
#ifdef _DISTANCE_LIMITS_
  static constexpr int8_t INTER_LIMIT = 3;
#endif
private:
  const Location *l = nullptr;
  const Value *i = nullptr;
#ifdef _DISTANCE_LIMITS_
  int8_t inter_distance =0;
#endif
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
#ifdef _DISTANCE_LIMITS_
int8_t SlicerFact::getInterDistance() const { return inter_distance; }
#else
int8_t SlicerFact::getInterDistance() const { return 0; }
#endif

namespace std {
size_t hash<SlicerFact>::operator()(const SlicerFact &x) const {
  return llvm::hash_value(x.i);
}
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

void compare_slice(string original, string module)
  {
    set<string> original_lines;
    set<string> module_lines;
    vector<string> intersection;
    vector<string> missing;
    vector<string> additional;
    {
      std::ifstream in(original);
      std::string line;
      int line_nr = 1;
      while (std::getline(in, line)) {
        original_lines.insert(line);
      }
    }
    {
      std::ifstream in(module);
      std::string line;
      while (std::getline(in, line)) {
        module_lines.insert(line);
      }
    }
    set_intersection(original_lines.begin(), original_lines.end(),
                     module_lines.begin(), module_lines.end(),
                     std::inserter(intersection, intersection.begin()));
    set_difference(original_lines.begin(), original_lines.end(),
                   module_lines.begin(), module_lines.end(),
                   std::inserter(missing, missing.begin()));
    set_difference(module_lines.begin(), module_lines.end(),
                   original_lines.begin(), original_lines.end(),

                   std::inserter(additional, additional.begin()));

    for (auto m : missing) {
      cout << m << "\n";
    }
    cout << "====================================\n";
    for (auto a : additional) {
      cout << a << "\n";
    }
    cout << "Original Size is:\t" << original_lines.size() << endl;
    cout << "Intersection Size is:\t" << intersection.size() << endl;
    cout << "Additional Size is:\t" << additional.size() << endl;
    cout << "Missing Size is:\t" << missing.size() << endl;
    cout << "\n\n\n";

}

shared_ptr<set<unsigned int>> add_block(std::string file, std::set<unsigned int> *target_lines);
#endif // PHASAR_SLICER_H
