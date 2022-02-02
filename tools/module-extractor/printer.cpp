//
// Created by jr on 12.11.21.
//

#include "printer.h"
#include "source_utils.h"

#include <clang/Basic/TokenKinds.h>
#include <clang/Lex/Lexer.h>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

// Config
#define EXTRACT_TYPES (true)
#define EXTRACT_FUNCTION_DECLS (true)
#define MIN_FILTERED_FOR_EXTRA_FUNCTION (3)
#define EXTRACT_TYPES_INTO_HEADER (true)

namespace printer {
void DeclPrinter::VisitFunctionDecl(const clang::FunctionDecl *decl) {
  if (!decl->doesThisDeclarationHaveABody()) {
    if (isInSourceFile(decl, SM)) {
      const auto Semicolon =
          utils::getSemicolonAfterStmtEndLoc(decl->getEndLoc(), SM, LO);
      Slice S;
      if (Semicolon.isValid()) {
        S = Slice(SM.getExpansionRange(decl->getBeginLoc()).getBegin(),
                  utils::getEndOfToken(Semicolon, SM, LO));
      } else {
        S = Slice(
            SM.getExpansionRange(decl->getBeginLoc()).getBegin(),
            utils::getEndOfToken(
                SM.getExpansionRange(decl->getEndLoc()).getBegin(), SM, LO));
      }
      if (!isAnyInWhitelist(decl, TargetLines, SM) && !EXTRACT_FUNCTION_DECLS) {
        S.NeedsDefine = true;
      }
      Slices.push_back(S);
    }

  } else {
    if (isInSourceFile(decl, SM)) {
      if (printer::isAnyInWhitelist(decl, TargetLines, SM)) {
        unsigned filtered;
        const auto Body = StmtPrinterFiltering::GetSlices(
            decl->getBody(), TargetLines, CTX, SM, LO, &filtered);
        std::vector<Slice> TmpSlices;
        TmpSlices.emplace_back(
            SM.getExpansionRange(decl->getBeginLoc()).getBegin(),
            utils::getEndOfToken(
                SM.getExpansionRange(decl->getBody()->getBeginLoc()).getBegin(),
                SM, LO));
        TmpSlices.emplace_back(
            SM.getExpansionRange(decl->getBody()->getEndLoc()).getEnd(),
            utils::getEndOfToken(
                SM.getExpansionRange(decl->getEndLoc()).getEnd(), SM, LO));
        TmpSlices.insert(TmpSlices.end(), Body.begin(), Body.end());
        if (filtered >= MIN_FILTERED_FOR_EXTRA_FUNCTION) {
          Slices.emplace_back(
              SM.getExpansionRange(decl->getBeginLoc()).getBegin(),
              utils::getEndOfToken(
                  SM.getExpansionRange(decl->getEndLoc()).getEnd(), SM, LO),
              TmpSlices);
        } else {
          Slices.insert(Slices.end(), TmpSlices.begin(), TmpSlices.end());
        }

        auto FirstDecl = decl->getFirstDecl();
        assert(FirstDecl);
        if (!isInSourceFile(FirstDecl, SM)) {
          FirstDecl = decl;
        }
        if (FirstDecl->doesThisDeclarationHaveABody()) {
          auto S = Slice::generateFromStartAndNext(
              FirstDecl->getBeginLoc(), FirstDecl->getBody()->getBeginLoc(), SM,
              LO);
          S.NeedsDefine = true;
          HeaderSlices.push_back(S);
        } else {
          const auto Semicolon = utils::getSemicolonAfterStmtEndLoc(
              FirstDecl->getEndLoc(), SM, LO);
          Slice S;
          if (Semicolon.isValid()) {
            S = Slice(SM.getExpansionRange(FirstDecl->getBeginLoc()).getBegin(),
                      utils::getEndOfToken(Semicolon, SM, LO));
          } else {
            S = Slice(SM.getExpansionRange(FirstDecl->getBeginLoc()).getBegin(),
                      utils::getEndOfToken(
                          SM.getExpansionRange(FirstDecl->getEndLoc()).getEnd(),
                          SM, LO));
          }
          S.NeedsDefine = false;
          HeaderSlices.push_back(S);
        }

      } else {
        Slices.emplace_back(
            SM.getExpansionRange(decl->getBeginLoc()).getBegin(),
            utils::getEndOfToken(
                SM.getExpansionRange(decl->getEndLoc()).getEnd(), SM, LO),
            true);
      }
    }
  }
}
void DeclPrinter::VisitDecl(const clang::Decl *decl) {
  if (!decl->isImplicit()) {
    // TODO
    // llvm_unreachable("Hit unsupported decl");
  }
}

void DeclPrinter::VisitTranslationUnitDecl(
    const clang::TranslationUnitDecl *decl) {
  // decl->dump();
  for (const auto &D : decl->decls()) {
    Visit(D);
  }
}

DeclPrinter::DeclPrinter(const std::set<unsigned int> &Target,
                         const clang::ASTContext &CTX,
                         const clang::SourceManager &SM,
                         const clang::LangOptions &LO)
    : TargetLines(Target), CTX(CTX), SM(SM), LO(LO) {}
std::pair<std::vector<Slice>, std::vector<Slice>> DeclPrinter::GetSlices(
    const clang::Decl *Decl, const std::set<unsigned int> &TargetLines,
    const clang::ASTContext &CTX, const clang::SourceManager &SM,
    const clang::LangOptions &LO) {
  DeclPrinter Visitor(TargetLines, CTX, SM, LO);
  Visitor.Visit(Decl);
  return {Visitor.Slices, Visitor.HeaderSlices};
}
std::pair<std::vector<printer::FileSlice>, std::vector<printer::FileSlice>>
DeclPrinter::GetFileSlices(const clang::Decl *Decl,
                           const std::set<unsigned int> &TargetLines,
                           const clang::ASTContext &CTX) {
  const auto &SM = CTX.getSourceManager();
  const auto Slices =
      DeclPrinter::GetSlices(Decl, TargetLines, CTX, SM, CTX.getLangOpts());
  std::pair<std::vector<printer::FileSlice>, std::vector<printer::FileSlice>>
      Result;
  Result.first.reserve(Slices.first.size());
  for (const auto &S : Slices.first) {
    Result.first.emplace_back(S, SM);
  }
  Result.second.reserve(Slices.second.size());
  for (const auto &S : Slices.second) {
    Result.second.emplace_back(S, SM);
  }
  return Result;
}
void DeclPrinter::VisitVarDecl(const clang::VarDecl *Decl) {
  //  Decl->dump();
  //  Decl->getSourceRange().getEnd().dump(SM);
  //  Decl->getEndLoc().dump(SM);
  if (isInSourceFile(Decl, SM)) {
    // Decl->dump();
    const auto Semicolon = utils::getSemicolonAfterStmtEndLoc(
        Decl->getSourceRange().getEnd(), SM, LO);
    Slice S;
    if (Semicolon.isValid()) {
      S = Slice(SM.getExpansionRange(Decl->getBeginLoc()).getBegin(),
                utils::getEndOfToken(Semicolon, SM, LO));
    } else {
      S = Slice(SM.getExpansionRange(Decl->getBeginLoc()).getBegin(),
                utils::getEndOfToken(
                    SM.getExpansionRange(Decl->getEndLoc()).getEnd(), SM, LO));
    }

    if (!isAnyInWhitelist(Decl, TargetLines, SM)) {
      S.NeedsDefine = true;
      // Decl->dump();
    }
    Slices.push_back(S);
  }
}
void DeclPrinter::VisitTypeDecl(const clang::TypeDecl *Decl) {
  if (isInSourceFile(Decl, SM)) {
    const auto Semicolon =
        utils::getSemicolonAfterStmtEndLoc(Decl->getEndLoc(), SM, LO);
    Slice S;
    if (Semicolon.isValid()) {
      S = Slice(SM.getExpansionRange(Decl->getBeginLoc()).getBegin(),
                utils::getEndOfToken(Semicolon, SM, LO));
    } else {
      S = Slice(SM.getExpansionRange(Decl->getBeginLoc()).getBegin(),
                utils::getEndOfToken(
                    SM.getExpansionRange(Decl->getEndLoc()).getEnd(), SM, LO));
    }

    if (!isAnyInWhitelist(Decl, TargetLines, SM) && !EXTRACT_TYPES) {
      S.NeedsDefine = true;
    }
    Slices.push_back(S);
#ifdef EXTRACT_TYPES_INTO_HEADER
    S.NeedsDefine = false;
    HeaderSlices.push_back(S);
#endif
  }
}

bool StmtPrinterFiltering::VisitStmt(const clang::Stmt *Stmt) {
  if (isAnyInWhitelist(Stmt, TargetLines, SM)) {
    Slices.emplace_back(
        SM.getExpansionRange(Stmt->getBeginLoc()).getBegin(),
        utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(),
                             SM, LO));
    return true;
  }
  Filtered++;
  Slices.emplace_back(
      SM.getExpansionRange(Stmt->getBeginLoc()).getBegin(),
      utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(), SM,
                           LO),
      true);
  // llvm_unreachable("Hit unsupported stmt");
  return false;
}
bool StmtPrinterFiltering::VisitCompoundStmt(const clang::CompoundStmt *Stmt) {
  Slices.emplace_back(
      SM.getExpansionRange(Stmt->getBeginLoc()).getBegin(),
      utils::getEndOfToken(SM.getExpansionRange(Stmt->getLBracLoc()).getBegin(),
                           SM, LO));
  Slices.emplace_back(
      SM.getExpansionRange(Stmt->getRBracLoc()).getEnd(),
      utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(), SM,
                           LO));
  for (const auto *I : Stmt->body()) {
    PrintStmt(I);
  }
  return true;
}
StmtPrinterFiltering::StmtPrinterFiltering(
    const std::set<unsigned int> &TargetLines, const clang::ASTContext &CTX,
    const clang::SourceManager &SM, const clang::LangOptions &LO)
    : TargetLines(TargetLines), CTX(CTX), SM(SM), LO(LO) {}
