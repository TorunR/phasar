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

namespace printer {
void DeclPrinter::VisitFunctionDecl(const clang::FunctionDecl *decl) {
  if (!decl->doesThisDeclarationHaveABody()) {
    // llvm_unreachable("Function declarations are not yet supported.");
    // TODO
  } else {
    if (isInSourceFile(decl, SM)) {
      if (printer::isAnyInWhitelist(decl, TargetLines, SM)) {
        //    const auto next = clang::Lexer::findLocationAfterToken(
        //        decl->getBeginLoc(), clang::tok::l_brace, SM,
        //        CTX.getLangOpts(), false);
        //    next.dump(SM);
        //    const auto text = clang::Lexer::getSourceText(
        //        clang::CharSourceRange::getTokenRange(decl->getBeginLoc(),
        //                                              decl->getBody()->getBeginLoc()),
        //        SM, LO);
        //      decl->dump();
        //      decl->getBeginLoc().dump(SM);
        //      decl->getEndLoc().dump(SM);
        this->Slices.emplace_back(
            SM.getExpansionRange(decl->getBeginLoc()).getBegin(),
            utils::getEndOfToken(
                SM.getExpansionRange(decl->getBody()->getBeginLoc()).getBegin(),
                SM, LO));

        //    llvm::errs() << text;
        //    const auto text2 = clang::Lexer::getSourceText(
        //        clang::CharSourceRange::getTokenRange(decl->getBody()->getEndLoc(),
        //                                              decl->getEndLoc()),
        //        SM, LO);
        this->Slices.emplace_back(
            SM.getExpansionRange(decl->getBody()->getEndLoc()).getEnd(),
            utils::getEndOfToken(
                SM.getExpansionRange(decl->getEndLoc()).getEnd(), SM, LO));
        //    llvm::errs() << text2;
        const auto Body = StmtPrinterFiltering::GetSlices(
            decl->getBody(), TargetLines, CTX, SM, LO);
        Slices.insert(Slices.end(), Body.begin(), Body.end());
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
std::vector<Slice> DeclPrinter::GetSlices(
    const clang::Decl *Decl, const std::set<unsigned int> &TargetLines,
    const clang::ASTContext &CTX, const clang::SourceManager &SM,
    const clang::LangOptions &LO) {
  DeclPrinter Visitor(TargetLines, CTX, SM, LO);
  Visitor.Visit(Decl);
  return Visitor.Slices;
}
std::vector<FileSlice>
DeclPrinter::GetFileSlices(const clang::Decl *Decl,
                           const std::set<unsigned int> &TargetLines,
                           const clang::ASTContext &CTX) {
  const auto &SM = CTX.getSourceManager();
  const auto Slices =
      DeclPrinter::GetSlices(Decl, TargetLines, CTX, SM, CTX.getLangOpts());
  std::vector<FileSlice> Result;
  Result.reserve(Slices.size());
  for (const auto &S : Slices) {
    Result.emplace_back(S, SM);
  }
  return Result;
}
void DeclPrinter::VisitVarDecl(const clang::VarDecl *Decl) {
  if (isInSourceFile(Decl, SM)) {
    //Decl->dump();
    const auto Semicolon =
        utils::getSemicolonAfterStmtEndLoc(Decl->getEndLoc(), SM, LO);
    assert(Semicolon.isValid());
    Slice S(SM.getExpansionRange(Decl->getBeginLoc()).getBegin(),
            utils::getEndOfToken(Semicolon, SM, LO));

    if (!isAnyInWhitelist(Decl, TargetLines, SM)) {
      S.NeedsDefine = true;
      // Decl->dump();
    }
    Slices.push_back(S);
  }
}
void DeclPrinter::VisitTypeDecl(const clang::TypeDecl *Decl) {
  // TODO: How do we want to deal with types?
  if (isInSourceFile(Decl, SM)) {
    const auto Semicolon =
        utils::getSemicolonAfterStmtEndLoc(Decl->getEndLoc(), SM, LO);
    assert(Semicolon.isValid());
    Slice S(SM.getExpansionRange(Decl->getBeginLoc()).getBegin(),
            utils::getEndOfToken(Semicolon, SM, LO));

    if (!isAnyInWhitelist(Decl, TargetLines, SM)) {
      S.NeedsDefine = true;
    }
    Slices.push_back(S);
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
    const clang::LangOptions &LO) {
  StmtPrinterFiltering Visitor(TargetLines, CTX, SM, LO);
  Visitor.Visit(Stmt);
  return Visitor.Slices;
}
void StmtPrinterFiltering::PrintStmt(const clang::Stmt *Stmt, bool required) {
  if (Stmt &&
      (llvm::isa<clang::Expr>(Stmt) || llvm::isa<clang::ContinueStmt>(Stmt) ||
       llvm::isa<clang::BreakStmt>(Stmt) ||
       llvm::isa<clang::ReturnStmt>(Stmt) ||
       llvm::isa<clang::GotoStmt>(Stmt))) {
    bool visited = Visit(Stmt);
    const auto Semicolon =
        utils::getSemicolonAfterStmtEndLoc(Stmt->getEndLoc(), SM, LO);
    assert(Semicolon.isValid());
    if (visited || required) {

      if (visited) {
        Slices.emplace_back(SM.getExpansionRange(Stmt->getEndLoc()).getEnd(),
                            utils::getEndOfToken(Semicolon, SM, LO));
      } else {
        Slices.emplace_back(Semicolon, utils::getEndOfToken(Semicolon, SM, LO));
      }
    } else {
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
      Slices.emplace_back(
          SM.getExpansionRange(Stmt->getElse()->getBeginLoc()).getBegin(),
          utils::getEndOfToken(
              SM.getExpansionRange(Stmt->getElse()->getEndLoc()).getEnd(), SM,
              LO),
          true);
    }
    return true;
  }
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
FileOffset::FileOffset(unsigned int Line, unsigned int Column)
    : Line(Line), Column(Column) {}
bool FileOffset::operator<(const FileOffset &rhs) const {
  if (Line < rhs.Line) {
    return true;
  }
  if (rhs.Line < Line) {
    return false;
  }
  return Column < rhs.Column;
}
bool FileOffset::operator>(const FileOffset &rhs) const { return rhs < *this; }
bool FileOffset::operator<=(const FileOffset &rhs) const {
  return !(rhs < *this);
}
bool FileOffset::operator>=(const FileOffset &rhs) const {
  return !(*this < rhs);
}
std::ostream &operator<<(std::ostream &os, const FileOffset &offset) {
  os << "[" << offset.Line << ":" << offset.Column << "]";
  return os;
}
FileOffset::FileOffset(const clang::PresumedLoc &Loc)
    : Line(Loc.getLine()), Column(Loc.getColumn()) {
  assert(Loc.isValid());
}
unsigned int FileOffset::GetSliceColumn() const {
  assert(Column >= 1);
  return Column - 1;
}
unsigned int FileOffset::GetSliceLine() const {
  assert(Line >= 1);
  return Line - 1;
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
      NeedsDefine(Slice.NeedsDefine) {}
void mergeSlices(std::vector<FileSlice> &Slices) {
  assert(Slices.empty());
  std::sort(
      Slices.begin(), Slices.end(),
      [](const FileSlice &a, const FileSlice &b) { return a.Begin < b.Begin; });
  auto OutputIndex = Slices.begin();
  for (std::size_t I = 1; I < Slices.size(); I++) {
    if (OutputIndex->End >= Slices[I].Begin) {
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
  std::copy_if(Slices.begin(), Slices.end(), std::back_inserter(Keep), [](const auto &Slice) { return !Slice.NeedsDefine; });
  std::copy_if(Slices.begin(), Slices.end(), std::back_inserter(Tmp), [](const auto &Slice) { return Slice.NeedsDefine; });
  Slices.clear();
  std::copy(Tmp.begin(), Tmp.end(), std::back_inserter(Slices));


  mergeSlices(Slices);
  if (!Keep.empty()) {
    mergeSlices(Keep);
  }

#ifdef MERGE_DEFINES
  auto OutputIndex = Slices.begin();
  auto KeepIndex = Keep.begin();
  for (std::size_t I = 1; I < Slices.size(); I++) {
    while (KeepIndex != Keep.end() && KeepIndex->Begin < OutputIndex->End) {
      KeepIndex++;
    }
    // Keep is after us or end
    if (KeepIndex == Keep.end() || KeepIndex->Begin > Slices[I].Begin) {
      OutputIndex->End = std::max(OutputIndex->End, Slices[I].End);
    } else {
      OutputIndex++;
      *OutputIndex = Slices[I];
    }
  }
  OutputIndex++;
  Slices.erase(OutputIndex, Slices.end());
#endif
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
          Output << "\n";
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
        Output << Line << "\n";
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
          Output << Line.substr(CurrentSlice->Begin.GetSliceColumn()) << "\n";
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

  std::string Line;
  unsigned int LineNumber = 0;

  auto CurrentSlice = Slices.begin();
  auto NextSlice = CurrentSlice + 1;
  while (std::getline(Input, Line)) {
    while (true) {
      // Case: We are in text before the slice
      if (CurrentSlice == Slices.end() ||
          CurrentSlice->Begin.GetSliceLine() > LineNumber) {
        Output << Line << "\n";
        break;
      }

      if (CurrentSlice->Begin.GetSliceLine() < LineNumber &&
          CurrentSlice->End.GetSliceLine() > LineNumber) {
        Output << Line << "\n";
        break;
      }
      if (CurrentSlice->Begin.GetSliceLine() == LineNumber) {
        if (CurrentSlice->Begin.GetSliceColumn() != 0) {
          auto Sub = Line.substr(0, CurrentSlice->Begin.GetSliceColumn());
          if (Sub.find_first_not_of(" \t") != std::string::npos) {
            Output << Sub << "\n";
          }
        }
        Output << "#ifndef SLICE\n";
        for (unsigned int I = 0; I < CurrentSlice->Begin.GetSliceColumn();
             I++) {
          Output << ' ';
        }
        // Copy our slice
        if (CurrentSlice->End.GetSliceLine() == LineNumber) {
          Output << Line.substr(CurrentSlice->Begin.GetSliceColumn(),
                                CurrentSlice->End.GetSliceColumn() -
                                    CurrentSlice->Begin.GetSliceColumn());
          Output << "\n#endif //Slice\n";
          if (NextSlice == Slices.end() ||
              NextSlice->Begin.GetSliceLine() > LineNumber) {
            auto Sub = Line.substr(CurrentSlice->End.GetSliceColumn());
            if(!Sub.empty()) {
              Output << Sub  << '\n';
            }

          } else {
            // Next slice to define is in the same column
            Output << Line.substr(CurrentSlice->End.GetSliceColumn(),
                                  NextSlice->Begin.GetSliceColumn() -
                                      CurrentSlice->End.GetSliceColumn());
          }
        } else {
          Output << Line.substr(CurrentSlice->Begin.GetSliceColumn()) << '\n';
          break;
        }
      } else {
        // Handle end case
        Output << Line.substr(0, CurrentSlice->End.GetSliceColumn());
        Output << "\n#endif //Slice\n";
        if (NextSlice == Slices.end() ||
            NextSlice->Begin.GetSliceLine() > LineNumber) {
          Output << Line.substr(CurrentSlice->End.GetSliceColumn()) << '\n';
        } else {
          // Next slice to define is in the same column
          Output << Line.substr(CurrentSlice->End.GetSliceColumn(),
                                NextSlice->Begin.GetSliceColumn() -
                                    CurrentSlice->End.GetSliceColumn());
        }
      }

      CurrentSlice++;
      NextSlice = CurrentSlice + 1;
      if (CurrentSlice == Slices.end() || CurrentSlice->Begin.GetSliceLine() > LineNumber) {
        break;
      }
    }
    LineNumber++;
  }
}
} // namespace printer