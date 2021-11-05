#include "back_mapper.h"

#include <boost/filesystem/operations.hpp>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <vector>

/**
 * Notes JR:
 * General Idea: Walk over the AST and store the source locations of interesting
 * constructs Hereby some special handling is needed, for example for compound
 * statements Another problem is that the clang AST is not consistent in the
 * handling of semicolons At the same time, extract the debug lines (and maybe
 * later columns for finer slicing) from the the LLVM IR Merge the extracted
 * source locations, e.g. nodes that are completely embedded in others Match the
 * source locations from the IR with the ones from the AST -> Extract if match.
 * For extracting we can use precise column + line information from the AST.
 *
 * Questions:
 * What needs special handling? Semicolons, Compound Statements, what else?
 * Data structure for storing and merging the source code ranges? (Overlapping
 * intervals problem)
 */

namespace {
class RewriteSourceVisitor
    : public clang::RecursiveASTVisitor<RewriteSourceVisitor> {
public:
  RewriteSourceVisitor(
      clang::ASTContext &context, std::set<unsigned int> *target_lines,
      const std::shared_ptr<std::set<unsigned int>> &resultingLines)
      : context(context), target_lines(target_lines),
        resulting_lines(resultingLines), candidate_lines(),
        sm(context.getSourceManager()) {}

  [[maybe_unused]] bool VisitStmt(clang::Stmt *S) {
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

  [[maybe_unused]] bool VisitFunctionDecl(clang::FunctionDecl *D) {
    if (context.getSourceManager().isInMainFile(D->getLocation())) {
      // D->dumpColor();
    }
    return true;
  }

  [[maybe_unused]] bool VisitCompoundStmt(clang::CompoundStmt *C) {
    if (context.getSourceManager().isInMainFile(C->getBeginLoc())) {
      C->dumpColor();
      C->getLBracLoc().dump(context.getSourceManager());
      C->getRBracLoc().dump(context.getSourceManager());
    }

    return true;
  }

  [[maybe_unused]] bool VisitReturnStmt(clang::ReturnStmt *R) {
    if (sm.isInMainFile(R->getBeginLoc())) {
      R->getBeginLoc().dump(sm);
      R->getEndLoc().dump(sm);
      clang::Lexer::getLocForEndOfToken(R->getEndLoc(), 0, sm,
                                        context.getLangOpts())
          .dump(sm);
    }
    return true;
  }

  [[maybe_unused]] bool VisitDefaultStmt(clang::DefaultStmt *S) {
    auto line =
        context.getSourceManager().getExpansionLineNumber(S->getBeginLoc());
    candidate_lines.insert(line);
    return true;
  }

  [[maybe_unused]] bool VisitCaseStmt(clang::CaseStmt *S) {
    auto line =
        context.getSourceManager().getExpansionLineNumber(S->getBeginLoc());
    candidate_lines.insert(line);
    return true;
  }

  [[maybe_unused]] bool VisitBreakStmt(clang::BreakStmt *S) {
    candidate_lines.clear();
    return true;
  }

private:
  clang::ASTContext &context;
  std::set<unsigned int> *target_lines;
  std::shared_ptr<std::set<unsigned int>> resulting_lines;
  std::set<unsigned int> candidate_lines;
  clang::SourceManager &sm;
};

class RewriteSourceConsumer : public clang::ASTConsumer {
public:
  RewriteSourceConsumer(
      std::set<unsigned int> *target_lines,
      const std::shared_ptr<std::set<unsigned int>> &resultingLines)
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
  std::shared_ptr<std::set<unsigned int>> resulting_lines;
};
class RewriteSourceAction
//    : public clang::ASTFrontendAction
{

public:
  RewriteSourceAction(
      std::set<unsigned int> *target_lines,
      const std::shared_ptr<std::set<unsigned int>> &resultingLines)
      : target_lines(target_lines), resulting_lines(resultingLines) {}

  std::unique_ptr<clang::ASTConsumer> newASTConsumer() {
    return std::unique_ptr<clang::ASTConsumer>(
        new RewriteSourceConsumer(target_lines, resulting_lines));
  }

private:
  std::set<unsigned int> *target_lines;
  std::shared_ptr<std::set<unsigned int>> resulting_lines;
};

} // namespace

std::shared_ptr<std::set<unsigned int>>
add_block(std::string file, std::set<unsigned int> *target_lines) {
  std::string err;
  auto db = clang::tooling::CompilationDatabase::autoDetectFromDirectory(
      boost::filesystem::path(file).parent_path().string(), err);
  if (!db) {
    llvm::errs() << err;
  }

  std::vector<std::string> Sources;
  Sources.push_back(file);
  clang::tooling::ClangTool Tool(*db, Sources);
  auto res = std::make_shared<std::set<unsigned int>>();
  Tool.run(clang::tooling::newFrontendActionFactory<RewriteSourceAction>(
               new RewriteSourceAction(target_lines, res))
               .get());
  return res;
}
