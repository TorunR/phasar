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

#include <iostream>

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
  SlicerFact(const Location *l, const Instruction *i) : l(l), i(i) {}
  bool operator<(const SlicerFact &rhs) const { return i < rhs.i; }
  bool operator==(const SlicerFact &rhs) const {
    return i == rhs.i && l == rhs.l;
  }

  friend std::ostream &operator<<(std::ostream &os, const SlicerFact &rhs);
  [[nodiscard]] bool isZero() const { return i == nullptr; }
  const llvm::Instruction *getInstruction() { return i; }
  const Location *getLocation() { return l; }

private:
  const Location *l = nullptr;
  const Instruction *i = nullptr;
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
  NormalFlowFunction(const Instruction *curr, const Instruction *succ,
                     const vector<Term> *terms)
      : terms(terms), curr(curr), succ(succ) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) override {
    set<SlicerFact> facts;
    facts.insert(source);
    return facts;
  }

private:
  const vector<Term> *terms;
  const Instruction *curr;
  const Instruction *succ;
};

class CallFlowFunction : public FlowFunction<SlicerFact> {
public:
  CallFlowFunction(const Instruction *callStmt, const Function *destMthd,
                   const vector<Term> *terms)
      : terms(terms), callStmt(callStmt), destMthd(destMthd) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) override {
    set<SlicerFact> facts;
    facts.insert(source);
    return facts;
  }

private:
  const vector<Term> *terms;
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
    set<SlicerFact> facts;
    facts.insert(source);
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
                        set<const Function *> callees,
                        const vector<Term> *terms)
      : callSite(callSite), retSite(retSite), callees(std::move(callees)),
        terms(terms) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) override {
    set<SlicerFact> facts;
    facts.insert(source);
    return facts;
  }

private:
  const Instruction *callSite;
  const Instruction *retSite;
  set<const Function *> callees;
  const vector<Term> *terms;
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
             const std::vector<Term> *terms)
      : IFDSTabulationProblem<const llvm::Instruction *, SlicerFact,
                              const llvm::Function *, const llvm::StructType *,
                              const llvm::Value *, ICFG_T>(irdb, th, icfg, PT),
        terms(terms), slicingCriteria(std::move(sc)) {}

