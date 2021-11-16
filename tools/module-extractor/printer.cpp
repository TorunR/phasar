//
// Created by jr on 12.11.21.
//

#include "printer.h"
void DeclPrinter::VisitFunctionDecl(const clang::FunctionDecl *decl) {
  assert(decl->hasBody());
  if(printer::isAnyInWhitelist(decl, TargetLines, SM)) {

  }
  llvm_unreachable("test");
}
void DeclPrinter::VisitDecl(const clang::Decl *decl) {
  llvm_unreachable("Hit unsupported decl");
}
DeclPrinter::DeclPrinter(const std::set<unsigned int> &targetLines,
                         const clang::ASTContext &ctx)
    : TargetLines(targetLines), SM(ctx.getSourceManager()), CTX(ctx) {}
void StmtPrinterFiltering::VisitStmt(const clang::Stmt *stmt) {
  llvm_unreachable("Hit unsupported stmt");
}
void StmtPrinterFiltering::VisitCompoundStmt(const clang::CompoundStmt *stmt) {
  llvm_unreachable("unimplemented");
}
StmtPrinterFiltering::StmtPrinterFiltering(const std::set<unsigned int> &targetLines)
    : TargetLines(targetLines) {}
printer::Slice::Slice(clang::SourceLocation begin,
                      clang::SourceLocation end,
                      printer::Terminator terminator)
    : Begin(begin), End(end), Terminator(terminator) {}
