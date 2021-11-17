//===--- DeclPrinter.cpp - Printing implementation for Decl ASTs ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Decl::print method, which pretty prints the
// AST back out to C/Objective-C/C++/Objective-C++ code.
//
//===----------------------------------------------------------------------===//

/**
 *  This file is ripped from llvm 10.0.1 and modified for selective printing
 */

#include "SelectiveDeclPrinter.h"
#include "SelectiveStmtPrinter.h"
#include "source_utils.h"

#include <cstring>
#include <type_traits>

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclObjC.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/DeclVisitor.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/Basic/Module.h>
#include <llvm/Support/raw_ostream.h>
using namespace clang;

namespace {
class DeclPrinterFiltering : public DeclVisitor<DeclPrinterFiltering> {
  const std::set<unsigned int> &lines;
  raw_ostream &Out;
  PrintingPolicy Policy;
  const ASTContext &Context;
  unsigned Indentation;
  bool PrintInstantiation;

public:
#define PTR(CLASS) typename std::add_pointer<CLASS>::type
#define DISPATCH(NAME, CLASS)                                                  \
  static_cast<DeclPrinterFiltering *>(this)->Visit##NAME(                      \
      static_cast<PTR(CLASS)>(D))

  bool Visit(PTR(Decl) D) {
    if (!utils::shouldBeSliced(D, Context.getSourceManager(), lines)) {
      return false;
    }
    switch (D->getKind()) {
#define DECL(DERIVED, BASE)                                                    \
  case Decl::DERIVED:                                                          \
    DISPATCH(DERIVED##Decl, DERIVED##Decl);                                    \
    return true;
#define ABSTRACT_DECL(DECL)
#include "clang/AST/DeclNodes.inc"
    }
    llvm_unreachable("Decl that isn't part of DeclNodes.inc!");
  }

  /*  //Patched to support filtering
  public:
  #define PTR(CLASS) typename std::add_pointer<CLASS>::type
  #define DISPATCH(NAME, CLASS) \
    return
  static_cast<DeclPrinterFiltering*>(this)->Visit##NAME(static_cast<PTR(CLASS)>(D))

    void Visit(PTR(Decl) D) {
      //D->dump();
      const auto& sm = Context.getSourceManager();
      const auto begin = utils::getLocationAsWritten(D->getBeginLoc(), sm);
      if(!utils::isInTargetFile(begin, sm)) {
        return;
      }
      D->dump();
      const auto end = utils::getLocationAsWritten(D->getEndLoc(), sm);
      if(!utils::isAnyInRangeInWhitelist(
              utils::getLineFromSourceLocation(begin),
              utils::getLineFromSourceLocation(end),
              lines)) {
        return;
      }
      switch (D->getKind()) {
  #define DECL(DERIVED, BASE) \
        case Decl::DERIVED: DISPATCH(DERIVED##Decl, DERIVED##Decl);
  #define ABSTRACT_DECL(DECL)
  #include "clang/AST/DeclNodes.inc"
      }
      llvm_unreachable("Decl that isn't part of DeclNodes.inc!");
    }
  #undef PTR
  #undef DISPATCH*/

private:
  raw_ostream &Indent() { return Indent(Indentation); }
  raw_ostream &Indent(unsigned Indentation);
  void ProcessDeclGroup(SmallVectorImpl<Decl *> &Decls);

  void Print(AccessSpecifier AS);
  void PrintConstructorInitializers(CXXConstructorDecl *CDecl,
                                    std::string &Proto);

  /// Print an Objective-C method type in parentheses.
  ///
  /// \param Quals The Objective-C declaration qualifiers.
  /// \param T The type to print.
  void PrintObjCMethodType(ASTContext &Ctx, Decl::ObjCDeclQualifier Quals,
                           QualType T);

  void PrintObjCTypeParams(ObjCTypeParamList *Params);

public:
  DeclPrinterFiltering(const std::set<unsigned int> &lines, raw_ostream &Out,
                       const PrintingPolicy &Policy, const ASTContext &Context,
                       unsigned Indentation = 0,
                       bool PrintInstantiation = false)
      : lines(lines), Out(Out), Policy(Policy), Context(Context),
        Indentation(Indentation), PrintInstantiation(PrintInstantiation) {}

  void VisitDeclContext(DeclContext *DC, bool Indent = true);

  void VisitTranslationUnitDecl(TranslationUnitDecl *D);
  void VisitTypedefDecl(TypedefDecl *D);
  void VisitTypeAliasDecl(TypeAliasDecl *D);
  void VisitEnumDecl(EnumDecl *D);
  void VisitRecordDecl(RecordDecl *D);
  void VisitEnumConstantDecl(EnumConstantDecl *D);
  void VisitEmptyDecl(EmptyDecl *D);
  void VisitFunctionDecl(FunctionDecl *D);
  void VisitFriendDecl(FriendDecl *D);
  void VisitFieldDecl(FieldDecl *D);
  void VisitVarDecl(VarDecl *D);
  void VisitLabelDecl(LabelDecl *D);
  void VisitParmVarDecl(ParmVarDecl *D);
  void VisitFileScopeAsmDecl(FileScopeAsmDecl *D);
  void VisitImportDecl(ImportDecl *D);
  void VisitStaticAssertDecl(StaticAssertDecl *D);
  void VisitNamespaceDecl(NamespaceDecl *D);
  void VisitUsingDirectiveDecl(UsingDirectiveDecl *D);
  void VisitNamespaceAliasDecl(NamespaceAliasDecl *D);
  void VisitCXXRecordDecl(CXXRecordDecl *D);
  void VisitLinkageSpecDecl(LinkageSpecDecl *D);
  void VisitTemplateDecl(const TemplateDecl *D);
  void VisitFunctionTemplateDecl(FunctionTemplateDecl *D);
  void VisitClassTemplateDecl(ClassTemplateDecl *D);
  void VisitClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl *D);
  void VisitClassTemplatePartialSpecializationDecl(
      ClassTemplatePartialSpecializationDecl *D);
  void VisitObjCMethodDecl(ObjCMethodDecl *D);
  void VisitObjCImplementationDecl(ObjCImplementationDecl *D);
  void VisitObjCInterfaceDecl(ObjCInterfaceDecl *D);
  void VisitObjCProtocolDecl(ObjCProtocolDecl *D);
  void VisitObjCCategoryImplDecl(ObjCCategoryImplDecl *D);
  void VisitObjCCategoryDecl(ObjCCategoryDecl *D);
  void VisitObjCCompatibleAliasDecl(ObjCCompatibleAliasDecl *D);
  void VisitObjCPropertyDecl(ObjCPropertyDecl *D);
  void VisitObjCPropertyImplDecl(ObjCPropertyImplDecl *D);
  void VisitUnresolvedUsingTypenameDecl(UnresolvedUsingTypenameDecl *D);
  void VisitUnresolvedUsingValueDecl(UnresolvedUsingValueDecl *D);
  void VisitUsingDecl(UsingDecl *D);
  void VisitUsingShadowDecl(UsingShadowDecl *D);
  void VisitOMPThreadPrivateDecl(OMPThreadPrivateDecl *D);
  void VisitOMPAllocateDecl(OMPAllocateDecl *D);
  void VisitOMPRequiresDecl(OMPRequiresDecl *D);
  void VisitOMPDeclareReductionDecl(OMPDeclareReductionDecl *D);
  void VisitOMPDeclareMapperDecl(OMPDeclareMapperDecl *D);
  void VisitOMPCapturedExprDecl(OMPCapturedExprDecl *D);

  void printTemplateParameters(const TemplateParameterList *Params,
                               bool OmitTemplateKW = false);
  void printTemplateArguments(llvm::ArrayRef<TemplateArgument> Args);
  void printTemplateArguments(llvm::ArrayRef<TemplateArgumentLoc> Args);
  void prettyPrintAttributes(Decl *D);
  void prettyPrintPragmas(Decl *D);
  void printDeclType(QualType T, StringRef DeclName, bool Pack = false);
};
} // namespace