bool StmtPrinterFiltering::VisitWhileStmt(const clang::WhileStmt *Stmt) {
  if (isAnyInWhitelist(Stmt, TargetLines, SM)) {

    //    Stmt->getBeginLoc().dump(SM);
    //    //   // Stmt->getBeginLoc()).dump(SM);
    //    //    SM.getExpansionLoc(Stmt->getBeginLoc()).dump(SM);
    //    //    Stmt->getEndLoc().dump(SM);
    //    //    //Stmt->getEndLoc()).dump(SM);
    //    //    SM.getExpansionLoc(Stmt->getEndLoc()).dump(SM);
    //    Stmt->getBody()->getBeginLoc().dump(SM);
    //    SM.getExpansionLoc(Stmt->getBody()->getBeginLoc()).dump(SM);
    //    utils::findPreviousTokenStart(SM.getFileLoc(Stmt->getBody()->getBeginLoc()),
    //                                  SM, LO)
    //        .dump(SM);
    //
    //    utils::getEndOfToken(
    //        utils::findPreviousTokenStart(
    //            SM.getFileLoc(Stmt->getBody()->getBeginLoc()), SM, LO),
    //        SM, LO)
    //        .dump(SM);
    //    SM.getSpellingLoc(
    //          utils::getEndOfToken(utils::findPreviousTokenStart(
    //                                   Stmt->getBody()->getBeginLoc(), SM,
    //                                   LO),
    //                               SM, LO))
    //        .dump(SM);

    Slices.emplace_back(Slice::generateFromStartAndNext(
        Stmt->getBeginLoc(), Stmt->getBody()->getBeginLoc(), SM, LO));
    //        SM.getSpellingLoc(Stmt->getBeginLoc()),
    //        utils::getEndOfToken(utils::findPreviousTokenStart(
    //                                 Stmt->getBody()->getBeginLoc(), SM, LO),
    //                             SM, LO));
    PrintStmt(Stmt->getBody(), true);
    return true;
  }
  Filtered++;
  Slices.emplace_back(
      SM.getExpansionRange(Stmt->getBeginLoc()).getBegin(),
      utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(), SM,
                           LO),
      true);
  return false;
}
bool StmtPrinterFiltering::VisitForStmt(const clang::ForStmt *Stmt) {
  if (isAnyInWhitelist(Stmt, TargetLines, SM)) {
    Slices.emplace_back(Slice::generateFromStartAndNext(
        Stmt->getBeginLoc(), Stmt->getBody()->getBeginLoc(), SM, LO));
    PrintStmt(Stmt->getBody(), true);
    return true;
  }
  Filtered++;
  Slices.emplace_back(
      SM.getExpansionRange(Stmt->getBeginLoc()).getBegin(),
      utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(), SM,
                           LO),
      true);
  return false;
}
bool StmtPrinterFiltering::VisitDoStmt(const clang::DoStmt *Stmt) {
  if (isAnyInWhitelist(Stmt, TargetLines, SM)) {
    Slices.emplace_back(Slice::generateFromStartAndNext(
        Stmt->getBeginLoc(), Stmt->getBody()->getBeginLoc(), SM, LO));
    PrintStmt(Stmt->getBody(), true);
    Slices.emplace_back(
        SM.getExpansionRange(Stmt->getWhileLoc()).getBegin(),
        utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(),
                             SM, LO));
    return true;
  }
  Filtered++;
  Slices.emplace_back(
      SM.getExpansionRange(Stmt->getBeginLoc()).getBegin(),
      utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(), SM,
                           LO),
      true);
  return false;
}
bool StmtPrinterFiltering::VisitSwitchStmt(const clang::SwitchStmt *Stmt) {
  if (isAnyInWhitelist(Stmt, TargetLines, SM)) {
    Slices.emplace_back(Slice::generateFromStartAndNext(
        Stmt->getBeginLoc(), Stmt->getBody()->getBeginLoc(), SM, LO));
    // Stmt->getBody()->dump();
    PrintStmt(Stmt->getBody(), true);
    return true;
  }
  Filtered++;
  Slices.emplace_back(
      SM.getExpansionRange(Stmt->getBeginLoc()).getBegin(),
      utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(), SM,
                           LO),
      true);
  return false;
}

