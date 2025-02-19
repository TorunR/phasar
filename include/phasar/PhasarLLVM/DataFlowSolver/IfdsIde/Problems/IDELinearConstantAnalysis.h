/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_IFDSIDE_PROBLEMS_IDELINEARCONSTANTANALYSIS_H_
#define PHASAR_PHASARLLVM_IFDSIDE_PROBLEMS_IDELINEARCONSTANTANALYSIS_H_

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/EdgeFunctionComposer.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/IDETabulationProblem.h"
#include "phasar/PhasarLLVM/Domain/AnalysisDomain.h"

namespace llvm {
class Instruction;
class Function;
class StructType;
class Value;
} // namespace llvm

namespace psr {

struct IDELinearConstantAnalysisDomain : public LLVMAnalysisDomainDefault {
  // int64_t corresponds to llvm's type of constant integer
  using l_t = int64_t;
};

class LLVMBasedICFG;
class LLVMTypeHierarchy;
class LLVMPointsToInfo;

class IDELinearConstantAnalysis
    : public IDETabulationProblem<IDELinearConstantAnalysisDomain> {
private:
  // For debug purpose only
  static unsigned CurrGenConstantId;
  static unsigned CurrLCAIDId;
  static unsigned CurrBinaryId;

public:
  using IDETabProblemType =
      IDETabulationProblem<IDELinearConstantAnalysisDomain>;
  using typename IDETabProblemType::d_t;
  using typename IDETabProblemType::f_t;
  using typename IDETabProblemType::i_t;
  using typename IDETabProblemType::l_t;
  using typename IDETabProblemType::n_t;
  using typename IDETabProblemType::t_t;
  using typename IDETabProblemType::v_t;

  static const l_t TOP;
  static const l_t BOTTOM;

  IDELinearConstantAnalysis(const ProjectIRDB *IRDB,
                            const LLVMTypeHierarchy *TH,
                            const LLVMBasedICFG *ICF, LLVMPointsToInfo *PT,
                            std::set<std::string> EntryPoints = {"main"});

  ~IDELinearConstantAnalysis() override;

  struct LCAResult {
    LCAResult() = default;
    unsigned line_nr = 0;
    std::string src_code;
    std::map<std::string, l_t> variableToValue;
    std::vector<n_t> ir_trace;
    void print(std::ostream &os);
    inline bool operator==(const LCAResult &rhs) const {
      return src_code == rhs.src_code &&
             variableToValue == rhs.variableToValue && ir_trace == rhs.ir_trace;
    }

    operator std::string() const {
      std::stringstream OS;
      OS << "Line " << line_nr << ": " << src_code << '\n';
      OS << "Var(s): ";
      for (auto It = variableToValue.begin(); It != variableToValue.end();
           ++It) {
        if (It != variableToValue.begin()) {
          OS << ", ";
        }
        OS << It->first << " = " << It->second;
      }
      OS << "\nCorresponding IR Instructions:\n";
      for (const auto *Ir : ir_trace) {
        OS << "  " << llvmIRToString(Ir) << '\n';
      }
      return OS.str();
    }
  };

  using lca_results_t = std::map<std::string, std::map<unsigned, LCAResult>>;

  static void stripBottomResults(std::unordered_map<d_t, l_t> &Res);

  // start formulating our analysis by specifying the parts required for IFDS

  FlowFunctionPtrType getNormalFlowFunction(n_t curr, n_t succ) override;

  FlowFunctionPtrType getCallFlowFunction(n_t callSite, f_t destFun) override;

  FlowFunctionPtrType getRetFlowFunction(n_t callSite, f_t calleeFun,
                                         n_t exitInst, n_t retSite) override;

  FlowFunctionPtrType getCallToRetFlowFunction(n_t callSite, n_t retSite,
                                               std::set<f_t> callees) override;

  FlowFunctionPtrType getSummaryFlowFunction(n_t callSite,
                                             f_t destFun) override;

  InitialSeeds<n_t, d_t, l_t> initialSeeds() override;

  d_t createZeroValue() const override;

  bool isZeroValue(d_t d) const override;

  // in addition provide specifications for the IDE parts

  std::shared_ptr<EdgeFunction<l_t>>
  getNormalEdgeFunction(n_t curr, d_t currNode, n_t succ,
                        d_t succNode) override;

  std::shared_ptr<EdgeFunction<l_t>>
  getCallEdgeFunction(n_t callSite, d_t srcNode, f_t destinationFunction,
                      d_t destNode) override;

  std::shared_ptr<EdgeFunction<l_t>>
  getReturnEdgeFunction(n_t callSite, f_t calleeFunction, n_t exitInst,
                        d_t exitNode, n_t reSite, d_t retNode) override;

  std::shared_ptr<EdgeFunction<l_t>>
  getCallToRetEdgeFunction(n_t callSite, d_t callNode, n_t retSite,
                           d_t retSiteNode, std::set<f_t> callees) override;

  std::shared_ptr<EdgeFunction<l_t>>
  getSummaryEdgeFunction(n_t callSite, d_t callNode, n_t retSite,
                         d_t retSiteNode) override;

  l_t topElement() override;

  l_t bottomElement() override;

  l_t join(l_t lhs, l_t rhs) override;

  std::shared_ptr<EdgeFunction<l_t>> allTopFunction() override;

  // Custom EdgeFunction declarations

  class LCAEdgeFunctionComposer : public EdgeFunctionComposer<l_t> {
  public:
    LCAEdgeFunctionComposer(std::shared_ptr<EdgeFunction<l_t>> F,
                            std::shared_ptr<EdgeFunction<l_t>> G)
        : EdgeFunctionComposer<l_t>(F, G){};

    std::shared_ptr<EdgeFunction<l_t>>
    composeWith(std::shared_ptr<EdgeFunction<l_t>> secondFunction) override;

    std::shared_ptr<EdgeFunction<l_t>>
    joinWith(std::shared_ptr<EdgeFunction<l_t>> otherFunction) override;
  };

  class GenConstant : public EdgeFunction<l_t>,
                      public std::enable_shared_from_this<GenConstant> {
  private:
    const unsigned GenConstant_Id;
    const l_t IntConst;

  public:
    explicit GenConstant(l_t IntConst);

    l_t computeTarget(l_t source) override;

    std::shared_ptr<EdgeFunction<l_t>>
    composeWith(std::shared_ptr<EdgeFunction<l_t>> secondFunction) override;

    std::shared_ptr<EdgeFunction<l_t>>
    joinWith(std::shared_ptr<EdgeFunction<l_t>> otherFunction) override;

    bool equal_to(std::shared_ptr<EdgeFunction<l_t>> other) const override;

    void print(std::ostream &OS, bool isForDebug = false) const override;
  };

  class LCAIdentity : public EdgeFunction<l_t>,
                      public std::enable_shared_from_this<LCAIdentity> {
  private:
    const unsigned LCAID_Id;

  public:
    explicit LCAIdentity();

    l_t computeTarget(l_t source) override;

    std::shared_ptr<EdgeFunction<l_t>>
    composeWith(std::shared_ptr<EdgeFunction<l_t>> secondFunction) override;

    std::shared_ptr<EdgeFunction<l_t>>
    joinWith(std::shared_ptr<EdgeFunction<l_t>> otherFunction) override;

    bool equal_to(std::shared_ptr<EdgeFunction<l_t>> other) const override;

    void print(std::ostream &OS, bool isForDebug = false) const override;
  };

  class BinOp : public EdgeFunction<l_t>,
                public std::enable_shared_from_this<BinOp> {
  private:
    const unsigned EdgeFunctionID, Op;
    d_t lop, rop, currNode;

  public:
    BinOp(const unsigned Op, d_t lop, d_t rop, d_t currNode);

    l_t computeTarget(l_t source) override;

    std::shared_ptr<EdgeFunction<l_t>>
    composeWith(std::shared_ptr<EdgeFunction<l_t>> secondFunction) override;

    std::shared_ptr<EdgeFunction<l_t>>
    joinWith(std::shared_ptr<EdgeFunction<l_t>> otherFunction) override;

    bool equal_to(std::shared_ptr<EdgeFunction<l_t>> other) const override;

    void print(std::ostream &OS, bool isForDebug = false) const override;
  };

  // Helper functions

  /**
   * The following binary operations are computed:
   *  - addition
   *  - subtraction
   *  - multiplication
   *  - division (signed/unsinged)
   *  - remainder (signed/unsinged)
   *
   * @brief Computes the result of a binary operation.
   * @param op operator
   * @param lop left operand
   * @param rop right operand
   * @return Result of binary operation
   */
  static l_t executeBinOperation(const unsigned op, l_t lop, l_t rop);

  static char opToChar(const unsigned op);

  bool isEntryPoint(const std::string &FunctionName) const;

  void printNode(std::ostream &os, n_t n) const override;

  void printDataFlowFact(std::ostream &os, d_t d) const override;

  void printFunction(std::ostream &os, f_t m) const override;

  void printEdgeFact(std::ostream &os, l_t l) const override;

  lca_results_t getLCAResults(SolverResults<n_t, d_t, l_t> SR);

  void emitTextReport(const SolverResults<n_t, d_t, l_t> &SR,
                      std::ostream &OS = std::cout) override;
};

} // namespace psr

#endif
