#include <memory>
#include <tuple>

#include "gtest/gtest.h"

#include "phasar/DB/ProjectIRDB.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/IDELinearConstantAnalysis.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Solver/IDESolver.h"
#include "phasar/PhasarLLVM/Passes/ValueAnnotationPass.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToSet.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMTypeHierarchy.h"

#include "TestConfig.h"
#include "phasar/Utils/LLVMShorthands.h"

using namespace psr;

/* ============== TEST FIXTURE ============== */
class IDELinearConstantAnalysisTest : public ::testing::Test {
protected:
  const std::string PathToLlFiles =
      unittest::PathToLLTestFiles + "linear_constant/";

  // Function - Line Nr - Variable - Value
  using LCACompactResult_t =
      std::tuple<std::string, std::size_t, std::string, int64_t>;
  std::unique_ptr<ProjectIRDB> IRDB;

  void SetUp() override { boost::log::core::get()->set_logging_enabled(false); }

  IDELinearConstantAnalysis::lca_results_t
  doAnalysis(const std::string &LlvmFilePath, bool PrintDump = false) {
    auto IR_Files = {PathToLlFiles + LlvmFilePath};
    IRDB = std::make_unique<ProjectIRDB>(IR_Files, IRDBOptions::WPA);
    ValueAnnotationPass::resetValueID();
    LLVMTypeHierarchy TH(*IRDB);
    LLVMPointsToSet PT(*IRDB);
    LLVMBasedICFG ICFG(*IRDB, CallGraphAnalysisType::OTF, {"main"}, &TH, &PT,
                       Soundness::Soundy, /*IncludeGlobals*/ true);

    auto hasGlobalCtor = IRDB->getFunctionDefinition(
                             LLVMBasedICFG::GlobalCRuntimeModelName) != nullptr;
    IDELinearConstantAnalysis LCAProblem(
        IRDB.get(), &TH, &ICFG, &PT,
        {hasGlobalCtor ? LLVMBasedICFG::GlobalCRuntimeModelName.str()
                       : "main"});
    IDESolver_P<IDELinearConstantAnalysis> LCASolver(LCAProblem);
    LCASolver.solve();
    if (PrintDump) {
      IRDB->print();
      ICFG.print();
      LCASolver.dumpResults();
    }
    return LCAProblem.getLCAResults(LCASolver.getSolverResults());
  }

  void TearDown() override {}

  /**
   * We map instruction id to value for the ground truth. ID has to be
   * a string since Argument ID's are not integer type (e.g. main.0 for argc).
   * @param groundTruth results to compare against
   * @param solver provides the results
   */
  static void compareResults(IDELinearConstantAnalysis::lca_results_t &Results,
                             std::set<LCACompactResult_t> &GroundTruth) {
    std::set<LCACompactResult_t> RelevantResults;
    for (auto G : GroundTruth) {
      std::string FName = std::get<0>(G);
      unsigned Line = std::get<1>(G);
      if (Results.find(FName) != Results.end()) {
        if (auto It = Results[FName].find(Line); It != Results[FName].end()) {
          for (const auto &VarToVal : It->second.variableToValue) {
            RelevantResults.emplace(FName, Line, VarToVal.first,
                                    VarToVal.second);
          }
        }
      }
    }
    EXPECT_EQ(RelevantResults, GroundTruth);
  }
}; // Test Fixture