namespace selective_printer {
bool print(const Decl *decl, const std::set<unsigned int> &lines,
           raw_ostream &Out, const PrintingPolicy &Policy, unsigned Indentation,
           bool PrintInstantiation) {
  DeclPrinterFiltering Printer(lines, Out, Policy, decl->getASTContext(),
                               Indentation, PrintInstantiation);
  return Printer.Visit(const_cast<Decl *>(decl));
}

bool print(const Decl *decl, const std::set<unsigned int> &lines,
           raw_ostream &Out, unsigned Indentation, bool PrintInstantiation) {
  return print(decl, lines, Out, decl->getASTContext().getPrintingPolicy(),
               Indentation, PrintInstantiation);
}

bool printGroup(Decl **Begin, unsigned NumDecls,
                const std::set<unsigned int> &lines, raw_ostream &Out,
                const PrintingPolicy &Policy, unsigned Indentation) {
  if (NumDecls == 1) {
    return print(*Begin, lines, Out, Policy, Indentation);
  }

  Decl **End = Begin + NumDecls;
  TagDecl *TD = dyn_cast<TagDecl>(*Begin);
  if (TD)
    ++Begin;

  PrintingPolicy SubPolicy(Policy);

  bool printed = false;
  bool isFirst = true;
  for (; Begin != End; ++Begin) {
    if (isFirst) {
      if (TD)
        SubPolicy.IncludeTagDefinition = true;
      SubPolicy.SuppressSpecifiers = false;
      isFirst = false;
    } else {
      if (!isFirst)
        Out << ", ";
      SubPolicy.IncludeTagDefinition = false;
      SubPolicy.SuppressSpecifiers = true;
    }
    printed |= print(*Begin, lines, Out, SubPolicy, Indentation);
  }
  return printed;
}
} // namespace selective_printer

// TODO JR
// void TemplateParameterList::print(raw_ostream &Out, const ASTContext
// &Context,
//                                  bool OmitTemplateKW) const {
//  print(Out, Context, Context.getPrintingPolicy(), OmitTemplateKW);
//}
//
// void TemplateParameterList::print(raw_ostream &Out, const ASTContext
// &Context,
//                                  const PrintingPolicy &Policy,
//                                  bool OmitTemplateKW) const {
//  DeclPrinterFiltering Printer(Out, Policy, Context);
//  Printer.printTemplateParameters(this, OmitTemplateKW);
//}

// Renamed to prevent ODR violation
static QualType GetBaseTypeCopy(QualType T) {
  // FIXME: This should be on the Type class!
  QualType BaseType = T;
  while (!BaseType->isSpecifierType()) {
    if (const PointerType *PTy = BaseType->getAs<PointerType>())
      BaseType = PTy->getPointeeType();
    else if (const BlockPointerType *BPy = BaseType->getAs<BlockPointerType>())
      BaseType = BPy->getPointeeType();
    else if (const ArrayType *ATy = dyn_cast<ArrayType>(BaseType))
      BaseType = ATy->getElementType();
    else if (const FunctionType *FTy = BaseType->getAs<FunctionType>())
      BaseType = FTy->getReturnType();
    else if (const VectorType *VTy = BaseType->getAs<VectorType>())
      BaseType = VTy->getElementType();
    else if (const ReferenceType *RTy = BaseType->getAs<ReferenceType>())
      BaseType = RTy->getPointeeType();
    else if (const AutoType *ATy = BaseType->getAs<AutoType>())
      BaseType = ATy->getDeducedType();
    else if (const ParenType *PTy = BaseType->getAs<ParenType>())
      BaseType = PTy->desugar();
    else
      // This must be a syntax error.
      break;
  }
  return BaseType;
}

// Renamed to prevent ODR violation
static QualType getDeclTypeCopy(Decl *D) {
  if (TypedefNameDecl *TDD = dyn_cast<TypedefNameDecl>(D))
    return TDD->getUnderlyingType();
  if (ValueDecl *VD = dyn_cast<ValueDecl>(D))
    return VD->getType();
  return QualType();
}

// TODO Jr
// LLVM_DUMP_METHOD void DeclContext::dumpDeclContext() const {
//  // Get the translation unit
//  const DeclContext *DC = this;
//  while (!DC->isTranslationUnit())
//    DC = DC->getParent();
//
//  ASTContext &Ctx = cast<TranslationUnitDecl>(DC)->getASTContext();
//  DeclPrinterFiltering Printer(llvm::errs(), Ctx.getPrintingPolicy(), Ctx, 0);
//  Printer.VisitDeclContext(const_cast<DeclContext *>(this), /*Indent=*/false);
//}

raw_ostream &DeclPrinterFiltering::Indent(unsigned Indentation) {
  for (unsigned i = 0; i != Indentation; ++i)
    Out << "  ";
  return Out;
}

void DeclPrinterFiltering::prettyPrintAttributes(Decl *D) {
  if (Policy.PolishForDeclaration)
    return;

  if (D->hasAttrs()) {
    AttrVec &Attrs = D->getAttrs();
    for (auto *A : Attrs) {
      if (A->isInherited() || A->isImplicit())
        continue;
      switch (A->getKind()) {
#define ATTR(X)
#define PRAGMA_SPELLING_ATTR(X) case attr::X:
#include <clang/Basic/AttrList.inc>
        break;
      default:
        // TODO Jr
        A->printPretty(Out, Policy);
        break;
      }
    }
  }
}

void DeclPrinterFiltering::prettyPrintPragmas(Decl *D) {
  if (Policy.PolishForDeclaration)
    return;

  if (D->hasAttrs()) {
    AttrVec &Attrs = D->getAttrs();
    for (auto *A : Attrs) {
      switch (A->getKind()) {
#define ATTR(X)
#define PRAGMA_SPELLING_ATTR(X) case attr::X:
#include <clang/Basic/AttrList.inc>
        // TODO JR
        A->printPretty(Out, Policy);
        Indent();
        break;
      default:
        break;
      }
    }
  }
}

void DeclPrinterFiltering::printDeclType(QualType T, StringRef DeclName,
                                         bool Pack) {
  // Normally, a PackExpansionType is written as T[3]... (for instance, as a
  // template argument), but if it is the type of a declaration, the ellipsis
  // is placed before the name being declared.
  if (auto *PET = T->getAs<PackExpansionType>()) {
    Pack = true;
    T = PET->getPattern();
  }
  T.print(Out, Policy, (Pack ? "..." : "") + DeclName, Indentation);
}

void DeclPrinterFiltering::ProcessDeclGroup(SmallVectorImpl<Decl *> &Decls) {
  this->Indent();
  bool printed = selective_printer::printGroup(
      Decls.data(), Decls.size(), this->lines, Out, Policy, Indentation);
  if (printed) {
    Out << ";\n";
  }
  Decls.clear();
}

void DeclPrinterFiltering::Print(AccessSpecifier AS) {
  switch (AS) {
  case AS_none:
    llvm_unreachable("No access specifier!");
  case AS_public:
    Out << "public";
    break;
  case AS_protected:
    Out << "protected";
    break;
  case AS_private:
    Out << "private";
    break;
  }
}

void DeclPrinterFiltering::PrintConstructorInitializers(
    CXXConstructorDecl *CDecl, std::string &Proto) {
  bool HasInitializerList = false;
  for (const auto *BMInitializer : CDecl->inits()) {
    if (BMInitializer->isInClassMemberInitializer())
      continue;

    if (!HasInitializerList) {
      Proto += " : ";
      Out << Proto;
      Proto.clear();
      HasInitializerList = true;
    } else
      Out << ", ";

    if (BMInitializer->isAnyMemberInitializer()) {
      FieldDecl *FD = BMInitializer->getAnyMember();
      Out << *FD;
    } else {
      Out << QualType(BMInitializer->getBaseClass(), 0).getAsString(Policy);
    }

    Out << "(";
    if (!BMInitializer->getInit()) {
      // Nothing to print
    } else {
      Expr *Init = BMInitializer->getInit();
      if (ExprWithCleanups *Tmp = dyn_cast<ExprWithCleanups>(Init))
        Init = Tmp->getSubExpr();

      Init = Init->IgnoreParens();

      Expr *SimpleInit = nullptr;
      Expr **Args = nullptr;
      unsigned NumArgs = 0;
      if (ParenListExpr *ParenList = dyn_cast<ParenListExpr>(Init)) {
        Args = ParenList->getExprs();
        NumArgs = ParenList->getNumExprs();
      } else if (CXXConstructExpr *Construct =
                     dyn_cast<CXXConstructExpr>(Init)) {
        Args = Construct->getArgs();
        NumArgs = Construct->getNumArgs();
      } else
        SimpleInit = Init;

      if (SimpleInit)
        selective_printer::printPretty(SimpleInit, lines, Out, nullptr, Policy,
                                       &Context, Indentation);
      else {
        for (unsigned I = 0; I != NumArgs; ++I) {
          assert(Args[I] != nullptr && "Expected non-null Expr");
          if (isa<CXXDefaultArgExpr>(Args[I]))
            break;

          if (I)
            Out << ", ";
          selective_printer::printPretty(Args[I], lines, Out, nullptr, Policy,
                                         &Context, Indentation);
        }
      }
    }
    Out << ")";
    if (BMInitializer->isPackExpansion())
      Out << "...";
  }
}

