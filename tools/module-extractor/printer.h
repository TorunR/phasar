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

template <typename T>
bool isInSourceFile(const T *Decl, const clang::SourceManager &SM) {
  if (SM.getFileID(SM.getSpellingLoc(Decl->getBeginLoc())) !=
      SM.getMainFileID()) {
    return false;
  }
  return true;
}

struct Slice {
  Slice(clang::SourceLocation Begin, clang::SourceLocation End, bool NeedsDefine = false);
  static Slice generateFromStartAndNext(clang::SourceLocation Start,
                                        clang::SourceLocation Next,
                                        const clang::SourceManager &SM,
                                        const clang::LangOptions &LO);
  clang::SourceLocation Begin;
  clang::SourceLocation End;
  bool NeedsDefine = false;
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
  //FileSlice(FileOffset Begin, FileOffset End);
  FileSlice(const Slice &Slice, const clang::SourceManager &SM);
  friend std::ostream &operator<<(std::ostream &os, const FileSlice &slice);
  FileOffset Begin;
  FileOffset End;
  bool NeedsDefine = false;
};

void mergeSlices(std::vector<FileSlice> &Slices);
void mergeAndSplitSlices(std::vector<FileSlice> &Slices);

/**
 *
 * @param FileIn
 * @param FileOut
 * @param Slices Slices to slices
 */
void extractSlices(const std::string &FileIn, const std::string &FileOut,
                   const std::vector<FileSlice> &Slices);

/**
 *
 * @param FileIn
 * @param FileOut
 * @param Slices Slices to put into define code
 */
void extractSlicesDefine(const std::string &FileIn, const std::string &FileOut, const std::vector<FileSlice> &Slices);

class StmtPrinterFiltering
    : public clang::ConstStmtVisitor<StmtPrinterFiltering, bool> {
public:
  bool VisitCompoundStmt(const clang::CompoundStmt *Stmt);
  bool VisitStmt(const clang::Stmt *stmt);
  bool VisitWhileStmt(const clang::WhileStmt *Stmt);
  bool VisitForStmt(const clang::ForStmt *Stmt);
  bool VisitDoStmt(const clang::DoStmt *Stmt);
  bool VisitSwitchStmt(const clang::SwitchStmt *Stmt);
  // bool VisitReturnStmt(const clang::ReturnStmt *Stmt);
  bool VisitIfStmt(const clang::IfStmt *Stmt);
  // bool VisitSwitchCase(const clang::SwitchCase *Stmt);
  bool VisitCaseStmt(const clang::CaseStmt *Stmt);
  bool VisitDefaultStmt(const clang::DefaultStmt *Stmt);
  static std::vector<Slice> GetSlices(const clang::Stmt *Stmt,
                                      const std::set<unsigned int> &TargetLines,
                                      const clang::ASTContext &CTX,
                                      const clang::SourceManager &SM,
                                      const clang::LangOptions &LO);

private:
  StmtPrinterFiltering(const std::set<unsigned int> &TargetLines,
                       const clang::ASTContext &CTX,
                       const clang::SourceManager &SM,
                       const clang::LangOptions &LO);

  void PrintStmt(const clang::Stmt *Stmt, bool required = false);

  std::vector<printer::Slice> Slices;
  const std::set<unsigned int> &TargetLines;
  const clang::ASTContext &CTX;
  const clang::SourceManager &SM;
  const clang::LangOptions &LO;
};
class DeclPrinter : public clang::ConstDeclVisitor<DeclPrinter> {
public:
  void VisitFunctionDecl(const clang::FunctionDecl *decl);
  void VisitVarDecl(const clang::VarDecl *decl);
  void VisitDecl(const clang::Decl *decl);
  void VisitTypeDecl(const clang::TypeDecl *Decl);
  void VisitTranslationUnitDecl(const clang::TranslationUnitDecl *decl);

  static std::vector<Slice> GetSlices(const clang::Decl *Decl,
                                      const std::set<unsigned int> &TargetLines,
                                      const clang::ASTContext &CTX,
                                      const clang::SourceManager &SM,
                                      const clang::LangOptions &LO);

  static std::vector<FileSlice>
  GetFileSlices(const clang::Decl *Decl,
                const std::set<unsigned int> &TargetLines,
                const clang::ASTContext &CTX);

private:
  DeclPrinter(const std::set<unsigned int> &Target,
              const clang::ASTContext &CTX, const clang::SourceManager &SM,
              const clang::LangOptions &LO);
  std::vector<printer::Slice> Slices;
  const std::set<unsigned int> &TargetLines;
  const clang::ASTContext &CTX;
  const clang::SourceManager &SM;
  const clang::LangOptions &LO;
};

} // namespace printer

#endif // PHASAR_PRINTER_H
