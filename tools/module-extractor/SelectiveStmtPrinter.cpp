//===- StmtPrinterFiltering.cpp - Printing implementation for Stmt ASTs
//------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Stmt::dumpPretty/Stmt::printPretty methods, which
// pretty print the AST back out to C code.
//
//===----------------------------------------------------------------------===//

/**
 *  This file is ripped from llvm 10.0.1 and modified for selective printing
 */

#include "SelectiveStmtPrinter.h"
#include "SelectiveDeclPrinter.h"
#include "source_utils.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclObjC.h>
#include <clang/AST/DeclOpenMP.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/ExprObjC.h>
#include <clang/AST/ExprOpenMP.h>
#include <clang/AST/NestedNameSpecifier.h>
#include <clang/AST/OpenMPClause.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/StmtCXX.h>
#include <clang/AST/StmtObjC.h>
#include <clang/AST/StmtOpenMP.h>
#include <clang/AST/StmtVisitor.h>
#include <clang/AST/TemplateBase.h>
#include <clang/AST/Type.h>
#include <clang/Basic/CharInfo.h>
#include <clang/Basic/ExpressionTraits.h>
#include <clang/Basic/IdentifierTable.h>
#include <clang/Basic/JsonSupport.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/Lambda.h>
#include <clang/Basic/OpenMPKinds.h>
#include <clang/Basic/OperatorKinds.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/TypeTraits.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Compiler.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/raw_ostream.h>

#include <cassert>
#include <string>

using namespace clang;

//===----------------------------------------------------------------------===//
// StmtPrinterFiltering Visitor
//===----------------------------------------------------------------------===//

namespace {

class StmtPrinterFiltering : public StmtVisitor<StmtPrinterFiltering> {
  const std::set<unsigned int> &lines;
  raw_ostream &OS;
  unsigned IndentLevel;
  PrinterHelper *Helper;
  PrintingPolicy Policy;
  std::string NL;
  const ASTContext *Context;

  // Overwrite to enable filtering
public:
#define PTR(CLASS) typename std::add_pointer<CLASS>::type
#define DISPATCH(NAME, CLASS)                                                  \
  return static_cast<StmtPrinterFiltering *>(this)->Visit##NAME(               \
      static_cast<PTR(CLASS)>(S))

  void Visit(PTR(Stmt) S) {
    if (!utils::shouldBeSliced(S, Context->getSourceManager(), lines)) {
      return;
    }

    if (Helper && Helper->handledStmt(S, OS)) {
      return;
    }
    // If we have a binary expr, dispatch to the subcode of the binop.  A smart
    // optimizer (e.g. LLVM) will fold this comparison into the switch stmt
    // below.
    if (PTR(BinaryOperator) BinOp = dyn_cast<BinaryOperator>(S)) {
      switch (BinOp->getOpcode()) {
      case BO_PtrMemD:
        DISPATCH(BinPtrMemD, BinaryOperator);
      case BO_PtrMemI:
        DISPATCH(BinPtrMemI, BinaryOperator);
      case BO_Mul:
        DISPATCH(BinMul, BinaryOperator);
      case BO_Div:
        DISPATCH(BinDiv, BinaryOperator);
      case BO_Rem:
        DISPATCH(BinRem, BinaryOperator);
      case BO_Add:
        DISPATCH(BinAdd, BinaryOperator);
      case BO_Sub:
        DISPATCH(BinSub, BinaryOperator);
      case BO_Shl:
        DISPATCH(BinShl, BinaryOperator);
      case BO_Shr:
        DISPATCH(BinShr, BinaryOperator);

      case BO_LT:
        DISPATCH(BinLT, BinaryOperator);
      case BO_GT:
        DISPATCH(BinGT, BinaryOperator);
      case BO_LE:
        DISPATCH(BinLE, BinaryOperator);
      case BO_GE:
        DISPATCH(BinGE, BinaryOperator);
      case BO_EQ:
        DISPATCH(BinEQ, BinaryOperator);
      case BO_NE:
        DISPATCH(BinNE, BinaryOperator);
      case BO_Cmp:
        DISPATCH(BinCmp, BinaryOperator);

      case BO_And:
        DISPATCH(BinAnd, BinaryOperator);
      case BO_Xor:
        DISPATCH(BinXor, BinaryOperator);
      case BO_Or:
        DISPATCH(BinOr, BinaryOperator);
      case BO_LAnd:
        DISPATCH(BinLAnd, BinaryOperator);
      case BO_LOr:
        DISPATCH(BinLOr, BinaryOperator);
      case BO_Assign:
        DISPATCH(BinAssign, BinaryOperator);
      case BO_MulAssign:
        DISPATCH(BinMulAssign, CompoundAssignOperator);
      case BO_DivAssign:
        DISPATCH(BinDivAssign, CompoundAssignOperator);
      case BO_RemAssign:
        DISPATCH(BinRemAssign, CompoundAssignOperator);
      case BO_AddAssign:
        DISPATCH(BinAddAssign, CompoundAssignOperator);
      case BO_SubAssign:
        DISPATCH(BinSubAssign, CompoundAssignOperator);
      case BO_ShlAssign:
        DISPATCH(BinShlAssign, CompoundAssignOperator);
      case BO_ShrAssign:
        DISPATCH(BinShrAssign, CompoundAssignOperator);
      case BO_AndAssign:
        DISPATCH(BinAndAssign, CompoundAssignOperator);
      case BO_OrAssign:
        DISPATCH(BinOrAssign, CompoundAssignOperator);
      case BO_XorAssign:
        DISPATCH(BinXorAssign, CompoundAssignOperator);
      case BO_Comma:
        DISPATCH(BinComma, BinaryOperator);
      }
    } else if (PTR(UnaryOperator) UnOp = dyn_cast<UnaryOperator>(S)) {
      switch (UnOp->getOpcode()) {
      case UO_PostInc:
        DISPATCH(UnaryPostInc, UnaryOperator);
      case UO_PostDec:
        DISPATCH(UnaryPostDec, UnaryOperator);
      case UO_PreInc:
        DISPATCH(UnaryPreInc, UnaryOperator);
      case UO_PreDec:
        DISPATCH(UnaryPreDec, UnaryOperator);
      case UO_AddrOf:
        DISPATCH(UnaryAddrOf, UnaryOperator);
      case UO_Deref:
        DISPATCH(UnaryDeref, UnaryOperator);
      case UO_Plus:
        DISPATCH(UnaryPlus, UnaryOperator);
      case UO_Minus:
        DISPATCH(UnaryMinus, UnaryOperator);
      case UO_Not:
        DISPATCH(UnaryNot, UnaryOperator);
      case UO_LNot:
        DISPATCH(UnaryLNot, UnaryOperator);
      case UO_Real:
        DISPATCH(UnaryReal, UnaryOperator);
      case UO_Imag:
        DISPATCH(UnaryImag, UnaryOperator);
      case UO_Extension:
        DISPATCH(UnaryExtension, UnaryOperator);
      case UO_Coawait:
        DISPATCH(UnaryCoawait, UnaryOperator);
      }
    }

    // Top switch stmt: dispatch to VisitFooStmt for each FooStmt.
    switch (S->getStmtClass()) {
    default:
      llvm_unreachable("Unknown stmt kind!");
#define ABSTRACT_STMT(STMT)
#define STMT(CLASS, PARENT)                                                    \
  case Stmt::CLASS##Class:                                                     \
    DISPATCH(CLASS, CLASS);
#include "clang/AST/StmtNodes.inc"
    }
  }

public:
  StmtPrinterFiltering(const std::set<unsigned int> &lines, raw_ostream &os,
                       PrinterHelper *helper, const PrintingPolicy &Policy,
                       unsigned Indentation = 0, StringRef NL = "\n",
                       const ASTContext *Context = nullptr)
      : lines(lines), OS(os), IndentLevel(Indentation), Helper(helper),
        Policy(Policy), NL(NL), Context(Context) {}

  void PrintStmt(Stmt *S, bool required = false) {
    PrintStmt(S, Policy.Indentation, required);
  }

  void PrintStmt(Stmt *S, int SubIndent, bool required = false) {
    bool used = utils::shouldBeSliced(S, Context->getSourceManager(), lines);
    if (!used) {
      if (required) {
        OS << "{}";
      }
      return;
    }
    IndentLevel += SubIndent;
    if (S && isa<Expr>(S)) {
      // If this is an expr used in a stmt context, indent and newline it.
      Indent();
      Visit(S);
      OS << ";" << NL;
    } else if (S) {
      Visit(S);
    } else {
      Indent() << "<<<NULL STATEMENT>>>" << NL;
    }
    IndentLevel -= SubIndent;
  }

  void PrintInitStmt(Stmt *S, unsigned PrefixWidth) {
    // FIXME: Cope better with odd prefix widths.
    IndentLevel += (PrefixWidth + 1) / 2;
    if (auto *DS = dyn_cast<DeclStmt>(S))
      PrintRawDeclStmt(DS);
    else
      PrintExpr(cast<Expr>(S));
    OS << "; ";
    IndentLevel -= (PrefixWidth + 1) / 2;
  }

  void PrintControlledStmt(Stmt *S) {
    if (auto *CS = dyn_cast<CompoundStmt>(S)) {
      OS << " ";
      PrintRawCompoundStmt(CS, true);
      OS << NL;
    } else {
      OS << NL;
      PrintStmt(S, true);
    }
  }

  void PrintRawCompoundStmt(CompoundStmt *S, bool required = false);
  void PrintRawDecl(Decl *D);
  void PrintRawDeclStmt(const DeclStmt *S);
  void PrintRawIfStmt(IfStmt *If);
  void PrintRawCXXCatchStmt(CXXCatchStmt *Catch);
  void PrintCallArgs(CallExpr *E);
  void PrintRawSEHExceptHandler(SEHExceptStmt *S);
  void PrintRawSEHFinallyStmt(SEHFinallyStmt *S);
  void PrintOMPExecutableDirective(OMPExecutableDirective *S,
                                   bool ForceNoStmt = false);

  void PrintExpr(Expr *E) {
    if (E)
      Visit(E);
    else
      OS << "<null expr>";
  }

  raw_ostream &Indent(int Delta = 0) {
    for (int i = 0, e = IndentLevel + Delta; i < e; ++i)
      OS << "  ";
    return OS;
  }

  //  void Visit(Stmt *S) {
  //    if (Helper && Helper->handledStmt(S, OS))
  //      return;
  //    else
  //      StmtVisitor<StmtPrinterFiltering>::Visit(S);
  //  }

  void VisitStmt(Stmt *Node) LLVM_ATTRIBUTE_UNUSED {
    Indent() << "<<unknown stmt type>>" << NL;
  }

  void VisitExpr(Expr *Node) LLVM_ATTRIBUTE_UNUSED {
    OS << "<<unknown expr type>>";
  }

  void VisitCXXNamedCastExpr(CXXNamedCastExpr *Node);

#define ABSTRACT_STMT(CLASS)
#define STMT(CLASS, PARENT) void Visit##CLASS(CLASS *Node);
#include "clang/AST/StmtNodes.inc"
};

} // namespace

//===----------------------------------------------------------------------===//
//  Stmt printing methods.
//===----------------------------------------------------------------------===//

/// PrintRawCompoundStmt - Print a compound stmt without indenting the {, and
/// with no newline after the }.
void StmtPrinterFiltering::PrintRawCompoundStmt(CompoundStmt *Node,
                                                bool required) {
  if (required &&
      !utils::shouldBeSliced(Node, Context->getSourceManager(), lines)) {
    OS << "{}";
    return;
  }
  OS << "{" << NL;
  for (auto *I : Node->body())
    PrintStmt(I);

  Indent() << "}";
}

