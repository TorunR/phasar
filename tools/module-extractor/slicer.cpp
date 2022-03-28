#include "slicer.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToSet.h"
#include <llvm/IR/IntrinsicInst.h>
#include <sys/resource.h>

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


  InitialSeeds<const Instruction*, SlicerFact, BinaryDomain>
  initialSeeds() override {
//    map<const Instruction *, set<SlicerFact>>
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

void copy_files(map<string, set<unsigned int>> &file_lines, string outPath) {
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
  if (true) {
    std::ofstream out("out/" + outPath + ".c");
    for (const auto &file : file_lines) {
      std::ifstream in("out/" + file.first.substr(file.first.find_last_of("/"),
                                                  string::npos));
      std::string line;
      bool last_empty = false;
      while (std::getline(in, line)) {
        if (line.empty()) {
          if (!last_empty) {
            out << endl;
          }
          last_empty = true;
        } else {
          out << line << endl;
          last_empty = false;
        }
      }
    }
    out.close();
  }
}

template <typename AnalysisDomainTy>
void process_results(ProjectIRDB &DB, IFDSSolver<AnalysisDomainTy> &solver,
                     LLVMBasedBackwardsICFG &cg, string outPath) {
  map<const Function *, set<const llvm::Value *>> slice_instruction;
  llvm::dbgs() << "SOLVING DONE\n";
  for (auto *const module : DB.getAllModules()) {
    for (auto &function : module->functions()) {
      auto fun_name = function.getName();
      llvm::dbgs() << "\n\n\n" << fun_name << "\n\n\n";
      bool isUsed = false;
      for (auto &bb : function) {
        for (const auto &i : bb) {
          //          llvm::dbgs()
          //              <<
          //              "========================================================\n";
          auto res = solver.ifdsResultsAt(&i);
          //          llvm::dbgs() << "INS: " << llvmIRToString(&i)
          //                       << " FACTS:" << res.size() << "\n";
          //          llvm::dbgs() << "SRC: " << psr::getSrcCodeFromIR(&i) <<
          //          "\n"
          //                       << res.size() << "\n";
          for (auto fact : res) {
            if (!fact.isZero()) {
              auto extractedInstruction = fact.getInstruction();
//                            llvm::dbgs() << "FACT INS: "
//                                         <<
//                                         llvmIRToString(fact.getInstruction())
//                                         << "\n"
//                                        << llvmIRToString(&i)
//                                          << "\n";
//                            llvm::dbgs() << "SRC: " << *fact.getLocation() <<
//                            " "
//                                         <<
//                                         psr::getSrcCodeFromIR(extractedInstruction)
//                                         << "\n"
//                                         << "\n";
              //              if (extractedInstruction-> == function &&
              //              extractedInstruction == &i || true) {

              const Instruction* ins = dyn_cast<Instruction>(fact.getInstruction());
              const auto parent =ins->getParent();

              if (ins && parent && ins->getFunction()
                             == &function) {
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
            //            llvm::dbgs() << "GOT NO DEBUG LOG"
            //                         << "\n";
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
    llvm::dbgs() << "F:\t" << p.first->getName() << "\t" << p.second.size()
                 << "\n";
    auto file = psr::getFilePathFromIR(p.first);
    auto first = true;

    std::set<unsigned int> block_lines;
    for (const auto &s : p.second) {
      if (first) {
        auto line = psr::getLineFromIR(p.first);
        llvm::dbgs() << psr::getSrcCodeFromIR(p.first) << "\t" << line << "\n";
        auto end_line = psr::getFunctionHeaderLines(p.first);
        for (unsigned int l = line; l <= end_line; ++l) {
          file_lines[file].insert(l);
        }
        first = false;
      }

      if (auto *phi = dyn_cast<PHINode>(s)) {
        llvm::dbgs() << "GOT PHI\t" << *s << "\n";
      } else if (auto *debug_call = dyn_cast<DbgValueInst>(s)) {
        if (auto *debug_info =
                dyn_cast<MetadataAsValue>(debug_call->getOperand(1))) {
          if (auto *localVariable =
                  dyn_cast<DILocalVariable>(debug_info->getMetadata())) {
            file_lines[file].insert(localVariable->getLine());
          }
        }
      } else {
        auto line = psr::getLineFromIR(s);
        auto src = psr::getSrcCodeFromIR(s);

        llvm::dbgs() << *s << "\t" << src << "\t" << line << "\n";
        file_lines[file].insert(line);
        if (auto *inst = dyn_cast<Instruction>(s)) {
          for (unsigned int i = 0; i < inst->getNumOperands(); ++i) {
            if (auto *global = dyn_cast<GlobalVariable>(inst->getOperand(i))) {
              auto g_line = psr::getLineFromIR(global);
              auto g_src = psr::getSrcCodeFromIR(global);
              file_lines[file].insert(g_line);
            }
          }
          if (inst->getDebugLoc()) {
            if (auto *scope =
                    dyn_cast<DILexicalBlock>(inst->getDebugLoc().getScope())) {
              block_lines.insert(line);
              file_lines[file].insert(scope->getLine());
            }
          }
          if (auto *br = dyn_cast<BranchInst>(inst)) {
            if (br->isUnconditional() && src.find('}') == src.npos &&
                line != 0) {
              file_lines[file].insert(line + 1);
            }
          }
          if (auto *ret = dyn_cast<ReturnInst>(inst)) {
            if (src.find('}') == src.npos && line != 0) {
              file_lines[file].insert(line + 1);
            }
          }
        }
      }
    }
    auto lines = add_block(file, &block_lines);
    for (auto line : *lines) {
      file_lines[file].insert(line);
    }
  }
  copy_files(file_lines, outPath);
}

std::string createSlice(string target, const set<string> &entrypoints,
                        const vector<Term> &terms,string outPath) {
  ProjectIRDB DB({std::move(target)}, IRDBOptions::WPA);
  initializeLogger(false);
  //    initializeLogger(true);
  LLVMTypeHierarchy th(DB);
  LLVMPointsToSet pt(DB);
  LLVMBasedICFG fcg(DB,psr::CallGraphAnalysisType::CHA,set(entrypoints),&th,&pt,Soundness::Soundy,true);
  LLVMBasedBackwardsICFG cg(fcg);
  LLVMPointsToGraph PT(DB);
  map<const Instruction *, set<SlicerFact>> sc;
  for (auto *module : DB.getAllModules()) {
    for (auto &function : module->functions()) {
      for (auto &bb : function) {
        auto sub = function.getSubprogram();
        for (const auto &i : bb) {
          set<SlicerFact> facts;
          if (const auto call = dyn_cast<CallInst>(&i)) {
          if (call->getCalledFunction()->getName() == "__mark_location") {
            llvm::dbgs() << llvmIRToString(call) << "\n";
            llvm::dbgs() << call->getCalledFunction()->getName() << "\n";
              for (auto& op: call->operands()) {

                llvm::dbgs() << llvmIRToString(op.get()) << "\n";


                if (auto* gep = dyn_cast<ConstantExpr>(op.get())) {
                  llvm::dbgs() << llvmIRToString(gep->getOperand(0))  << "\n";
                  if (auto* gv = dyn_cast<GlobalVariable> (gep->getOperand(0))) {
                    if (auto* cs  =dyn_cast<ConstantDataArray>(gv->getInitializer())) {
                      llvm::dbgs() << cs->getAsCString() << "\n";
                    }
                  } else {
                    llvm::dbgs() <<"Not STRING" << "\n";
                  }
                  llvm::dbgs() << gep << "\t" << gep->isGEPWithNoNotionalOverIndexing() << "\n";
                }
              }
              llvm::dbgs() << call << "\n";
            }
          }
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
              const size_t last_slash_idx = t.file.find_last_of("\\/");
              std::string term_file_name(t.file);
              if (std::string::npos != last_slash_idx) {
                term_file_name.erase(0, last_slash_idx + 1);
              }
              for (const auto &l : t.locations) {
                // check same statement
                if (file.endswith(term_file_name) &&
                    (l.line == line || l.line == sub->getLine())) {
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
//  out.open("out/graph.dot");
  //  solver.emitESGAsDot(out);
  out.close();
//  out.open("out/results.txt");
  //  solver.dumpResults(out);
  out.close();
  cout << "\n";
  process_results(DB, solver, cg, outPath);
  return "";
}

int main(int argc, const char **argv) {
  const rlim_t kStackSize = 512 * 1024 * 1024; // min stack size = 128 MB
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
  if (argc < 5) {
    cout << "Please provide the correct params" << endl;
    // TODO USAGE
    return -1;
  }
//  std::cout << "Current path is " << bofs::current_path() << '\n';
  const auto target = argv[1];
  std::ifstream in(argv[2]);
  json j;
  in >> j;
  auto terms = j.get<vector<Term>>();
  string outPath = argv[3];
  set<std::string> entrypoints;
  for (int i = 4; i < argc; ++i) {
    entrypoints.insert(argv[i]);
    cout << argv[i] << endl;
  }
  std::chrono::steady_clock::time_point begin =
      std::chrono::steady_clock::now();
  createSlice(target, entrypoints, terms,outPath);
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
  std::cout << "Extracted code is in out/" + outPath + ".c" << std::endl;
  return 0;
}


std::set<SlicerFact> NormalFlowFunction::computeTargets(SlicerFact source) {
  set<SlicerFact> facts;

  // check logic for conditions

  if (!source.isZero() && source.is_within_limits()) {
    if (auto branch = dyn_cast<BranchInst>(source.getInstruction())) {
      if (!branch->isUnconditional()) {
        facts.insert(source);
      }
    } else if (auto call =
                   dyn_cast<DbgInfoIntrinsic>(source.getInstruction())) {
      // Don't propagate further
    } else {
      facts.insert(source);
    }
    //            llvm::dbgs() << "===================\n";
    //            llvm::dbgs() << "curr: " << *curr << "\n";
    //            llvm::dbgs() << "Source: " << *source.getInstruction() <<
    //            "\n"; llvm::dbgs() << "src: " << psr::getSrcCodeFromIR(curr)
    //            << "\n"; llvm::dbgs() << curr->getFunction()->getName() <<
    //            "\n";
    auto fun = curr->getFunction()->getName();

    for (const auto &user : curr->users()) {
      if (user == source.getInstruction()) {
        facts.insert(
            SlicerFact(source.getLocation(), curr, source.getInterDistance()));
      }
    }
#ifdef OPERAND_PROP
    if (auto load = dyn_cast<LoadInst>(curr)) {
      if (auto ins = dyn_cast<Instruction>(source.getInstruction())) {
        for (unsigned int i = 0; i < load->getNumOperands(); ++i) {
          for (unsigned int j = 0; j < ins->getNumOperands(); ++j) {
            if (load->getOperand(i) == ins->getOperand(j)) {
              facts.insert(SlicerFact(source.getLocation(), load,
                                      source.getInterDistance()));
            }
          }
        }
      }
    }
#endif
    if (auto gep = dyn_cast<GetElementPtrInst>(curr)) {
      if (auto ins = dyn_cast<Instruction>(source.getInstruction())) {
        for (unsigned int i = 0; i < gep->getNumOperands(); ++i) {
          for (unsigned int j = 0; j < ins->getNumOperands(); ++j) {
            if (gep->getOperand(i) == ins->getOperand(j)) {
              facts.insert(SlicerFact(source.getLocation(), gep,
                                      source.getInterDistance()));
            }
          }
        }
      }
    }
    if (const auto *br = dyn_cast<BranchInst>(curr)) {
      for (unsigned int i = 0; i < br->getNumOperands(); ++i) {
        if (auto *target = dyn_cast<BasicBlock>(br->getOperand(i))) {
          if (source.getInstruction() == &target->front()) {
            facts.insert(SlicerFact(source.getLocation(), curr,
                                    source.getInterDistance()));
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
  if (!source.isZero() && source.getInstruction() != callSite &&
      source.is_within_limits()) {
    //    llvm::dbgs() << "===================\n";
    //    llvm::dbgs() << "curr: " << *callSite << "\n";
    //    llvm::dbgs() << "Source: " << *source.getInstruction() << "\n";
    //    llvm::dbgs() << callSite->getFunction()->getName() << "\n";
    //    auto fun = callSite->getFunction()->getName();
    //    if (fun == "process_string") {
    //      llvm::dbgs() << "";
    //    }
    if (auto ins = dyn_cast<Instruction>(source.getInstruction())) {
      for (unsigned int j = 0; j < ins->getNumOperands(); ++j) {
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
  if (!source.isZero() && source.is_within_limits()) {
    //    llvm::dbgs() << "===================\n";
    //    llvm::dbgs() <<  "Curr:" <<*callStmt << "\n";
    //    llvm::dbgs() << "Source: " << *source.getInstruction() << "\n";
    //    llvm::dbgs() << callStmt->getFunction()->getName() << "\n";

    //    if (callStmt->getFunction()->getName() ==
    //    "inotifytools_get_stat_by_filename") {
    //      llvm::dbgs() << "";
    //    }
    auto targets = ICF->getStartPointsOf(destMthd);
    bool isRelevant = false;
    // source is global or uses the return value
#ifdef OPERAND_PROP
    if (auto inst = dyn_cast<Instruction>(source.getInstruction())) {
      for (unsigned int i = 0; i < inst->getNumOperands(); ++i) {
        auto op = inst->getOperand(i);
        isRelevant |= isa<GlobalVariable>(op);
      }
      for (const auto &use : callStmt->uses()) {
        isRelevant |= (use.getUser() == inst);
      }
    }
    if (isRelevant) {
#endif
      for (auto target : targets) {
        facts.insert(SlicerFact(source.getLocation(), target,
                                source.getInterDistance() + 1));
      }
#ifdef OPERAND_PROP
    }
#endif // OPERAND_PROP
  }
#endif // INTERPROCEDURAL
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
  for (unsigned int i = 0; i < callSite->getNumOperands(); ++i) {
    const auto op = callSite->getOperand(i);
    if (source.getInstruction() == op) {
      facts.insert(SlicerFact(source.getLocation(),
                              getNthFunctionArgument(calleeMthd, i),
                              source.getInterDistance() + 1));
    }
  }
  if (!source.isZero() && source.is_within_limits()) {
    facts.insert(SlicerFact(source.getLocation(), exitStmt,
                            source.getInterDistance() + 1));
  }
#endif
  return facts;
}

class RewriteSourceVisitor
    : public clang::RecursiveASTVisitor<RewriteSourceVisitor> {
public:
  RewriteSourceVisitor(clang::ASTContext &context,
                       std::set<unsigned int> *target_lines,
                       const shared_ptr<std::set<unsigned int>> &resultingLines)
      : context(context), target_lines(target_lines),
        resulting_lines(resultingLines), candidate_lines() {}

  [[maybe_unused]] virtual bool VisitStmt(clang::Stmt *S) {
    auto es =
        context.getSourceManager().getExpansionLineNumber(S->getBeginLoc());
    auto es4 =
        context.getSourceManager().getExpansionLineNumber(S->getEndLoc());
    if (es == es4 && target_lines->find(es) != target_lines->end()) {
      for (auto &l : candidate_lines) {
        resulting_lines->insert(l);
      }
    }
    return true;
  }

  [[maybe_unused]] virtual bool VisitDefaultStmt(clang::DefaultStmt *S) {
    auto line =
        context.getSourceManager().getExpansionLineNumber(S->getBeginLoc());
    candidate_lines.insert(line);
    return true;
  }

  bool VisitCaseStmt(clang::CaseStmt *S) {
    auto line =
        context.getSourceManager().getExpansionLineNumber(S->getBeginLoc());
    candidate_lines.insert(line);
    return true;
  }

  virtual bool VisitBreakStmt(clang::BreakStmt *S) {
    candidate_lines.clear();
    return true;
  }

private:
  clang::ASTContext &context;
  std::set<unsigned int> *target_lines;
  shared_ptr<std::set<unsigned int>> resulting_lines;
  std::set<unsigned int> candidate_lines;
};

class RewriteSourceConsumer : public clang::ASTConsumer {
public:
  RewriteSourceConsumer(
      std::set<unsigned int> *target_lines,
      const shared_ptr<std::set<unsigned int>> &resultingLines)
      : target_lines(target_lines), resulting_lines(resultingLines) {}
  virtual void HandleTranslationUnit(clang::ASTContext &Context) {
    // Traversing the translation unit decl via a RecursiveASTVisitor
    // will visit all nodes in the AST.
    //    llvm::dbgs << Context.getTranslationUnitDecl();
    RewriteSourceVisitor Visitor(Context, target_lines, resulting_lines);
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  // A RecursiveASTVisitor implementation.

  std::set<unsigned int> *target_lines;
  shared_ptr<std::set<unsigned int>> resulting_lines;
};
class RewriteSourceAction
//    : public clang::ASTFrontendAction
{

public:
  RewriteSourceAction(std::set<unsigned int> *target_lines,
                      const shared_ptr<std::set<unsigned int>> &resultingLines)
      : target_lines(target_lines), resulting_lines(resultingLines) {}

  std::unique_ptr<clang::ASTConsumer> newASTConsumer() {
    return std::unique_ptr<clang::ASTConsumer>(
        new RewriteSourceConsumer(target_lines, resulting_lines));
  }

private:
  std::set<unsigned int> *target_lines;
  shared_ptr<std::set<unsigned int>> resulting_lines;
};

shared_ptr<std::set<unsigned int>>
add_block(std::string file, std::set<unsigned int> *target_lines) {
  string err = "ERROR_MY";
  auto db = clang::tooling::CompilationDatabase::autoDetectFromDirectory(
      boost::filesystem::path(file).parent_path().string(), err);
  std::vector<std::string> Sources;
  Sources.push_back(file);
  clang::tooling::ClangTool Tool(*db, Sources);
  auto res = make_shared<std::set<unsigned int>>();
  Tool.run(clang::tooling::newFrontendActionFactory<RewriteSourceAction>(
               new RewriteSourceAction(target_lines, res))
               .get());
  return res;
}
