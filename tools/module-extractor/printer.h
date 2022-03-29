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

/**
 * Checks if any of the lines contained in Lines is between the Begin and End
 * Location of Decl (inclusive)
 * @tparam T
 * @param Decl
 * @param Lines
 * @param SM
 * @return
 */
template <typename T>
bool isAnyInWhitelist(const T *Decl, const std::set<unsigned int> &Lines,
                      const clang::SourceManager &SM) {
  unsigned int Begin = SM.getPresumedLineNumber(Decl->getBeginLoc());
  unsigned int End = SM.getPresumedLineNumber(Decl->getEndLoc());
  assert(Begin <= End);
  return Lines.lower_bound(Begin) != Lines.upper_bound(End);
}

/**
 * Check if Decl is in the main source file
 * @tparam T
 * @param Decl
 * @param SM
 * @return
 */
template <typename T>
bool isInSourceFile(const T *Decl, const clang::SourceManager &SM) {
  return SM.getFileID(SM.getSpellingLoc(Decl->getBeginLoc())) ==
         SM.getMainFileID();
}

/**
 * A generated slice, based on clang SourceLocations
 */
struct Slice {
  Slice();
  Slice(clang::SourceLocation Begin, clang::SourceLocation End,
        bool NeedsDefine = false);
  Slice(clang::SourceLocation Begin, clang::SourceLocation End,
        std::vector<Slice> Keep);
  static Slice generateFromStartAndNext(clang::SourceLocation Start,
                                        clang::SourceLocation Next,
                                        const clang::SourceManager &SM,
                                        const clang::LangOptions &LO);
  clang::SourceLocation Begin;
  clang::SourceLocation End;
  bool NeedsDefine =
      false; // True if the Slice should be commented out. If it is a header
             // slice, true means that it is a function definition where we need
             // to cut away the body.
  std::vector<Slice> Keep; // Slices that we need to extract a function if we
                           // rewrite it completely
};

/**
 * An offset into a file (line and column based)
 */
struct FileOffset {
  FileOffset(const clang::PresumedLoc &Loc);

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
  bool operator==(const FileOffset &rhs) const;
  bool operator!=(const FileOffset &rhs) const;
  bool operator<(const FileOffset &rhs) const;
  bool operator>(const FileOffset &rhs) const;
  bool operator<=(const FileOffset &rhs) const;
  bool operator>=(const FileOffset &rhs) const;

private:
  unsigned int Line;
  unsigned int Column;
};

/**
 * A File based slice
 * End is just one character after the target slice
 */
struct FileSlice {
  // FileSlice(FileOffset Begin, FileOffset End);
  FileSlice(const Slice &Slice, const clang::SourceManager &SM);
  friend std::ostream &operator<<(std::ostream &os, const FileSlice &slice);
  bool operator==(const FileSlice &rhs) const;
  bool operator!=(const FileSlice &rhs) const;
  FileOffset Begin;
  FileOffset End;
  bool NeedsDefine = false;
  std::vector<FileSlice> Keep;
};

/**
 * Merge adjacent slices together (inplace)
 * @param Slices
 */
void mergeSlices(std::vector<FileSlice> &Slices);

/**
 * Merge adjacent slices together if they need a define (inplace)
 * @param Slices
 */
void mergeAndSplitSlices(std::vector<FileSlice> &Slices);

/** Extract code from FileIn bases on Slices and write it to Output
 *
 * @param FileIn
 * @param Output
 * @param Slices Slices to put into define code
 */
void extractSlicesDefine(const std::string &FileIn, std::ofstream &Output,
                         const std::vector<FileSlice> &Slices);

/** Extract a function that got rewritten by the slicer
 *
 * @param Lines Lines of the file to slice
 * @param Slices Slices of the rewritten function
 * @param Output
 */
void extractRewrittenFunction(const std::vector<std::string> &Lines,
                              const std::vector<FileSlice> &Slices,
                              std::ofstream &Output);