void StmtPrinterFiltering::PrintRawDecl(Decl *D) {
  selective_printer::print(D, lines, OS, Policy, IndentLevel);
}

void StmtPrinterFiltering::PrintRawDeclStmt(const DeclStmt *S) {
  SmallVector<Decl *, 2> Decls(S->decls());
  selective_printer::printGroup(Decls.data(), Decls.size(), lines, OS, Policy,
                                IndentLevel);
}

void StmtPrinterFiltering::VisitNullStmt(NullStmt *Node) {
  Indent() << ";" << NL;
}

void StmtPrinterFiltering::VisitDeclStmt(DeclStmt *Node) {
  Indent();
  PrintRawDeclStmt(Node);
  OS << ";" << NL;
}

void StmtPrinterFiltering::VisitCompoundStmt(CompoundStmt *Node) {
  Indent();
  PrintRawCompoundStmt(Node);
  OS << "" << NL;
}

void StmtPrinterFiltering::VisitCaseStmt(CaseStmt *Node) {
  Indent(-1) << "case ";
  // Node->dump();
  // Node->getLHS()->dump();
  PrintExpr(Node->getLHS());

  if (Node->getRHS()) {
    OS << " ... ";
    PrintExpr(Node->getRHS());
  }
  OS << ":" << NL;

  PrintStmt(Node->getSubStmt(), 0);
}

void StmtPrinterFiltering::VisitDefaultStmt(DefaultStmt *Node) {
  Indent(-1) << "default:" << NL;
  PrintStmt(Node->getSubStmt(), 0);
}

void StmtPrinterFiltering::VisitLabelStmt(LabelStmt *Node) {
  Indent(-1) << Node->getName() << ":" << NL;
  PrintStmt(Node->getSubStmt(), 0);
}

void StmtPrinterFiltering::VisitAttributedStmt(AttributedStmt *Node) {
  for (const auto *Attr : Node->getAttrs()) {
    // TODO Jr
    Attr->printPretty(OS, Policy);
  }

  PrintStmt(Node->getSubStmt(), 0);
}

void StmtPrinterFiltering::PrintRawIfStmt(IfStmt *If) {
  OS << "if (";
  if (If->getInit())
    PrintInitStmt(If->getInit(), 4);
  if (const DeclStmt *DS = If->getConditionVariableDeclStmt())
    PrintRawDeclStmt(DS);
  else
    PrintExpr(If->getCond());
  OS << ')';

  if (auto *CS = dyn_cast<CompoundStmt>(If->getThen())) {
    OS << ' ';
    PrintRawCompoundStmt(CS);
    OS << (If->getElse() ? " " : NL);
  } else {
    OS << NL;
    PrintStmt(If->getThen(), true);
    if (If->getElse())
      Indent();
  }

  if (Stmt *Else = If->getElse()) {
    OS << "else";

    if (auto *CS = dyn_cast<CompoundStmt>(Else)) {
      OS << ' ';
      PrintRawCompoundStmt(CS);
      OS << NL;
    } else if (auto *ElseIf = dyn_cast<IfStmt>(Else)) {
      OS << ' ';
      PrintRawIfStmt(ElseIf);
    } else {
      OS << NL;
      PrintStmt(If->getElse(), true);
    }
  }
}

void StmtPrinterFiltering::VisitIfStmt(IfStmt *If) {
  Indent();
  PrintRawIfStmt(If);
}

void StmtPrinterFiltering::VisitSwitchStmt(SwitchStmt *Node) {
  Indent() << "switch (";
  if (Node->getInit())
    PrintInitStmt(Node->getInit(), 8);
  if (const DeclStmt *DS = Node->getConditionVariableDeclStmt())
    PrintRawDeclStmt(DS);
  else
    PrintExpr(Node->getCond());
  OS << ")";
  PrintControlledStmt(Node->getBody());
}

void StmtPrinterFiltering::VisitWhileStmt(WhileStmt *Node) {
  Indent() << "while (";
  if (const DeclStmt *DS = Node->getConditionVariableDeclStmt())
    PrintRawDeclStmt(DS);
  else
    PrintExpr(Node->getCond());
  OS << ")" << NL;
  PrintStmt(Node->getBody(), true);
}

void StmtPrinterFiltering::VisitDoStmt(DoStmt *Node) {
  Indent() << "do ";
  if (auto *CS = dyn_cast<CompoundStmt>(Node->getBody())) {
    PrintRawCompoundStmt(CS);
    OS << " ";
  } else {
    OS << NL;
    PrintStmt(Node->getBody(), true);
    Indent();
  }

  OS << "while (";
  PrintExpr(Node->getCond());
  OS << ");" << NL;
}

void StmtPrinterFiltering::VisitForStmt(ForStmt *Node) {
  Indent() << "for (";
  if (Node->getInit())
    PrintInitStmt(Node->getInit(), 5);
  else
    OS << (Node->getCond() ? "; " : ";");
  if (Node->getCond())
    PrintExpr(Node->getCond());
  OS << ";";
  if (Node->getInc()) {
    OS << " ";
    PrintExpr(Node->getInc());
  }
  OS << ")";
  PrintControlledStmt(Node->getBody());
}

void StmtPrinterFiltering::VisitObjCForCollectionStmt(
    ObjCForCollectionStmt *Node) {
  Indent() << "for (";
  if (auto *DS = dyn_cast<DeclStmt>(Node->getElement()))
    PrintRawDeclStmt(DS);
  else
    PrintExpr(cast<Expr>(Node->getElement()));
  OS << " in ";
  PrintExpr(Node->getCollection());
  OS << ")";
  PrintControlledStmt(Node->getBody());
}

void StmtPrinterFiltering::VisitCXXForRangeStmt(CXXForRangeStmt *Node) {
  Indent() << "for (";
  if (Node->getInit())
    PrintInitStmt(Node->getInit(), 5);
  PrintingPolicy SubPolicy(Policy);
  SubPolicy.SuppressInitializers = true;
  selective_printer::print(Node->getLoopVariable(), lines, OS, SubPolicy,
                           IndentLevel);
  OS << " : ";
  PrintExpr(Node->getRangeInit());
  OS << ")";
  PrintControlledStmt(Node->getBody());
}

void StmtPrinterFiltering::VisitMSDependentExistsStmt(
    MSDependentExistsStmt *Node) {
  Indent();
  if (Node->isIfExists())
    OS << "__if_exists (";
  else
    OS << "__if_not_exists (";

  if (NestedNameSpecifier *Qualifier =
          Node->getQualifierLoc().getNestedNameSpecifier())
    Qualifier->print(OS, Policy);

  OS << Node->getNameInfo() << ") ";

  PrintRawCompoundStmt(Node->getSubStmt());
}

void StmtPrinterFiltering::VisitGotoStmt(GotoStmt *Node) {
  Indent() << "goto " << Node->getLabel()->getName() << ";";
  if (Policy.IncludeNewlines)
    OS << NL;
}

void StmtPrinterFiltering::VisitIndirectGotoStmt(IndirectGotoStmt *Node) {
  Indent() << "goto *";
  PrintExpr(Node->getTarget());
  OS << ";";
  if (Policy.IncludeNewlines)
    OS << NL;
}

void StmtPrinterFiltering::VisitContinueStmt(ContinueStmt *Node) {
  Indent() << "continue;";
  if (Policy.IncludeNewlines)
    OS << NL;
}

void StmtPrinterFiltering::VisitBreakStmt(BreakStmt *Node) {
  Indent() << "break;";
  if (Policy.IncludeNewlines)
    OS << NL;
}

void StmtPrinterFiltering::VisitReturnStmt(ReturnStmt *Node) {
  Indent() << "return";
  if (Node->getRetValue()) {
    OS << " ";
    PrintExpr(Node->getRetValue());
  }
  OS << ";";
  if (Policy.IncludeNewlines)
    OS << NL;
}

void StmtPrinterFiltering::VisitGCCAsmStmt(GCCAsmStmt *Node) {
  Indent() << "asm ";

  if (Node->isVolatile())
    OS << "volatile ";

  if (Node->isAsmGoto())
    OS << "goto ";

  OS << "(";
  VisitStringLiteral(Node->getAsmString());

  // Outputs
  if (Node->getNumOutputs() != 0 || Node->getNumInputs() != 0 ||
      Node->getNumClobbers() != 0 || Node->getNumLabels() != 0)
    OS << " : ";

  for (unsigned i = 0, e = Node->getNumOutputs(); i != e; ++i) {
    if (i != 0)
      OS << ", ";

    if (!Node->getOutputName(i).empty()) {
      OS << '[';
      OS << Node->getOutputName(i);
      OS << "] ";
    }

    VisitStringLiteral(Node->getOutputConstraintLiteral(i));
    OS << " (";
    Visit(Node->getOutputExpr(i));
    OS << ")";
  }

  // Inputs
  if (Node->getNumInputs() != 0 || Node->getNumClobbers() != 0 ||
      Node->getNumLabels() != 0)
    OS << " : ";

  for (unsigned i = 0, e = Node->getNumInputs(); i != e; ++i) {
    if (i != 0)
      OS << ", ";

    if (!Node->getInputName(i).empty()) {
      OS << '[';
      OS << Node->getInputName(i);
      OS << "] ";
    }

    VisitStringLiteral(Node->getInputConstraintLiteral(i));
    OS << " (";
    Visit(Node->getInputExpr(i));
    OS << ")";
  }

  // Clobbers
  if (Node->getNumClobbers() != 0 || Node->getNumLabels())
    OS << " : ";

  for (unsigned i = 0, e = Node->getNumClobbers(); i != e; ++i) {
    if (i != 0)
      OS << ", ";

    VisitStringLiteral(Node->getClobberStringLiteral(i));
  }

  // Labels
  if (Node->getNumLabels() != 0)
    OS << " : ";

  for (unsigned i = 0, e = Node->getNumLabels(); i != e; ++i) {
    if (i != 0)
      OS << ", ";
    OS << Node->getLabelName(i);
  }

  OS << ");";
  if (Policy.IncludeNewlines)
    OS << NL;
}

void StmtPrinterFiltering::VisitMSAsmStmt(MSAsmStmt *Node) {
  // FIXME: Implement MS style inline asm statement printer.
  Indent() << "__asm ";
  if (Node->hasBraces())
    OS << "{" << NL;
  OS << Node->getAsmString() << NL;
  if (Node->hasBraces())
    Indent() << "}" << NL;
}

void StmtPrinterFiltering::VisitCapturedStmt(CapturedStmt *Node) {
  PrintStmt(Node->getCapturedDecl()->getBody());
}

void StmtPrinterFiltering::VisitObjCAtTryStmt(ObjCAtTryStmt *Node) {
  Indent() << "@try";
  if (auto *TS = dyn_cast<CompoundStmt>(Node->getTryBody())) {
    PrintRawCompoundStmt(TS);
    OS << NL;
  }

  for (unsigned I = 0, N = Node->getNumCatchStmts(); I != N; ++I) {
    ObjCAtCatchStmt *catchStmt = Node->getCatchStmt(I);
    Indent() << "@catch(";
    if (catchStmt->getCatchParamDecl()) {
      if (Decl *DS = catchStmt->getCatchParamDecl())
        PrintRawDecl(DS);
    }
    OS << ")";
    if (auto *CS = dyn_cast<CompoundStmt>(catchStmt->getCatchBody())) {
      PrintRawCompoundStmt(CS);
      OS << NL;
    }
  }

  if (auto *FS = static_cast<ObjCAtFinallyStmt *>(Node->getFinallyStmt())) {
    Indent() << "@finally";
    PrintRawCompoundStmt(dyn_cast<CompoundStmt>(FS->getFinallyBody()));
    OS << NL;
  }
}