// bool StmtPrinterFiltering::VisitReturnStmt(const clang::ReturnStmt *Stmt) {
//   if (isAnyInWhitelist(Stmt, TargetLines, SM)) {
//     Slices.emplace_back(
//         SM.getExpansionRange(Stmt->getBeginLoc()).getBegin(),
//         utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(),
//                              SM, LO));
//     const auto Semicolon =
//         utils::getSemicolonAfterStmtEndLoc(Stmt->getEndLoc(), SM, LO);
//     assert(Semicolon.isValid());
//     Slices.emplace_back(SM.getExpansionRange(Stmt->getEndLoc()).getBegin(),
//                         utils::getEndOfToken(Semicolon, SM, LO));
//     return true;
//   }
//   return false;
// }
std::vector<Slice> StmtPrinterFiltering::GetSlices(
    const clang::Stmt *Stmt, const std::set<unsigned int> &TargetLines,
    const clang::ASTContext &CTX, const clang::SourceManager &SM,
    const clang::LangOptions &LO, unsigned *filtered) {
  StmtPrinterFiltering Visitor(TargetLines, CTX, SM, LO);
  Visitor.Visit(Stmt);
  if (filtered != nullptr) {
    *filtered = Visitor.Filtered;
  }
  return Visitor.Slices;
}
void StmtPrinterFiltering::PrintStmt(const clang::Stmt *Stmt, bool required) {
  if (Stmt &&
      (llvm::isa<clang::Expr>(Stmt) || llvm::isa<clang::ContinueStmt>(Stmt) ||
       llvm::isa<clang::BreakStmt>(Stmt) ||
       llvm::isa<clang::ReturnStmt>(Stmt) ||
       llvm::isa<clang::GotoStmt>(Stmt))) {
    bool visited = Visit(Stmt);
    auto Semicolon =
        utils::getSemicolonAfterStmtEndLoc(Stmt->getEndLoc(), SM, LO);
    if (!Semicolon.isValid()) {
      Semicolon = SM.getExpansionRange(Stmt->getEndLoc()).getEnd();
    }
    if (visited || required) {

      if (visited) {
        Slices.emplace_back(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(),
                            utils::getEndOfToken(Semicolon, SM, LO));
      } else {
        Slices.emplace_back(Semicolon, utils::getEndOfToken(Semicolon, SM, LO));
      }
    } else {
      Filtered++;
      Slices.emplace_back(Semicolon, utils::getEndOfToken(Semicolon, SM, LO),
                          true);
    }
  } else if (Stmt) {
    // Stmt->dump();
    Visit(Stmt);
  } else {
    llvm_unreachable("Null Stmt");
  }
}
bool StmtPrinterFiltering::VisitIfStmt(const clang::IfStmt *Stmt) {
  if (isAnyInWhitelist(Stmt, TargetLines, SM)) {
    //    Stmt->getBeginLoc().dump(SM);
    //    Stmt->getEndLoc().dump(SM);
    //    Stmt->dump();
    Slices.emplace_back(Slice::generateFromStartAndNext(
        Stmt->getBeginLoc(), Stmt->getThen()->getBeginLoc(), SM, LO));
    PrintStmt(Stmt->getThen(), true);
    if (Stmt->getElse() && isAnyInWhitelist(Stmt->getElse(), TargetLines, SM)) {
      //      Stmt->getElseLoc().dump(SM);
      //      Stmt->getEndLoc().dump(SM);
      //      SM.getImmediateExpansionRange(Stmt->getEndLoc()).getEnd().dump(SM);
      //      SM.getExpansionRange(Stmt->getEndLoc()).getEnd().dump(SM);
      Slices.emplace_back(Slice::generateFromStartAndNext(
          Stmt->getElseLoc(), Stmt->getElse()->getBeginLoc(), SM, LO));
      PrintStmt(Stmt->getElse(), true);
    } else if (Stmt->getElse()) {
      Filtered++;
      Slices.emplace_back(
          SM.getExpansionRange(Stmt->getElse()->getBeginLoc()).getBegin(),
          utils::getEndOfToken(
              SM.getExpansionRange(Stmt->getElse()->getEndLoc()).getEnd(), SM,
              LO),
          true);
    }
    return true;
  }
  Filtered++;
  Slices.emplace_back(
      SM.getExpansionRange(Stmt->getBeginLoc()).getBegin(),
      utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(), SM,
                           LO),
      true);
  return false;
}
bool StmtPrinterFiltering::VisitCaseStmt(const clang::CaseStmt *Stmt) {
  if (isAnyInWhitelist(Stmt, TargetLines, SM)) {
    Slices.emplace_back(Slice::generateFromStartAndNext(
        Stmt->getBeginLoc(), Stmt->getSubStmt()->getBeginLoc(), SM, LO));
    PrintStmt(Stmt->getSubStmt());
    return true;
  }
  Filtered++;
  Slices.emplace_back(
      SM.getExpansionRange(Stmt->getBeginLoc()).getBegin(),
      utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(), SM,
                           LO),
      true);

  return false;
}
bool StmtPrinterFiltering::VisitDefaultStmt(const clang::DefaultStmt *Stmt) {
  if (isAnyInWhitelist(Stmt, TargetLines, SM)) {
    Slices.emplace_back(Slice::generateFromStartAndNext(
        Stmt->getBeginLoc(), Stmt->getSubStmt()->getBeginLoc(), SM, LO));
    PrintStmt(Stmt->getSubStmt());
    return true;
  }
  Filtered++;
  Slices.emplace_back(
      SM.getExpansionRange(Stmt->getBeginLoc()).getBegin(),
      utils::getEndOfToken(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(), SM,
                           LO),
      true);
  return false;
}

