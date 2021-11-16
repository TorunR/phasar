//
// Created by jr on 12.11.21.
//

#ifndef PHASAR_PRINTER_H
#define PHASAR_PRINTER_H

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclVisitor.h>
#include <clang/AST/StmtVisitor.h>
#include <clang/Basic/SourceManager.h>

#include <cassert>
#include <set>
#include <utility>
#include <vector>

namespace printer {

template <typename T>
bool isAnyInWhitelist(const T *Decl, const std::set<unsigned int> &Lines,
                      const clang::SourceManager &SM) {
  unsigned int Begin = SM.getPresumedLineNumber(Decl->getBeginLoc());
  unsigned int End = SM.getPresumedLineNumber(Decl->getEndLoc());
  assert(Begin <= End);
  return Lines.lower_bound(Begin) != Lines.upper_bound(End);
}

enum class Terminator { None, Semicolon };

struct Slice {
  Slice(clang::SourceLocation begin, clang::SourceLocation end,
        Terminator terminator);
  const clang::SourceLocation Begin;
  const clang::SourceLocation End;
  const Terminator Terminator;
};

} // namespace printer

class StmtPrinterFiltering
    : public clang::ConstStmtVisitor<StmtPrinterFiltering> {
public:
  StmtPrinterFiltering(const std::set<unsigned int> &targetLines);
  void VisitCompoundStmt(const clang::CompoundStmt *stmt);
  void VisitStmt(const clang::Stmt *stmt);

private:
  const std::set<unsigned int> &TargetLines;
};
class DeclPrinter : public clang::ConstDeclVisitor<DeclPrinter> {
public:
  DeclPrinter(const std::set<unsigned int> &targetLines,
              const clang::ASTContext &ctx);
  void VisitFunctionDecl(const clang::FunctionDecl *decl);
  void VisitDecl(const clang::Decl *decl);

private:
  std::vector<printer::Slice> Slices;
  const std::set<unsigned int> &TargetLines;
  const clang::SourceManager &SM;
  const clang::ASTContext &CTX;
};

#endif // PHASAR_PRINTER_H
