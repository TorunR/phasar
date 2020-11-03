#include <boost/filesystem/operations.hpp>
#include <nlohmann/json.hpp>

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
#include <phasar/PhasarLLVM/Pointer/LLVMPointsToInfo.h>
#include <phasar/PhasarLLVM/TypeHierarchy/LLVMTypeHierarchy.h>
#include <phasar/Utils/LLVMIRToSrc.h>
#include <phasar/Utils/Logger.h>

#include <deque>
#include <map>

#include <chrono>
#include <iostream>
#include <utility>
#include <vector>
#define __INTERPROCEDURAL__
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
};
// TODO Memory Leak here
Location *createLocation(unsigned int line, unsigned int column) {
  return new Location{.line = line, .column = column};
}

bool operator<(const Location &l1, const Location &l2) {
  return l1.line < l2.line || (l1.line == l2.line && l1.column < l2.column);
}
struct SliceComparator {
  template <typename T>
  bool operator()(const tuple<string, Location, T> &l,
                  const tuple<string, Location, T> &r) const {
    return std::get<1>(l) < std::get<1>(r);
  }
};
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

class SlicerFact {
public:
  SlicerFact() = default;
  SlicerFact(const Location *l, const Value *i) : l(l), i(i) { }
  bool operator<(const SlicerFact &rhs) const { return i < rhs.i; }
  bool operator==(const SlicerFact &rhs) const {
    return i == rhs.i && l == rhs.l;
  }

  friend std::ostream &operator<<(std::ostream &os, const SlicerFact &rhs);
  [[nodiscard]] bool isZero() const { return i == nullptr; }
  const llvm::Value *getInstruction() { return i; }
  const Location *getLocation() { return l; }

private:
  const Location *l = nullptr;
  const Value *i = nullptr;
};

std::ostream &operator<<(std::ostream &os, const SlicerFact &rhs) {
  std::string temp;
  llvm::raw_string_ostream rso(temp);
  if (!rhs.isZero()) {
    rhs.i->print(rso, true);
    os << temp;
  } else {
    os << "Zero";
  }
  return os;
}
namespace std {
template <> struct hash<SlicerFact> {
  size_t operator()(const SlicerFact &x) const { return 0; }
};
} // namespace std

class NormalFlowFunction : public FlowFunction<SlicerFact> {
public:
  NormalFlowFunction(const Instruction *curr, const Instruction *succ)
      : curr(curr), succ(succ) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) override {
    set<SlicerFact> facts;
    facts.insert(source);
    // check logic for conditions
    if (!source.isZero()) {
      const auto* user = dyn_cast<Instruction>(source.getInstruction());
      if (user) {
        llvm::dbgs() << "USER:\n";
        user->dump();
        llvm::dbgs() << curr->getFunction()->getName() << "\n";
        for (unsigned int i = 0; i < user->getNumOperands(); ++i) {
          if (const auto *use =
                  dyn_cast<Instruction>(user->getOperandUse(i).get())) {
            if (use == curr || use == source.getInstruction()) {
              llvm::dbgs() << "USE:\n";
              user->getOperandUse(i)->dump();
              llvm::dbgs() << "\n\n";
              if (auto *dl = use->getDebugLoc().get()) {
                facts.insert(SlicerFact(
                    createLocation(dl->getLine(), dl->getColumn()), use));
              }
            }
          }
        }
      }
    }
    return facts;
  }

private:
  const Instruction *curr;
  const Instruction *succ;
};

class CallFlowFunction : public FlowFunction<SlicerFact> {
public:
  CallFlowFunction(const Instruction *callStmt, const Function *destMthd)
      : callStmt(callStmt), destMthd(destMthd) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) override {
    set<SlicerFact> facts;
    facts.insert(source);
#ifdef __INTERPROCEDURAL__
    if (!source.isZero()) {
      for (unsigned int i = 0; i < callStmt->getNumOperands(); ++i) {
        const auto op = callStmt->getOperand(i);
        if (source.getInstruction() == op) {
          facts.insert(SlicerFact(source.getLocation(),
                                  getNthFunctionArgument(destMthd, i)));
        }
      }
    }
#endif
    return facts;
  }