//----------------------------------------------------------------------------
// Common C declarations
//----------------------------------------------------------------------------

void DeclPrinterFiltering::VisitDeclContext(DeclContext *DC, bool Indent) {
  if (Policy.TerseOutput)
    return;

  if (Indent)
    Indentation += Policy.Indentation;

  SmallVector<Decl *, 2> Decls;
  for (DeclContext::decl_iterator D = DC->decls_begin(), DEnd = DC->decls_end();
       D != DEnd; ++D) {

    // Don't print ObjCIvarDecls, as they are printed when visiting the
    // containing ObjCInterfaceDecl.
    if (isa<ObjCIvarDecl>(*D))
      continue;

    // Skip over implicit declarations in pretty-printing mode.
    if (D->isImplicit())
      continue;

    // Don't print implicit specializations, as they are printed when visiting
    // corresponding templates.
    if (auto FD = dyn_cast<FunctionDecl>(*D))
      if (FD->getTemplateSpecializationKind() == TSK_ImplicitInstantiation &&
          !isa<ClassTemplateSpecializationDecl>(DC))
        continue;

    // The next bits of code handle stuff like "struct {int x;} a,b"; we're
    // forced to merge the declarations because there's no other way to
    // refer to the struct in question.  When that struct is named instead, we
    // also need to merge to avoid splitting off a stand-alone struct
    // declaration that produces the warning ext_no_declarators in some
    // contexts.
    //
    // This limited merging is safe without a bunch of other checks because it
    // only merges declarations directly referring to the tag, not typedefs.
    //
    // Check whether the current declaration should be grouped with a previous
    // non-free-standing tag declaration.
    QualType CurDeclType = getDeclTypeCopy(*D);
    if (!Decls.empty() && !CurDeclType.isNull()) {
      QualType BaseType = GetBaseTypeCopy(CurDeclType);
      if (!BaseType.isNull() && isa<ElaboratedType>(BaseType) &&
          cast<ElaboratedType>(BaseType)->getOwnedTagDecl() == Decls[0]) {
        Decls.push_back(*D);
        continue;
      }
    }

    // If we have a merged group waiting to be handled, handle it now.
    if (!Decls.empty())
      ProcessDeclGroup(Decls);

    // If the current declaration is not a free standing declaration, save it
    // so we can merge it with the subsequent declaration(s) using it.
    if (isa<TagDecl>(*D) && !cast<TagDecl>(*D)->isFreeStanding()) {
      Decls.push_back(*D);
      continue;
    }

    if (isa<AccessSpecDecl>(*D)) {
      Indentation -= Policy.Indentation;
      this->Indent();
      Print(D->getAccess());
      Out << ":\n";
      Indentation += Policy.Indentation;
      continue;
    }

    this->Indent();
    bool printed = Visit(*D);

    // FIXME: Need to be able to tell the DeclPrinter when
    const char *Terminator = nullptr;
    if (isa<OMPThreadPrivateDecl>(*D) || isa<OMPDeclareReductionDecl>(*D) ||
        isa<OMPDeclareMapperDecl>(*D) || isa<OMPRequiresDecl>(*D) ||
        isa<OMPAllocateDecl>(*D))
      Terminator = nullptr;
    else if (isa<ObjCMethodDecl>(*D) && cast<ObjCMethodDecl>(*D)->hasBody())
      Terminator = nullptr;
    else if (auto FD = dyn_cast<FunctionDecl>(*D)) {
      if (FD->isThisDeclarationADefinition())
        Terminator = nullptr;
      else
        Terminator = ";";
    } else if (auto TD = dyn_cast<FunctionTemplateDecl>(*D)) {
      if (TD->getTemplatedDecl()->isThisDeclarationADefinition())
        Terminator = nullptr;
      else
        Terminator = ";";
    } else if (isa<NamespaceDecl>(*D) || isa<LinkageSpecDecl>(*D) ||
               isa<ObjCImplementationDecl>(*D) || isa<ObjCInterfaceDecl>(*D) ||
               isa<ObjCProtocolDecl>(*D) || isa<ObjCCategoryImplDecl>(*D) ||
               isa<ObjCCategoryDecl>(*D))
      Terminator = nullptr;
    else if (isa<EnumConstantDecl>(*D)) {
      DeclContext::decl_iterator Next = D;
      ++Next;
      if (Next != DEnd)
        Terminator = ",";
    } else
      Terminator = ";";

    if (Terminator) {
      if (printed || std::strcmp(Terminator, ";") != 0) {
        Out << Terminator;
      }
    }
    if (!Policy.TerseOutput &&
        ((isa<FunctionDecl>(*D) &&
          cast<FunctionDecl>(*D)->doesThisDeclarationHaveABody()) ||
         (isa<FunctionTemplateDecl>(*D) &&
          cast<FunctionTemplateDecl>(*D)
              ->getTemplatedDecl()
              ->doesThisDeclarationHaveABody())))
      ; // StmtPrinter already added '\n' after CompoundStmt.
    else {
      if (printed) {
        Out << "\n";
      }
    }

    // Declare target attribute is special one, natural spelling for the pragma
    // assumes "ending" construct so print it here.
    if (D->hasAttr<OMPDeclareTargetDeclAttr>())
      Out << "#pragma omp end declare target\n";
  }

  if (!Decls.empty())
    ProcessDeclGroup(Decls);

  if (Indent)
    Indentation -= Policy.Indentation;
}

void DeclPrinterFiltering::VisitTranslationUnitDecl(TranslationUnitDecl *D) {
  VisitDeclContext(D, false);
}

void DeclPrinterFiltering::VisitTypedefDecl(TypedefDecl *D) {
  if (!Policy.SuppressSpecifiers) {
    Out << "typedef ";

    if (D->isModulePrivate())
      Out << "__module_private__ ";
  }
  QualType Ty = D->getTypeSourceInfo()->getType();
  Ty.print(Out, Policy, D->getName(), Indentation);
  prettyPrintAttributes(D);
}

void DeclPrinterFiltering::VisitTypeAliasDecl(TypeAliasDecl *D) {
  Out << "using " << *D;
  prettyPrintAttributes(D);
  Out << " = " << D->getTypeSourceInfo()->getType().getAsString(Policy);
}

void DeclPrinterFiltering::VisitEnumDecl(EnumDecl *D) {
  if (!Policy.SuppressSpecifiers && D->isModulePrivate())
    Out << "__module_private__ ";
  Out << "enum";
  if (D->isScoped()) {
    if (D->isScopedUsingClassTag())
      Out << " class";
    else
      Out << " struct";
  }

  prettyPrintAttributes(D);

  Out << ' ' << *D;

  if (D->isFixed() && D->getASTContext().getLangOpts().CPlusPlus11)
    Out << " : " << D->getIntegerType().stream(Policy);

  if (D->isCompleteDefinition()) {
    Out << " {\n";
    VisitDeclContext(D);
    Indent() << "}";
  }
}

void DeclPrinterFiltering::VisitRecordDecl(RecordDecl *D) {
  if (!Policy.SuppressSpecifiers && D->isModulePrivate())
    Out << "__module_private__ ";
  Out << D->getKindName();

  prettyPrintAttributes(D);

  if (D->getIdentifier())
    Out << ' ' << *D;

  if (D->isCompleteDefinition()) {
    Out << " {\n";
    VisitDeclContext(D);
    Indent() << "}";
  }
}

