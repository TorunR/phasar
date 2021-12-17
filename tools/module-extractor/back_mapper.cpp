#include "back_mapper.h"
#include "SelectiveDeclPrinter.h"
#include "printer.h"

#include <boost/filesystem/operations.hpp>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <utility>
#include <vector>
#include <iostream>

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
 * Presumed, expansion, or spelling loc?
 */

namespace {

/*
class RewriteSourceVisitor
    : public clang::RecursiveASTVisitor<RewriteSourceVisitor> {
public:
  RewriteSourceVisitor(clang::ASTContext &context,
                       std::set<unsigned int> *target_lines,
                       std::shared_ptr<std::set<unsigned int>> resultingLines)
      : context(context), target_lines(target_lines),
        resulting_lines(std::move(resultingLines)), candidate_lines(),
        sm(context.getSourceManager()) {}

  bool VisitTranslationUnitDecl(clang::TranslationUnitDecl *D) {
    selective_printer::print(D, *target_lines, llvm::errs());
    return false;
  }

//  [[maybe_unused]] bool VisitFunctionDecl(clang::FunctionDecl *F) {
//    //    if (sm.isInMainFile(F->getBeginLoc())) {
//    //      F->getBeginLoc().dump(sm);
//    //      F->getEndLoc().dump(sm);
//    //      if (F->hasBody()) {
//    //        {
//    //          std::string buffer;
//    //          llvm::raw_string_ostream RSO(buffer);
//    //          F->print(RSO, context.getPrintingPolicy());
//    //          llvm::errs() << RSO.str() << "\n";
//    //        }
//    //      }
//    //    }
//    F->getBeginLoc().dump(sm);
//    F->getEndLoc().dump(sm);
//    F->getLocation().dump(sm);
//    // F->getloc
//    selective_printer::print(F, *target_lines, llvm::errs());
//    return true;
//  }
//
//  [[maybe_unused]] bool VisitStmt(clang::Stmt *S) {
//    auto es =
//        context.getSourceManager().getExpansionLineNumber(S->getBeginLoc());
//    auto es4 =
//        context.getSourceManager().getExpansionLineNumber(S->getEndLoc());
//    if (es == es4 && target_lines->find(es) != target_lines->end()) {
//      for (auto &l : candidate_lines) {
//        resulting_lines->insert(l);
//      }
//    }
//    if (context.getSourceManager().isInMainFile(S->getBeginLoc())) {
//      // S->dumpPretty(context);
//    }
//    if (llvm::isa<clang::Expr>(S)) {
//      // S->dump();
//    }
//    return true;
//  }
//
//  [[maybe_unused]] bool VisitCompoundStmt(clang::CompoundStmt *C) {
//    if (context.getSourceManager().isInMainFile(C->getBeginLoc())) {
//      //      C->dumpColor();
//      //      C->getLBracLoc().dump(context.getSourceManager());
//      //      C->getRBracLoc().dump(context.getSourceManager());
//    }
//
//    return true;
//  }
//
//  [[maybe_unused]] bool VisitReturnStmt(clang::ReturnStmt *R) {
//    if (sm.isInMainFile(R->getBeginLoc())) {
//      // R->getBeginLoc().dump(sm);
//      // R->getEndLoc().dump(sm);
//      //      R->dumpPretty(context);
//      //      clang::Lexer::getLocForEndOfToken(R->getEndLoc(), 0, sm,
//      //                                        context.getLangOpts())
//      //          .dump(sm);
//      //      auto PLoc = sm.getPresumedLoc(R->getBeginLoc());
//      //      llvm::errs() << PLoc.getFilename() << ':' << PLoc.getLine() <<
':'
//      //                   << PLoc.getColumn() << "\n";
//      //      sm.getExpansionLoc(R->getBeginLoc()).dump(sm);
//      //      sm.getSpellingLoc(R->getBeginLoc()).dump(sm);
//    }
//    return true;
//  }
//
//  [[maybe_unused]] bool VisitDefaultStmt(clang::DefaultStmt *S) {
//    auto line =
//        context.getSourceManager().getExpansionLineNumber(S->getBeginLoc());
//    candidate_lines.insert(line);
//    return true;
//  }
//
//  [[maybe_unused]] bool VisitCaseStmt(clang::CaseStmt *S) {
//    auto line =
//        context.getSourceManager().getExpansionLineNumber(S->getBeginLoc());
//    candidate_lines.insert(line);
//    return true;
//  }
//
//  [[maybe_unused]] bool VisitBreakStmt(clang::BreakStmt *S) {
//    candidate_lines.clear();
//    return true;
//  }

private:
  clang::ASTContext &context;
  std::set<unsigned int> *target_lines;
  std::shared_ptr<std::set<unsigned int>> resulting_lines;
  std::set<unsigned int> candidate_lines;
  clang::SourceManager &sm;
};
*/

class RewriteSourceConsumer : public clang::ASTConsumer {
public:
  RewriteSourceConsumer(const std::set<unsigned int> *target_lines,
                        std::vector<printer::FileSlice> &output)
      : target_lines(target_lines), output(output) {}
  void HandleTranslationUnit(clang::ASTContext &Context) override {
    // Traversing the translation unit decl via a RecursiveASTVisitor
    // will visit all nodes in the AST.
    //    llvm::dbgs << Context.getTranslationUnitDecl();
    {
      //      llvm::raw_string_ostream out(output);
      //      selective_printer::print(Context.getTranslationUnitDecl(),
      //      *target_lines,
      //                               out);
      output = printer::DeclPrinter::GetFileSlices(
          Context.getTranslationUnitDecl(), *target_lines, Context);
    }
    // RewriteSourceVisitor Visitor(Context, target_lines, resulting_lines);
    // Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  // A RecursiveASTVisitor implementation.

  const std::set<unsigned int> *target_lines;
  std::vector<printer::FileSlice> &output;
};
class RewriteSourceAction
//    : public clang::ASTFrontendAction
{

public:
  RewriteSourceAction(const std::set<unsigned int> *target_lines,
                      std::vector<printer::FileSlice> &output)
      : target_lines(target_lines), output(output) {}

  std::unique_ptr<clang::ASTConsumer> newASTConsumer() {
    return std::unique_ptr<clang::ASTConsumer>(
        new RewriteSourceConsumer(target_lines, output));
  }

private:
  const std::set<unsigned int> *target_lines;
  std::vector<printer::FileSlice> &output;
};

} // namespace

std::vector<printer::FileSlice>
add_block(std::string file, const std::set<unsigned int> *target_lines) {
  std::string err;
  auto db = clang::tooling::CompilationDatabase::autoDetectFromDirectory(
      boost::filesystem::path(file).parent_path().string(), err);
  if (!db) {
    throw std::runtime_error(err);
  }
  std::vector<std::string> Sources;
  Sources.push_back(file);
  clang::tooling::ClangTool Tool(*db, Sources);
  std::vector<printer::FileSlice> buffer;

  std::cerr << file << std::endl;

  Tool.run(clang::tooling::newFrontendActionFactory<RewriteSourceAction>(
               new RewriteSourceAction(target_lines, buffer))
               .get());
  printer::mergeAndSplitSlices(buffer);
  return buffer;
}