private:
  const Instruction *callStmt;
  const Function *destMthd;
};

class RetFlowFunction : public FlowFunction<SlicerFact> {
public:
  RetFlowFunction(const Instruction *callSite, const Function *calleeMthd,
                  const Instruction *exitStmt, const Instruction *retSite)
      : callSite(callSite), calleeMthd(calleeMthd), exitStmt(exitStmt),
        retSite(retSite) {}
  std::set<SlicerFact> computeTargets(SlicerFact source) override {
    llvm::dbgs() << "\n";
    callSite->dump();
    calleeMthd->dump();
    exitStmt->dump();
    retSite->dump();
    set<SlicerFact> facts;
    facts.insert(source);
#ifdef __INTERPROCEDURAL__
    for (unsigned int i = 0; i < callSite->getNumOperands(); ++i) {
      const auto op = callSite->getOperand(i);
      if (source.getInstruction() == op) {
        facts.insert(SlicerFact(source.getLocation(),
                                getNthFunctionArgument(calleeMthd, i)));
      }
    }
    if (!source.isZero()) {
      facts.insert(SlicerFact(source.getLocation(), exitStmt));
    }
#endif
    return facts;
  }

private:
  const Instruction *callSite;
  const Function *calleeMthd;
  const Instruction *exitStmt;

  const Instruction *retSite;
};

class CallToRetFlowFunction : public FlowFunction<SlicerFact> {
public:
  CallToRetFlowFunction(const Instruction *callSite, const Instruction *retSite,
                        set<const Function *> callees)
      : callSite(callSite), retSite(retSite), callees(std::move(callees)) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) override {
    set<SlicerFact> facts;
    facts.insert(source);
    return facts;
  }

private:
  const Instruction *callSite;
  const Instruction *retSite;
  set<const Function *> callees;
};

template <typename ICFG_T>
class IFDSSLicer
    : public IFDSTabulationProblem<
          const llvm::Instruction *, SlicerFact, const llvm::Function *,
          const llvm::StructType *, const llvm::Value *, ICFG_T> {
public:
  IFDSSLicer(ICFG_T *icfg, const LLVMTypeHierarchy *th, const ProjectIRDB *irdb,
             const LLVMPointsToInfo *PT,
             map<const Instruction *, set<SlicerFact>> sc,
             const std::vector<Term> *terms,
             const std::set<std::string> &entrypoints)
      : IFDSTabulationProblem<const llvm::Instruction *, SlicerFact,
                              const llvm::Function *, const llvm::StructType *,
                              const llvm::Value *, ICFG_T>(irdb, th, icfg, PT),
        terms(terms), slicingCriteria(std::move(sc)), entrypoints(entrypoints) {
  }

private:
  [[nodiscard]] SlicerFact createZeroValue() const override {
    return SlicerFact();
  }
  shared_ptr<FlowFunction<SlicerFact>>
  getNormalFlowFunction(const Instruction *curr,
                        const Instruction *succ) override {
    return make_shared<NormalFlowFunction>(curr, succ);
  }
  shared_ptr<FlowFunction<SlicerFact>>
  getCallFlowFunction(const Instruction *callStmt,
                      const Function *destMthd) override {
    return make_shared<CallFlowFunction>(callStmt, destMthd);
  }
  shared_ptr<FlowFunction<SlicerFact>>
  getRetFlowFunction(const Instruction *callSite, const Function *calleeMthd,
                     const Instruction *exitStmt,
                     const Instruction *retSite) override {
    return make_shared<RetFlowFunction>(callSite, calleeMthd, exitStmt,
                                        retSite);
  }
  shared_ptr<FlowFunction<SlicerFact>>
  getCallToRetFlowFunction(const Instruction *callSite,
                           const Instruction *retSite,
                           set<const Function *> callees) override {
    return make_shared<CallToRetFlowFunction>(callSite, retSite, callees);
  }
  map<const Instruction *, set<SlicerFact>> initialSeeds() override {
    for (auto &entrypoint : entrypoints) {
      const Instruction *i = &this->ICF->getFunction(entrypoint)->back().back();
      SlicerFact sf = SlicerFact();
      if (slicingCriteria.find(i) == slicingCriteria.end()) {
        set<SlicerFact> facts;
        facts.insert(sf);
        slicingCriteria[i] = facts;
      } else {
        slicingCriteria[i].insert(sf);
      }
    }
    return slicingCriteria;
  }

  [[nodiscard]] bool isZeroValue(SlicerFact d) const override {
    return d.isZero();
  }
  void printNode(std::ostream &os, const Instruction *n) const override {
    std::string temp;
    llvm::raw_string_ostream rso(temp);
    n->print(rso, true);
    os << temp;
  }
  void printDataFlowFact(std::ostream &os, SlicerFact d) const override {
    os << d;
  }
  void printFunction(std::ostream &os, const Function *m) const override {}

  shared_ptr<FlowFunction<SlicerFact>>
  getSummaryFlowFunction(const Instruction *curr,
                         const Function *destFun) override {
    return nullptr;
  }

  const std::vector<Term> *terms;
  map<const Instruction *, set<SlicerFact>> slicingCriteria;
  const std::set<string> &entrypoints;
};

