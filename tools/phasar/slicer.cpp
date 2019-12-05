#include <boost/filesystem/operations.hpp>

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

#include <phasar/DB/ProjectIRDB.h>
#include <phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h>
#include <phasar/PhasarLLVM/IfdsIde/LLVMDefaultIFDSTabulationProblem.h>
#include <phasar/PhasarLLVM/IfdsIde/Solver/LLVMIFDSSolver.h>
#include <phasar/PhasarLLVM/Pointer/LLVMTypeHierarchy.h>
#include <phasar/Utils/LLVMIRToSrc.h>
#include <phasar/Utils/Logger.h>

#include <deque>
#include <map>

#include <fstream>
#include <iostream>
#include <utility>
#include <vector>
namespace bpo = boost::program_options;
namespace bofs = boost::filesystem;
using namespace llvm;
using namespace std;
using namespace psr;

class SlicerFact{
public:
  SlicerFact() = default;

  bool operator <(const SlicerFact& rhs) const {
    return i < rhs.i;
  }
  bool operator ==(const SlicerFact& rhs) const{
    return  i == rhs.i;
  }

  friend std::ostream& operator<<(std::ostream& os, const SlicerFact& rhs);
  bool isZero() const { return  i == NULL;  }
private:
  Instruction* i = NULL;
};

std::ostream& operator<<(std::ostream& os, const SlicerFact& rhs) {
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
  size_t operator()(const SlicerFact & x) const{
    return 0;
  }
};
}

class NormalFlowFunction : public FlowFunction<SlicerFact> {
public:

  NormalFlowFunction(const Instruction *curr,
                     const Instruction *succ): curr(curr), succ(succ) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) {
    set<SlicerFact> facts;
    facts.insert(source);
    return facts;
  }


private:
  const Instruction *curr;
  const Instruction *succ;
};

class CallFlowFunction : public FlowFunction<SlicerFact> {
public:
  CallFlowFunction(const Instruction *callStmt,
                   const Function *destMthd):
                   callStmt(callStmt),
                   destMthd(destMthd) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) {
    std::cout << "Calling " << destMthd->getName().str() << std::endl;
    set<SlicerFact> facts;
    facts.insert(source);
    return facts;
  }

private:
  const Instruction *callStmt;
  const Function *destMthd;
};

