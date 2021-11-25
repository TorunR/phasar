//
// Created by jr on 12.11.21.
//

#ifndef PHASAR_PRINTER_H
#define PHASAR_PRINTER_H

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclVisitor.h>
#include <clang/AST/StmtVisitor.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>

#include <cassert>
#include <ostream>
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

struct Slice {
  Slice(clang::SourceLocation Begin, clang::SourceLocation End);
  const clang::SourceLocation Begin;
  const clang::SourceLocation End;
};

struct FileOffset {
  FileOffset(unsigned int Line, unsigned int Column);
  FileOffset(const clang::PresumedLoc &Loc);
  bool operator<(const FileOffset &rhs) const;
  bool operator>(const FileOffset &rhs) const;
  bool operator<=(const FileOffset &rhs) const;
  bool operator>=(const FileOffset &rhs) const;
  friend std::ostream &operator<<(std::ostream &os, const FileOffset &offset);

  /**
   *
   * @return Zero based column offset
   */
  unsigned int GetSliceColumn() const;

  /**
   *
   * @return Zero based line offset
   */
  unsigned int GetSliceLine() const;

private:
  unsigned int Line;
  unsigned int Column;
};

/**
 * End is just one character after the target slice
 */
struct FileSlice {
  FileSlice(FileOffset Begin, FileOffset End);
  FileSlice(const Slice &Slice, const clang::SourceManager &SM);
  friend std::ostream &operator<<(std::ostream &os, const FileSlice &slice);
  FileOffset Begin;
  FileOffset End;
};

void mergeSlices(std::vector<FileSlice> &Slices);

void extractSlices(const std::string &FileIn, const std::string &FileOut,
                   const std::vector<FileSlice> &Slices);

// namespace printer

class StmtPrinterFiltering
    : public clang::ConstStmtVisitor<StmtPrinterFiltering> {
public:
  StmtPrinterFiltering(const std::set<unsigned int> &targetLines);
  void VisitCompoundStmt(const clang::CompoundStmt *stmt);
  void VisitStmt(const clang::Stmt *stmt);
  void VisitWhileStmt(const clang::WhileStmt *Stmt);
  void VisitForStmt(const clang::ForStmt *Stmt);
  void VisitDoStmt(const clang::DoStmt *Stmt);
  void VisitSwitchStmt(const clang::SwitchStmt *Stmt);
  void VisitReturnStmt(const clang::ReturnStmt *Stmt);

private:
  const std::set<unsigned int> &TargetLines;
};
class DeclPrinter : public clang::ConstDeclVisitor<DeclPrinter> {
public:
  DeclPrinter(const std::set<unsigned int> &targetLines,
              const clang::ASTContext &ctx);
  void VisitFunctionDecl(const clang::FunctionDecl *decl);
  void VisitDecl(const clang::Decl *decl);
  void VisitTranslationUnitDecl(const clang::TranslationUnitDecl *decl);
  std::vector<FileSlice> GetSlices() const;

private:
  std::vector<printer::Slice> Slices;
  const std::set<unsigned int> &TargetLines;
  const clang::SourceManager &SM;
  const clang::ASTContext &CTX;
};

} // namespace printer

#endif // PHASAR_PRINTER_H
