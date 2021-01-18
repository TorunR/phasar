#include "slicer.h"
#include <llvm/IR/IntrinsicInst.h>
#include <sys/resource.h>
#define __INTERPROCEDURAL__

struct SliceComparator {
  template <typename T>
  bool operator()(const tuple<string, Location, T> &l,
                  const tuple<string, Location, T> &r) const {
    return std::get<1>(l) < std::get<1>(r) || std::get<0>(l) < std::get<0>(r);
  }
};

template <typename ICFG_T>
class IFDSSlicer : public IFDSTabulationProblem<SlicerAnalysisDomain<ICFG_T>> {
public:
  IFDSSlicer(ICFG_T *icfg, const LLVMTypeHierarchy *th, const ProjectIRDB *irdb,
             LLVMPointsToInfo *PT, map<const Instruction *, set<SlicerFact>> sc,
             const std::vector<Term> *terms,
             const std::set<std::string> &entrypoints)
      : IFDSTabulationProblem<SlicerAnalysisDomain<ICFG_T>>(irdb, th, icfg, PT),
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
    return make_shared<CallFlowFunction<ICFG_T>>(callStmt, destMthd,
                                                 this->getICFG());
  }
  shared_ptr<FlowFunction<SlicerFact>>
  getRetFlowFunction(const Instruction *callSite, const Function *calleeMthd,
                     const Instruction *exitStmt,
                     const Instruction *retSite) override {
    return make_shared<RetFlowFunction<ICFG_T>>(callSite, calleeMthd, exitStmt,
                                                retSite);
  }
  shared_ptr<FlowFunction<SlicerFact>>
  getCallToRetFlowFunction(const Instruction *callSite,
                           const Instruction *retSite,
                           set<const Function *> callees) override {
    return make_shared<CallToRetFlowFunction>(callSite, retSite, callees);
  }
  map<const Instruction *, set<SlicerFact>> initialSeeds() override {
    for (const auto &entrypoint : entrypoints) {
      const Instruction *i;
      if (std::is_same<ICFG_T, LLVMBasedBackwardsICFG>::value) {
        i = &this->ICF->getFunction(entrypoint)->back().back();
      } else if (std::is_same<ICFG_T, LLVMBasedICFG>::value) {
        i = &this->ICF->getFunction(entrypoint)->front().front();
      } else {
        assert(false && "Only defined for LLVMBASED ICFGs");
      }
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
  void printFunction(std::ostream &os, const Function *m) const override {
    os << psr::llvmIRToString(m) << endl;
  }

  shared_ptr<FlowFunction<SlicerFact>>
  getSummaryFlowFunction([[maybe_unused]] const Instruction *curr,
                         [[maybe_unused]] const Function *destFun) override {
    return nullptr;
  }

  const std::vector<Term> *terms;
  map<const Instruction *, set<SlicerFact>> slicingCriteria;
  const std::set<string> &entrypoints;
};

void copy_files(map<string, set<unsigned int>> &file_lines) {
  for (const auto &file : file_lines) {
    std::ifstream in(file.first);
    std::ofstream out(
        "out/" + file.first.substr(file.first.find_last_of("/"), string::npos));
    std::string line;
    int line_nr = 1;
    while (std::getline(in, line)) {
      if (file.second.find(line_nr) != file.second.end()) {
        out << line << endl;
      } else {
        out << endl;
      }
      line_nr++;
    }
    out.close();
  }
}

template <typename AnalysisDomainTy>
void process_results(ProjectIRDB &DB, IFDSSolver<AnalysisDomainTy> &solver,
                     LLVMBasedBackwardsICFG &cg) {
  map<const Function *, set<const llvm::Value *>> slice_instruction;
  llvm::dbgs() << "SOLVING DONE\n";
  for (auto *const module : DB.getAllModules()) {
    for (auto &function : module->functions()) {
      llvm::dbgs() << "\n\n\n" << function.getName() << "\n\n\n";
      bool isUsed = false;
      for (auto &bb : function) {
        for (const auto &i : bb) {
          llvm::dbgs()
              << "========================================================\n";
          auto res = solver.ifdsResultsAt(&i);
          llvm::dbgs() << "INS: " << llvmIRToString(&i)
                       << " FACTS:" << res.size() << "\n";
          llvm::dbgs() << "SRC: " << psr::getSrcCodeFromIR(&i) << "\n"
                       << res.size() << "\n";
          for (auto fact : res) {
            if (!fact.isZero()) {
              auto extractedInstruction = fact.getInstruction();
              llvm::dbgs() << "FACT INS: "
                           << llvmIRToString(fact.getInstruction()) << "\n";
              llvm::dbgs() << "SRC: " << *fact.getLocation() << " "
                           << psr::getSrcCodeFromIR(extractedInstruction)
                           << "\n"
                           << "\n";
              //              if (extractedInstruction-> == function &&
              //              extractedInstruction == &i || true) {

              const auto ins = dyn_cast<Instruction>(fact.getInstruction());

              if (ins && ins->getFunction() == &function) {
                slice_instruction[&function].insert(extractedInstruction);
                const auto *blockExit = ins->getParent()->getTerminator();
                slice_instruction[&function].insert(blockExit);
                isUsed = true;
              }
              //              else {
              //                if (auto arg =
              //                dyn_cast<Argument>(fact.getInstruction())) {
              //                  slice_instruction[&function].insert(arg);
              //                }
              //              }
            } else {
            }
          }
        }
      }
      if (isUsed) {
        // Check whether this works everywhere
        const auto *entry = &function.getEntryBlock().front();
        const auto source = psr::getSrcCodeFromIR(&function);
        for (auto &bb : function) {
          for (auto &ins : bb) {
            if (auto *dl = ins.getDebugLoc().get()) {
              //              slice.insert(std::make_tuple(
              //                  source, *createLocation(dl->getLine(),
              //                  dl->getColumn()), entry));
              goto added; // we found the first instruction with debug info
            }
          }
        }
        //        slices[&function].insert(
        //            std::make_tuple(source, *createLocation(0, 0), entry));
      added:
        const auto exits = cg.getStartPointsOf(&function);
        for (const auto *exit : exits) {
          if (auto *dl = exit->getDebugLoc().get()) {
            //            slices[&function].insert(std::make_tuple(
            //                psr::getSrcCodeFromIR(exit),
            //                *createLocation(dl->getLine(), dl->getColumn()),
            //                exit));
          } else {
            //            slices[&function].insert(
            //                std::make_tuple(psr::getSrcCodeFromIR(exit),
            //                                *createLocation(INT_MAX, INT_MAX),
            //                                exit));
            llvm::dbgs() << "GOT NO DEBUG LOG"
                         << "\n";
          }
        }
      }
    }
  }
  cout << "\n";
  llvm::dbgs() << "\n"
               << "\n"
               << "\n"
               << "\n";
  map<std::string, set<unsigned int>> file_lines;
  for (auto &p : slice_instruction) {
    llvm::dbgs() << p.first->getName() << "\t" << p.second.size() << "\n";
    auto file = psr::getFilePathFromIR(p.first);
    auto first = true;
    //    file_lines[file]
    //    file_lines[file] = set<unsigned int> ();
    for (const auto &s : p.second) {
      if (first) {
        auto line = psr::getLineFromIR(p.first);
        llvm::dbgs() << psr::getSrcCodeFromIR(p.first) << "\t" << line << "\n";
        file_lines[file].insert(line);
        first = false;
      }

      if (auto phi = dyn_cast<PHINode>(s)) {
        llvm::dbgs() << "GOT PHI\t" << *s << "\n";
      } else if (auto debug_call = dyn_cast<DbgValueInst>(s)) {
        if (auto debug_info =
                dyn_cast<MetadataAsValue>(debug_call->getOperand(1))) {
          if (auto localVariable =
                  dyn_cast<DILocalVariable>(debug_info->getMetadata())) {
            file_lines[file].insert(localVariable->getLine());
          }
        }

      } else {
        auto line = psr::getLineFromIR(s);
        auto src = psr::getSrcCodeFromIR(s);
        llvm::dbgs() << *s << "\t" << src << "\t" << line << "\n";
        file_lines[file].insert(line);
        if (auto inst = dyn_cast<Instruction>(s)) {
          if (inst->getDebugLoc()) {
            if (auto scope =
                    dyn_cast<DILexicalBlock>(inst->getDebugLoc().getScope())) {
              file_lines[file].insert(scope->getLine());
            }
          }
          if (auto br = dyn_cast<BranchInst>(inst)) {
            if (br->getNumOperands() == 1 && src.find("}") == src.npos) {
              file_lines[file].insert(line + 1);
            }
          }
          if (auto ret = dyn_cast<ReturnInst>(inst)) {
            if (src.find("}") == src.npos) {
              file_lines[file].insert(line + 1);
            }
          }
        }
      }
    }
  }
  copy_files(file_lines);
}

std::string createSlice(string target, const set<string> &entrypoints,
                        const vector<Term> &terms) {
  ProjectIRDB DB({std::move(target)}, IRDBOptions::WPA);
  initializeLogger(false);
  LLVMTypeHierarchy th(DB);
  LLVMBasedBackwardsICFG cg(DB, CallGraphAnalysisType::DTA, set(entrypoints),
                            &th);
  LLVMBasedICFG fcg(DB, CallGraphAnalysisType::DTA, set(entrypoints), &th);
  LLVMPointsToGraph PT(DB);
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
            for (const auto &t : terms) {
              for (const auto &l : t.locations) {
                // check same statement
                if (l.line == line && t.file == file) {
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
  IFDSSlicer<LLVMBasedBackwardsICFG> slicer(&cg, &th, &DB, &PT, sc, &terms,
                                            entrypoints);
  IFDSSlicer<LLVMBasedICFG> slicer2(&fcg, &th, &DB, &PT, sc, &terms,
                                    entrypoints);
  IFDSSolver solver(slicer);
  solver.solve();
  ofstream out;
  out.open("out/graph.dot");
  solver.emitESGAsDot(out);
  out.close();
  out.open("out/results.txt");
  solver.dumpResults(out);
  out.close();
  cout << "\n";
  process_results(DB, solver, cg);
  return "";
}

int main(int argc, const char **argv) {
  const rlim_t kStackSize = 128 * 1024 * 1024; // min stack size = 128 MB
  struct rlimit rl;
  int result;
  result = getrlimit(RLIMIT_STACK, &rl);
  if (result == 0) {
    if (rl.rlim_cur < kStackSize) {
      rl.rlim_cur = kStackSize;
      result = setrlimit(RLIMIT_STACK, &rl);
      if (result != 0) {
        fprintf(stderr, "setrlimit returned result = %d\n", result);
      }
    }
  }
  if (argc < 3) {
    cout << "Please provide the correct params" << endl;
    // TODO USAGE
    return -1;
  }
  std::cout << "Current path is " << bofs::current_path() << '\n';
  const auto target = argv[1];
  //  const auto target = "./targets/w_defects.ll";
  //    const auto target = "./targets/main_linked.ll";
  //    const auto target = "./targets/ex.ll";
  //    const auto target =
  //    "/media/Volume/Arbeit/Arbeit/code/slicing-eval/memcached/memcached.ll";
  //  const auto target = "./targets/toSlice.ll";
  //  const auto target = "./targets/min_ex_ssa.ll";
  //  const auto target = "./targets/parson_linked.ll";
  //  const auto target =
  //  "/media/Volume/Arbeit/Arbeit/code/slicing-eval/git/git-linked.ll";
  //  const auto entryfunction = "main";
  //    const auto entryfunction = "parse_command";
  //  initializeLogger(true);
  //    std::ifstream  in("targets/locations_json.json");
  //    std::ifstream in("targets/locations_memcached_command.json");
  //    std::ifstream in("targets/locations_memcached_single.json");
  //  std::ifstream in("targets/toSlice.json");
  //  std::ifstream in("targets/min_ex.json");
  //  std::ifstream  in("targets/parson_serialize.json");
  std::ifstream in(argv[2]);
  json j;
  in >> j;
  auto terms = j.get<vector<Term>>();
  set<std::string> entrypoints;
  for (int i = 3; i < argc; ++i) {
    entrypoints.insert(argv[i]);
    cout << argv[i] << endl;
  }
  std::chrono::steady_clock::time_point begin =
      std::chrono::steady_clock::now();
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
  //  compare_slice(
  //  "/home/torun/Volume/Arbeit/Arbeit/code/slicing-eval/smaller/the_silver_searcher/src/ignore.c",
  //      "/media/Volume/Arbeit/Arbeit/code/phasar/out/ignore.c");
    compare_slice(
        "/media/Volume/Arbeit/Arbeit/code/slicing-eval/smaller/parson/parse.c",
        "/media/Volume/Arbeit/Arbeit/code/phasar/out/parson.c");
  //  compare_slice(
  //      "/media/Volume/Arbeit/Arbeit/code/slicing-eval/smaller/parson/serialize.c",
  //      "/media/Volume/Arbeit/Arbeit/code/phasar/out/parson.c");
  //    compare_slice("/media/Volume/Arbeit/Arbeit/code/slicing-eval/smaller/inotify-tools/libinotifytools/src/stats.c",
  //                  "/media/Volume/Arbeit/Arbeit/code/phasar/out/inotifytools.c");
  //    compare_slice("/media/Volume/Arbeit/Arbeit/code/slicing-eval/smaller/fping/src/stats.c",
  //                  "/media/Volume/Arbeit/Arbeit/code/phasar/out/fping.c");
  return 0;
}
// TODO:
// Recursive Control dependency backward - branch conditions
// branch conditions forward -> include block
// Coll Declarations

std::set<SlicerFact> NormalFlowFunction::computeTargets(SlicerFact source) {
  set<SlicerFact> facts;

  // check logic for conditions

  if (!source.isZero()) {
    facts.insert(source);
            llvm::dbgs() << "===================\n";
            llvm::dbgs() << "curr: " << *curr << "\n";
            llvm::dbgs() << "Source: " << *source.getInstruction() << "\n";
            llvm::dbgs() << curr->getFunction()->getName() << "\n";
    auto fun = curr->getFunction()->getName();
    if (fun == "bar" && isa<GetElementPtrInst>(curr)) {
      llvm::dbgs() << "";
    }
    if (auto load = dyn_cast<LoadInst>(curr)) {
      llvm::dbgs() << "";
    }
    //        if (auto gep = dyn_cast<GetElementPtrInst>(curr)) {
    //          llvm::dbgs() << "GEP";
    //        }
    //    if (auto store  = dyn_cast<StoreInst>(curr)) {
    //      llvm::dbgs() << "Store";
    //    }
    for (const auto &user : curr->users()) {
      //                            llvm::dbgs() << "USE" << *user << "\n";
      if (user == source.getInstruction()) {
        //                                    llvm::dbgs() << "RELEVANT USE" <<
        //                                    "\n";
        facts.insert(SlicerFact(source.getLocation(), curr));
      }
    }
    if (auto gep = dyn_cast<GetElementPtrInst>(curr)){
      if (auto ins = dyn_cast<Instruction>(source.getInstruction())) {
        for (unsigned int i = 0; i < gep->getNumOperands(); ++i) {
          for (unsigned int j = 0; j < ins->getNumOperands(); ++j) {
            if (gep->getOperand(i) == ins->getOperand(j)) {
              facts.insert(SlicerFact(source.getLocation(),gep));
            }
          }
        }
      }
    }
    if (const auto *br = dyn_cast<BranchInst>(curr)) {
      for (unsigned int i = 0; i < br->getNumOperands(); ++i) {
        if (auto *target = dyn_cast<BasicBlock>(br->getOperand(i))) {
          if (source.getInstruction() == &target->front()) {
            facts.insert(SlicerFact(source.getLocation(), curr));
            // This is the phi instruction after the branch if we are the target
            // we are relevant
          }
        }
      }
    }
  }
  return facts;
}
std::set<SlicerFact> CallToRetFlowFunction::computeTargets(SlicerFact source) {
  set<SlicerFact> facts;
  if (!source.isZero() && source.getInstruction() != callSite) {
    llvm::dbgs() << "===================\n";
    llvm::dbgs() << "curr: " << *callSite << "\n";
    llvm::dbgs() << "Source: " << *source.getInstruction() << "\n";
    llvm::dbgs() << callSite->getFunction()->getName() << "\n";
    auto fun = callSite->getFunction()->getName();
    if (fun == "bar" && isa<AddOperator>(source.getInstruction())) {
      llvm::dbgs() << "";
    }
    if (auto ins = dyn_cast<Instruction>(source.getInstruction())) {
      for (unsigned int j = 0; j < ins->getNumOperands(); ++j) {
        auto op2 = ins->getOperand(j);
        for (unsigned int i = 0; i < callSite->getNumOperands(); ++i) {
          auto op1 = callSite->getOperand(i);
          if (auto metadata = dyn_cast<MetadataAsValue>(op1)) {
            if (auto local =
                    dyn_cast<ValueAsMetadata>(metadata->getMetadata())) {
              if (local->getValue() == ins->getOperand(j)) {
                facts.insert(SlicerFact(source.getLocation(), callSite));
              }
            }
          }
          if (callSite->getOperand(i) == ins->getOperand(j)) {
            facts.insert(SlicerFact(source.getLocation(), callSite));
          }
        }
      }
    }
    for (const auto &user : callSite->users()) {
      if (user == source.getInstruction()) {
        //                                    llvm::dbgs() << "RELEVANT USE" <<
        //                                    "\n";
        facts.insert(SlicerFact(source.getLocation(), callSite));
      }
    }
    facts.insert(source);
  }
  return facts;
}
template <typename ICFG_T>
std::set<SlicerFact>
CallFlowFunction<ICFG_T>::computeTargets(SlicerFact source) {
  set<SlicerFact> facts;
//    facts.insert(source);
#ifdef __INTERPROCEDURAL__
  if (!source.isZero()) {
    auto targets = ICF->getStartPointsOf(destMthd);
    for (auto target : targets) {
      facts.insert(SlicerFact(source.getLocation(), target));
    }
  }
#endif
  return facts;
}
template <typename ICFG_T>
std::set<SlicerFact>
RetFlowFunction<ICFG_T>::computeTargets(SlicerFact source) {
  //    llvm::dbgs() << "\n";
  //    callSite->dump();
  //    calleeMthd->dump();
  //    exitStmt->dump();
  //    retSite->dump();
  //    llvm::dbgs() << "\n";
  set<SlicerFact> facts;
//    facts.insert(source);
#ifdef __INTERPROCEDURAL__
  //    for (unsigned int i = 0; i < callSite->getNumOperands(); ++i) {
//      const auto op = callSite->getOperand(i);
//      if (source.getInstruction() == op) {
//        facts.insert(SlicerFact(source.getLocation(),
//                                getNthFunctionArgument(calleeMthd, i)));
//      }
//    }
//    if (!source.isZero()) {
//      facts.insert(SlicerFact(source.getLocation(), exitStmt));
//    }
#endif
  return facts;
}

void compare_slice(string original, string module) {
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
  cout << "Intersection Size is\t:" << intersection.size() << endl;
  cout << "Additional Size is:\t" << additional.size() << endl;
  cout << "Missing Size is:\t" << missing.size() << endl;
  cout << "\n\n\n";
  for (auto m : missing) {
    cout << m << "\n";
  }
}

// TODO Check for opening brace