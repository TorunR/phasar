#include "back_mapper.h"
#include "printer.h"

#include <boost/filesystem/operations.hpp>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <utility>
#include <vector>

namespace {

class RewriteSourceConsumer : public clang::ASTConsumer {
public:
  RewriteSourceConsumer(const std::set<unsigned int> *target_lines,
                        std::pair<std::vector<printer::FileSlice>,
                                  std::vector<printer::FileSlice>> &output)
      : target_lines(target_lines), output(output) {}
  void HandleTranslationUnit(clang::ASTContext &Context) override {
    // Traversing the translation unit decl via a RecursiveASTVisitor
    // will visit all nodes in the AST.
    //    llvm::dbgs << Context.getTranslationUnitDecl();
    output = printer::DeclPrinterFiltering::GetFileSlices(
        Context.getTranslationUnitDecl(), *target_lines, Context);
  }

private:
  const std::set<unsigned int> *target_lines;
  std::pair<std::vector<printer::FileSlice>, std::vector<printer::FileSlice>>
      &output;
};

class RewriteSourceAction {
public:
  RewriteSourceAction(const std::set<unsigned int> *target_lines,
                      std::pair<std::vector<printer::FileSlice>,
                                std::vector<printer::FileSlice>> &output)
      : target_lines(target_lines), output(output) {}

  std::unique_ptr<clang::ASTConsumer> newASTConsumer() {
    return std::unique_ptr<clang::ASTConsumer>(
        new RewriteSourceConsumer(target_lines, output));
  }

private:
  const std::set<unsigned int> *target_lines;
  std::pair<std::vector<printer::FileSlice>, std::vector<printer::FileSlice>>
      &output;
};

} // namespace

std::pair<std::vector<printer::FileSlice>, std::vector<printer::FileSlice>>
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
  // Add an include that clang tooling seems to have trouble finding in some
  // cases
  const auto Adjuster = clang::tooling::getInsertArgumentAdjuster(
      "-isystem/usr/local/llvm-10/lib/clang/10.0.1/include",
      clang::tooling::ArgumentInsertPosition::BEGIN);
  Tool.appendArgumentsAdjuster(Adjuster);
  std::pair<std::vector<printer::FileSlice>, std::vector<printer::FileSlice>>
      buffer;

  std::cerr << file << std::endl;

  Tool.run(clang::tooling::newFrontendActionFactory<RewriteSourceAction>(
               new RewriteSourceAction(target_lines, buffer))
               .get());
  printer::mergeAndSplitSlices(buffer.first);
  if (!buffer.second.empty()) {
    printer::mergeSlices(buffer.second);
  }
  return buffer;
}