std::string createSlice(string target, set<string> entrypoints,
                        vector<Term> terms) {
  ProjectIRDB DB({target}, IRDBOptions::WPA);
  LOG_IF_ENABLE(BOOST_LOG(lg::get()) << "STARTED")
  LLVMTypeHierarchy th(DB);
  LLVMBasedBackwardsICFG cg(DB, CallGraphAnalysisType::DTA, set(entrypoints),
                            &th);
  LLVMPointsToInfo PT(DB);
  map<const Instruction *, set<SlicerFact>> sc;
  for (auto *module : DB.getAllModules()) {
    for (auto &function : module->functions()) {
      for (auto &bb : function) {
        for (const auto &i : bb) {
          set<SlicerFact> facts;

          if (auto *debuglog = i.getDebugLoc().get()) {
            auto line = debuglog->getLine();
            auto column = debuglog->getColumn();
            auto file = debuglog->getFilename();
            if (const auto *store = dyn_cast<StoreInst>(&i)) {
              if (const auto *global =
                      dyn_cast<GlobalValue>(store->getPointerOperand())) {
                //                global->dump();
                llvm::dbgs() << global->getValueName()->first() << "\n";

                auto name = global->getValueName()->first();
                for (const auto &t : terms) {
                  if (name.find(t.term) != StringRef::npos) {
                    for (const auto &l : t.locations) {
                      if (l.line == line && false) {
                        facts.insert(SlicerFact(&l, &i));
                      }
                    }
                  }
                }
              }
            }
            for (auto &t : terms) {
              for (auto &l : t.locations) {
                // check same statement
                if (l.line == line && t.file == file) {
                  i.dump();
                  facts.insert(SlicerFact(&l, &i));
                  if (const auto *branch = dyn_cast<BranchInst>(&i)) {
                    if (branch->isConditional()) {
                      const auto *constbranch = branch->getSuccessor(0);
                      // Assume first branch is true branch -- we're pretty sure
                      // that is always the case
                      for (const auto &inst : *constbranch) {
                        if (auto *debuglog2 = inst.getDebugLoc().get()) {
                          const auto *loc = createLocation(
                              debuglog2->getLine(), debuglog2->getColumn());
                          facts.insert(SlicerFact(loc, &inst));
                        }
                      }
                    }
                  }
                }
              }
            }
          }
          if (!facts.empty()) {
            sc[&i] = facts;
          }
        }
      }
    }
  }
  IFDSSLicer<LLVMBasedBackwardsICFG> slicer(&cg, &th, &DB, &PT, sc, &terms,
                                            entrypoints);

  IFDSSolver test(slicer);
  test.solve();
  ofstream out;
  out.open("out/graph.dot");
  test.emitESGAsDot(out);
  out.close();
  out.open("out/results.txt");
  test.dumpResults(out);
  out.close();
  cout << endl;
  set<std::tuple<std::string, Location, const llvm::Value *>, SliceComparator>
      slice;
  vector<std::string> textSlice;
  map<const Function *,
      set<std::tuple<std::string, Location, const llvm::Value *>,
          SliceComparator>>
      slices;
  map<const Function *, std::string> textSlices;
  for (const auto &module : DB.getAllModules()) {
    for (auto &function : module->functions()) {
      llvm::dbgs() << function.getName() << "\n";
      bool isUsed = false;
      for (auto &bb : function) {
        for (const auto &i : bb) {
          auto res = test.ifdsResultsAt(&i);
          if (llvm::isa<llvm::ReturnInst>(&i)) {
            //            cout << "RET: " << llvmIRToString(&i) << endl;
          }
          cout << "INS: " << llvmIRToString(&i) << endl;
          cout << "SRC: " << psr::getSrcCodeFromIR(&i) << endl
               << res.size() << endl;
          for (auto fact : res) {
            if (!fact.isZero()) {
              auto extractedInstruction = fact.getInstruction();
              cout << "FACT INS: " << llvmIRToString(fact.getInstruction())
                   << endl;
              cout << "SRC: " << *fact.getLocation() << " "
                   << psr::getSrcCodeFromIR(&i) << endl
                   << endl;
              if (extractedInstruction == &i) {
              slice.insert(
                  std::make_tuple(psr::getSrcCodeFromIR(extractedInstruction),
                                  *fact.getLocation(), extractedInstruction));
              slices[&function].insert(
                  std::make_tuple(psr::getSrcCodeFromIR(extractedInstruction),
                                  *fact.getLocation(), extractedInstruction));
              const auto ins = dyn_cast<Instruction>(fact.getInstruction());
              if (ins) {
                const auto *blockExit = ins->getParent()->getTerminator();
                const auto instStr = psr::llvmIRToString(blockExit);
                const auto exitSrc = psr::getSrcCodeFromIR(blockExit);
                if (auto *dl = blockExit->getDebugLoc().get()) {
                  cout << "ADDING " << llvmIRToString(blockExit) << " "
                       << exitSrc << endl;
                  slice.insert(std::make_tuple(
                      exitSrc, *createLocation(dl->getLine(), dl->getColumn()),
                      blockExit));
                  slices[&function].insert(std::make_tuple(
                      exitSrc, *createLocation(dl->getLine(), dl->getColumn()),
                      blockExit));
                } else {
                  cerr << "DID NOT FIND LOCATION" << endl;
                  slice.insert(std::make_tuple(exitSrc, *createLocation(0, 0),
                                               blockExit));
                  slices[&function].insert(std::make_tuple(
                      exitSrc, *createLocation(0, 0), blockExit));
                }

                isUsed = true;
              }
            } else {
              cout << "DISCARDED PREVIOUS" << endl;
            }
            } else {
              cout << "ZERO" << endl;
            }
          }
          //          cout << endl;
        }
      }
      if (isUsed) {
        // Check whether this works everywhere
        const auto *entry = &function.getEntryBlock().front();
        const auto source = psr::getSrcCodeFromIR(&function);
        for (auto &bb : function) {
          for (auto &ins : bb) {
            if (auto *dl = ins.getDebugLoc().get()) {
              slice.insert(std::make_tuple(
                  source, *createLocation(dl->getLine(), dl->getColumn()), entry));
              slices[&function].insert(std::make_tuple(
                  source, *createLocation(dl->getLine(), dl->getColumn()), entry));
              goto added; // we found the first instruction with debug info
            }
          }
        }
          slice.insert(std::make_tuple(source, *createLocation(0, 0), entry));
          slices[&function].insert(
              std::make_tuple(source, *createLocation(0, 0), entry));
      added:
        const auto exits = cg.getStartPointsOf(&function);
        for (const auto *exit : exits) {
          if (auto *dl = exit->getDebugLoc().get()) {
            slice.insert(std::make_tuple(
                psr::getSrcCodeFromIR(exit),
                *createLocation(dl->getLine(), dl->getColumn()), exit));
            slices[&function].insert(std::make_tuple(
                psr::getSrcCodeFromIR(exit),
                *createLocation(dl->getLine(), dl->getColumn()), exit));
          } else {
            slice.insert(std::make_tuple(psr::getSrcCodeFromIR(exit),
                                         *createLocation(INT_MAX, INT_MAX), exit));
            slices[&function].insert(std::make_tuple(
                psr::getSrcCodeFromIR(exit), *createLocation(INT_MAX, INT_MAX), exit));
            cout << "GOT NO DEBUG LOG" << endl;
          }
        }
      }
    }
  }
  string prev;
  for (auto i : slice) {
    auto curr = std::get<0>(i);
    cout << std::get<0>(i) << "\t\t\t\t\t\t\t\t\t\t\t\t" << std::get<1>(i)
         << "\t\t\t\t\t" << psr::llvmIRToString(std::get<2>(i)) << endl;
    if (curr != prev) {
      textSlice.emplace_back(curr);
    }
    prev = curr;
  }
  cout << endl << endl << endl << endl;

  for (auto &s : textSlice) {
    cout << s << endl;
  }
  cout << endl << endl << endl << endl;
  for (auto &p : slices) {
    llvm::dbgs() << p.first->getName() << "\t" << p.second.size() << "\n";
    for (auto &s : p.second) {
      llvm::dbgs() << std::get<0>(s) << "\n";
    }
  }
  return "";
}