// bool StmtPrinterFiltering::VisitSwitchCase(const clang::SwitchCase *Stmt) {
//   if(isAnyInWhitelist(Stmt, TargetLines, SM)) {
//     //Slices.emplace_back(Slice::generateFromStartAndNext(Stmt->getBeginLoc(),
//     Stmt.g)) return true;
//   }
//   return false;
// }

printer::Slice::Slice(clang::SourceLocation Begin, clang::SourceLocation End,
                      bool NeedsDefine)
    : Begin(Begin), End(End), NeedsDefine(NeedsDefine) {
  assert(Begin.isValid());
  assert(End.isValid());
}
Slice Slice::generateFromStartAndNext(clang::SourceLocation Start,
                                      clang::SourceLocation Next,
                                      const clang::SourceManager &SM,
                                      const clang::LangOptions &LO) {
  const auto TmpEnd = SM.getExpansionRange(Next).getEnd();
  const auto TmpStart = SM.getExpansionRange(Start).getBegin();
  if (TmpEnd == TmpStart) {
    return {TmpStart, utils::getEndOfToken(TmpStart, SM, LO)};
  }
  return {TmpStart, utils::getEndOfToken(
                        utils::findPreviousTokenStart(TmpEnd, SM, LO), SM, LO)};
}
Slice::Slice(clang::SourceLocation Begin, clang::SourceLocation End,
             std::vector<Slice> Keep)
    : Begin(Begin), End(End), NeedsDefine(true), Keep(std::move(Keep)) {
  assert(Begin.isValid());
  assert(End.isValid());
}
Slice::Slice() {}
FileOffset::FileOffset(unsigned int Line, unsigned int Column)
    : Line(Line), Column(Column) {
  assert(Line > 0 && Column > 0);
}