void DeclPrinterFiltering::VisitEnumConstantDecl(EnumConstantDecl *D) {
  Out << *D;
  prettyPrintAttributes(D);
  if (Expr *Init = D->getInitExpr()) {
    Out << " = ";
    selective_printer::printPretty(Init, lines, Out, nullptr, Policy, &Context,
                                   Indentation, "\n");
  }
}

// ODR rename
static void printExplicitSpecifierCopy(const std::set<unsigned int> &lines,
                                       const ASTContext *Context,
                                       ExplicitSpecifier ES,
                                       llvm::raw_ostream &Out,
                                       PrintingPolicy &Policy,
                                       unsigned Indentation) {
  std::string Proto = "explicit";
  llvm::raw_string_ostream EOut(Proto);
  if (ES.getExpr()) {
    EOut << "(";
    selective_printer::printPretty(ES.getExpr(), lines, EOut, nullptr, Policy,
                                   Context, Indentation);
    EOut << ")";
  }
  EOut << " ";
  EOut.flush();
  Out << EOut.str();
}

void DeclPrinterFiltering::VisitFunctionDecl(FunctionDecl *D) {
  if (!D->getDescribedFunctionTemplate() &&
      !D->isFunctionTemplateSpecialization())
    prettyPrintPragmas(D);

  if (D->isFunctionTemplateSpecialization())
    Out << "template<> ";
  else if (!D->getDescribedFunctionTemplate()) {
    for (unsigned I = 0, NumTemplateParams = D->getNumTemplateParameterLists();
         I < NumTemplateParams; ++I)
      printTemplateParameters(D->getTemplateParameterList(I));
  }

  CXXConstructorDecl *CDecl = dyn_cast<CXXConstructorDecl>(D);
  CXXConversionDecl *ConversionDecl = dyn_cast<CXXConversionDecl>(D);
  CXXDeductionGuideDecl *GuideDecl = dyn_cast<CXXDeductionGuideDecl>(D);
  if (!Policy.SuppressSpecifiers) {
    switch (D->getStorageClass()) {
    case SC_None:
      break;
    case SC_Extern:
      Out << "extern ";
      break;
    case SC_Static:
      Out << "static ";
      break;
    case SC_PrivateExtern:
      Out << "__private_extern__ ";
      break;
    case SC_Auto:
    case SC_Register:
      llvm_unreachable("invalid for functions");
    }

    if (D->isInlineSpecified())
      Out << "inline ";
    if (D->isVirtualAsWritten())
      Out << "virtual ";
    if (D->isModulePrivate())
      Out << "__module_private__ ";
    if (D->isConstexprSpecified() && !D->isExplicitlyDefaulted())
      Out << "constexpr ";
    if (D->isConsteval())
      Out << "consteval ";
    ExplicitSpecifier ExplicitSpec = ExplicitSpecifier::getFromDecl(D);
    if (ExplicitSpec.isSpecified())
      printExplicitSpecifierCopy(lines, &Context, ExplicitSpec, Out, Policy,
                                 Indentation);
  }

  PrintingPolicy SubPolicy(Policy);
  SubPolicy.SuppressSpecifiers = false;
  std::string Proto;

  if (Policy.FullyQualifiedName) {
    Proto += D->getQualifiedNameAsString();
  } else {
    llvm::raw_string_ostream OS(Proto);
    if (!Policy.SuppressScope) {
      if (const NestedNameSpecifier *NS = D->getQualifier()) {
        // TODO jr
        NS->print(OS, Policy);
      }
    }
    D->getNameInfo().printName(OS, Policy);
  }

  if (GuideDecl)
    Proto = GuideDecl->getDeducedTemplate()->getDeclName().getAsString();
  if (D->isFunctionTemplateSpecialization()) {
    llvm::raw_string_ostream POut(Proto);
    DeclPrinterFiltering TArgPrinter(this->lines, POut, SubPolicy, Context,
                                     Indentation);
    const auto *TArgAsWritten = D->getTemplateSpecializationArgsAsWritten();
    if (TArgAsWritten && !Policy.PrintCanonicalTypes)
      TArgPrinter.printTemplateArguments(TArgAsWritten->arguments());
    else if (const TemplateArgumentList *TArgs =
                 D->getTemplateSpecializationArgs())
      TArgPrinter.printTemplateArguments(TArgs->asArray());
  }

  QualType Ty = D->getType();
  while (const ParenType *PT = dyn_cast<ParenType>(Ty)) {
    Proto = '(' + Proto + ')';
    Ty = PT->getInnerType();
  }

  if (const FunctionType *AFT = Ty->getAs<FunctionType>()) {
    const FunctionProtoType *FT = nullptr;
    if (D->hasWrittenPrototype())
      FT = dyn_cast<FunctionProtoType>(AFT);

    Proto += "(";
    if (FT) {
      llvm::raw_string_ostream POut(Proto);
      DeclPrinterFiltering ParamPrinter(this->lines, POut, SubPolicy, Context,
                                        Indentation);
      for (unsigned i = 0, e = D->getNumParams(); i != e; ++i) {
        if (i)
          POut << ", ";
        ParamPrinter.VisitParmVarDecl(D->getParamDecl(i));
      }

      if (FT->isVariadic()) {
        if (D->getNumParams())
          POut << ", ";
        POut << "...";
      }
    } else if (D->doesThisDeclarationHaveABody() && !D->hasPrototype()) {
      for (unsigned i = 0, e = D->getNumParams(); i != e; ++i) {
        if (i)
          Proto += ", ";
        Proto += D->getParamDecl(i)->getNameAsString();
      }
    }

    Proto += ")";

    if (FT) {
      if (FT->isConst())
        Proto += " const";
      if (FT->isVolatile())
        Proto += " volatile";
      if (FT->isRestrict())
        Proto += " restrict";

      switch (FT->getRefQualifier()) {
      case RQ_None:
        break;
      case RQ_LValue:
        Proto += " &";
        break;
      case RQ_RValue:
        Proto += " &&";
        break;
      }
    }

    if (FT && FT->hasDynamicExceptionSpec()) {
      Proto += " throw(";
      if (FT->getExceptionSpecType() == EST_MSAny)
        Proto += "...";
      else
        for (unsigned I = 0, N = FT->getNumExceptions(); I != N; ++I) {
          if (I)
            Proto += ", ";

          Proto += FT->getExceptionType(I).getAsString(SubPolicy);
        }
      Proto += ")";
    } else if (FT && isNoexceptExceptionSpec(FT->getExceptionSpecType())) {
      Proto += " noexcept";
      if (isComputedNoexcept(FT->getExceptionSpecType())) {
        Proto += "(";
        llvm::raw_string_ostream EOut(Proto);
        selective_printer::printPretty(FT->getNoexceptExpr(), lines, EOut,
                                       nullptr, SubPolicy, &Context,
                                       Indentation);
        EOut.flush();
        Proto += EOut.str();
        Proto += ")";
      }
    }

    if (CDecl) {
      if (!Policy.TerseOutput)
        PrintConstructorInitializers(CDecl, Proto);
    } else if (!ConversionDecl && !isa<CXXDestructorDecl>(D)) {
      if (FT && FT->hasTrailingReturn()) {
        if (!GuideDecl)
          Out << "auto ";
        Out << Proto << " -> ";
        Proto.clear();
      }
      AFT->getReturnType().print(Out, Policy, Proto);
      Proto.clear();
    }
    Out << Proto;

    if (Expr *TrailingRequiresClause = D->getTrailingRequiresClause()) {
      Out << " requires ";
      selective_printer::printPretty(TrailingRequiresClause, lines, Out,
                                     nullptr, SubPolicy, &Context, Indentation);
    }
  } else {
    Ty.print(Out, Policy, Proto);
  }

  prettyPrintAttributes(D);

  if (D->isPure())
    Out << " = 0";
  else if (D->isDeletedAsWritten())
    Out << " = delete";
  else if (D->isExplicitlyDefaulted())
    Out << " = default";
  else if (D->doesThisDeclarationHaveABody()) {
    if (!Policy.TerseOutput) {
      if (!D->hasPrototype() && D->getNumParams()) {
        // This is a K&R function definition, so we need to print the
        // parameters.
        Out << '\n';
        DeclPrinterFiltering ParamPrinter(this->lines, Out, SubPolicy, Context,
                                          Indentation);
        Indentation += Policy.Indentation;
        for (unsigned i = 0, e = D->getNumParams(); i != e; ++i) {
          Indent();
          ParamPrinter.VisitParmVarDecl(D->getParamDecl(i));
          Out << ";\n";
        }
        Indentation -= Policy.Indentation;
      } else
        Out << ' ';

      if (D->getBody())
        selective_printer::printPretty(D->getBody(), lines, Out, nullptr,
                                       SubPolicy, &Context, Indentation);
    } else {
      if (!Policy.TerseOutput && isa<CXXConstructorDecl>(*D))
        Out << " {}";
    }
  }
}