void StmtPrinterFiltering::VisitObjCAtFinallyStmt(ObjCAtFinallyStmt *Node) {}

void StmtPrinterFiltering::VisitObjCAtCatchStmt(ObjCAtCatchStmt *Node) {
  Indent() << "@catch (...) { /* todo */ } " << NL;
}

void StmtPrinterFiltering::VisitObjCAtThrowStmt(ObjCAtThrowStmt *Node) {
  Indent() << "@throw";
  if (Node->getThrowExpr()) {
    OS << " ";
    PrintExpr(Node->getThrowExpr());
  }
  OS << ";" << NL;
}

void StmtPrinterFiltering::VisitObjCAvailabilityCheckExpr(
    ObjCAvailabilityCheckExpr *Node) {
  OS << "@available(...)";
}

void StmtPrinterFiltering::VisitObjCAtSynchronizedStmt(
    ObjCAtSynchronizedStmt *Node) {
  Indent() << "@synchronized (";
  PrintExpr(Node->getSynchExpr());
  OS << ")";
  PrintRawCompoundStmt(Node->getSynchBody());
  OS << NL;
}

void StmtPrinterFiltering::VisitObjCAutoreleasePoolStmt(
    ObjCAutoreleasePoolStmt *Node) {
  Indent() << "@autoreleasepool";
  PrintRawCompoundStmt(dyn_cast<CompoundStmt>(Node->getSubStmt()));
  OS << NL;
}

void StmtPrinterFiltering::PrintRawCXXCatchStmt(CXXCatchStmt *Node) {
  OS << "catch (";
  if (Decl *ExDecl = Node->getExceptionDecl())
    PrintRawDecl(ExDecl);
  else
    OS << "...";
  OS << ") ";
  PrintRawCompoundStmt(cast<CompoundStmt>(Node->getHandlerBlock()));
}

void StmtPrinterFiltering::VisitCXXCatchStmt(CXXCatchStmt *Node) {
  Indent();
  PrintRawCXXCatchStmt(Node);
  OS << NL;
}

void StmtPrinterFiltering::VisitCXXTryStmt(CXXTryStmt *Node) {
  Indent() << "try ";
  PrintRawCompoundStmt(Node->getTryBlock());
  for (unsigned i = 0, e = Node->getNumHandlers(); i < e; ++i) {
    OS << " ";
    PrintRawCXXCatchStmt(Node->getHandler(i));
  }
  OS << NL;
}

void StmtPrinterFiltering::VisitSEHTryStmt(SEHTryStmt *Node) {
  Indent() << (Node->getIsCXXTry() ? "try " : "__try ");
  PrintRawCompoundStmt(Node->getTryBlock());
  SEHExceptStmt *E = Node->getExceptHandler();
  SEHFinallyStmt *F = Node->getFinallyHandler();
  if (E)
    PrintRawSEHExceptHandler(E);
  else {
    assert(F && "Must have a finally block...");
    PrintRawSEHFinallyStmt(F);
  }
  OS << NL;
}

void StmtPrinterFiltering::PrintRawSEHFinallyStmt(SEHFinallyStmt *Node) {
  OS << "__finally ";
  PrintRawCompoundStmt(Node->getBlock());
  OS << NL;
}

void StmtPrinterFiltering::PrintRawSEHExceptHandler(SEHExceptStmt *Node) {
  OS << "__except (";
  VisitExpr(Node->getFilterExpr());
  OS << ")" << NL;
  PrintRawCompoundStmt(Node->getBlock());
  OS << NL;
}

void StmtPrinterFiltering::VisitSEHExceptStmt(SEHExceptStmt *Node) {
  Indent();
  PrintRawSEHExceptHandler(Node);
  OS << NL;
}

void StmtPrinterFiltering::VisitSEHFinallyStmt(SEHFinallyStmt *Node) {
  Indent();
  PrintRawSEHFinallyStmt(Node);
  OS << NL;
}

void StmtPrinterFiltering::VisitSEHLeaveStmt(SEHLeaveStmt *Node) {
  Indent() << "__leave;";
  if (Policy.IncludeNewlines)
    OS << NL;
}

//===----------------------------------------------------------------------===//
//  OpenMP directives printing methods
//===----------------------------------------------------------------------===//

void StmtPrinterFiltering::PrintOMPExecutableDirective(
    OMPExecutableDirective *S, bool ForceNoStmt) {
  OMPClausePrinter Printer(OS, Policy);
  ArrayRef<OMPClause *> Clauses = S->clauses();
  for (auto *Clause : Clauses)
    if (Clause && !Clause->isImplicit()) {
      OS << ' ';
      Printer.Visit(Clause);
    }
  OS << NL;
  if (!ForceNoStmt && S->hasAssociatedStmt())
    PrintStmt(S->getInnermostCapturedStmt()->getCapturedStmt());
}