std::ostream &operator<<(std::ostream &os, const FileOffset &offset) {
  os << "[" << offset.Line << ":" << offset.Column << "]";
  return os;
}
FileOffset::FileOffset(const clang::PresumedLoc &Loc)
    : Line(Loc.getLine()), Column(Loc.getColumn()) {
  assert(Loc.isValid());
  assert(Line > 0 && Column > 0);
}
unsigned int FileOffset::GetSliceColumn() const {
  assert(Column >= 1);
  return Column - 1;
}
unsigned int FileOffset::GetSliceLine() const {
  assert(Line >= 1);
  return Line - 1;
}
bool FileOffset::operator==(const FileOffset &rhs) const {
  return std::tie(Line, Column) == std::tie(rhs.Line, rhs.Column);
}
bool FileOffset::operator!=(const FileOffset &rhs) const {
  return !(rhs == *this);
}
bool FileOffset::operator<(const FileOffset &rhs) const {
  return std::tie(Line, Column) < std::tie(rhs.Line, rhs.Column);
}
bool FileOffset::operator>(const FileOffset &rhs) const { return rhs < *this; }
bool FileOffset::operator<=(const FileOffset &rhs) const {
  return !(rhs < *this);
}
bool FileOffset::operator>=(const FileOffset &rhs) const {
  return !(*this < rhs);
}
// FileSlice::FileSlice(FileOffset Begin, FileOffset End)
//     : Begin(Begin), End(End) {
//   assert(Begin < End);
// }
std::ostream &operator<<(std::ostream &os, const FileSlice &slice) {
  os << slice.Begin << " - " << slice.End;
  return os;
}
FileSlice::FileSlice(const Slice &Slice, const clang::SourceManager &SM)
    : Begin(utils::getLocationAsWritten(Slice.Begin, SM)),
      End(utils::getLocationAsWritten(Slice.End, SM)),
      NeedsDefine(Slice.NeedsDefine) {
  for (const auto &S : Slice.Keep) {
    Keep.emplace_back(S, SM);
  }
}
bool FileSlice::operator==(const FileSlice &rhs) const {
  return std::tie(Begin, End, NeedsDefine, Keep) ==
         std::tie(rhs.Begin, rhs.End, rhs.NeedsDefine, rhs.Keep);
}
bool FileSlice::operator!=(const FileSlice &rhs) const {
  return !(rhs == *this);
}
void mergeSlices(std::vector<FileSlice> &Slices) {
  assert(!Slices.empty());
  std::sort(
      Slices.begin(), Slices.end(),
      [](const FileSlice &a, const FileSlice &b) { return a.Begin < b.Begin; });
  auto OutputIndex = Slices.begin();
  for (std::size_t I = 1; I < Slices.size(); I++) {
    if (OutputIndex->End >= Slices[I].Begin &&
        OutputIndex->Keep == Slices[I].Keep) {
      OutputIndex->End = std::max(OutputIndex->End, Slices[I].End);
    } else {
      OutputIndex++;
      *OutputIndex = Slices[I];
    }
  }
  OutputIndex++;
  Slices.erase(OutputIndex, Slices.end());
}

void mergeAndSplitSlices(std::vector<FileSlice> &Slices) {
  assert(!Slices.empty());

  std::vector<FileSlice> Keep;
  std::vector<FileSlice> Tmp;
  std::copy_if(Slices.begin(), Slices.end(), std::back_inserter(Keep),
               [](const auto &Slice) { return !Slice.NeedsDefine; });
  std::copy_if(Slices.begin(), Slices.end(), std::back_inserter(Tmp),
               [](const auto &Slice) { return Slice.NeedsDefine; });
  Slices.clear();
  std::copy(Tmp.begin(), Tmp.end(), std::back_inserter(Slices));

  if (!Slices.empty()) {
    mergeSlices(Slices);
  }
  if (!Keep.empty()) {
    mergeSlices(Keep);
  }
}