void DeclPrinterFiltering::VisitFriendDecl(FriendDecl *D) {
  if (TypeSourceInfo *TSI = D->getFriendType()) {
    unsigned NumTPLists = D->getFriendTypeNumTemplateParameterLists();
    for (unsigned i = 0; i < NumTPLists; ++i)
      printTemplateParameters(D->getFriendTypeTemplateParameterList(i));
    Out << "friend ";
    Out << " " << TSI->getType().getAsString(Policy);
  } else if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D->getFriendDecl())) {
    Out << "friend ";
    VisitFunctionDecl(FD);
  } else if (FunctionTemplateDecl *FTD =
                 dyn_cast<FunctionTemplateDecl>(D->getFriendDecl())) {
    Out << "friend ";
    VisitFunctionTemplateDecl(FTD);
  } else if (ClassTemplateDecl *CTD =
                 dyn_cast<ClassTemplateDecl>(D->getFriendDecl())) {
    Out << "friend ";
    VisitRedeclarableTemplateDecl(CTD);
  }
}

void DeclPrinterFiltering::VisitFieldDecl(FieldDecl *D) {
  // FIXME: add printing of pragma attributes if required.
  if (!Policy.SuppressSpecifiers && D->isMutable())
    Out << "mutable ";
  if (!Policy.SuppressSpecifiers && D->isModulePrivate())
    Out << "__module_private__ ";

  Out << D->getASTContext()
             .getUnqualifiedObjCPointerType(D->getType())
             .stream(Policy, D->getName(), Indentation);

  if (D->isBitField()) {
    Out << " : ";
    selective_printer::printPretty(D->getBitWidth(), lines, Out, nullptr,
                                   Policy, &Context, Indentation);
  }

  Expr *Init = D->getInClassInitializer();
  if (!Policy.SuppressInitializers && Init) {
    if (D->getInClassInitStyle() == ICIS_ListInit)
      Out << " ";
    else
      Out << " = ";
    selective_printer::printPretty(Init, lines, Out, nullptr, Policy, &Context,
                                   Indentation);
  }
  prettyPrintAttributes(D);
}

void DeclPrinterFiltering::VisitLabelDecl(LabelDecl *D) { Out << *D << ":"; }

void DeclPrinterFiltering::VisitVarDecl(VarDecl *D) {
  prettyPrintPragmas(D);

  QualType T =
      D->getTypeSourceInfo()
          ? D->getTypeSourceInfo()->getType()
          : D->getASTContext().getUnqualifiedObjCPointerType(D->getType());

  if (!Policy.SuppressSpecifiers) {
    StorageClass SC = D->getStorageClass();
    if (SC != SC_None)
      Out << VarDecl::getStorageClassSpecifierString(SC) << " ";

    switch (D->getTSCSpec()) {
    case TSCS_unspecified:
      break;
    case TSCS___thread:
      Out << "__thread ";
      break;
    case TSCS__Thread_local:
      Out << "_Thread_local ";
      break;
    case TSCS_thread_local:
      Out << "thread_local ";
      break;
    }

    if (D->isModulePrivate())
      Out << "__module_private__ ";

    if (D->isConstexpr()) {
      Out << "constexpr ";
      T.removeLocalConst();
    }
  }

  printDeclType(T, D->getName());
  Expr *Init = D->getInit();
  if (!Policy.SuppressInitializers && Init) {
    bool ImplicitInit = false;
    if (CXXConstructExpr *Construct =
            dyn_cast<CXXConstructExpr>(Init->IgnoreImplicit())) {
      if (D->getInitStyle() == VarDecl::CallInit &&
          !Construct->isListInitialization()) {
        ImplicitInit = Construct->getNumArgs() == 0 ||
                       Construct->getArg(0)->isDefaultArgument();
      }
    }
    if (!ImplicitInit) {
      if ((D->getInitStyle() == VarDecl::CallInit) && !isa<ParenListExpr>(Init))
        Out << "(";
      else if (D->getInitStyle() == VarDecl::CInit) {
        Out << " = ";
      }
      PrintingPolicy SubPolicy(Policy);
      SubPolicy.SuppressSpecifiers = false;
      SubPolicy.IncludeTagDefinition = false;
      selective_printer::printPretty(Init, lines, Out, nullptr, SubPolicy,
                                     &Context, Indentation);
      if ((D->getInitStyle() == VarDecl::CallInit) && !isa<ParenListExpr>(Init))
        Out << ")";
    }
  }
  prettyPrintAttributes(D);
}

void DeclPrinterFiltering::VisitParmVarDecl(ParmVarDecl *D) { VisitVarDecl(D); }

void DeclPrinterFiltering::VisitFileScopeAsmDecl(FileScopeAsmDecl *D) {
  Out << "__asm (";
  selective_printer::printPretty(D->getAsmString(), lines, Out, nullptr, Policy,
                                 &Context, Indentation);
  Out << ")";
}

void DeclPrinterFiltering::VisitImportDecl(ImportDecl *D) {
  Out << "@import " << D->getImportedModule()->getFullModuleName() << ";\n";
}

void DeclPrinterFiltering::VisitStaticAssertDecl(StaticAssertDecl *D) {
  Out << "static_assert(";
  selective_printer::printPretty(D->getAssertExpr(), lines, Out, nullptr,
                                 Policy, &Context, Indentation);
  if (StringLiteral *SL = D->getMessage()) {
    Out << ", ";
    selective_printer::printPretty(SL, lines, Out, nullptr, Policy, &Context,
                                   Indentation);
  }
  Out << ")";
}

//----------------------------------------------------------------------------
// C++ declarations
//----------------------------------------------------------------------------
void DeclPrinterFiltering::VisitNamespaceDecl(NamespaceDecl *D) {
  if (D->isInline())
    Out << "inline ";
  Out << "namespace " << *D << " {\n";
  VisitDeclContext(D);
  Indent() << "}";
}

void DeclPrinterFiltering::VisitUsingDirectiveDecl(UsingDirectiveDecl *D) {
  Out << "using namespace ";
  // TODO jr
  if (D->getQualifier())
    D->getQualifier()->print(Out, Policy);
  Out << *D->getNominatedNamespaceAsWritten();
}

void DeclPrinterFiltering::VisitNamespaceAliasDecl(NamespaceAliasDecl *D) {
  Out << "namespace " << *D << " = ";
  // TODO jr
  if (D->getQualifier())
    D->getQualifier()->print(Out, Policy);
  Out << *D->getAliasedNamespace();
}

void DeclPrinterFiltering::VisitEmptyDecl(EmptyDecl *D) {
  prettyPrintAttributes(D);
}