class RetFlowFunction : public FlowFunction<SlicerFact> {
public:
  RetFlowFunction(const Instruction *callSite,
      const Function *calleeMthd,
                  const Instruction *exitStmt,
                  const Instruction *retSite):
                  callSite(callSite), calleeMthd(calleeMthd),
                  exitStmt(exitStmt), retSite(retSite)
                  {
  }
  std::set<SlicerFact> computeTargets(SlicerFact source) {
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
  CallToRetFlowFunction(const Instruction *callSite,
                        const Instruction *retSite,
                        set<const Function*> callees):
                        callSite(callSite),
                        retSite(retSite),
                        callees(std::move(callees)) {}

  std::set<SlicerFact> computeTargets(SlicerFact source) {
    set<SlicerFact> facts;
    facts.insert(source);
    return facts;
  }
private:
  const Instruction *callSite;
  const Instruction *retSite;
  set<const Function*> callees;
};


class IFDSSLicer : public LLVMDefaultIFDSTabulationProblem<SlicerFact, LLVMBasedICFG &> {
public:
  IFDSSLicer(LLVMBasedICFG &icfg, const LLVMTypeHierarchy &th,
             const ProjectIRDB &irdb, const std::set<const llvm::Value *> sc)
      : LLVMDefaultIFDSTabulationProblem(icfg, th, irdb), slicingCriteria(sc) {}
private:
  SlicerFact createZeroValue() override { return SlicerFact(); }
  shared_ptr<FlowFunction<SlicerFact>>
  getNormalFlowFunction(const Instruction *curr,
                        const Instruction *succ) override {
    if (slicingCriteria.find(curr) != slicingCriteria.end()){

    }
    if (auto debuglog = curr->getDebugLoc().get()) {

      auto line = debuglog->getLine();
      auto column = debuglog->getColumn();
      auto file = debuglog->getFilename();
      debuglog->dump();

    }
    return make_shared<NormalFlowFunction>(curr,succ);
  }
  shared_ptr<FlowFunction<SlicerFact>>
  getCallFlowFunction(const Instruction *callStmt,
                      const Function *destMthd) override {
    if (auto debuglog = callStmt->getDebugLoc().get()) {

      auto line = debuglog->getLine();
      auto column = debuglog->getColumn();
      auto file = debuglog->getFilename();
      debuglog->dump();

    }
    return make_shared<CallFlowFunction>(callStmt,destMthd);
  }
  shared_ptr<FlowFunction<SlicerFact>>
  getRetFlowFunction(const Instruction *callSite, const Function *calleeMthd,
                     const Instruction *exitStmt,
                     const Instruction *retSite) override {
    return make_shared<RetFlowFunction>(callSite,calleeMthd,exitStmt,retSite);
  }
  shared_ptr<FlowFunction<SlicerFact>>
  getCallToRetFlowFunction(const Instruction *callSite,
                           const Instruction *retSite,
                           set<const Function*> callees) override {
    return make_shared<CallToRetFlowFunction>(callSite,retSite,callees);
  }
  map<const Instruction* , set<SlicerFact>> initialSeeds() override {
    map<const Instruction*, set<SlicerFact>> res;
    set<SlicerFact> facts;
    const Instruction* i = &icfg.getMethod("main")->front().front();
    SlicerFact sf = SlicerFact();
    facts.insert(sf);
    res[i] = facts;
    return res;
  }
  bool isZeroValue(SlicerFact d) const override { return d.isZero(); }
  void printNode(std::ostream &os, const Instruction *n) const override {
    std::string temp;
    llvm::raw_string_ostream rso(temp);
    n->print(rso,true);
    os << temp;
  }
  void printDataFlowFact(std::ostream &os, SlicerFact d) const override {
    os << d;
  }
  void printMethod(std::ostream &os, const Function *m) const override {}

private:
  std::set<const llvm::Value*> slicingCriteria;
};

int main(int argc, const char **argv) {
  initializeLogger(true);
  //  ProjectIRDB DB({argv[1]}, IRDBOptions::WPA);
  ProjectIRDB DB({"/home/pmueller/Arbeit/code/phasar/targets/toSlice.ll"}, IRDBOptions::WPA);
//    ProjectIRDB DB({"/home/pmueller/Arbeit/code/phasar/targets/cd.ll"},
//                 IRDBOptions::WPA);
  LOG_IF_ENABLE(BOOST_LOG(lg::get()) << "STARTED")
  DB.preprocessIR();
  LLVMTypeHierarchy th(DB);
  LLVMBasedCFG CFG;
  LLVMBasedICFG cg(th, DB, CallGraphAnalysisType::DTA);
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
//    }
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
//    //        llvm::dbgs() << "META: " << pair.second->getMetadataID() << "\n";
//    //        llvm::dbgs() << "META: " << pair.second->getNumOperands() << "\n";
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
//  //            }
//  //        }
//  // for (auto &bb : *f){
//  // llvm::dbgs() << bb << "\n";
//  // for (auto &i : bb){
//  // llvm::dbgs() << i <<" " << i.getOpcode() <<"  " << i.getOpcodeName()<<
//  // "\n\n"; if (auto *call = dyn_cast<CallInst>(&i)) { llvm::dbgs() <<
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
  std::set<const llvm::Value*> sc;
  IFDSSLicer slicer(cg,th,DB,sc);
  LLVMIFDSSolver<SlicerFact, LLVMBasedICFG &> test(slicer);
  test.solve();

  return 0;
}

// What is our slicing Criterion?
// Entry or somewhere?
// Regardless we have to build an SDG