/**
 * Create a header for a a sliced file
 * @param FileIn The original source file
 * @param FileOut  The header file to write to
 * @param Slices  Slices containing information about types that may need to be
 * copied
 * @param FileName Name of the original source file (used for creating include
 * guards)
 * @param includes Includes that need to be copied
 */
void extractHeaderSlices(const std::string &FileIn, const std::string &FileOut,
                         const std::vector<FileSlice> &Slices,
                         const std::string &FileName,
                         const std::vector<std::string> &includes);

/**
 * StmtVisitor, walking over the AST and collecting the source locations of
 * slices to extract
 */
class StmtPrinterFiltering
    : public clang::ConstStmtVisitor<StmtPrinterFiltering, bool> {
public:
  bool VisitCompoundStmt(const clang::CompoundStmt *Stmt);
  bool VisitStmt(const clang::Stmt *stmt);
  bool VisitWhileStmt(const clang::WhileStmt *Stmt);
  bool VisitForStmt(const clang::ForStmt *Stmt);
  bool VisitDoStmt(const clang::DoStmt *Stmt);
  bool VisitSwitchStmt(const clang::SwitchStmt *Stmt);
  bool VisitIfStmt(const clang::IfStmt *Stmt);
  bool VisitCaseStmt(const clang::CaseStmt *Stmt);
  bool VisitDefaultStmt(const clang::DefaultStmt *Stmt);
  static std::vector<Slice>
  GetSlices(const clang::Stmt *Stmt, const std::set<unsigned int> &TargetLines,
            const clang::ASTContext &CTX, const clang::SourceManager &SM,
            const clang::LangOptions &LO, unsigned *filtered = nullptr);

private:
  StmtPrinterFiltering(const std::set<unsigned int> &TargetLines,
                       const clang::SourceManager &SM,
                       const clang::LangOptions &LO);

  void PrintStmt(const clang::Stmt *Stmt, bool required = false);

  unsigned int Filtered = 0;
  std::vector<printer::Slice> Slices;
  const std::set<unsigned int> &TargetLines;
  const clang::SourceManager &SM;
  const clang::LangOptions &LO;
};

/**
 * DeclVisitor, walking over the AST and collecting the source locations of
 * Slices to extract
 */
class DeclPrinterFiltering
    : public clang::ConstDeclVisitor<DeclPrinterFiltering> {
public:
  void VisitFunctionDecl(const clang::FunctionDecl *decl);
  void VisitVarDecl(const clang::VarDecl *decl);
  void VisitDecl(const clang::Decl *decl);
  void VisitTypeDecl(const clang::TypeDecl *Decl);
  void VisitTranslationUnitDecl(const clang::TranslationUnitDecl *decl);

  /**
   *
   * @param Decl
   * @param TargetLines
   * @param CTX
   * @param SM
   * @param LO
   * @return First are the file slices, then the ones required for the header
   */
  static std::pair<std::vector<Slice>, std::vector<Slice>>
  GetSlices(const clang::Decl *Decl, const std::set<unsigned int> &TargetLines,
            const clang::ASTContext &CTX, const clang::SourceManager &SM,
            const clang::LangOptions &LO);

  /**
   *
   * @param Decl
   * @param TargetLines
   * @param CTX
   * @return  * @return First are the file slices, then the ones required for
   * the header
   */
  static std::pair<std::vector<printer::FileSlice>,
                   std::vector<printer::FileSlice>>
  GetFileSlices(const clang::Decl *Decl,
                const std::set<unsigned int> &TargetLines,
                const clang::ASTContext &CTX);

private:
  DeclPrinterFiltering(const std::set<unsigned int> &Target,
                       const clang::ASTContext &CTX,
                       const clang::SourceManager &SM,
                       const clang::LangOptions &LO);
  std::vector<printer::Slice> Slices;
  std::vector<printer::Slice> HeaderSlices;
  const std::set<unsigned int> &TargetLines;
  const clang::ASTContext &CTX;
  const clang::SourceManager &SM;
  const clang::LangOptions &LO;
};
} // namespace printer

#endif // PHASAR_PRINTER_H