void DeclPrinterFiltering::VisitCXXRecordDecl(CXXRecordDecl *D) {
  // FIXME: add printing of pragma attributes if required.
  if (!Policy.SuppressSpecifiers && D->isModulePrivate())
    Out << "__module_private__ ";
  Out << D->getKindName();

  prettyPrintAttributes(D);

  if (D->getIdentifier()) {
    Out << ' ' << *D;

    if (auto S = dyn_cast<ClassTemplateSpecializationDecl>(D)) {
      ArrayRef<TemplateArgument> Args = S->getTemplateArgs().asArray();
      if (!Policy.PrintCanonicalTypes)
        if (const auto *TSI = S->getTypeAsWritten())
          if (const auto *TST =
                  dyn_cast<TemplateSpecializationType>(TSI->getType()))
            Args = TST->template_arguments();
      printTemplateArguments(Args);
    }
  }

  if (D->isCompleteDefinition()) {
    // Print the base classes
    if (D->getNumBases()) {
      Out << " : ";
      for (CXXRecordDecl::base_class_iterator Base = D->bases_begin(),
                                              BaseEnd = D->bases_end();
           Base != BaseEnd; ++Base) {
        if (Base != D->bases_begin())
          Out << ", ";

        if (Base->isVirtual())
          Out << "virtual ";

        AccessSpecifier AS = Base->getAccessSpecifierAsWritten();
        if (AS != AS_none) {
          Print(AS);
          Out << " ";
        }
        Out << Base->getType().getAsString(Policy);

        if (Base->isPackExpansion())
          Out << "...";
      }
    }

    // Print the class definition
    // FIXME: Doesn't print access specifiers, e.g., "public:"
    if (Policy.TerseOutput) {
      Out << " {}";
    } else {
      Out << " {\n";
      VisitDeclContext(D);
      Indent() << "}";
    }
  }
}

void DeclPrinterFiltering::VisitLinkageSpecDecl(LinkageSpecDecl *D) {
  const char *l;
  if (D->getLanguage() == LinkageSpecDecl::lang_c)
    l = "C";
  else {
    assert(D->getLanguage() == LinkageSpecDecl::lang_cxx &&
           "unknown language in linkage specification");
    l = "C++";
  }

  Out << "extern \"" << l << "\" ";
  if (D->hasBraces()) {
    Out << "{\n";
    VisitDeclContext(D);
    Indent() << "}";
  } else
    Visit(*D->decls_begin());
}

void DeclPrinterFiltering::printTemplateParameters(
    const TemplateParameterList *Params, bool OmitTemplateKW) {
  assert(Params);

  if (!OmitTemplateKW)
    Out << "template ";
  Out << '<';

  bool NeedComma = false;
  for (const Decl *Param : *Params) {
    if (Param->isImplicit())
      continue;

    if (NeedComma)
      Out << ", ";
    else
      NeedComma = true;

    if (auto TTP = dyn_cast<TemplateTypeParmDecl>(Param)) {

      // TODO jr
      if (const TypeConstraint *TC = TTP->getTypeConstraint())
        TC->print(Out, Policy);
      else if (TTP->wasDeclaredWithTypename())
        Out << "typename";
      else
        Out << "class";

      if (TTP->isParameterPack())
        Out << " ...";
      else if (!TTP->getName().empty())
        Out << ' ';

      Out << *TTP;

      if (TTP->hasDefaultArgument()) {
        Out << " = ";
        Out << TTP->getDefaultArgument().getAsString(Policy);
      };
    } else if (auto NTTP = dyn_cast<NonTypeTemplateParmDecl>(Param)) {
      StringRef Name;
      if (IdentifierInfo *II = NTTP->getIdentifier())
        Name = II->getName();
      printDeclType(NTTP->getType(), Name, NTTP->isParameterPack());

      if (NTTP->hasDefaultArgument()) {
        Out << " = ";
        selective_printer::printPretty(NTTP->getDefaultArgument(), lines, Out,
                                       nullptr, Policy, &Context, Indentation);
      }
    } else if (auto TTPD = dyn_cast<TemplateTemplateParmDecl>(Param)) {
      VisitTemplateDecl(TTPD);
      // FIXME: print the default argument, if present.
    }
  }

  Out << '>';
  if (!OmitTemplateKW)
    Out << ' ';
}

void DeclPrinterFiltering::printTemplateArguments(
    ArrayRef<TemplateArgument> Args) {
  // TODO JR
  Out << "<";
  for (size_t I = 0, E = Args.size(); I < E; ++I) {
    if (I)
      Out << ", ";
    Args[I].print(Policy, Out);
  }
  Out << ">";
}

void DeclPrinterFiltering::printTemplateArguments(
    ArrayRef<TemplateArgumentLoc> Args) {
  // TODO JR
  Out << "<";
  for (size_t I = 0, E = Args.size(); I < E; ++I) {
    if (I)
      Out << ", ";
    Args[I].getArgument().print(Policy, Out);
  }
  Out << ">";
}

void DeclPrinterFiltering::VisitTemplateDecl(const TemplateDecl *D) {
  printTemplateParameters(D->getTemplateParameters());

  if (const TemplateTemplateParmDecl *TTP =
          dyn_cast<TemplateTemplateParmDecl>(D)) {
    Out << "class ";
    if (TTP->isParameterPack())
      Out << "...";
    Out << D->getName();
  } else if (auto *TD = D->getTemplatedDecl())
    Visit(TD);
  else if (const auto *Concept = dyn_cast<ConceptDecl>(D)) {
    Out << "concept " << Concept->getName() << " = ";
    selective_printer::printPretty(Concept->getConstraintExpr(), lines, Out,
                                   nullptr, Policy, &Context, Indentation);
    Out << ";";
  }
}

void DeclPrinterFiltering::VisitFunctionTemplateDecl(FunctionTemplateDecl *D) {
  prettyPrintPragmas(D->getTemplatedDecl());
  // Print any leading template parameter lists.
  if (const FunctionDecl *FD = D->getTemplatedDecl()) {
    for (unsigned I = 0, NumTemplateParams = FD->getNumTemplateParameterLists();
         I < NumTemplateParams; ++I)
      printTemplateParameters(FD->getTemplateParameterList(I));
  }
  VisitRedeclarableTemplateDecl(D);
  // Declare target attribute is special one, natural spelling for the pragma
  // assumes "ending" construct so print it here.
  if (D->getTemplatedDecl()->hasAttr<OMPDeclareTargetDeclAttr>())
    Out << "#pragma omp end declare target\n";

  // Never print "instantiations" for deduction guides (they don't really
  // have them).
  if (PrintInstantiation &&
      !isa<CXXDeductionGuideDecl>(D->getTemplatedDecl())) {
    FunctionDecl *PrevDecl = D->getTemplatedDecl();
    const FunctionDecl *Def;
    if (PrevDecl->isDefined(Def) && Def != PrevDecl)
      return;
    for (auto *I : D->specializations())
      if (I->getTemplateSpecializationKind() == TSK_ImplicitInstantiation) {
        if (!PrevDecl->isThisDeclarationADefinition())
          Out << ";\n";
        Indent();
        prettyPrintPragmas(I);
        Visit(I);
      }
  }
}

void DeclPrinterFiltering::VisitClassTemplateDecl(ClassTemplateDecl *D) {
  VisitRedeclarableTemplateDecl(D);

  if (PrintInstantiation) {
    for (auto *I : D->specializations())
      if (I->getSpecializationKind() == TSK_ImplicitInstantiation) {
        if (D->isThisDeclarationADefinition())
          Out << ";";
        Out << "\n";
        Visit(I);
      }
  }
}

void DeclPrinterFiltering::VisitClassTemplateSpecializationDecl(
    ClassTemplateSpecializationDecl *D) {
  Out << "template<> ";
  VisitCXXRecordDecl(D);
}

void DeclPrinterFiltering::VisitClassTemplatePartialSpecializationDecl(
    ClassTemplatePartialSpecializationDecl *D) {
  printTemplateParameters(D->getTemplateParameters());
  VisitCXXRecordDecl(D);
}

//----------------------------------------------------------------------------
// Objective-C declarations
//----------------------------------------------------------------------------

void DeclPrinterFiltering::PrintObjCMethodType(ASTContext &Ctx,
                                               Decl::ObjCDeclQualifier Quals,
                                               QualType T) {
  Out << '(';
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_In)
    Out << "in ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_Inout)
    Out << "inout ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_Out)
    Out << "out ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_Bycopy)
    Out << "bycopy ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_Byref)
    Out << "byref ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_Oneway)
    Out << "oneway ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_CSNullability) {
    if (auto nullability = AttributedType::stripOuterNullability(T))
      Out << getNullabilitySpelling(*nullability, true) << ' ';
  }

  Out << Ctx.getUnqualifiedObjCPointerType(T).getAsString(Policy);
  Out << ')';
}