void StmtPrinterFiltering::VisitOMPParallelDirective(
    OMPParallelDirective *Node) {
  Indent() << "#pragma omp parallel";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPSimdDirective(OMPSimdDirective *Node) {
  Indent() << "#pragma omp simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPForDirective(OMPForDirective *Node) {
  Indent() << "#pragma omp for";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPForSimdDirective(OMPForSimdDirective *Node) {
  Indent() << "#pragma omp for simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPSectionsDirective(
    OMPSectionsDirective *Node) {
  Indent() << "#pragma omp sections";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPSectionDirective(OMPSectionDirective *Node) {
  Indent() << "#pragma omp section";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPSingleDirective(OMPSingleDirective *Node) {
  Indent() << "#pragma omp single";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPMasterDirective(OMPMasterDirective *Node) {
  Indent() << "#pragma omp master";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPCriticalDirective(
    OMPCriticalDirective *Node) {
  Indent() << "#pragma omp critical";
  if (Node->getDirectiveName().getName()) {
    OS << " (";
    Node->getDirectiveName().printName(OS, Policy);
    OS << ")";
  }
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPParallelForDirective(
    OMPParallelForDirective *Node) {
  Indent() << "#pragma omp parallel for";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPParallelForSimdDirective(
    OMPParallelForSimdDirective *Node) {
  Indent() << "#pragma omp parallel for simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPParallelMasterDirective(
    OMPParallelMasterDirective *Node) {
  Indent() << "#pragma omp parallel master";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPParallelSectionsDirective(
    OMPParallelSectionsDirective *Node) {
  Indent() << "#pragma omp parallel sections";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTaskDirective(OMPTaskDirective *Node) {
  Indent() << "#pragma omp task";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTaskyieldDirective(
    OMPTaskyieldDirective *Node) {
  Indent() << "#pragma omp taskyield";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPBarrierDirective(OMPBarrierDirective *Node) {
  Indent() << "#pragma omp barrier";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTaskwaitDirective(
    OMPTaskwaitDirective *Node) {
  Indent() << "#pragma omp taskwait";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTaskgroupDirective(
    OMPTaskgroupDirective *Node) {
  Indent() << "#pragma omp taskgroup";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPFlushDirective(OMPFlushDirective *Node) {
  Indent() << "#pragma omp flush";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPOrderedDirective(OMPOrderedDirective *Node) {
  Indent() << "#pragma omp ordered";
  PrintOMPExecutableDirective(Node, Node->hasClausesOfKind<OMPDependClause>());
}

void StmtPrinterFiltering::VisitOMPAtomicDirective(OMPAtomicDirective *Node) {
  Indent() << "#pragma omp atomic";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTargetDirective(OMPTargetDirective *Node) {
  Indent() << "#pragma omp target";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTargetDataDirective(
    OMPTargetDataDirective *Node) {
  Indent() << "#pragma omp target data";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTargetEnterDataDirective(
    OMPTargetEnterDataDirective *Node) {
  Indent() << "#pragma omp target enter data";
  PrintOMPExecutableDirective(Node, /*ForceNoStmt=*/true);
}

void StmtPrinterFiltering::VisitOMPTargetExitDataDirective(
    OMPTargetExitDataDirective *Node) {
  Indent() << "#pragma omp target exit data";
  PrintOMPExecutableDirective(Node, /*ForceNoStmt=*/true);
}

void StmtPrinterFiltering::VisitOMPTargetParallelDirective(
    OMPTargetParallelDirective *Node) {
  Indent() << "#pragma omp target parallel";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTargetParallelForDirective(
    OMPTargetParallelForDirective *Node) {
  Indent() << "#pragma omp target parallel for";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTeamsDirective(OMPTeamsDirective *Node) {
  Indent() << "#pragma omp teams";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPCancellationPointDirective(
    OMPCancellationPointDirective *Node) {
  Indent() << "#pragma omp cancellation point "
           << getOpenMPDirectiveName(Node->getCancelRegion());
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPCancelDirective(OMPCancelDirective *Node) {
  Indent() << "#pragma omp cancel "
           << getOpenMPDirectiveName(Node->getCancelRegion());
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTaskLoopDirective(
    OMPTaskLoopDirective *Node) {
  Indent() << "#pragma omp taskloop";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTaskLoopSimdDirective(
    OMPTaskLoopSimdDirective *Node) {
  Indent() << "#pragma omp taskloop simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPMasterTaskLoopDirective(
    OMPMasterTaskLoopDirective *Node) {
  Indent() << "#pragma omp master taskloop";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPMasterTaskLoopSimdDirective(
    OMPMasterTaskLoopSimdDirective *Node) {
  Indent() << "#pragma omp master taskloop simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPParallelMasterTaskLoopDirective(
    OMPParallelMasterTaskLoopDirective *Node) {
  Indent() << "#pragma omp parallel master taskloop";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPParallelMasterTaskLoopSimdDirective(
    OMPParallelMasterTaskLoopSimdDirective *Node) {
  Indent() << "#pragma omp parallel master taskloop simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPDistributeDirective(
    OMPDistributeDirective *Node) {
  Indent() << "#pragma omp distribute";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTargetUpdateDirective(
    OMPTargetUpdateDirective *Node) {
  Indent() << "#pragma omp target update";
  PrintOMPExecutableDirective(Node, /*ForceNoStmt=*/true);
}

void StmtPrinterFiltering::VisitOMPDistributeParallelForDirective(
    OMPDistributeParallelForDirective *Node) {
  Indent() << "#pragma omp distribute parallel for";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPDistributeParallelForSimdDirective(
    OMPDistributeParallelForSimdDirective *Node) {
  Indent() << "#pragma omp distribute parallel for simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPDistributeSimdDirective(
    OMPDistributeSimdDirective *Node) {
  Indent() << "#pragma omp distribute simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTargetParallelForSimdDirective(
    OMPTargetParallelForSimdDirective *Node) {
  Indent() << "#pragma omp target parallel for simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTargetSimdDirective(
    OMPTargetSimdDirective *Node) {
  Indent() << "#pragma omp target simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTeamsDistributeDirective(
    OMPTeamsDistributeDirective *Node) {
  Indent() << "#pragma omp teams distribute";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTeamsDistributeSimdDirective(
    OMPTeamsDistributeSimdDirective *Node) {
  Indent() << "#pragma omp teams distribute simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTeamsDistributeParallelForSimdDirective(
    OMPTeamsDistributeParallelForSimdDirective *Node) {
  Indent() << "#pragma omp teams distribute parallel for simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTeamsDistributeParallelForDirective(
    OMPTeamsDistributeParallelForDirective *Node) {
  Indent() << "#pragma omp teams distribute parallel for";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTargetTeamsDirective(
    OMPTargetTeamsDirective *Node) {
  Indent() << "#pragma omp target teams";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTargetTeamsDistributeDirective(
    OMPTargetTeamsDistributeDirective *Node) {
  Indent() << "#pragma omp target teams distribute";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTargetTeamsDistributeParallelForDirective(
    OMPTargetTeamsDistributeParallelForDirective *Node) {
  Indent() << "#pragma omp target teams distribute parallel for";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::
    VisitOMPTargetTeamsDistributeParallelForSimdDirective(
        OMPTargetTeamsDistributeParallelForSimdDirective *Node) {
  Indent() << "#pragma omp target teams distribute parallel for simd";
  PrintOMPExecutableDirective(Node);
}

void StmtPrinterFiltering::VisitOMPTargetTeamsDistributeSimdDirective(
    OMPTargetTeamsDistributeSimdDirective *Node) {
  Indent() << "#pragma omp target teams distribute simd";
  PrintOMPExecutableDirective(Node);
}

//===----------------------------------------------------------------------===//
//  Expr printing methods.
//===----------------------------------------------------------------------===//

void StmtPrinterFiltering::VisitSourceLocExpr(SourceLocExpr *Node) {
  OS << Node->getBuiltinStr() << "()";
}

void StmtPrinterFiltering::VisitConstantExpr(ConstantExpr *Node) {
  PrintExpr(Node->getSubExpr());
}

void StmtPrinterFiltering::VisitDeclRefExpr(DeclRefExpr *Node) {
  if (const auto *OCED = dyn_cast<OMPCapturedExprDecl>(Node->getDecl())) {
    selective_printer::printPretty(OCED->getInit()->IgnoreImpCasts(), lines, OS,
                                   nullptr, Policy, Context);
    return;
  }
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getNameInfo();
  if (Node->hasExplicitTemplateArgs())
    printTemplateArgumentList(OS, Node->template_arguments(), Policy);
}

void StmtPrinterFiltering::VisitDependentScopeDeclRefExpr(
    DependentScopeDeclRefExpr *Node) {
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getNameInfo();
  if (Node->hasExplicitTemplateArgs())
    printTemplateArgumentList(OS, Node->template_arguments(), Policy);
}

void StmtPrinterFiltering::VisitUnresolvedLookupExpr(
    UnresolvedLookupExpr *Node) {
  if (Node->getQualifier())
    Node->getQualifier()->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getNameInfo();
  if (Node->hasExplicitTemplateArgs())
    printTemplateArgumentList(OS, Node->template_arguments(), Policy);
}

// ODR rename
static bool isImplicitSelfCopy(const Expr *E) {
  if (const auto *DRE = dyn_cast<DeclRefExpr>(E)) {
    if (const auto *PD = dyn_cast<ImplicitParamDecl>(DRE->getDecl())) {
      if (PD->getParameterKind() == ImplicitParamDecl::ObjCSelf &&
          DRE->getBeginLoc().isInvalid())
        return true;
    }
  }
  return false;
}

void StmtPrinterFiltering::VisitObjCIvarRefExpr(ObjCIvarRefExpr *Node) {
  if (Node->getBase()) {
    if (!Policy.SuppressImplicitBase ||
        !isImplicitSelfCopy(Node->getBase()->IgnoreImpCasts())) {
      PrintExpr(Node->getBase());
      OS << (Node->isArrow() ? "->" : ".");
    }
  }
  OS << *Node->getDecl();
}

void StmtPrinterFiltering::VisitObjCPropertyRefExpr(ObjCPropertyRefExpr *Node) {
  if (Node->isSuperReceiver())
    OS << "super.";
  else if (Node->isObjectReceiver() && Node->getBase()) {
    PrintExpr(Node->getBase());
    OS << ".";
  } else if (Node->isClassReceiver() && Node->getClassReceiver()) {
    OS << Node->getClassReceiver()->getName() << ".";
  }

  if (Node->isImplicitProperty()) {
    if (const auto *Getter = Node->getImplicitPropertyGetter())
      Getter->getSelector().print(OS);
    else
      OS << SelectorTable::getPropertyNameFromSetterSelector(
          Node->getImplicitPropertySetter()->getSelector());
  } else
    OS << Node->getExplicitProperty()->getName();
}

void StmtPrinterFiltering::VisitObjCSubscriptRefExpr(
    ObjCSubscriptRefExpr *Node) {
  PrintExpr(Node->getBaseExpr());
  OS << "[";
  PrintExpr(Node->getKeyExpr());
  OS << "]";
}

void StmtPrinterFiltering::VisitPredefinedExpr(PredefinedExpr *Node) {
  OS << PredefinedExpr::getIdentKindName(Node->getIdentKind());
}

void StmtPrinterFiltering::VisitCharacterLiteral(CharacterLiteral *Node) {
  unsigned value = Node->getValue();

  switch (Node->getKind()) {
  case CharacterLiteral::Ascii:
    break; // no prefix.
  case CharacterLiteral::Wide:
    OS << 'L';
    break;
  case CharacterLiteral::UTF8:
    OS << "u8";
    break;
  case CharacterLiteral::UTF16:
    OS << 'u';
    break;
  case CharacterLiteral::UTF32:
    OS << 'U';
    break;
  }

  switch (value) {
  case '\\':
    OS << "'\\\\'";
    break;
  case '\'':
    OS << "'\\''";
    break;
  case '\a':
    // TODO: K&R: the meaning of '\\a' is different in traditional C
    OS << "'\\a'";
    break;
  case '\b':
    OS << "'\\b'";
    break;
  // Nonstandard escape sequence.
  /*case '\e':
    OS << "'\\e'";
    break;*/
  case '\f':
    OS << "'\\f'";
    break;
  case '\n':
    OS << "'\\n'";
    break;
  case '\r':
    OS << "'\\r'";
    break;
  case '\t':
    OS << "'\\t'";
    break;
  case '\v':
    OS << "'\\v'";
    break;
  default:
    // A character literal might be sign-extended, which
    // would result in an invalid \U escape sequence.
    // FIXME: multicharacter literals such as '\xFF\xFF\xFF\xFF'
    // are not correctly handled.
    if ((value & ~0xFFu) == ~0xFFu &&
        Node->getKind() == CharacterLiteral::Ascii)
      value &= 0xFFu;
    if (value < 256 && isPrintable((unsigned char)value))
      OS << "'" << (char)value << "'";
    else if (value < 256)
      OS << "'\\x" << llvm::format("%02x", value) << "'";
    else if (value <= 0xFFFF)
      OS << "'\\u" << llvm::format("%04x", value) << "'";
    else
      OS << "'\\U" << llvm::format("%08x", value) << "'";
  }
}

/// Prints the given expression using the original source text. Returns true on
/// success, false otherwise.
// Renamed to prevent ODR violation
static bool printExprAsWrittenCopy(raw_ostream &OS, Expr *E,
                                   const ASTContext *Context) {
  if (!Context)
    return false;
  bool Invalid = false;
  StringRef Source = Lexer::getSourceText(
      CharSourceRange::getTokenRange(E->getSourceRange()),
      Context->getSourceManager(), Context->getLangOpts(), &Invalid);
  if (!Invalid) {
    OS << Source;
    return true;
  }
  return false;
}

void StmtPrinterFiltering::VisitIntegerLiteral(IntegerLiteral *Node) {
  if (Policy.ConstantsAsWritten && printExprAsWrittenCopy(OS, Node, Context))
    return;
  bool isSigned = Node->getType()->isSignedIntegerType();
  OS << Node->getValue().toString(10, isSigned);

  // Emit suffixes.  Integer literals are always a builtin integer type.
  switch (Node->getType()->castAs<BuiltinType>()->getKind()) {
  default:
    llvm_unreachable("Unexpected type for integer literal!");
  case BuiltinType::Char_S:
  case BuiltinType::Char_U:
    OS << "i8";
    break;
  case BuiltinType::UChar:
    OS << "Ui8";
    break;
  case BuiltinType::Short:
    OS << "i16";
    break;
  case BuiltinType::UShort:
    OS << "Ui16";
    break;
  case BuiltinType::Int:
    break; // no suffix.
  case BuiltinType::UInt:
    OS << 'U';
    break;
  case BuiltinType::Long:
    OS << 'L';
    break;
  case BuiltinType::ULong:
    OS << "UL";
    break;
  case BuiltinType::LongLong:
    OS << "LL";
    break;
  case BuiltinType::ULongLong:
    OS << "ULL";
    break;
  }
}

void StmtPrinterFiltering::VisitFixedPointLiteral(FixedPointLiteral *Node) {
  if (Policy.ConstantsAsWritten && printExprAsWrittenCopy(OS, Node, Context))
    return;
  OS << Node->getValueAsString(/*Radix=*/10);

  switch (Node->getType()->castAs<BuiltinType>()->getKind()) {
  default:
    llvm_unreachable("Unexpected type for fixed point literal!");
  case BuiltinType::ShortFract:
    OS << "hr";
    break;
  case BuiltinType::ShortAccum:
    OS << "hk";
    break;
  case BuiltinType::UShortFract:
    OS << "uhr";
    break;
  case BuiltinType::UShortAccum:
    OS << "uhk";
    break;
  case BuiltinType::Fract:
    OS << "r";
    break;
  case BuiltinType::Accum:
    OS << "k";
    break;
  case BuiltinType::UFract:
    OS << "ur";
    break;
  case BuiltinType::UAccum:
    OS << "uk";
    break;
  case BuiltinType::LongFract:
    OS << "lr";
    break;
  case BuiltinType::LongAccum:
    OS << "lk";
    break;
  case BuiltinType::ULongFract:
    OS << "ulr";
    break;
  case BuiltinType::ULongAccum:
    OS << "ulk";
    break;
  }
}

// Renamed to prevent ODR violation
static void PrintFloatingLiteralCopy(raw_ostream &OS, FloatingLiteral *Node,
                                     bool PrintSuffix) {
  SmallString<16> Str;
  Node->getValue().toString(Str);
  OS << Str;
  if (Str.find_first_not_of("-0123456789") == StringRef::npos)
    OS << '.'; // Trailing dot in order to separate from ints.

  if (!PrintSuffix)
    return;

  // Emit suffixes.  Float literals are always a builtin float type.
  switch (Node->getType()->castAs<BuiltinType>()->getKind()) {
  default:
    llvm_unreachable("Unexpected type for float literal!");
  case BuiltinType::Half:
    break; // FIXME: suffix?
  case BuiltinType::Double:
    break; // no suffix.
  case BuiltinType::Float16:
    OS << "F16";
    break;
  case BuiltinType::Float:
    OS << 'F';
    break;
  case BuiltinType::LongDouble:
    OS << 'L';
    break;
  case BuiltinType::Float128:
    OS << 'Q';
    break;
  }
}

void StmtPrinterFiltering::VisitFloatingLiteral(FloatingLiteral *Node) {
  if (Policy.ConstantsAsWritten && printExprAsWrittenCopy(OS, Node, Context))
    return;
  PrintFloatingLiteralCopy(OS, Node, /*PrintSuffix=*/true);
}

void StmtPrinterFiltering::VisitImaginaryLiteral(ImaginaryLiteral *Node) {
  PrintExpr(Node->getSubExpr());
  OS << "i";
}

void StmtPrinterFiltering::VisitStringLiteral(StringLiteral *Str) {
  Str->outputString(OS);
}

void StmtPrinterFiltering::VisitParenExpr(ParenExpr *Node) {
  OS << "(";
  PrintExpr(Node->getSubExpr());
  OS << ")";
}

void StmtPrinterFiltering::VisitUnaryOperator(UnaryOperator *Node) {
  if (!Node->isPostfix()) {
    OS << UnaryOperator::getOpcodeStr(Node->getOpcode());

    // Print a space if this is an "identifier operator" like __real, or if
    // it might be concatenated incorrectly like '+'.
    switch (Node->getOpcode()) {
    default:
      break;
    case UO_Real:
    case UO_Imag:
    case UO_Extension:
      OS << ' ';
      break;
    case UO_Plus:
    case UO_Minus:
      if (isa<UnaryOperator>(Node->getSubExpr()))
        OS << ' ';
      break;
    }
  }
  PrintExpr(Node->getSubExpr());

  if (Node->isPostfix())
    OS << UnaryOperator::getOpcodeStr(Node->getOpcode());
}

void StmtPrinterFiltering::VisitOffsetOfExpr(OffsetOfExpr *Node) {
  OS << "__builtin_offsetof(";
  Node->getTypeSourceInfo()->getType().print(OS, Policy);
  OS << ", ";
  bool PrintedSomething = false;
  for (unsigned i = 0, n = Node->getNumComponents(); i < n; ++i) {
    OffsetOfNode ON = Node->getComponent(i);
    if (ON.getKind() == OffsetOfNode::Array) {
      // Array node
      OS << "[";
      PrintExpr(Node->getIndexExpr(ON.getArrayExprIndex()));
      OS << "]";
      PrintedSomething = true;
      continue;
    }

    // Skip implicit base indirections.
    if (ON.getKind() == OffsetOfNode::Base)
      continue;

    // Field or identifier node.
    IdentifierInfo *Id = ON.getFieldName();
    if (!Id)
      continue;

    if (PrintedSomething)
      OS << ".";
    else
      PrintedSomething = true;
    OS << Id->getName();
  }
  OS << ")";
}

void StmtPrinterFiltering::VisitUnaryExprOrTypeTraitExpr(
    UnaryExprOrTypeTraitExpr *Node) {
  switch (Node->getKind()) {
  case UETT_SizeOf:
    OS << "sizeof";
    break;
  case UETT_AlignOf:
    if (Policy.Alignof)
      OS << "alignof";
    else if (Policy.UnderscoreAlignof)
      OS << "_Alignof";
    else
      OS << "__alignof";
    break;
  case UETT_PreferredAlignOf:
    OS << "__alignof";
    break;
  case UETT_VecStep:
    OS << "vec_step";
    break;
  case UETT_OpenMPRequiredSimdAlign:
    OS << "__builtin_omp_required_simd_align";
    break;
  }
  if (Node->isArgumentType()) {
    OS << '(';
    Node->getArgumentType().print(OS, Policy);
    OS << ')';
  } else {
    OS << " ";
    PrintExpr(Node->getArgumentExpr());
  }
}

void StmtPrinterFiltering::VisitGenericSelectionExpr(
    GenericSelectionExpr *Node) {
  OS << "_Generic(";
  PrintExpr(Node->getControllingExpr());
  for (const GenericSelectionExpr::Association Assoc : Node->associations()) {
    OS << ", ";
    QualType T = Assoc.getType();
    if (T.isNull())
      OS << "default";
    else
      T.print(OS, Policy);
    OS << ": ";
    PrintExpr(Assoc.getAssociationExpr());
  }
  OS << ")";
}

void StmtPrinterFiltering::VisitArraySubscriptExpr(ArraySubscriptExpr *Node) {
  PrintExpr(Node->getLHS());
  OS << "[";
  PrintExpr(Node->getRHS());
  OS << "]";
}

void StmtPrinterFiltering::VisitOMPArraySectionExpr(OMPArraySectionExpr *Node) {
  PrintExpr(Node->getBase());
  OS << "[";
  if (Node->getLowerBound())
    PrintExpr(Node->getLowerBound());
  if (Node->getColonLoc().isValid()) {
    OS << ":";
    if (Node->getLength())
      PrintExpr(Node->getLength());
  }
  OS << "]";
}

void StmtPrinterFiltering::PrintCallArgs(CallExpr *Call) {
  for (unsigned i = 0, e = Call->getNumArgs(); i != e; ++i) {
    if (isa<CXXDefaultArgExpr>(Call->getArg(i))) {
      // Don't print any defaulted arguments
      break;
    }

    if (i)
      OS << ", ";
    PrintExpr(Call->getArg(i));
  }
}

void StmtPrinterFiltering::VisitCallExpr(CallExpr *Call) {
  PrintExpr(Call->getCallee());
  OS << "(";
  PrintCallArgs(Call);
  OS << ")";
}

// Renamed to prevent ODR violation
static bool isImplicitThisCopy(const Expr *E) {
  if (const auto *TE = dyn_cast<CXXThisExpr>(E))
    return TE->isImplicit();
  return false;
}

void StmtPrinterFiltering::VisitMemberExpr(MemberExpr *Node) {
  if (!Policy.SuppressImplicitBase || !isImplicitThisCopy(Node->getBase())) {
    PrintExpr(Node->getBase());

    auto *ParentMember = dyn_cast<MemberExpr>(Node->getBase());
    FieldDecl *ParentDecl =
        ParentMember ? dyn_cast<FieldDecl>(ParentMember->getMemberDecl())
                     : nullptr;

    if (!ParentDecl || !ParentDecl->isAnonymousStructOrUnion())
      OS << (Node->isArrow() ? "->" : ".");
  }

  if (auto *FD = dyn_cast<FieldDecl>(Node->getMemberDecl()))
    if (FD->isAnonymousStructOrUnion())
      return;

  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getMemberNameInfo();
  if (Node->hasExplicitTemplateArgs())
    printTemplateArgumentList(OS, Node->template_arguments(), Policy);
}

void StmtPrinterFiltering::VisitObjCIsaExpr(ObjCIsaExpr *Node) {
  PrintExpr(Node->getBase());
  OS << (Node->isArrow() ? "->isa" : ".isa");
}

void StmtPrinterFiltering::VisitExtVectorElementExpr(
    ExtVectorElementExpr *Node) {
  PrintExpr(Node->getBase());
  OS << ".";
  OS << Node->getAccessor().getName();
}

void StmtPrinterFiltering::VisitCStyleCastExpr(CStyleCastExpr *Node) {
  OS << '(';
  Node->getTypeAsWritten().print(OS, Policy);
  OS << ')';
  PrintExpr(Node->getSubExpr());
}

void StmtPrinterFiltering::VisitCompoundLiteralExpr(CompoundLiteralExpr *Node) {
  OS << '(';
  Node->getType().print(OS, Policy);
  OS << ')';
  PrintExpr(Node->getInitializer());
}

void StmtPrinterFiltering::VisitImplicitCastExpr(ImplicitCastExpr *Node) {
  // No need to print anything, simply forward to the subexpression.
  PrintExpr(Node->getSubExpr());
}

void StmtPrinterFiltering::VisitBinaryOperator(BinaryOperator *Node) {
  PrintExpr(Node->getLHS());
  OS << " " << BinaryOperator::getOpcodeStr(Node->getOpcode()) << " ";
  PrintExpr(Node->getRHS());
}

void StmtPrinterFiltering::VisitCompoundAssignOperator(
    CompoundAssignOperator *Node) {
  PrintExpr(Node->getLHS());
  OS << " " << BinaryOperator::getOpcodeStr(Node->getOpcode()) << " ";
  PrintExpr(Node->getRHS());
}

void StmtPrinterFiltering::VisitConditionalOperator(ConditionalOperator *Node) {
  PrintExpr(Node->getCond());
  OS << " ? ";
  PrintExpr(Node->getLHS());
  OS << " : ";
  PrintExpr(Node->getRHS());
}

// GNU extensions.

void StmtPrinterFiltering::VisitBinaryConditionalOperator(
    BinaryConditionalOperator *Node) {
  PrintExpr(Node->getCommon());
  OS << " ?: ";
  PrintExpr(Node->getFalseExpr());
}

void StmtPrinterFiltering::VisitAddrLabelExpr(AddrLabelExpr *Node) {
  OS << "&&" << Node->getLabel()->getName();
}

void StmtPrinterFiltering::VisitStmtExpr(StmtExpr *E) {
  OS << "(";
  PrintRawCompoundStmt(E->getSubStmt());
  OS << ")";
}

void StmtPrinterFiltering::VisitChooseExpr(ChooseExpr *Node) {
  OS << "__builtin_choose_expr(";
  PrintExpr(Node->getCond());
  OS << ", ";
  PrintExpr(Node->getLHS());
  OS << ", ";
  PrintExpr(Node->getRHS());
  OS << ")";
}

void StmtPrinterFiltering::VisitGNUNullExpr(GNUNullExpr *) { OS << "__null"; }

void StmtPrinterFiltering::VisitShuffleVectorExpr(ShuffleVectorExpr *Node) {
  OS << "__builtin_shufflevector(";
  for (unsigned i = 0, e = Node->getNumSubExprs(); i != e; ++i) {
    if (i)
      OS << ", ";
    PrintExpr(Node->getExpr(i));
  }
  OS << ")";
}

void StmtPrinterFiltering::VisitConvertVectorExpr(ConvertVectorExpr *Node) {
  OS << "__builtin_convertvector(";
  PrintExpr(Node->getSrcExpr());
  OS << ", ";
  Node->getType().print(OS, Policy);
  OS << ")";
}

void StmtPrinterFiltering::VisitInitListExpr(InitListExpr *Node) {
  if (Node->getSyntacticForm()) {
    Visit(Node->getSyntacticForm());
    return;
  }

  OS << "{";
  for (unsigned i = 0, e = Node->getNumInits(); i != e; ++i) {
    if (i)
      OS << ", ";
    if (Node->getInit(i))
      PrintExpr(Node->getInit(i));
    else
      OS << "{}";
  }
  OS << "}";
}

void StmtPrinterFiltering::VisitArrayInitLoopExpr(ArrayInitLoopExpr *Node) {
  // There's no way to express this expression in any of our supported
  // languages, so just emit something terse and (hopefully) clear.
  OS << "{";
  PrintExpr(Node->getSubExpr());
  OS << "}";
}

void StmtPrinterFiltering::VisitArrayInitIndexExpr(ArrayInitIndexExpr *Node) {
  OS << "*";
}

void StmtPrinterFiltering::VisitParenListExpr(ParenListExpr *Node) {
  OS << "(";
  for (unsigned i = 0, e = Node->getNumExprs(); i != e; ++i) {
    if (i)
      OS << ", ";
    PrintExpr(Node->getExpr(i));
  }
  OS << ")";
}

void StmtPrinterFiltering::VisitDesignatedInitExpr(DesignatedInitExpr *Node) {
  bool NeedsEquals = true;
  for (const DesignatedInitExpr::Designator &D : Node->designators()) {
    if (D.isFieldDesignator()) {
      if (D.getDotLoc().isInvalid()) {
        if (IdentifierInfo *II = D.getFieldName()) {
          OS << II->getName() << ":";
          NeedsEquals = false;
        }
      } else {
        OS << "." << D.getFieldName()->getName();
      }
    } else {
      OS << "[";
      if (D.isArrayDesignator()) {
        PrintExpr(Node->getArrayIndex(D));
      } else {
        PrintExpr(Node->getArrayRangeStart(D));
        OS << " ... ";
        PrintExpr(Node->getArrayRangeEnd(D));
      }
      OS << "]";
    }
  }

  if (NeedsEquals)
    OS << " = ";
  else
    OS << " ";
  PrintExpr(Node->getInit());
}

void StmtPrinterFiltering::VisitDesignatedInitUpdateExpr(
    DesignatedInitUpdateExpr *Node) {
  OS << "{";
  OS << "/*base*/";
  PrintExpr(Node->getBase());
  OS << ", ";

  OS << "/*updater*/";
  PrintExpr(Node->getUpdater());
  OS << "}";
}

void StmtPrinterFiltering::VisitNoInitExpr(NoInitExpr *Node) {
  OS << "/*no init*/";
}

void StmtPrinterFiltering::VisitImplicitValueInitExpr(
    ImplicitValueInitExpr *Node) {
  if (Node->getType()->getAsCXXRecordDecl()) {
    OS << "/*implicit*/";
    Node->getType().print(OS, Policy);
    OS << "()";
  } else {
    OS << "/*implicit*/(";
    Node->getType().print(OS, Policy);
    OS << ')';
    if (Node->getType()->isRecordType())
      OS << "{}";
    else
      OS << 0;
  }
}

void StmtPrinterFiltering::VisitVAArgExpr(VAArgExpr *Node) {
  OS << "__builtin_va_arg(";
  PrintExpr(Node->getSubExpr());
  OS << ", ";
  Node->getType().print(OS, Policy);
  OS << ")";
}

void StmtPrinterFiltering::VisitPseudoObjectExpr(PseudoObjectExpr *Node) {
  PrintExpr(Node->getSyntacticForm());
}

void StmtPrinterFiltering::VisitAtomicExpr(AtomicExpr *Node) {
  const char *Name = nullptr;
  switch (Node->getOp()) {
#define BUILTIN(ID, TYPE, ATTRS)
#define ATOMIC_BUILTIN(ID, TYPE, ATTRS)                                        \
  case AtomicExpr::AO##ID:                                                     \
    Name = #ID "(";                                                            \
    break;
#include "clang/Basic/Builtins.def"
  }
  OS << Name;

  // AtomicExpr stores its subexpressions in a permuted order.
  PrintExpr(Node->getPtr());
  if (Node->getOp() != AtomicExpr::AO__c11_atomic_load &&
      Node->getOp() != AtomicExpr::AO__atomic_load_n &&
      Node->getOp() != AtomicExpr::AO__opencl_atomic_load) {
    OS << ", ";
    PrintExpr(Node->getVal1());
  }
  if (Node->getOp() == AtomicExpr::AO__atomic_exchange || Node->isCmpXChg()) {
    OS << ", ";
    PrintExpr(Node->getVal2());
  }
  if (Node->getOp() == AtomicExpr::AO__atomic_compare_exchange ||
      Node->getOp() == AtomicExpr::AO__atomic_compare_exchange_n) {
    OS << ", ";
    PrintExpr(Node->getWeak());
  }
  if (Node->getOp() != AtomicExpr::AO__c11_atomic_init &&
      Node->getOp() != AtomicExpr::AO__opencl_atomic_init) {
    OS << ", ";
    PrintExpr(Node->getOrder());
  }
  if (Node->isCmpXChg()) {
    OS << ", ";
    PrintExpr(Node->getOrderFail());
  }
  OS << ")";
}

// C++
void StmtPrinterFiltering::VisitCXXOperatorCallExpr(CXXOperatorCallExpr *Node) {
  OverloadedOperatorKind Kind = Node->getOperator();
  if (Kind == OO_PlusPlus || Kind == OO_MinusMinus) {
    if (Node->getNumArgs() == 1) {
      OS << getOperatorSpelling(Kind) << ' ';
      PrintExpr(Node->getArg(0));
    } else {
      PrintExpr(Node->getArg(0));
      OS << ' ' << getOperatorSpelling(Kind);
    }
  } else if (Kind == OO_Arrow) {
    PrintExpr(Node->getArg(0));
  } else if (Kind == OO_Call) {
    PrintExpr(Node->getArg(0));
    OS << '(';
    for (unsigned ArgIdx = 1; ArgIdx < Node->getNumArgs(); ++ArgIdx) {
      if (ArgIdx > 1)
        OS << ", ";
      if (!isa<CXXDefaultArgExpr>(Node->getArg(ArgIdx)))
        PrintExpr(Node->getArg(ArgIdx));
    }
    OS << ')';
  } else if (Kind == OO_Subscript) {
    PrintExpr(Node->getArg(0));
    OS << '[';
    PrintExpr(Node->getArg(1));
    OS << ']';
  } else if (Node->getNumArgs() == 1) {
    OS << getOperatorSpelling(Kind) << ' ';
    PrintExpr(Node->getArg(0));
  } else if (Node->getNumArgs() == 2) {
    PrintExpr(Node->getArg(0));
    OS << ' ' << getOperatorSpelling(Kind) << ' ';
    PrintExpr(Node->getArg(1));
  } else {
    llvm_unreachable("unknown overloaded operator");
  }
}

void StmtPrinterFiltering::VisitCXXMemberCallExpr(CXXMemberCallExpr *Node) {
  // If we have a conversion operator call only print the argument.
  CXXMethodDecl *MD = Node->getMethodDecl();
  if (MD && isa<CXXConversionDecl>(MD)) {
    PrintExpr(Node->getImplicitObjectArgument());
    return;
  }
  VisitCallExpr(cast<CallExpr>(Node));
}

void StmtPrinterFiltering::VisitCUDAKernelCallExpr(CUDAKernelCallExpr *Node) {
  PrintExpr(Node->getCallee());
  OS << "<<<";
  PrintCallArgs(Node->getConfig());
  OS << ">>>(";
  PrintCallArgs(Node);
  OS << ")";
}

void StmtPrinterFiltering::VisitCXXRewrittenBinaryOperator(
    CXXRewrittenBinaryOperator *Node) {
  CXXRewrittenBinaryOperator::DecomposedForm Decomposed =
      Node->getDecomposedForm();
  PrintExpr(const_cast<Expr *>(Decomposed.LHS));
  OS << ' ' << BinaryOperator::getOpcodeStr(Decomposed.Opcode) << ' ';
  PrintExpr(const_cast<Expr *>(Decomposed.RHS));
}

void StmtPrinterFiltering::VisitCXXNamedCastExpr(CXXNamedCastExpr *Node) {
  OS << Node->getCastName() << '<';
  Node->getTypeAsWritten().print(OS, Policy);
  OS << ">(";
  PrintExpr(Node->getSubExpr());
  OS << ")";
}

void StmtPrinterFiltering::VisitCXXStaticCastExpr(CXXStaticCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void StmtPrinterFiltering::VisitCXXDynamicCastExpr(CXXDynamicCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void StmtPrinterFiltering::VisitCXXReinterpretCastExpr(
    CXXReinterpretCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void StmtPrinterFiltering::VisitCXXConstCastExpr(CXXConstCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void StmtPrinterFiltering::VisitBuiltinBitCastExpr(BuiltinBitCastExpr *Node) {
  OS << "__builtin_bit_cast(";
  Node->getTypeInfoAsWritten()->getType().print(OS, Policy);
  OS << ", ";
  PrintExpr(Node->getSubExpr());
  OS << ")";
}

void StmtPrinterFiltering::VisitCXXTypeidExpr(CXXTypeidExpr *Node) {
  OS << "typeid(";
  if (Node->isTypeOperand()) {
    Node->getTypeOperandSourceInfo()->getType().print(OS, Policy);
  } else {
    PrintExpr(Node->getExprOperand());
  }
  OS << ")";
}

void StmtPrinterFiltering::VisitCXXUuidofExpr(CXXUuidofExpr *Node) {
  OS << "__uuidof(";
  if (Node->isTypeOperand()) {
    Node->getTypeOperandSourceInfo()->getType().print(OS, Policy);
  } else {
    PrintExpr(Node->getExprOperand());
  }
  OS << ")";
}

void StmtPrinterFiltering::VisitMSPropertyRefExpr(MSPropertyRefExpr *Node) {
  PrintExpr(Node->getBaseExpr());
  if (Node->isArrow())
    OS << "->";
  else
    OS << ".";
  if (NestedNameSpecifier *Qualifier =
          Node->getQualifierLoc().getNestedNameSpecifier())
    Qualifier->print(OS, Policy);
  OS << Node->getPropertyDecl()->getDeclName();
}

void StmtPrinterFiltering::VisitMSPropertySubscriptExpr(
    MSPropertySubscriptExpr *Node) {
  PrintExpr(Node->getBase());
  OS << "[";
  PrintExpr(Node->getIdx());
  OS << "]";
}

void StmtPrinterFiltering::VisitUserDefinedLiteral(UserDefinedLiteral *Node) {
  switch (Node->getLiteralOperatorKind()) {
  case UserDefinedLiteral::LOK_Raw:
    OS << cast<StringLiteral>(Node->getArg(0)->IgnoreImpCasts())->getString();
    break;
  case UserDefinedLiteral::LOK_Template: {
    const auto *DRE = cast<DeclRefExpr>(Node->getCallee()->IgnoreImpCasts());
    const TemplateArgumentList *Args =
        cast<FunctionDecl>(DRE->getDecl())->getTemplateSpecializationArgs();
    assert(Args);

    if (Args->size() != 1) {
      OS << "operator\"\"" << Node->getUDSuffix()->getName();
      printTemplateArgumentList(OS, Args->asArray(), Policy);
      OS << "()";
      return;
    }

    const TemplateArgument &Pack = Args->get(0);
    for (const auto &P : Pack.pack_elements()) {
      char C = (char)P.getAsIntegral().getZExtValue();
      OS << C;
    }
    break;
  }
  case UserDefinedLiteral::LOK_Integer: {
    // Print integer literal without suffix.
    const auto *Int = cast<IntegerLiteral>(Node->getCookedLiteral());
    OS << Int->getValue().toString(10, /*isSigned*/ false);
    break;
  }
  case UserDefinedLiteral::LOK_Floating: {
    // Print floating literal without suffix.
    auto *Float = cast<FloatingLiteral>(Node->getCookedLiteral());
    PrintFloatingLiteralCopy(OS, Float, /*PrintSuffix=*/false);
    break;
  }
  case UserDefinedLiteral::LOK_String:
  case UserDefinedLiteral::LOK_Character:
    PrintExpr(Node->getCookedLiteral());
    break;
  }
  OS << Node->getUDSuffix()->getName();
}

void StmtPrinterFiltering::VisitCXXBoolLiteralExpr(CXXBoolLiteralExpr *Node) {
  OS << (Node->getValue() ? "true" : "false");
}

void StmtPrinterFiltering::VisitCXXNullPtrLiteralExpr(
    CXXNullPtrLiteralExpr *Node) {
  OS << "nullptr";
}

void StmtPrinterFiltering::VisitCXXThisExpr(CXXThisExpr *Node) { OS << "this"; }

void StmtPrinterFiltering::VisitCXXThrowExpr(CXXThrowExpr *Node) {
  if (!Node->getSubExpr())
    OS << "throw";
  else {
    OS << "throw ";
    PrintExpr(Node->getSubExpr());
  }
}

void StmtPrinterFiltering::VisitCXXDefaultArgExpr(CXXDefaultArgExpr *Node) {
  // Nothing to print: we picked up the default argument.
}

void StmtPrinterFiltering::VisitCXXDefaultInitExpr(CXXDefaultInitExpr *Node) {
  // Nothing to print: we picked up the default initializer.
}

void StmtPrinterFiltering::VisitCXXFunctionalCastExpr(
    CXXFunctionalCastExpr *Node) {
  Node->getType().print(OS, Policy);
  // If there are no parens, this is list-initialization, and the braces are
  // part of the syntax of the inner construct.
  if (Node->getLParenLoc().isValid())
    OS << "(";
  PrintExpr(Node->getSubExpr());
  if (Node->getLParenLoc().isValid())
    OS << ")";
}

void StmtPrinterFiltering::VisitCXXBindTemporaryExpr(
    CXXBindTemporaryExpr *Node) {
  PrintExpr(Node->getSubExpr());
}

void StmtPrinterFiltering::VisitCXXTemporaryObjectExpr(
    CXXTemporaryObjectExpr *Node) {
  Node->getType().print(OS, Policy);
  if (Node->isStdInitListInitialization())
    /* Nothing to do; braces are part of creating the std::initializer_list. */;
  else if (Node->isListInitialization())
    OS << "{";
  else
    OS << "(";
  for (CXXTemporaryObjectExpr::arg_iterator Arg = Node->arg_begin(),
                                            ArgEnd = Node->arg_end();
       Arg != ArgEnd; ++Arg) {
    if ((*Arg)->isDefaultArgument())
      break;
    if (Arg != Node->arg_begin())
      OS << ", ";
    PrintExpr(*Arg);
  }
  if (Node->isStdInitListInitialization())
    /* See above. */;
  else if (Node->isListInitialization())
    OS << "}";
  else
    OS << ")";
}

void StmtPrinterFiltering::VisitLambdaExpr(LambdaExpr *Node) {
  OS << '[';
  bool NeedComma = false;
  switch (Node->getCaptureDefault()) {
  case LCD_None:
    break;

  case LCD_ByCopy:
    OS << '=';
    NeedComma = true;
    break;

  case LCD_ByRef:
    OS << '&';
    NeedComma = true;
    break;
  }
  for (LambdaExpr::capture_iterator C = Node->explicit_capture_begin(),
                                    CEnd = Node->explicit_capture_end();
       C != CEnd; ++C) {
    if (C->capturesVLAType())
      continue;

    if (NeedComma)
      OS << ", ";
    NeedComma = true;

    switch (C->getCaptureKind()) {
    case LCK_This:
      OS << "this";
      break;

    case LCK_StarThis:
      OS << "*this";
      break;

    case LCK_ByRef:
      if (Node->getCaptureDefault() != LCD_ByRef || Node->isInitCapture(C))
        OS << '&';
      OS << C->getCapturedVar()->getName();
      break;

    case LCK_ByCopy:
      OS << C->getCapturedVar()->getName();
      break;

    case LCK_VLAType:
      llvm_unreachable("VLA type in explicit captures.");
    }

    if (C->isPackExpansion())
      OS << "...";

    if (Node->isInitCapture(C))
      PrintExpr(C->getCapturedVar()->getInit());
  }
  OS << ']';

  if (!Node->getExplicitTemplateParameters().empty()) {
    Node->getTemplateParameterList()->print(
        OS, Node->getLambdaClass()->getASTContext(),
        /*OmitTemplateKW*/ true);
  }

  if (Node->hasExplicitParameters()) {
    OS << '(';
    CXXMethodDecl *Method = Node->getCallOperator();
    NeedComma = false;
    for (const auto *P : Method->parameters()) {
      if (NeedComma) {
        OS << ", ";
      } else {
        NeedComma = true;
      }
      std::string ParamStr = P->getNameAsString();
      P->getOriginalType().print(OS, Policy, ParamStr);
    }
    if (Method->isVariadic()) {
      if (NeedComma)
        OS << ", ";
      OS << "...";
    }
    OS << ')';

    if (Node->isMutable())
      OS << " mutable";

    auto *Proto = Method->getType()->castAs<FunctionProtoType>();
    Proto->printExceptionSpecification(OS, Policy);

    // FIXME: Attributes

    // Print the trailing return type if it was specified in the source.
    if (Node->hasExplicitResultType()) {
      OS << " -> ";
      Proto->getReturnType().print(OS, Policy);
    }
  }

  // Print the body.
  OS << ' ';
  if (Policy.TerseOutput)
    OS << "{}";
  else
    PrintRawCompoundStmt(Node->getBody());
}

void StmtPrinterFiltering::VisitCXXScalarValueInitExpr(
    CXXScalarValueInitExpr *Node) {
  if (TypeSourceInfo *TSInfo = Node->getTypeSourceInfo())
    TSInfo->getType().print(OS, Policy);
  else
    Node->getType().print(OS, Policy);
  OS << "()";
}

void StmtPrinterFiltering::VisitCXXNewExpr(CXXNewExpr *E) {
  if (E->isGlobalNew())
    OS << "::";
  OS << "new ";
  unsigned NumPlace = E->getNumPlacementArgs();
  if (NumPlace > 0 && !isa<CXXDefaultArgExpr>(E->getPlacementArg(0))) {
    OS << "(";
    PrintExpr(E->getPlacementArg(0));
    for (unsigned i = 1; i < NumPlace; ++i) {
      if (isa<CXXDefaultArgExpr>(E->getPlacementArg(i)))
        break;
      OS << ", ";
      PrintExpr(E->getPlacementArg(i));
    }
    OS << ") ";
  }
  if (E->isParenTypeId())
    OS << "(";
  std::string TypeS;
  if (Optional<Expr *> Size = E->getArraySize()) {
    llvm::raw_string_ostream s(TypeS);
    s << '[';
    if (*Size)
      selective_printer::printPretty(*Size, lines, s, Helper, Policy, Context);
    s << ']';
  }
  E->getAllocatedType().print(OS, Policy, TypeS);
  if (E->isParenTypeId())
    OS << ")";

  CXXNewExpr::InitializationStyle InitStyle = E->getInitializationStyle();
  if (InitStyle) {
    if (InitStyle == CXXNewExpr::CallInit)
      OS << "(";
    PrintExpr(E->getInitializer());
    if (InitStyle == CXXNewExpr::CallInit)
      OS << ")";
  }
}

void StmtPrinterFiltering::VisitCXXDeleteExpr(CXXDeleteExpr *E) {
  if (E->isGlobalDelete())
    OS << "::";
  OS << "delete ";
  if (E->isArrayForm())
    OS << "[] ";
  PrintExpr(E->getArgument());
}

void StmtPrinterFiltering::VisitCXXPseudoDestructorExpr(
    CXXPseudoDestructorExpr *E) {
  PrintExpr(E->getBase());
  if (E->isArrow())
    OS << "->";
  else
    OS << '.';
  if (E->getQualifier())
    E->getQualifier()->print(OS, Policy);
  OS << "~";

  if (IdentifierInfo *II = E->getDestroyedTypeIdentifier())
    OS << II->getName();
  else
    E->getDestroyedType().print(OS, Policy);
}

void StmtPrinterFiltering::VisitCXXConstructExpr(CXXConstructExpr *E) {
  if (E->isListInitialization() && !E->isStdInitListInitialization())
    OS << "{";

  for (unsigned i = 0, e = E->getNumArgs(); i != e; ++i) {
    if (isa<CXXDefaultArgExpr>(E->getArg(i))) {
      // Don't print any defaulted arguments
      break;
    }

    if (i)
      OS << ", ";
    PrintExpr(E->getArg(i));
  }

  if (E->isListInitialization() && !E->isStdInitListInitialization())
    OS << "}";
}

void StmtPrinterFiltering::VisitCXXInheritedCtorInitExpr(
    CXXInheritedCtorInitExpr *E) {
  // Parens are printed by the surrounding context.
  OS << "<forwarded>";
}

void StmtPrinterFiltering::VisitCXXStdInitializerListExpr(
    CXXStdInitializerListExpr *E) {
  PrintExpr(E->getSubExpr());
}

void StmtPrinterFiltering::VisitExprWithCleanups(ExprWithCleanups *E) {
  // Just forward to the subexpression.
  PrintExpr(E->getSubExpr());
}

void StmtPrinterFiltering::VisitCXXUnresolvedConstructExpr(
    CXXUnresolvedConstructExpr *Node) {
  Node->getTypeAsWritten().print(OS, Policy);
  OS << "(";
  for (CXXUnresolvedConstructExpr::arg_iterator Arg = Node->arg_begin(),
                                                ArgEnd = Node->arg_end();
       Arg != ArgEnd; ++Arg) {
    if (Arg != Node->arg_begin())
      OS << ", ";
    PrintExpr(*Arg);
  }
  OS << ")";
}

void StmtPrinterFiltering::VisitCXXDependentScopeMemberExpr(
    CXXDependentScopeMemberExpr *Node) {
  if (!Node->isImplicitAccess()) {
    PrintExpr(Node->getBase());
    OS << (Node->isArrow() ? "->" : ".");
  }
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getMemberNameInfo();
  if (Node->hasExplicitTemplateArgs())
    printTemplateArgumentList(OS, Node->template_arguments(), Policy);
}

void StmtPrinterFiltering::VisitUnresolvedMemberExpr(
    UnresolvedMemberExpr *Node) {
  if (!Node->isImplicitAccess()) {
    PrintExpr(Node->getBase());
    OS << (Node->isArrow() ? "->" : ".");
  }
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getMemberNameInfo();
  if (Node->hasExplicitTemplateArgs())
    printTemplateArgumentList(OS, Node->template_arguments(), Policy);
}

// Renamed to prevent ODR violation
static const char *getTypeTraitNameCopy(TypeTrait TT) {
  switch (TT) {
#define TYPE_TRAIT_1(Spelling, Name, Key)                                      \
  case clang::UTT_##Name:                                                      \
    return #Spelling;
#define TYPE_TRAIT_2(Spelling, Name, Key)                                      \
  case clang::BTT_##Name:                                                      \
    return #Spelling;
#define TYPE_TRAIT_N(Spelling, Name, Key)                                      \
  case clang::TT_##Name:                                                       \
    return #Spelling;
#include "clang/Basic/TokenKinds.def"
  }
  llvm_unreachable("Type trait not covered by switch");
}

// Renamed to prevent ODR violation
static const char *getTypeTraitNameCopy(ArrayTypeTrait ATT) {
  switch (ATT) {
  case ATT_ArrayRank:
    return "__array_rank";
  case ATT_ArrayExtent:
    return "__array_extent";
  }
  llvm_unreachable("Array type trait not covered by switch");
}

// Renamed to prevent ODR violation
static const char *getExpressionTraitNameCopy(ExpressionTrait ET) {
  switch (ET) {
  case ET_IsLValueExpr:
    return "__is_lvalue_expr";
  case ET_IsRValueExpr:
    return "__is_rvalue_expr";
  }
  llvm_unreachable("Expression type trait not covered by switch");
}

void StmtPrinterFiltering::VisitTypeTraitExpr(TypeTraitExpr *E) {
  OS << getTypeTraitNameCopy(E->getTrait()) << "(";
  for (unsigned I = 0, N = E->getNumArgs(); I != N; ++I) {
    if (I > 0)
      OS << ", ";
    E->getArg(I)->getType().print(OS, Policy);
  }
  OS << ")";
}

void StmtPrinterFiltering::VisitArrayTypeTraitExpr(ArrayTypeTraitExpr *E) {
  OS << getTypeTraitNameCopy(E->getTrait()) << '(';
  E->getQueriedType().print(OS, Policy);
  OS << ')';
}

void StmtPrinterFiltering::VisitExpressionTraitExpr(ExpressionTraitExpr *E) {
  OS << getExpressionTraitNameCopy(E->getTrait()) << '(';
  PrintExpr(E->getQueriedExpression());
  OS << ')';
}

void StmtPrinterFiltering::VisitCXXNoexceptExpr(CXXNoexceptExpr *E) {
  OS << "noexcept(";
  PrintExpr(E->getOperand());
  OS << ")";
}

void StmtPrinterFiltering::VisitPackExpansionExpr(PackExpansionExpr *E) {
  PrintExpr(E->getPattern());
  OS << "...";
}

void StmtPrinterFiltering::VisitSizeOfPackExpr(SizeOfPackExpr *E) {
  OS << "sizeof...(" << *E->getPack() << ")";
}

void StmtPrinterFiltering::VisitSubstNonTypeTemplateParmPackExpr(
    SubstNonTypeTemplateParmPackExpr *Node) {
  OS << *Node->getParameterPack();
}

void StmtPrinterFiltering::VisitSubstNonTypeTemplateParmExpr(
    SubstNonTypeTemplateParmExpr *Node) {
  Visit(Node->getReplacement());
}

void StmtPrinterFiltering::VisitFunctionParmPackExpr(FunctionParmPackExpr *E) {
  OS << *E->getParameterPack();
}

void StmtPrinterFiltering::VisitMaterializeTemporaryExpr(
    MaterializeTemporaryExpr *Node) {
  PrintExpr(Node->getSubExpr());
}

void StmtPrinterFiltering::VisitCXXFoldExpr(CXXFoldExpr *E) {
  OS << "(";
  if (E->getLHS()) {
    PrintExpr(E->getLHS());
    OS << " " << BinaryOperator::getOpcodeStr(E->getOperator()) << " ";
  }
  OS << "...";
  if (E->getRHS()) {
    OS << " " << BinaryOperator::getOpcodeStr(E->getOperator()) << " ";
    PrintExpr(E->getRHS());
  }
  OS << ")";
}

void StmtPrinterFiltering::VisitConceptSpecializationExpr(
    ConceptSpecializationExpr *E) {
  NestedNameSpecifierLoc NNS = E->getNestedNameSpecifierLoc();
  if (NNS)
    NNS.getNestedNameSpecifier()->print(OS, Policy);
  if (E->getTemplateKWLoc().isValid())
    OS << "template ";
  OS << E->getFoundDecl()->getName();
  printTemplateArgumentList(OS, E->getTemplateArgsAsWritten()->arguments(),
                            Policy);
}

void StmtPrinterFiltering::VisitRequiresExpr(RequiresExpr *E) {
  OS << "requires ";
  auto LocalParameters = E->getLocalParameters();
  if (!LocalParameters.empty()) {
    OS << "(";
    for (ParmVarDecl *LocalParam : LocalParameters) {
      PrintRawDecl(LocalParam);
      if (LocalParam != LocalParameters.back())
        OS << ", ";
    }

    OS << ") ";
  }
  OS << "{ ";
  auto Requirements = E->getRequirements();
  for (concepts::Requirement *Req : Requirements) {
    if (auto *TypeReq = dyn_cast<concepts::TypeRequirement>(Req)) {
      if (TypeReq->isSubstitutionFailure())
        OS << "<<error-type>>";
      else
        TypeReq->getType()->getType().print(OS, Policy);
    } else if (auto *ExprReq = dyn_cast<concepts::ExprRequirement>(Req)) {
      if (ExprReq->isCompound())
        OS << "{ ";
      if (ExprReq->isExprSubstitutionFailure())
        OS << "<<error-expression>>";
      else
        PrintExpr(ExprReq->getExpr());
      if (ExprReq->isCompound()) {
        OS << " }";
        if (ExprReq->getNoexceptLoc().isValid())
          OS << " noexcept";
        const auto &RetReq = ExprReq->getReturnTypeRequirement();
        if (!RetReq.isEmpty()) {
          OS << " -> ";
          if (RetReq.isSubstitutionFailure())
            OS << "<<error-type>>";
          else if (RetReq.isTypeConstraint())
            RetReq.getTypeConstraint()->print(OS, Policy);
        }
      }
    } else {
      auto *NestedReq = cast<concepts::NestedRequirement>(Req);
      OS << "requires ";
      if (NestedReq->isSubstitutionFailure())
        OS << "<<error-expression>>";
      else
        PrintExpr(NestedReq->getConstraintExpr());
    }
    OS << "; ";
  }
  OS << "}";
}

// C++ Coroutines TS

void StmtPrinterFiltering::VisitCoroutineBodyStmt(CoroutineBodyStmt *S) {
  Visit(S->getBody());
}

void StmtPrinterFiltering::VisitCoreturnStmt(CoreturnStmt *S) {
  OS << "co_return";
  if (S->getOperand()) {
    OS << " ";
    Visit(S->getOperand());
  }
  OS << ";";
}

void StmtPrinterFiltering::VisitCoawaitExpr(CoawaitExpr *S) {
  OS << "co_await ";
  PrintExpr(S->getOperand());
}

void StmtPrinterFiltering::VisitDependentCoawaitExpr(DependentCoawaitExpr *S) {
  OS << "co_await ";
  PrintExpr(S->getOperand());
}

void StmtPrinterFiltering::VisitCoyieldExpr(CoyieldExpr *S) {
  OS << "co_yield ";
  PrintExpr(S->getOperand());
}

// Obj-C

void StmtPrinterFiltering::VisitObjCStringLiteral(ObjCStringLiteral *Node) {
  OS << "@";
  VisitStringLiteral(Node->getString());
}

void StmtPrinterFiltering::VisitObjCBoxedExpr(ObjCBoxedExpr *E) {
  OS << "@";
  Visit(E->getSubExpr());
}

void StmtPrinterFiltering::VisitObjCArrayLiteral(ObjCArrayLiteral *E) {
  OS << "@[ ";
  ObjCArrayLiteral::child_range Ch = E->children();
  for (auto I = Ch.begin(), E = Ch.end(); I != E; ++I) {
    if (I != Ch.begin())
      OS << ", ";
    Visit(*I);
  }
  OS << " ]";
}

void StmtPrinterFiltering::VisitObjCDictionaryLiteral(
    ObjCDictionaryLiteral *E) {
  OS << "@{ ";
  for (unsigned I = 0, N = E->getNumElements(); I != N; ++I) {
    if (I > 0)
      OS << ", ";

    ObjCDictionaryElement Element = E->getKeyValueElement(I);
    Visit(Element.Key);
    OS << " : ";
    Visit(Element.Value);
    if (Element.isPackExpansion())
      OS << "...";
  }
  OS << " }";
}

void StmtPrinterFiltering::VisitObjCEncodeExpr(ObjCEncodeExpr *Node) {
  OS << "@encode(";
  Node->getEncodedType().print(OS, Policy);
  OS << ')';
}

void StmtPrinterFiltering::VisitObjCSelectorExpr(ObjCSelectorExpr *Node) {
  OS << "@selector(";
  Node->getSelector().print(OS);
  OS << ')';
}

void StmtPrinterFiltering::VisitObjCProtocolExpr(ObjCProtocolExpr *Node) {
  OS << "@protocol(" << *Node->getProtocol() << ')';
}

void StmtPrinterFiltering::VisitObjCMessageExpr(ObjCMessageExpr *Mess) {
  OS << "[";
  switch (Mess->getReceiverKind()) {
  case ObjCMessageExpr::Instance:
    PrintExpr(Mess->getInstanceReceiver());
    break;

  case ObjCMessageExpr::Class:
    Mess->getClassReceiver().print(OS, Policy);
    break;

  case ObjCMessageExpr::SuperInstance:
  case ObjCMessageExpr::SuperClass:
    OS << "Super";
    break;
  }

  OS << ' ';
  Selector selector = Mess->getSelector();
  if (selector.isUnarySelector()) {
    OS << selector.getNameForSlot(0);
  } else {
    for (unsigned i = 0, e = Mess->getNumArgs(); i != e; ++i) {
      if (i < selector.getNumArgs()) {
        if (i > 0)
          OS << ' ';
        if (selector.getIdentifierInfoForSlot(i))
          OS << selector.getIdentifierInfoForSlot(i)->getName() << ':';
        else
          OS << ":";
      } else
        OS << ", "; // Handle variadic methods.

      PrintExpr(Mess->getArg(i));
    }
  }
  OS << "]";
}

void StmtPrinterFiltering::VisitObjCBoolLiteralExpr(ObjCBoolLiteralExpr *Node) {
  OS << (Node->getValue() ? "__objc_yes" : "__objc_no");
}

void StmtPrinterFiltering::VisitObjCIndirectCopyRestoreExpr(
    ObjCIndirectCopyRestoreExpr *E) {
  PrintExpr(E->getSubExpr());
}

void StmtPrinterFiltering::VisitObjCBridgedCastExpr(ObjCBridgedCastExpr *E) {
  OS << '(' << E->getBridgeKindName();
  E->getType().print(OS, Policy);
  OS << ')';
  PrintExpr(E->getSubExpr());
}

void StmtPrinterFiltering::VisitBlockExpr(BlockExpr *Node) {
  BlockDecl *BD = Node->getBlockDecl();
  OS << "^";

  const FunctionType *AFT = Node->getFunctionType();

  if (isa<FunctionNoProtoType>(AFT)) {
    OS << "()";
  } else if (!BD->param_empty() || cast<FunctionProtoType>(AFT)->isVariadic()) {
    OS << '(';
    for (BlockDecl::param_iterator AI = BD->param_begin(), E = BD->param_end();
         AI != E; ++AI) {
      if (AI != BD->param_begin())
        OS << ", ";
      std::string ParamStr = (*AI)->getNameAsString();
      (*AI)->getType().print(OS, Policy, ParamStr);
    }

    const auto *FT = cast<FunctionProtoType>(AFT);
    if (FT->isVariadic()) {
      if (!BD->param_empty())
        OS << ", ";
      OS << "...";
    }
    OS << ')';
  }
  OS << "{ }";
}

void StmtPrinterFiltering::VisitOpaqueValueExpr(OpaqueValueExpr *Node) {
  PrintExpr(Node->getSourceExpr());
}

void StmtPrinterFiltering::VisitTypoExpr(TypoExpr *Node) {
  // TODO: Print something reasonable for a TypoExpr, if necessary.
  llvm_unreachable("Cannot print TypoExpr nodes");
}

void StmtPrinterFiltering::VisitAsTypeExpr(AsTypeExpr *Node) {
  OS << "__builtin_astype(";
  PrintExpr(Node->getSrcExpr());
  OS << ", ";
  Node->getType().print(OS, Policy);
  OS << ")";
}

//===----------------------------------------------------------------------===//
// Stmt method implementations
//===----------------------------------------------------------------------===//

namespace selective_printer {
void printPretty(const Stmt *stmt, const std::set<unsigned int> &lines,
                 raw_ostream &Out, PrinterHelper *Helper,
                 const PrintingPolicy &Policy, const clang::ASTContext *Context,
                 unsigned Indentation, StringRef NL) {
  StmtPrinterFiltering P(lines, Out, Helper, Policy, Indentation, NL, Context);
  P.Visit(const_cast<Stmt *>(stmt));
}

} // namespace selective_printer