void extractSlices(const std::string &FileIn, const std::string &FileOut,
                   const std::vector<FileSlice> &Slices) {
  assert(std::is_sorted(Slices.begin(), Slices.end(),
                        [](const FileSlice &a, const FileSlice &b) {
                          return a.Begin < b.Begin;
                        }));
  std::ifstream Input(FileIn);
  if (!Input.is_open()) {
    throw std::runtime_error("Could not open input file " + FileIn);
  }
  std::ofstream Output(FileOut);
  if (!Output.is_open()) {
    throw std::runtime_error("Could not open output file " + FileOut);
  }

  std::string Line;
  unsigned int LineNumber = 0;
  auto PreviousSlice = Slices.end();
  auto CurrentSlice = Slices.begin();
  while (std::getline(Input, Line)) {
    while (true) {
      // Case: We are in text befor the slice
      if (CurrentSlice == Slices.end() ||
          CurrentSlice->Begin.GetSliceLine() > LineNumber) {
        if (CurrentSlice != Slices.end() &&
            CurrentSlice->Begin.GetSliceLine() == (LineNumber + 1)) {
          Output << '\n';
        }
        break;
      }
      // Handle the following cases:
      // We are in a completely sliced line, excluding the end or begin: Copy
      // the complete line
      // We are in the first line: Pad from the previous slice with spaces
      // and add the text
      // We are in the last line: Add text, check if
      // others slices are needed in the same line
      if (CurrentSlice->Begin.GetSliceLine() < LineNumber &&
          CurrentSlice->End.GetSliceLine() > LineNumber) {
        Output << Line << '\n';
        break;
      }
      if (CurrentSlice->Begin.GetSliceLine() == LineNumber) {
        if (PreviousSlice != Slices.end() &&
            PreviousSlice->End.GetSliceLine() == LineNumber) {
          // We need to pad only after the last slice
          for (unsigned int I = 0; I < CurrentSlice->Begin.GetSliceColumn() -
                                           PreviousSlice->End.GetSliceColumn();
               I++) {
            Output << ' ';
          }
        } else {
          for (unsigned int I = 0; I < CurrentSlice->Begin.GetSliceColumn();
               I++) {
            Output << ' ';
          }
        }
        // Copy our slice
        if (CurrentSlice->End.GetSliceLine() == LineNumber) {
          Output << Line.substr(CurrentSlice->Begin.GetSliceColumn(),
                                CurrentSlice->End.GetSliceColumn() -
                                    CurrentSlice->Begin.GetSliceColumn());
        } else {
          Output << Line.substr(CurrentSlice->Begin.GetSliceColumn()) << '\n';
          break;
        }
      } else {
        // Handle end case
        Output << Line.substr(0, CurrentSlice->End.GetSliceColumn());
      }

      PreviousSlice = CurrentSlice;
      CurrentSlice++;
      if (CurrentSlice == Slices.end()) {
        Output << '\n';
        break;
      }
    }
    LineNumber++;
  }
}
bool notOnlyWhitespace(const std::string &Str) {
  return std::any_of(Str.begin(), Str.end(),
                     [](unsigned char c) { return !isspace(c); });
}