void DeclPrinterFiltering::PrintObjCTypeParams(ObjCTypeParamList *Params) {
  Out << "<";
  unsigned First = true;
  for (auto *Param : *Params) {
    if (First) {
      First = false;
    } else {
      Out << ", ";
    }

    switch (Param->getVariance()) {
    case ObjCTypeParamVariance::Invariant:
      break;

    case ObjCTypeParamVariance::Covariant:
      Out << "__covariant ";
      break;

    case ObjCTypeParamVariance::Contravariant:
      Out << "__contravariant ";
      break;
    }

    Out << Param->getDeclName().getAsString();

    if (Param->hasExplicitBound()) {
      Out << " : " << Param->getUnderlyingType().getAsString(Policy);
    }
  }
  Out << ">";
}

void DeclPrinterFiltering::VisitObjCMethodDecl(ObjCMethodDecl *OMD) {
  if (OMD->isInstanceMethod())
    Out << "- ";
  else
    Out << "+ ";
  if (!OMD->getReturnType().isNull()) {
    PrintObjCMethodType(OMD->getASTContext(), OMD->getObjCDeclQualifier(),
                        OMD->getReturnType());
  }

  std::string name = OMD->getSelector().getAsString();
  std::string::size_type pos, lastPos = 0;
  for (const auto *PI : OMD->parameters()) {
    // FIXME: selector is missing here!
    pos = name.find_first_of(':', lastPos);
    if (lastPos != 0)
      Out << " ";
    Out << name.substr(lastPos, pos - lastPos) << ':';
    PrintObjCMethodType(OMD->getASTContext(), PI->getObjCDeclQualifier(),
                        PI->getType());
    Out << *PI;
    lastPos = pos + 1;
  }

  if (OMD->param_begin() == OMD->param_end())
    Out << name;

  if (OMD->isVariadic())
    Out << ", ...";

  prettyPrintAttributes(OMD);

  if (OMD->getBody() && !Policy.TerseOutput) {
    Out << ' ';
    selective_printer::printPretty(OMD->getBody(), lines, Out, nullptr, Policy,
                                   &Context);
  } else if (Policy.PolishForDeclaration)
    Out << ';';
}

void DeclPrinterFiltering::VisitObjCImplementationDecl(
    ObjCImplementationDecl *OID) {
  std::string I = OID->getNameAsString();
  ObjCInterfaceDecl *SID = OID->getSuperClass();

  bool eolnOut = false;
  if (SID)
    Out << "@implementation " << I << " : " << *SID;
  else
    Out << "@implementation " << I;

  if (OID->ivar_size() > 0) {
    Out << "{\n";
    eolnOut = true;
    Indentation += Policy.Indentation;
    for (const auto *I : OID->ivars()) {
      Indent() << I->getASTContext()
                      .getUnqualifiedObjCPointerType(I->getType())
                      .getAsString(Policy)
               << ' ' << *I << ";\n";
    }
    Indentation -= Policy.Indentation;
    Out << "}\n";
  } else if (SID || (OID->decls_begin() != OID->decls_end())) {
    Out << "\n";
    eolnOut = true;
  }
  VisitDeclContext(OID, false);
  if (!eolnOut)
    Out << "\n";
  Out << "@end";
}

void DeclPrinterFiltering::VisitObjCInterfaceDecl(ObjCInterfaceDecl *OID) {
  std::string I = OID->getNameAsString();
  ObjCInterfaceDecl *SID = OID->getSuperClass();

  if (!OID->isThisDeclarationADefinition()) {
    Out << "@class " << I;

    if (auto TypeParams = OID->getTypeParamListAsWritten()) {
      PrintObjCTypeParams(TypeParams);
    }

    Out << ";";
    return;
  }
  bool eolnOut = false;
  Out << "@interface " << I;

  if (auto TypeParams = OID->getTypeParamListAsWritten()) {
    PrintObjCTypeParams(TypeParams);
  }

  if (SID)
    Out << " : " << QualType(OID->getSuperClassType(), 0).getAsString(Policy);

  // Protocols?
  const ObjCList<ObjCProtocolDecl> &Protocols = OID->getReferencedProtocols();
  if (!Protocols.empty()) {
    for (ObjCList<ObjCProtocolDecl>::iterator I = Protocols.begin(),
                                              E = Protocols.end();
         I != E; ++I)
      Out << (I == Protocols.begin() ? '<' : ',') << **I;
    Out << "> ";
  }

  if (OID->ivar_size() > 0) {
    Out << "{\n";
    eolnOut = true;
    Indentation += Policy.Indentation;
    for (const auto *I : OID->ivars()) {
      Indent() << I->getASTContext()
                      .getUnqualifiedObjCPointerType(I->getType())
                      .getAsString(Policy)
               << ' ' << *I << ";\n";
    }
    Indentation -= Policy.Indentation;
    Out << "}\n";
  } else if (SID || (OID->decls_begin() != OID->decls_end())) {
    Out << "\n";
    eolnOut = true;
  }

  VisitDeclContext(OID, false);
  if (!eolnOut)
    Out << "\n";
  Out << "@end";
  // FIXME: implement the rest...
}

void DeclPrinterFiltering::VisitObjCProtocolDecl(ObjCProtocolDecl *PID) {
  if (!PID->isThisDeclarationADefinition()) {
    Out << "@protocol " << *PID << ";\n";
    return;
  }
  // Protocols?
  const ObjCList<ObjCProtocolDecl> &Protocols = PID->getReferencedProtocols();
  if (!Protocols.empty()) {
    Out << "@protocol " << *PID;
    for (ObjCList<ObjCProtocolDecl>::iterator I = Protocols.begin(),
                                              E = Protocols.end();
         I != E; ++I)
      Out << (I == Protocols.begin() ? '<' : ',') << **I;
    Out << ">\n";
  } else
    Out << "@protocol " << *PID << '\n';
  VisitDeclContext(PID, false);
  Out << "@end";
}

void DeclPrinterFiltering::VisitObjCCategoryImplDecl(
    ObjCCategoryImplDecl *PID) {
  Out << "@implementation " << *PID->getClassInterface() << '(' << *PID
      << ")\n";

  VisitDeclContext(PID, false);
  Out << "@end";
  // FIXME: implement the rest...
}

void DeclPrinterFiltering::VisitObjCCategoryDecl(ObjCCategoryDecl *PID) {
  Out << "@interface " << *PID->getClassInterface();
  if (auto TypeParams = PID->getTypeParamList()) {
    PrintObjCTypeParams(TypeParams);
  }
  Out << "(" << *PID << ")\n";
  if (PID->ivar_size() > 0) {
    Out << "{\n";
    Indentation += Policy.Indentation;
    for (const auto *I : PID->ivars())
      Indent() << I->getASTContext()
                      .getUnqualifiedObjCPointerType(I->getType())
                      .getAsString(Policy)
               << ' ' << *I << ";\n";
    Indentation -= Policy.Indentation;
    Out << "}\n";
  }

  VisitDeclContext(PID, false);
  Out << "@end";

  // FIXME: implement the rest...
}

void DeclPrinterFiltering::VisitObjCCompatibleAliasDecl(
    ObjCCompatibleAliasDecl *AID) {
  Out << "@compatibility_alias " << *AID << ' ' << *AID->getClassInterface()
      << ";\n";
}