private:
  [[nodiscard]] SlicerFact createZeroValue() const override {
    return SlicerFact();
  }
  shared_ptr<FlowFunction<SlicerFact>>
  getNormalFlowFunction(const Instruction *curr,
                        const Instruction *succ) override {
    return make_shared<NormalFlowFunction>(curr, succ, terms);
  }
  shared_ptr<FlowFunction<SlicerFact>>
  getCallFlowFunction(const Instruction *callStmt,
                      const Function *destMthd) override {
    return make_shared<CallFlowFunction>(callStmt, destMthd, terms);
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
    return make_shared<CallToRetFlowFunction>(callSite, retSite, callees,
                                              terms);
  }
  map<const Instruction *, set<SlicerFact>> initialSeeds() override {
    const Instruction *i =
        //            &this->ICF->getFunction("parse_command")->front().front();
        &this->ICF->getFunction("parse_command")->back().front();
    SlicerFact sf = SlicerFact();
    if (slicingCriteria.find(i) == slicingCriteria.end()) {
      set<SlicerFact> facts;
      facts.insert(sf);
      slicingCriteria[i] = facts;
    } else {
      slicingCriteria[i].insert(sf);
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
};

int main(int argc, const char **argv) {
  std::cout << "Current path is " << bofs::current_path() << '\n';
  initializeLogger(true);
  //  ProjectIRDB DB({argv[1]}, IRDBOptions::WPA);
  ProjectIRDB DB({"./targets/ex.ll"}, IRDBOptions::WPA);
  //    ProjectIRDB DB({"/home/pmueller/Arbeit/code/phasar/targets/cd.ll"},
  //                 IRDBOptions::WPA);
  LOG_IF_ENABLE(BOOST_LOG(lg::get()) << "STARTED")
  //  DB.preprocessIR();
  LLVMTypeHierarchy th(DB);
  LLVMBasedBackwardsICFG cg(DB, CallGraphAnalysisType::DTA, {"parse_command"},
                            &th);
  LLVMPointsToInfo PT(DB);
  std::ifstream in("targets/locations.json");
  json j;
  in >> j;
  auto terms = j.get<vector<Term>>();
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
                      if (l.line == line) {
                        facts.insert(SlicerFact(&l, &i));
                      }
                    }
                  }
                }
              }
            }
            for (auto &t : terms) {
              for (auto &l : t.locations) {
                if (l.line == line && l.column == column && t.file == file) {
                  facts.insert(SlicerFact(&l, &i));
                  if (const auto *branch = dyn_cast<BranchInst>(&i)) {
                    if (branch->isConditional()) {
                      const auto *constbranch = branch->getSuccessor(
                          0); // Assume first branch is true branch
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
  IFDSSLicer<LLVMBasedBackwardsICFG> slicer(&cg, &th, &DB, &PT, sc, &terms);

  IFDSSolver test(slicer);
  test.enableESGAsDot();
  test.solve();
  cout << endl;
  set<std::tuple<std::string, Location, const llvm::Value *>, SliceComparator>
      slice;
  vector<std::string> textSlice;
  for (const auto &module : DB.getAllModules()) {
    for (auto &function : module->functions()) {
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
              cout << "FACT INS: " << llvmIRToString(fact.getInstruction())
                   << endl;
              cout << "SRC: " << *fact.getLocation() << " "
                   << psr::getSrcCodeFromIR(fact.getInstruction()) << endl
                   << endl;
              slice.insert(
                  std::make_tuple(psr::getSrcCodeFromIR(fact.getInstruction()),
                                  *fact.getLocation(), fact.getInstruction()));
              const auto *blockExit =
                  fact.getInstruction()->getParent()->getTerminator();
              const auto instStr = psr::llvmIRToString(blockExit);
              const auto exitSrc = psr::getSrcCodeFromIR(blockExit);
              if (auto *dl = blockExit->getDebugLoc().get()) {
                cout << "ADDING " << llvmIRToString(blockExit) << " " << exitSrc
                     << endl;
                slice.insert(std::make_tuple(
                    exitSrc, *createLocation(dl->getLine(), dl->getColumn()),
                    blockExit));
              } else {
                cerr << "DID NOT FIND LOCATION" << endl;
                slice.insert(
                    std::make_tuple(exitSrc, *createLocation(0, 0), blockExit));
              }
              isUsed = true;
            }
          }
          //          cout << endl;
        }
      }
      if (isUsed) {
        // Check whether this works everywhere
        const auto *entry = &function.getEntryBlock().front();
        if (auto *dl = entry->getDebugLoc().get()) {
          slice.insert(std::make_tuple(
              psr::getSrcCodeFromIR(entry),
              *createLocation(dl->getLine(), dl->getColumn()), entry));
        } else {
          slice.insert(std::make_tuple(psr::getSrcCodeFromIR(entry),
                                       *createLocation(0, 0), entry));
          cout << "GOT NO DEBUG LOG" << endl;
        }
        const auto exits = cg.getStartPointsOf(&function);
        for (const auto *exit : exits) {
          if (auto *dl = exit->getDebugLoc().get()) {
            slice.insert(std::make_tuple(
                psr::getSrcCodeFromIR(exit),
                *createLocation(dl->getLine(), dl->getColumn()), exit));
          } else {
            slice.insert(std::make_tuple(psr::getSrcCodeFromIR(exit),
                                         *createLocation(0, 0), exit));
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
  return 0;
}
// TODO:
// Recursive Control dependency backward - branch conditions
// branch conditions forward -> include block
// Collect Declarations

// What is our slicing Criterion?
// Entry or somewhere?
// Regardless we have to build an SDG

//  vector<Function *> debug_functions;
//  for (auto module : DB.getAllModules()) {
//    auto f = module->getFunction("llvm.dbg.declare");
//    if (f != nullptr) {
//      debug_functions.push_back(f);
//    }
//    f = module->getFunction("llvm.dbg.value");
//    if (f != nullptr) {
//      debug_functions.push_back(f);
//    }
//    f = module->getFunction("llvm.dbg.addr");
//    if (f != nullptr) {
//      debug_functions.push_back(f);
//    }
//  }
//
//  set<const llvm::Instruction *> callers;
//  for (auto debug : debug_functions) {
//    for (auto call : cg.getCallersOf(debug)) {
//      callers.insert(call);
//    }
//  }
//
//  for (auto module : DB.getAllModules()) {
//    for (auto &f : *module) {
//      CFG.getNonTerminationSensitiveControlDependence(f);
//      CFG.getNonTerminationInsensitiveControlDependence(f);
//      CFG.getDecisiveControlDependence(f);
//    }std::cout << "Current path is " << fs::current_path() << '\n';
//  }
//
//  std::set<std::pair<unsigned, MDNode *>> all_metadata;
//  for (auto c : callers) {
//    //        llvm::dbgs()<<"CALL :" << *c << "\n";
//    SmallVector<std::pair<unsigned, MDNode *>, 6> metadata;
//    c->getAllMetadata(metadata);
//
//    for (auto pair : metadata) {
//      all_metadata.insert(pair);
//    }
//  }
//  for (auto pair : all_metadata) {
//    switch (pair.second->getMetadataID()) {
//    case DINode::MetadataKind::DIFileKind:
//      //                cout<< "FILE" <<endl;
//      //                    pair.second->print(llvm::dbgs());
//      break;
//    case DINode::MetadataKind::DILocationKind:
//      //                cout<< "Location" <<endl;
//      //                    pair.second->print(llvm::dbgs());
//      break;
//    case DINode::MetadataKind::DICompositeTypeKind:
//      //                cout<< "Location" <<endl;
//      break;
//    case DINode::MetadataKind::MDTupleKind: {
//      auto *tuple = dyn_cast<MDTuple>(pair.second);
//      //                cout << "Tuple" << endl;
//      break;
//    }
//    default:
//      break;
//    }
//    //        llvm::dbgs() << "META: " << *pair.second << "\n";
//    //        llvm::dbgs() << "META: " << pair.second->getMetadataID() <<
//    "\n";
//    //        llvm::dbgs() << "META: " << pair.second->getNumOperands() <<
//    "\n";
//    //            pair.second->print(llvm::dbgs());
//    //        llvm::dbgs() << "\n";
//    //        for (auto &ops : pair.second->operands()){
//    //            llvm::dbgs() << *ops << "\n";
//    //        }
//  }
//
//  deque<Instruction *> wl;
//  for (auto module : DB.getAllModules()) {
//    for (auto &function : module->functions()) {
//      for (auto &bb : function) {
//        for (auto &i : bb) {
//          SmallVector<std::pair<unsigned, MDNode *>, 6> metadata;
//          //                    i.getAllMetadata(metadata);
//          //                    for (auto& m: metadata){
//          //                        llvm::dbgs() << *m.second << "\n";
//          //                    }
//          auto const &loc = i.getDebugLoc();
//          if (loc) {
//            //            llvm::dbgs() << i << "\n";
//            if (loc.getLine() == 52 || loc.getLine() == 45) {
//              wl.push_back(&i);
//            }
//          }
//        }
//      }
//    }
//  }
//  while (!wl.empty()) {
//    auto i = wl.front();
//    wl.pop_front();
//    llvm::dbgs() << "==================================================\n";
//    llvm::dbgs() << "GOT LINE: "
//                 << "\n";
//    llvm::dbgs() << *i << "\n";
//    llvm::dbgs() << "NAME: " << i->getName() << "\n";
//    llvm::dbgs() << i->getNumUses() << "\n" << i->getNumOperands() << "\n";
//    for (auto *u : i->users()) {
//      llvm::dbgs() << "USER: " << *u << "\n";
//    }
//    for (auto &u : i->uses()) {
//      llvm::dbgs() << "USE: " << *u << "\n";
//    }
//    for (auto &u : i->operands()) {
//      llvm::dbgs() << "OPS: " << *u << "\n";
//    }
//    llvm::dbgs() << "==================================================\n";
//  }
//
//  //        // auto module = DB.getModule("../targets/main_linked.ll");
//  //        for (auto &m : module->named_metadata()) {
//  //            m.print(llvm::dbgs());
//  //            llvm::dbgs() << "\n";
//  //        }
//  //        for (auto f : DB.getAllFunctions()) {
//  //            SmallVector<std::pair<unsigned, MDNode *>, 10> metas;
//  //            f->getAllMetadata(metas);
//  //
//  //            for (auto meta: metas) {
//  //                meta.second->print(llvm::dbgs());
//  //                llvm::dbgs() << "\n";
//  //            }
//  //            for (auto m : DB.getAllModules()) {
//  //
//  //                for (auto &meta : m->getNamedMDList()) {
//  //
//  //                    meta.print(llvm::dbgs());
//  //                    for (auto op: meta.operands()){
//  //                        op->print(llvm::dbgs());
//  //                    }
//  //                }
//  //            }cout << terms;
//  //        }
//  // for (auto &bb : *f){
//  // llvm::dbgs() << bb << "\n";
//  // for (auto &i : bb){
//  // llvm::dbgs() << i <<" " << i.getOpcode() <<"  " << i.getOpcodeName()<<
//  // "\n\n"; if (auto *call = dyn_cast<CallInst>(&ia)) { llvm::dbgs() <<
//  // call->getNumArgOperands() << "\n"; if (call->getNumArgOperands() > 1) {
//  // auto op = call->getArgOperand(1);
//  // llvm::dbgs()<< "ID:" << op->getName() << "\n";
//  // llvm::dbgs() << *op << "\n";
//  // }
//  // }
//  // }
//  // }
//
//  //    }