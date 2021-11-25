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
  if (!decl->hasBody()) {
    llvm_unreachable("Function declarations are not yet supported.");
  }
  if (printer::isAnyInWhitelist(decl, TargetLines, SM)) {
    //    const auto next = clang::Lexer::findLocationAfterToken(
    //        decl->getBeginLoc(), clang::tok::l_brace, SM, CTX.getLangOpts(),
    //        false);
    //    next.dump(SM);
    const auto text = clang::Lexer::getSourceText(
        clang::CharSourceRange::getTokenRange(decl->getBeginLoc(),
                                              decl->getBody()->getBeginLoc()),
        SM, CTX.getLangOpts());
    this->Slices.emplace_back(
        decl->getBeginLoc(),
        utils::getEndOfToken(decl->getBody()->getBeginLoc(), SM,
                             CTX.getLangOpts()));

    llvm::errs() << text;
    const auto text2 = clang::Lexer::getSourceText(
        clang::CharSourceRange::getTokenRange(decl->getBody()->getEndLoc(),
                                              decl->getEndLoc()),
        SM, CTX.getLangOpts());
    this->Slices.emplace_back(
        decl->getBody()->getEndLoc(),
        utils::getEndOfToken(decl->getEndLoc(), SM, CTX.getLangOpts()));
    llvm::errs() << text2;
  }
  // llvm_unreachable("test");
}
void DeclPrinter::VisitDecl(const clang::Decl *decl) {
  if (!decl->isImplicit()) {
    llvm_unreachable("Hit unsupported decl");
  }
}
DeclPrinter::DeclPrinter(const std::set<unsigned int> &targetLines,
                         const clang::ASTContext &ctx)
    : TargetLines(targetLines), SM(ctx.getSourceManager()), CTX(ctx) {}

void DeclPrinter::VisitTranslationUnitDecl(
    const clang::TranslationUnitDecl *decl) {
  decl->dump();
  for (const auto &D : decl->decls()) {
    Visit(D);
  }
}
std::vector<FileSlice> DeclPrinter::GetSlices() const {
  std::vector<FileSlice> Result;
  for (const auto &S : Slices) {
    Result.emplace_back(S, SM);
  }
  return Result;
}

void StmtPrinterFiltering::VisitStmt(const clang::Stmt *stmt) {
  llvm_unreachable("Hit unsupported stmt");
}
void StmtPrinterFiltering::VisitCompoundStmt(const clang::CompoundStmt *stmt) {
  llvm_unreachable("unimplemented");
}
StmtPrinterFiltering::StmtPrinterFiltering(
    const std::set<unsigned int> &targetLines)
    : TargetLines(targetLines) {}
void StmtPrinterFiltering::VisitWhileStmt(const clang::WhileStmt *Stmt) {
  llvm_unreachable("WhileStmt");
}
void StmtPrinterFiltering::VisitForStmt(const clang::ForStmt *Stmt) {
  llvm_unreachable("ForStmt");
}
void StmtPrinterFiltering::VisitDoStmt(const clang::DoStmt *Stmt) {
  llvm_unreachable("DoStmt");
}
void StmtPrinterFiltering::VisitSwitchStmt(const clang::SwitchStmt *Stmt) {
  llvm_unreachable("SwitchStmt");
}
void StmtPrinterFiltering::VisitReturnStmt(const clang::ReturnStmt *Stmt) {
  llvm_unreachable("ReturnStatement");
}
printer::Slice::Slice(clang::SourceLocation Begin, clang::SourceLocation End)
    : Begin(Begin), End(End) {
  assert(Begin.isValid());
  assert(End.isValid());
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
FileSlice::FileSlice(FileOffset Begin, FileOffset End)
    : Begin(Begin), End(End) {
  assert(Begin < End);
}
std::ostream &operator<<(std::ostream &os, const FileSlice &slice) {
  os << slice.Begin << " - " << slice.End;
  return os;
}
FileSlice::FileSlice(const Slice &Slice, const clang::SourceManager &SM)
    : Begin(utils::getLocationAsWritten(Slice.Begin, SM)),
      End(utils::getLocationAsWritten(Slice.End, SM)) {}
void mergeSlices(std::vector<FileSlice> &Slices) {
  assert(!Slices.empty());
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
        Output << "\n";
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
          for (unsigned int I = 0; I < PreviousSlice->End.GetSliceColumn() -
                                           CurrentSlice->Begin.GetSliceColumn();
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
} // namespace printer