int main(int argc, const char **argv) {
  std::chrono::steady_clock::time_point begin =
      std::chrono::steady_clock::now();
  std::cout << "Current path is " << bofs::current_path() << '\n';
  //  const auto target = "./targets/w_defects.ll";
  //    const auto target = "./targets/main_linked.ll";
//    const auto target = "./targets/ex.ll";
//    const auto target =
//    "/media/Volume/Arbeit/Arbeit/code/slicing-eval/memcached/memcached.ll";
//  const auto target = "./targets/toSlice.ll";
    const auto target = "./targets/min_ex.ll";
  //  const auto target =
  //  "/media/Volume/Arbeit/Arbeit/code/slicing-eval/git/git-linked.ll";
  const auto entryfunction = "main";
//    const auto entryfunction = "parse_command";
  const set<std::string> entrypoints{entryfunction};
  //  initializeLogger(true);
//    std::ifstream  in("targets/locations_json.json");
//    std::ifstream in("targets/locations_memcached_command.json");
//    std::ifstream in("targets/locations_memcached_single.json");
//  std::ifstream in("targets/toSlice.json");
  std::ifstream in("targets/min_ex.json");
  json j;
  in >> j;
  auto terms = j.get<vector<Term>>();
  ProjectIRDB DB({target}, IRDBOptions::WPA);
  LOG_IF_ENABLE(BOOST_LOG(lg::get()) << "STARTED")
  LLVMTypeHierarchy th(DB);
  LLVMBasedBackwardsICFG cg(DB, CallGraphAnalysisType::DTA, entrypoints, &th);
  LLVMPointsToInfo PT(DB);
  createSlice(target, entrypoints, terms);
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::cout << "Time difference = "
            << std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                     begin)
                   .count()
            << "[µs]" << std::endl;
  std::cout << "Time difference = "
            << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin)
                   .count()
            << "[ns]" << std::endl;
  std::cout
      << "Time difference = "
      << std::chrono::duration_cast<std::chrono::seconds>(end - begin).count()
      << "[s]" << std::endl;
  std::cout
      << "Time difference = "
      << std::chrono::duration_cast<std::chrono::minutes>(end - begin).count()
      << "[m]" << std::endl;
  return 0;
}
// TODO:
// Recursive Control dependency backward - branch conditions
// branch conditions forward -> include block
// Coll Declarations