void extractHeaderSlices(const std::string &FileIn, const std::string &FileOut,
                         const std::vector<FileSlice> &Slices,
                         const std::string &FileName,
                         const std::vector<std::string> &includes) {
  std::ifstream Input(FileIn);
  if (!Input.is_open()) {
    throw std::runtime_error("Could not open input file " + FileIn);
  }
  std::ofstream Output(FileOut);
  if (!Output.is_open()) {
    throw std::runtime_error("Could not open output file " + FileOut);
  }
  std::string HeaderName = FileName.substr(0, FileName.find('.'));
  std::transform(HeaderName.begin(), HeaderName.end(), HeaderName.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  HeaderName += "_H";
  Output << "#ifndef " << HeaderName << '\n';
  Output << "#define " << HeaderName << '\n';

  Output << "\n// Includes\n";
  for (const auto &Include : includes) {
    Output << Include << '\n';
  }
  Output << "// End Includes\n";

  std::string Line;
  unsigned int LineNumber = 0;
  auto PreviousSlice = Slices.end();
  auto CurrentSlice = Slices.begin();
  while (std::getline(Input, Line)) {
    while (true) {
      // Case: We are in text befor the slice
      if (CurrentSlice == Slices.end() ||
          CurrentSlice->Begin.GetSliceLine() > LineNumber) {
        break;
      }
      // Handle the following cases:
      // We are in a completely sliced line, excluding the end or begin: Copy
      // the complete line
      // We are in the first line: Pad from the previous slice with spaces
      // and add the text
      // We are in the last line: Add text, check if
      // others slices are needed in the same line
      if (CurrentSlice->Begin.GetSliceLine() < LineNumber &&
          CurrentSlice->End.GetSliceLine() > LineNumber) {
        Output << Line << '\n';
        break;
      }
      if (CurrentSlice->Begin.GetSliceLine() == LineNumber) {
        if (PreviousSlice != Slices.end() &&
            PreviousSlice->End.GetSliceLine() == LineNumber) {
          // We need to pad only after the last slice
          for (unsigned int I = 0; I < CurrentSlice->Begin.GetSliceColumn() -
                                           PreviousSlice->End.GetSliceColumn();
               I++) {
            Output << ' ';
          }
        } else {
          for (unsigned int I = 0; I < CurrentSlice->Begin.GetSliceColumn();
               I++) {
            Output << ' ';
          }
        }
        // Copy our slice
        if (CurrentSlice->End.GetSliceLine() == LineNumber) {
          Output << Line.substr(CurrentSlice->Begin.GetSliceColumn(),
                                CurrentSlice->End.GetSliceColumn() -
                                    CurrentSlice->Begin.GetSliceColumn());
          if (CurrentSlice->NeedsDefine) {
            Output << ';';
          }
          Output << '\n';
        } else {
          Output << Line.substr(CurrentSlice->Begin.GetSliceColumn()) << '\n';
          break;
        }
      } else {
        // Handle end case
        Output << Line.substr(0, CurrentSlice->End.GetSliceColumn());
        if (CurrentSlice->NeedsDefine) {
          Output << ';';
        }
        Output << '\n';
      }

      PreviousSlice = CurrentSlice;
      CurrentSlice++;
      if (CurrentSlice == Slices.end()) {
        Output << '\n';
        break;
      }
    }
    LineNumber++;
  }

  Output << "#endif //" << HeaderName << '\n';
}

void extractSlicesDefine(const std::string &FileIn, const std::string &FileOut,
                         const std::vector<FileSlice> &Slices) {
  assert(std::is_sorted(Slices.begin(), Slices.end(),
                        [](const FileSlice &a, const FileSlice &b) {
                          return a.Begin < b.Begin;
                        }));
  std::ifstream Input(FileIn);
  if (!Input.is_open()) {
    throw std::runtime_error("Could not open input file " + FileIn);
  }
  std::ofstream Output(FileOut);
  if (!Output.is_open()) {
    throw std::runtime_error("Could not open output file " + FileOut);
  }

  std::vector<std::string> Lines;
  {
    std::string Line;
    while (std::getline(Input, Line)) {
      Lines.push_back(Line);
    }
  }

  std::string Line;
  unsigned int LineNumber = 0;
  bool InSliceGroup = false;

  auto CurrentSlice = Slices.begin();
  auto NextSlice = CurrentSlice + 1;
  for (unsigned I = 0; I < Lines.size(); I++) {
    Line = Lines[I];
    while (true) {
      // Case: We are in text before the slice
      if (CurrentSlice == Slices.end() ||
          CurrentSlice->Begin.GetSliceLine() > LineNumber) {
        Output << Line << '\n';
        break;
      }

      // Case: We are in the middle of a slice
      if (CurrentSlice->Begin.GetSliceLine() < LineNumber &&
          CurrentSlice->End.GetSliceLine() > LineNumber) {
        Output << Line << '\n';
        break;
      }

      // Case Slice is starting in this line
      if (CurrentSlice->Begin.GetSliceLine() == LineNumber) {
        if (!CurrentSlice->Keep.empty()) {
          extractRewrittenFunction(Lines, CurrentSlice->Keep, Output);
        }
        if (CurrentSlice->Begin.GetSliceColumn() != 0) {
          const auto Sub = Line.substr(0, CurrentSlice->Begin.GetSliceColumn());
          if (notOnlyWhitespace(Sub)) {
            Output << Sub;
            if (!InSliceGroup) {
              Output << '\n';
            }
          }
        }
        if (!InSliceGroup) {
          Output << "#ifndef SLICE\n";
        } else {
          Output << '\n';
        }
        InSliceGroup = false;
        for (unsigned int I = 0; I < CurrentSlice->Begin.GetSliceColumn();
             I++) {
          Output << ' ';
        }
        // Copy our slice
        if (CurrentSlice->End.GetSliceLine() == LineNumber) {
          // This slice is ending in the same line it is starting in
          Output << Line.substr(CurrentSlice->Begin.GetSliceColumn(),
                                CurrentSlice->End.GetSliceColumn() -
                                    CurrentSlice->Begin.GetSliceColumn());

          if (NextSlice == Slices.end() ||
              NextSlice->Begin.GetSliceLine() > LineNumber) {
            // The next slice does not start in the same line
            auto Sub = Line.substr(CurrentSlice->End.GetSliceColumn());
            if (notOnlyWhitespace(Sub)) {
              Output << "\n#endif //Slice\n";
              Output << Sub << '\n';
            } else {
              if (NextSlice != Slices.end() && NextSlice->Keep.empty()) {
                bool FoundText = false;
                unsigned TmpLine = I + 1;
                while (!FoundText &&
                       NextSlice->Begin.GetSliceLine() > TmpLine) {
                  FoundText |= notOnlyWhitespace(Lines[TmpLine]);
                  TmpLine++;
                }
                const auto TmpSub =
                    Lines[NextSlice->Begin.GetSliceLine()].substr(
                        0, NextSlice->Begin.GetSliceColumn());
                FoundText |= notOnlyWhitespace(TmpSub);
                if (!FoundText) {
                  InSliceGroup = true;
                } else {
                  Output << "\n#endif //Slice\n";
                }
              } else {
                Output << "\n#endif //Slice\n";
              }
            }

          } else {

            // Next slice to define is in the same column
            const auto TmpSub =
                Line.substr(CurrentSlice->End.GetSliceColumn(),
                            NextSlice->Begin.GetSliceColumn() -
                                CurrentSlice->End.GetSliceColumn());
            const bool FoundText = notOnlyWhitespace(TmpSub);
            if (FoundText || !NextSlice->Keep.empty()) {
              Output << "\n#endif //Slice\n";
            } else {
              InSliceGroup = true;
            }
            Output << TmpSub;
          }
        } else {
          // The current slice is spanning multiple lines
          Output << Line.substr(CurrentSlice->Begin.GetSliceColumn()) << '\n';
          break;
        }
      } else {
        // Handle end case
        Output << Line.substr(0, CurrentSlice->End.GetSliceColumn());
        if (NextSlice == Slices.end() ||
            NextSlice->Begin.GetSliceLine() > LineNumber) {
          auto Sub = Line.substr(CurrentSlice->End.GetSliceColumn());
          if (notOnlyWhitespace(Sub)) {
            Output << "\n#endif //Slice\n";
            Output << Sub << '\n';
          } else {
            if (NextSlice != Slices.end() && NextSlice->Keep.empty()) {
              bool FoundText = false;
              unsigned TmpLine = I + 1;
              while (!FoundText && NextSlice->Begin.GetSliceLine() > TmpLine) {
                FoundText |= notOnlyWhitespace(Lines[TmpLine]);
                TmpLine++;
              }
              const auto TmpSub = Lines[NextSlice->Begin.GetSliceLine()].substr(
                  0, NextSlice->Begin.GetSliceColumn());
              FoundText |= notOnlyWhitespace(TmpSub);
              if (!FoundText) {
                InSliceGroup = true;
              } else {
                Output << "\n#endif //Slice\n";
              }
            } else {
              Output << "\n#endif //Slice\n";
            }
          }
        } else {
          // Next slice to define is in the same column
          const auto TmpSub =
              Line.substr(CurrentSlice->End.GetSliceColumn(),
                          NextSlice->Begin.GetSliceColumn() -
                              CurrentSlice->End.GetSliceColumn());
          const bool FoundText = notOnlyWhitespace(TmpSub);
          if (FoundText || !NextSlice->Keep.empty()) {
            Output << "\n#endif //Slice\n";
          } else {
            InSliceGroup = true;
          }
          Output << TmpSub;
        }
      }

      CurrentSlice++;
      NextSlice = CurrentSlice + 1;
      if (CurrentSlice == Slices.end() ||
          CurrentSlice->Begin.GetSliceLine() > LineNumber) {
        break;
      }
    }
    LineNumber++;
  }
}
void extractRewrittenFunction(const std::vector<std::string> &Lines,
                              const std::vector<FileSlice> &SlicesIn,
                              std::ofstream &Output) {
  Output << "#ifdef SLICE\n";

  std::vector<FileSlice> Slices;
  std::copy_if(SlicesIn.begin(), SlicesIn.end(), std::back_inserter(Slices),
               [](const FileSlice &Slice) { return !Slice.NeedsDefine; });

  mergeSlices(Slices);

  auto PreviousSlice = Slices.end();
  auto CurrentSlice = Slices.begin();
  auto LineNumber = CurrentSlice->Begin.GetSliceLine();
  while (LineNumber < Lines.size()) {
    const std::string Line = Lines[LineNumber];
    while (true) {
      // Case: We are in text befor the slice
      if (CurrentSlice == Slices.end() ||
          CurrentSlice->Begin.GetSliceLine() > LineNumber) {
        if (CurrentSlice != Slices.end() &&
            CurrentSlice->Begin.GetSliceLine() == (LineNumber + 1)) {
          Output << '\n';
        }
        break;
      }
      // Handle the following cases:
      // We are in a completely sliced line, excluding the end or begin: Copy
      // the complete line
      // We are in the first line: Pad from the previous slice with spaces
      // and add the text
      // We are in the last line: Add text, check if
      // others slices are needed in the same line
      if (CurrentSlice->Begin.GetSliceLine() < LineNumber &&
          CurrentSlice->End.GetSliceLine() > LineNumber) {
        Output << Line << '\n';
        break;
      }
      if (CurrentSlice->Begin.GetSliceLine() == LineNumber) {
        if (PreviousSlice != Slices.end() &&
            PreviousSlice->End.GetSliceLine() == LineNumber) {
          // We need to pad only after the last slice
          for (unsigned int I = 0; I < CurrentSlice->Begin.GetSliceColumn() -
                                           PreviousSlice->End.GetSliceColumn();
               I++) {
            Output << ' ';
          }
        } else {
          for (unsigned int I = 0; I < CurrentSlice->Begin.GetSliceColumn();
               I++) {
            Output << ' ';
          }
        }
        // Copy our slice
        if (CurrentSlice->End.GetSliceLine() == LineNumber) {
          Output << Line.substr(CurrentSlice->Begin.GetSliceColumn(),
                                CurrentSlice->End.GetSliceColumn() -
                                    CurrentSlice->Begin.GetSliceColumn());
        } else {
          Output << Line.substr(CurrentSlice->Begin.GetSliceColumn()) << '\n';
          break;
        }
      } else {
        // Handle end case
        Output << Line.substr(0, CurrentSlice->End.GetSliceColumn());
      }

      PreviousSlice = CurrentSlice;
      CurrentSlice++;
      if (CurrentSlice == Slices.end()) {
        Output << '\n';
        break;
      }
    }
    LineNumber++;
  }
  Output << "#endif //SLICE\n";
}
} // namespace printer