/// PrintObjCPropertyDecl - print a property declaration.
///
/// Print attributes in the following order:
/// - class
/// - nonatomic | atomic
/// - assign | retain | strong | copy | weak | unsafe_unretained
/// - readwrite | readonly
/// - getter & setter
/// - nullability
void DeclPrinterFiltering::VisitObjCPropertyDecl(ObjCPropertyDecl *PDecl) {
  if (PDecl->getPropertyImplementation() == ObjCPropertyDecl::Required)
    Out << "@required\n";
  else if (PDecl->getPropertyImplementation() == ObjCPropertyDecl::Optional)
    Out << "@optional\n";

  QualType T = PDecl->getType();

  Out << "@property";
  if (PDecl->getPropertyAttributes() != ObjCPropertyDecl::OBJC_PR_noattr) {
    bool first = true;
    Out << "(";
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_class) {
      Out << (first ? "" : ", ") << "class";
      first = false;
    }

    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_direct) {
      Out << (first ? "" : ", ") << "direct";
      first = false;
    }

    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_nonatomic) {
      Out << (first ? "" : ", ") << "nonatomic";
      first = false;
    }
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_atomic) {
      Out << (first ? "" : ", ") << "atomic";
      first = false;
    }

    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_assign) {
      Out << (first ? "" : ", ") << "assign";
      first = false;
    }
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_retain) {
      Out << (first ? "" : ", ") << "retain";
      first = false;
    }

    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_strong) {
      Out << (first ? "" : ", ") << "strong";
      first = false;
    }
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_copy) {
      Out << (first ? "" : ", ") << "copy";
      first = false;
    }
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_weak) {
      Out << (first ? "" : ", ") << "weak";
      first = false;
    }
    if (PDecl->getPropertyAttributes() &
        ObjCPropertyDecl::OBJC_PR_unsafe_unretained) {
      Out << (first ? "" : ", ") << "unsafe_unretained";
      first = false;
    }

    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_readwrite) {
      Out << (first ? "" : ", ") << "readwrite";
      first = false;
    }
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_readonly) {
      Out << (first ? "" : ", ") << "readonly";
      first = false;
    }

    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_getter) {
      Out << (first ? "" : ", ") << "getter = ";
      PDecl->getGetterName().print(Out);
      first = false;
    }
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_setter) {
      Out << (first ? "" : ", ") << "setter = ";
      PDecl->getSetterName().print(Out);
      first = false;
    }

    if (PDecl->getPropertyAttributes() &
        ObjCPropertyDecl::OBJC_PR_nullability) {
      if (auto nullability = AttributedType::stripOuterNullability(T)) {
        if (*nullability == NullabilityKind::Unspecified &&
            (PDecl->getPropertyAttributes() &
             ObjCPropertyDecl::OBJC_PR_null_resettable)) {
          Out << (first ? "" : ", ") << "null_resettable";
        } else {
          Out << (first ? "" : ", ")
              << getNullabilitySpelling(*nullability, true);
        }
        first = false;
      }
    }

    (void)first; // Silence dead store warning due to idiomatic code.
    Out << ")";
  }
  std::string TypeStr =
      PDecl->getASTContext().getUnqualifiedObjCPointerType(T).getAsString(
          Policy);
  Out << ' ' << TypeStr;
  if (!StringRef(TypeStr).endswith("*"))
    Out << ' ';
  Out << *PDecl;
  if (Policy.PolishForDeclaration)
    Out << ';';
}

void DeclPrinterFiltering::VisitObjCPropertyImplDecl(
    ObjCPropertyImplDecl *PID) {
  if (PID->getPropertyImplementation() == ObjCPropertyImplDecl::Synthesize)
    Out << "@synthesize ";
  else
    Out << "@dynamic ";
  Out << *PID->getPropertyDecl();
  if (PID->getPropertyIvarDecl())
    Out << '=' << *PID->getPropertyIvarDecl();
}

void DeclPrinterFiltering::VisitUsingDecl(UsingDecl *D) {
  if (!D->isAccessDeclaration())
    Out << "using ";
  if (D->hasTypename())
    Out << "typename ";
  // TODO jr
  D->getQualifier()->print(Out, Policy);

  // Use the correct record name when the using declaration is used for
  // inheriting constructors.
  for (const auto *Shadow : D->shadows()) {
    if (const auto *ConstructorShadow =
            dyn_cast<ConstructorUsingShadowDecl>(Shadow)) {
      assert(Shadow->getDeclContext() == ConstructorShadow->getDeclContext());
      Out << *ConstructorShadow->getNominatedBaseClass();
      return;
    }
  }
  Out << *D;
}

void DeclPrinterFiltering::VisitUnresolvedUsingTypenameDecl(
    UnresolvedUsingTypenameDecl *D) {
  Out << "using typename ";
  // TODO jr
  D->getQualifier()->print(Out, Policy);
  Out << D->getDeclName();
}

void DeclPrinterFiltering::VisitUnresolvedUsingValueDecl(
    UnresolvedUsingValueDecl *D) {
  if (!D->isAccessDeclaration())
    Out << "using ";
  // TODO jr
  D->getQualifier()->print(Out, Policy);
  Out << D->getDeclName();
}

void DeclPrinterFiltering::VisitUsingShadowDecl(UsingShadowDecl *D) {
  // ignore
}

void DeclPrinterFiltering::VisitOMPThreadPrivateDecl(OMPThreadPrivateDecl *D) {
  Out << "#pragma omp threadprivate";
  if (!D->varlist_empty()) {
    for (OMPThreadPrivateDecl::varlist_iterator I = D->varlist_begin(),
                                                E = D->varlist_end();
         I != E; ++I) {
      Out << (I == D->varlist_begin() ? '(' : ',');
      NamedDecl *ND = cast<DeclRefExpr>(*I)->getDecl();
      ND->printQualifiedName(Out);
    }
    Out << ")";
  }
}

void DeclPrinterFiltering::VisitOMPAllocateDecl(OMPAllocateDecl *D) {
  Out << "#pragma omp allocate";
  if (!D->varlist_empty()) {
    for (OMPAllocateDecl::varlist_iterator I = D->varlist_begin(),
                                           E = D->varlist_end();
         I != E; ++I) {
      Out << (I == D->varlist_begin() ? '(' : ',');
      NamedDecl *ND = cast<DeclRefExpr>(*I)->getDecl();
      ND->printQualifiedName(Out);
    }
    Out << ")";
  }
  if (!D->clauselist_empty()) {
    Out << " ";
    OMPClausePrinter Printer(Out, Policy);
    for (OMPClause *C : D->clauselists())
      Printer.Visit(C);
  }
}

void DeclPrinterFiltering::VisitOMPRequiresDecl(OMPRequiresDecl *D) {
  Out << "#pragma omp requires ";
  if (!D->clauselist_empty()) {
    OMPClausePrinter Printer(Out, Policy);
    for (auto I = D->clauselist_begin(), E = D->clauselist_end(); I != E; ++I)
      Printer.Visit(*I);
  }
}

void DeclPrinterFiltering::VisitOMPDeclareReductionDecl(
    OMPDeclareReductionDecl *D) {
  if (!D->isInvalidDecl()) {
    Out << "#pragma omp declare reduction (";
    if (D->getDeclName().getNameKind() == DeclarationName::CXXOperatorName) {
      const char *OpName =
          getOperatorSpelling(D->getDeclName().getCXXOverloadedOperator());
      assert(OpName && "not an overloaded operator");
      Out << OpName;
    } else {
      assert(D->getDeclName().isIdentifier());
      D->printName(Out);
    }
    Out << " : ";
    D->getType().print(Out, Policy);
    Out << " : ";
    selective_printer::printPretty(D->getCombiner(), lines, Out, nullptr,
                                   Policy, 0);
    Out << ")";
    if (auto *Init = D->getInitializer()) {
      Out << " initializer(";
      switch (D->getInitializerKind()) {
      case OMPDeclareReductionDecl::DirectInit:
        Out << "omp_priv(";
        break;
      case OMPDeclareReductionDecl::CopyInit:
        Out << "omp_priv = ";
        break;
      case OMPDeclareReductionDecl::CallInit:
        break;
      }
      selective_printer::printPretty(Init, lines, Out, nullptr, Policy, 0);
      if (D->getInitializerKind() == OMPDeclareReductionDecl::DirectInit)
        Out << ")";
      Out << ")";
    }
  }
}

void DeclPrinterFiltering::VisitOMPDeclareMapperDecl(OMPDeclareMapperDecl *D) {
  if (!D->isInvalidDecl()) {
    Out << "#pragma omp declare mapper (";
    D->printName(Out);
    Out << " : ";
    D->getType().print(Out, Policy);
    Out << " ";
    Out << D->getVarName();
    Out << ")";
    if (!D->clauselist_empty()) {
      OMPClausePrinter Printer(Out, Policy);
      for (auto *C : D->clauselists()) {
        Out << " ";
        Printer.Visit(C);
      }
    }
  }
}

void DeclPrinterFiltering::VisitOMPCapturedExprDecl(OMPCapturedExprDecl *D) {
  selective_printer::printPretty(D->getInit(), lines, Out, nullptr, Policy,
                                 &Context, Indentation);
}