/* ============== BASIC TESTS ============== */
TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_01) {
  auto Results = doAnalysis("basic_01_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 13);
  GroundTruth.emplace("main", 3, "i", 13);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_02) {
  auto Results = doAnalysis("basic_02_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 13);
  GroundTruth.emplace("main", 3, "i", 17);
  GroundTruth.emplace("main", 4, "i", 17);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_03) {
  auto Results = doAnalysis("basic_03_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 10);
  GroundTruth.emplace("main", 3, "i", 10);
  GroundTruth.emplace("main", 3, "j", 14);
  GroundTruth.emplace("main", 4, "i", 14);
  GroundTruth.emplace("main", 4, "j", 14);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_04) {
  auto Results = doAnalysis("basic_04_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 3, "i", 14);
  GroundTruth.emplace("main", 4, "i", 14);
  GroundTruth.emplace("main", 4, "j", 20);
  GroundTruth.emplace("main", 5, "i", 14);
  GroundTruth.emplace("main", 5, "j", 20);
  GroundTruth.emplace("main", 6, "i", 14);
  GroundTruth.emplace("main", 6, "j", 20);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_05) {
  auto Results = doAnalysis("basic_05_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 3);
  GroundTruth.emplace("main", 3, "i", 3);
  GroundTruth.emplace("main", 3, "j", 14);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_06) {
  auto Results = doAnalysis("basic_06_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 4);
  GroundTruth.emplace("main", 3, "i", 16);
  GroundTruth.emplace("main", 4, "i", 16);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_07) {
  auto Results = doAnalysis("basic_07_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 4);
  GroundTruth.emplace("main", 3, "i", 4);
  GroundTruth.emplace("main", 3, "j", 3);
  GroundTruth.emplace("main", 4, "j", 3);
  GroundTruth.emplace("main", 5, "j", 3);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_08) {
  auto Results = doAnalysis("basic_08_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 42);
  GroundTruth.emplace("main", 3, "i", 42);
  GroundTruth.emplace("main", 3, "j", 40);
  GroundTruth.emplace("main", 4, "i", 42);
  GroundTruth.emplace("main", 4, "j", 40);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_09) {
  auto Results = doAnalysis("basic_09_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 42);
  GroundTruth.emplace("main", 3, "i", 42);
  GroundTruth.emplace("main", 3, "j", 126);
  GroundTruth.emplace("main", 4, "i", 42);
  GroundTruth.emplace("main", 4, "j", 126);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_10) {
  auto Results = doAnalysis("basic_10_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 42);
  GroundTruth.emplace("main", 3, "i", 42);
  GroundTruth.emplace("main", 3, "j", 14);
  GroundTruth.emplace("main", 4, "i", 42);
  GroundTruth.emplace("main", 4, "j", 14);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_11) {
  auto Results = doAnalysis("basic_11_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 42);
  GroundTruth.emplace("main", 3, "i", 42);
  GroundTruth.emplace("main", 3, "j", 2);
  GroundTruth.emplace("main", 4, "i", 42);
  GroundTruth.emplace("main", 4, "j", 2);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBasicTest_12) {
  auto Results = doAnalysis("basic_12_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  compareResults(Results, GroundTruth);
}

/* ============== BRANCH TESTS ============== */
TEST_F(IDELinearConstantAnalysisTest, HandleBranchTest_01) {
  auto Results = doAnalysis("branch_01_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 3, "i", 10);
  GroundTruth.emplace("main", 5, "i", 2);
  compareResults(Results, GroundTruth);
  // Results available for line 5 but not for line 7
  EXPECT_FALSE(Results["main"].find(5) == Results["main"].end());
  EXPECT_TRUE(Results["main"].find(7) == Results["main"].end());
}

TEST_F(IDELinearConstantAnalysisTest, HandleBranchTest_02) {
  auto Results = doAnalysis("branch_02_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 6, "i", 10);
  compareResults(Results, GroundTruth);
  // Results available for line 6 but not for line 8
  EXPECT_FALSE(Results["main"].find(6) == Results["main"].end());
  EXPECT_TRUE(Results["main"].find(8) == Results["main"].end());
}

TEST_F(IDELinearConstantAnalysisTest, HandleBranchTest_03) {
  auto Results = doAnalysis("branch_03_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 3, "i", 42);
  GroundTruth.emplace("main", 5, "i", 10);
  GroundTruth.emplace("main", 7, "i", 30);
  GroundTruth.emplace("main", 8, "i", 30);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBranchTest_04) {
  auto Results = doAnalysis("branch_04_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 3, "j", 10);
  GroundTruth.emplace("main", 4, "j", 10);
  GroundTruth.emplace("main", 4, "i", 42);
  GroundTruth.emplace("main", 6, "j", 10);
  GroundTruth.emplace("main", 6, "i", 20);
  GroundTruth.emplace("main", 8, "j", 10);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBranchTest_05) {
  auto Results = doAnalysis("branch_05_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 3, "j", 10);
  GroundTruth.emplace("main", 4, "j", 10);
  GroundTruth.emplace("main", 4, "i", 42);
  GroundTruth.emplace("main", 6, "j", 10);
  GroundTruth.emplace("main", 6, "i", 42);
  GroundTruth.emplace("main", 8, "j", 10);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBranchTest_06) {
  auto Results = doAnalysis("branch_06_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 3, "i", 10);
  GroundTruth.emplace("main", 5, "i", 10);
  GroundTruth.emplace("main", 7, "i", 10);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleBranchTest_07) {
  auto Results = doAnalysis("branch_07_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 3, "j", 10);
  GroundTruth.emplace("main", 4, "j", 10);
  GroundTruth.emplace("main", 4, "i", 30);
  GroundTruth.emplace("main", 6, "j", 10);
  GroundTruth.emplace("main", 6, "i", 30);
  GroundTruth.emplace("main", 8, "j", 10);
  compareResults(Results, GroundTruth);
}

/* ============== LOOP TESTS ============== */
TEST_F(IDELinearConstantAnalysisTest, HandleLoopTest_01) {
  auto Results = doAnalysis("while_01_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 42);
  compareResults(Results, GroundTruth);
  EXPECT_TRUE(Results["main"].find(4) == Results["main"].end());
  EXPECT_TRUE(Results["main"].find(6) == Results["main"].end());
}

TEST_F(IDELinearConstantAnalysisTest, HandleLoopTest_02) {
  auto Results = doAnalysis("while_02_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  compareResults(Results, GroundTruth);
  EXPECT_TRUE(Results["main"].find(2) == Results["main"].end());
  EXPECT_TRUE(Results["main"].find(4) == Results["main"].end());
  EXPECT_TRUE(Results["main"].find(6) == Results["main"].end());
}

TEST_F(IDELinearConstantAnalysisTest, HandleLoopTest_03) {
  auto Results = doAnalysis("while_03_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 42);
  GroundTruth.emplace("main", 7, "a", 13);
  GroundTruth.emplace("main", 8, "a", 13);
  compareResults(Results, GroundTruth);
  EXPECT_TRUE(Results["main"].find(4) == Results["main"].end());
  EXPECT_TRUE(Results["main"].find(6) == Results["main"].end());
}

TEST_F(IDELinearConstantAnalysisTest, HandleLoopTest_04) {
  auto Results = doAnalysis("while_04_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "i", 42);
  GroundTruth.emplace("main", 4, "a", 0);
  GroundTruth.emplace("main", 5, "a", 0);
  compareResults(Results, GroundTruth);
  EXPECT_TRUE(Results["main"].find(7) == Results["main"].end());
}

TEST_F(IDELinearConstantAnalysisTest, HandleLoopTest_05) {
  auto Results = doAnalysis("for_01_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 2, "a", 0);
  compareResults(Results, GroundTruth);
  EXPECT_TRUE(Results["main"].find(4) == Results["main"].end());
  EXPECT_TRUE(Results["main"].find(6) == Results["main"].end());
}

/* ============== CALL TESTS ============== */
TEST_F(IDELinearConstantAnalysisTest, HandleCallTest_01) {
  auto Results = doAnalysis("call_01_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3fooi", 1, "a", 42);
  GroundTruth.emplace("_Z3fooi", 2, "a", 42);
  GroundTruth.emplace("_Z3fooi", 2, "b", 42);

  GroundTruth.emplace("main", 6, "i", 42);
  GroundTruth.emplace("main", 7, "i", 42);
  GroundTruth.emplace("main", 8, "i", 42);
  compareResults(Results, GroundTruth);
  EXPECT_EQ(Results["_Z3fooi"].find(4), Results["_Z3fooi"].end());
}

TEST_F(IDELinearConstantAnalysisTest, HandleCallTest_02) {
  auto Results = doAnalysis("call_02_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3fooi", 1, "a", 2);
  GroundTruth.emplace("_Z3fooi", 2, "a", 2);

  GroundTruth.emplace("main", 7, "i", 42);
  GroundTruth.emplace("main", 8, "i", 42);
  compareResults(Results, GroundTruth);
  EXPECT_TRUE(Results["main"].find(6) == Results["main"].end());
}

TEST_F(IDELinearConstantAnalysisTest, HandleCallTest_03) {
  auto Results = doAnalysis("call_03_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 6, "i", 42);
  GroundTruth.emplace("main", 7, "i", 42);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleCallTest_04) {
  auto Results = doAnalysis("call_04_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 6, "i", 10);
  GroundTruth.emplace("main", 7, "i", 10);
  GroundTruth.emplace("main", 8, "i", 10);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleCallTest_05) {
  auto Results = doAnalysis("call_05_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  EXPECT_TRUE(Results["main"].empty());
}

TEST_F(IDELinearConstantAnalysisTest, HandleCallTest_06) {
  auto Results = doAnalysis("call_06_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z9incrementi", 1, "a", 42);
  GroundTruth.emplace("_Z9incrementi", 2, "a", 43);

  GroundTruth.emplace("main", 6, "i", 42);
  GroundTruth.emplace("main", 7, "i", 43);
  GroundTruth.emplace("main", 8, "i", 43);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleCallTest_07) {
  auto Results = doAnalysis("call_07_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 6, "i", 42);
  GroundTruth.emplace("main", 7, "i", 42);
  GroundTruth.emplace("main", 7, "j", 43);
  GroundTruth.emplace("main", 8, "i", 42);
  GroundTruth.emplace("main", 8, "j", 43);
  GroundTruth.emplace("main", 8, "k", 44);
  GroundTruth.emplace("main", 9, "i", 42);
  GroundTruth.emplace("main", 9, "j", 43);
  GroundTruth.emplace("main", 9, "k", 44);
  compareResults(Results, GroundTruth);
  EXPECT_TRUE(Results["_Z9incrementi"].find(1) ==
              Results["_Z9incrementi"].end());
  EXPECT_TRUE(Results["_Z9incrementi"].find(2) ==
              Results["_Z9incrementi"].end());
}

TEST_F(IDELinearConstantAnalysisTest, HandleCallTest_08) {
  auto Results = doAnalysis("call_08_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3fooii", 1, "a", 10);
  GroundTruth.emplace("_Z3fooii", 1, "b", 1);
  GroundTruth.emplace("_Z3fooii", 2, "a", 10);
  GroundTruth.emplace("_Z3fooii", 2, "b", 1);

  GroundTruth.emplace("main", 6, "i", 10);
  GroundTruth.emplace("main", 7, "i", 10);
  GroundTruth.emplace("main", 7, "j", 1);
  GroundTruth.emplace("main", 8, "i", 10);
  GroundTruth.emplace("main", 8, "j", 1);
  GroundTruth.emplace("main", 9, "i", 10);
  GroundTruth.emplace("main", 9, "j", 1);
  GroundTruth.emplace("main", 10, "i", 10);
  GroundTruth.emplace("main", 10, "j", 1);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleCallTest_09) {
  auto Results = doAnalysis("call_09_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z9incrementi", 1, "a", 42);
  GroundTruth.emplace("_Z9incrementi", 2, "a", 43);

  GroundTruth.emplace("main", 6, "i", 43);
  GroundTruth.emplace("main", 7, "i", 43);
  GroundTruth.emplace("main", 7, "j", 43);
  GroundTruth.emplace("main", 8, "i", 43);
  GroundTruth.emplace("main", 8, "j", 43);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleCallTest_10) {
  auto Results = doAnalysis("call_10_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3bari", 1, "b", 2);
  GroundTruth.emplace("_Z3fooi", 3, "a", 2);
  GroundTruth.emplace("_Z3fooi", 4, "a", 2);
  compareResults(Results, GroundTruth);
  EXPECT_TRUE(Results["main"].find(8) == Results["main"].end());
  EXPECT_TRUE(Results["main"].find(9) == Results["main"].end());
}

TEST_F(IDELinearConstantAnalysisTest, HandleCallTest_11) {
  auto Results = doAnalysis("call_11_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3bari", 1, "b", 2);
  GroundTruth.emplace("_Z3bari", 2, "b", 2);

  GroundTruth.emplace("_Z3fooi", 5, "a", 2);
  GroundTruth.emplace("_Z3fooi", 6, "a", 2);

  GroundTruth.emplace("main", 11, "i", 2);
  GroundTruth.emplace("main", 12, "i", 2);
  compareResults(Results, GroundTruth);
}

/* ============== RECURSION TESTS ============== */

TEST_F(IDELinearConstantAnalysisTest, HandleRecursionTest_01) {
  auto Results = doAnalysis("recursion_01_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 10, "j", -1);
  GroundTruth.emplace("main", 11, "j", -1);
  compareResults(Results, GroundTruth);
  EXPECT_TRUE(Results["_Z9decrementi"].find(2) ==
              Results["_Z9decrementi"].end());
  EXPECT_TRUE(Results["_Z9decrementi"].find(4) ==
              Results["_Z9decrementi"].end());
}

TEST_F(IDELinearConstantAnalysisTest, HandleRecursionTest_02) {
  auto Results = doAnalysis("recursion_02_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleRecursionTest_03) {
  auto Results = doAnalysis("recursion_03_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 9, "a", 1);
  GroundTruth.emplace("main", 10, "a", 1);
  compareResults(Results, GroundTruth);
  EXPECT_TRUE(Results["_Z3fooj"].find(1) == Results["_Z3fooj"].end());
  EXPECT_TRUE(Results["_Z3fooj"].find(3) == Results["_Z3fooj"].end());
  EXPECT_TRUE(Results["_Z3fooj"].find(5) == Results["_Z3fooj"].end());
}

/* ============== GLOBAL VARIABLE TESTS ============== */

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_01) {
  auto Results = doAnalysis("global_01_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 6, "i", 666);
  GroundTruth.emplace("main", 6, "g1", 10);
  GroundTruth.emplace("main", 6, "g2", 1);
  GroundTruth.emplace("main", 9, "i", 666);
  GroundTruth.emplace("main", 9, "g1", 42);
  GroundTruth.emplace("main", 9, "g2", 42);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_02) {
  auto Results = doAnalysis("global_02_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 4, "g", 10);
  GroundTruth.emplace("main", 4, "i", 10);
  GroundTruth.emplace("main", 5, "g", 10);
  GroundTruth.emplace("main", 5, "i", -10);
  GroundTruth.emplace("main", 6, "g", -10);
  GroundTruth.emplace("main", 6, "i", -10);
  GroundTruth.emplace("main", 7, "g", -10);
  GroundTruth.emplace("main", 7, "i", -10);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_03) {
  auto Results = doAnalysis("global_03_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3foov", 4, "g", 2);
  GroundTruth.emplace("main", 8, "g", 0);
  GroundTruth.emplace("main", 8, "i", 42);
  GroundTruth.emplace("main", 9, "g", 1);
  GroundTruth.emplace("main", 9, "i", 42);
  GroundTruth.emplace("main", 10, "g", 2);
  GroundTruth.emplace("main", 10, "i", 42);
  GroundTruth.emplace("main", 11, "g", 2);
  GroundTruth.emplace("main", 11, "i", 42);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_04) {
  auto Results = doAnalysis("global_04_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3fooi", 3, "g", 1);
  GroundTruth.emplace("_Z3fooi", 3, "a", 1);
  GroundTruth.emplace("_Z3fooi", 4, "g", 1);
  GroundTruth.emplace("_Z3fooi", 4, "a", 2);

  GroundTruth.emplace("main", 8, "g", 1);
  GroundTruth.emplace("main", 9, "g", 1);
  GroundTruth.emplace("main", 9, "i", 2);
  GroundTruth.emplace("main", 10, "g", 1);
  GroundTruth.emplace("main", 10, "i", 2);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_05) {
  auto Results = doAnalysis("global_05_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3fooi", 3, "g", 2);
  GroundTruth.emplace("_Z3fooi", 3, "a", 2);
  GroundTruth.emplace("_Z3fooi", 4, "g", 2);
  GroundTruth.emplace("_Z3fooi", 4, "a", 3);

  GroundTruth.emplace("main", 8, "g", 1);
  GroundTruth.emplace("main", 9, "g", 2);
  GroundTruth.emplace("main", 9, "i", 3);
  GroundTruth.emplace("main", 10, "g", 2);
  GroundTruth.emplace("main", 10, "i", 3);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_06) {
  auto Results = doAnalysis("global_06_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3foov", 4, "g", 2);
  GroundTruth.emplace("main", 8, "g", 1);
  GroundTruth.emplace("main", 9, "g", 2);
  GroundTruth.emplace("main", 9, "i", 2);
  GroundTruth.emplace("main", 10, "g", 2);
  GroundTruth.emplace("main", 10, "i", 2);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_07) {
  auto Results = doAnalysis("global_07_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3fooi", 3, "g", 1);
  GroundTruth.emplace("_Z3fooi", 3, "a", 10);
  GroundTruth.emplace("_Z3fooi", 4, "g", 1);
  GroundTruth.emplace("_Z3fooi", 5, "g", 1);

  GroundTruth.emplace("_Z3bari", 8, "g", 1);
  GroundTruth.emplace("_Z3bari", 8, "b", 3);
  GroundTruth.emplace("_Z3bari", 9, "g", 2);
  GroundTruth.emplace("_Z3bari", 9, "b", 3);
  GroundTruth.emplace("_Z3bari", 10, "g", 2);
  GroundTruth.emplace("_Z3bari", 10, "b", 3);

  GroundTruth.emplace("main", 14, "g", 1);
  GroundTruth.emplace("main", 15, "g", 1);
  GroundTruth.emplace("main", 15, "i", 0);
  GroundTruth.emplace("main", 16, "g", 1);
  GroundTruth.emplace("main", 17, "g", 2);
  GroundTruth.emplace("main", 17, "i", 4);
  GroundTruth.emplace("main", 18, "g", 2);
  GroundTruth.emplace("main", 18, "i", 4);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_08) {
  auto Results = doAnalysis("global_08_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3bari", 7, "b", 2);
  GroundTruth.emplace("_Z3bari", 7, "g", 2);
  GroundTruth.emplace("_Z3bari", 8, "b", 2);
  GroundTruth.emplace("_Z3bari", 8, "g", 2);

  GroundTruth.emplace("_Z3bazi", 3, "g", 2);
  GroundTruth.emplace("_Z3bazi", 3, "c", 3);
  GroundTruth.emplace("_Z3bazi", 4, "g", 2);
  GroundTruth.emplace("_Z3bazi", 4, "c", 3);

  GroundTruth.emplace("_Z3fooi", 11, "g", 2);
  GroundTruth.emplace("_Z3fooi", 11, "a", 1);
  GroundTruth.emplace("_Z3fooi", 12, "g", 2);
  GroundTruth.emplace("_Z3fooi", 12, "a", 1);

  GroundTruth.emplace("main", 16, "g", 2);
  GroundTruth.emplace("main", 17, "g", 2);
  GroundTruth.emplace("main", 17, "i", 0);
  GroundTruth.emplace("main", 18, "g", 2);
  GroundTruth.emplace("main", 19, "g", 2);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_10) {
  auto Results = doAnalysis("global_10_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 5, "g1", 42);
  GroundTruth.emplace("main", 5, "g2", 9001);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_11) {
  auto Results = doAnalysis("global_11_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 10, "a", 13);
  GroundTruth.emplace("main", 10, "g1", 42);
  GroundTruth.emplace("main", 10, "g2", 9001);
  GroundTruth.emplace("_Z3fooi", 5, "x", 14);
  GroundTruth.emplace("_Z3fooi", 5, "g1", 42);
  GroundTruth.emplace("_Z3fooi", 5, "g2", 9001);
  GroundTruth.emplace("main", 11, "a", 14);
  GroundTruth.emplace("main", 11, "g1", 42);
  GroundTruth.emplace("main", 11, "g2", 9001);
  GroundTruth.emplace("main", 12, "a", 14);
  GroundTruth.emplace("main", 12, "g1", 42);
  GroundTruth.emplace("main", 12, "g2", 9001);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_12) {
  auto Results = doAnalysis("global_12_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z11global_ctorv", 3, "g", 42);
  GroundTruth.emplace("_Z3fooi", 6, "x", 43);
  GroundTruth.emplace("_Z3fooi", 6, "g", 42);
  GroundTruth.emplace("main", 11, "a", 42);
  GroundTruth.emplace("main", 11, "g", 42);
  GroundTruth.emplace("main", 13, "a", 43);
  GroundTruth.emplace("main", 13, "g", 42);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_13) {
  auto Results = doAnalysis("global_13_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z11global_ctorv", 3, "g", 42);
  GroundTruth.emplace("_Z11global_dtorv", 5, "g", 666);
  GroundTruth.emplace("_Z3fooi", 8, "x", 43);
  GroundTruth.emplace("_Z3fooi", 8, "g", 42);
  GroundTruth.emplace("_Z3fooi", 9, "x", 43);
  GroundTruth.emplace("_Z3fooi", 9, "g", 42);
  GroundTruth.emplace("main", 13, "a", 42);
  GroundTruth.emplace("main", 13, "g", 42);
  GroundTruth.emplace("main", 15, "a", 42);
  GroundTruth.emplace("main", 15, "b", 43);
  GroundTruth.emplace("main", 15, "g", 42);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_14) {
  auto Results = doAnalysis("global_14_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_ZN1XC2Ev", 4, "g", 1024);
  GroundTruth.emplace("_Z3fooi", 9, "x", 1025);
  GroundTruth.emplace("_Z3fooi", 9, "g", 1024);
  GroundTruth.emplace("main", 15, "a", 1024);
  GroundTruth.emplace("main", 15, "g", 1024);
  GroundTruth.emplace("main", 17, "a", 1025);
  GroundTruth.emplace("main", 17, "g", 1024);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_15) {
  auto Results = doAnalysis("global_15_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_ZN1XC2Ev", 5, "g1", 1024);
  GroundTruth.emplace("_ZN1XC2Ev", 5, "g2", 99);
  GroundTruth.emplace("_ZN1YC2Ev", 9, "g1", 1024);
  GroundTruth.emplace("_ZN1YC2Ev", 9, "g2", 100);
  GroundTruth.emplace("_ZN1YD2Ev", 10, "g1", 113);
  GroundTruth.emplace("_ZN1YD2Ev", 10, "g2", 100);
  GroundTruth.emplace("_Z3fooi", 15, "x", 1025);
  GroundTruth.emplace("_Z3fooi", 15, "g1", 1024);
  GroundTruth.emplace("_Z3fooi", 15, "g2", 100);
  GroundTruth.emplace("main", 22, "a", 1024);
  GroundTruth.emplace("main", 22, "g1", 1024);
  GroundTruth.emplace("main", 22, "g2", 100);
  GroundTruth.emplace("main", 25, "a", 1025);
  GroundTruth.emplace("main", 25, "b", 100);
  GroundTruth.emplace("main", 25, "g1", 1024);
  GroundTruth.emplace("main", 25, "g2", 100);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleGlobalsTest_16) {
  auto Results = doAnalysis("global_16_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("_Z3fooi", 4, "x", 16);
  GroundTruth.emplace("_Z3fooi", 4, "g", 15);
  GroundTruth.emplace("_Z3fooi", 5, "x", 16);
  GroundTruth.emplace("_Z3fooi", 5, "g", 15);
  GroundTruth.emplace("main", 9, "a", 15);
  GroundTruth.emplace("main", 9, "g", 15);
  GroundTruth.emplace("main", 11, "a", 16);
  GroundTruth.emplace("main", 11, "g", 15);
  compareResults(Results, GroundTruth);
}

/* ============== OVERFLOW TESTS ============== */

TEST_F(IDELinearConstantAnalysisTest, HandleAddOverflow) {
  auto Results = doAnalysis("overflow_add_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 6, "i", 9223372036854775806);
  GroundTruth.emplace("main", 6, "j", IDELinearConstantAnalysis::TOP);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleSubOverflow) {
  auto Results = doAnalysis("overflow_sub_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 6, "i", -9223372036854775807);
  GroundTruth.emplace("main", 6, "j", IDELinearConstantAnalysis::TOP);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleMulOverflow) {
  auto Results = doAnalysis("overflow_mul_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 6, "i", 9223372036854775806);
  GroundTruth.emplace("main", 6, "j", IDELinearConstantAnalysis::TOP);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleDivOverflowForMinIntDivByOne) {
  auto Results = doAnalysis("overflow_div_min_by_neg_one_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 6, "i", -9223372036854775807);
  GroundTruth.emplace("main", 6, "j", IDELinearConstantAnalysis::TOP);
  GroundTruth.emplace("main", 6, "k", IDELinearConstantAnalysis::TOP);
  compareResults(Results, GroundTruth);
}

/* ============== ERROR TESTS ============== */

TEST_F(IDELinearConstantAnalysisTest, HandleDivisionByZero) {
  auto Results = doAnalysis("ub_division_by_zero_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 4, "i", 42);
  GroundTruth.emplace("main", 4, "j", IDELinearConstantAnalysis::TOP);
  compareResults(Results, GroundTruth);
}

TEST_F(IDELinearConstantAnalysisTest, HandleModuloByZero) {
  auto Results = doAnalysis("ub_modulo_by_zero_cpp_dbg.ll");
  std::set<LCACompactResult_t> GroundTruth;
  GroundTruth.emplace("main", 4, "i", 42);
  GroundTruth.emplace("main", 4, "j", IDELinearConstantAnalysis::TOP);
  compareResults(Results, GroundTruth);
}

// main function for the test case
int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
