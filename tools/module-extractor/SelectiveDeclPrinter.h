#ifndef PHASAR_SELECTIVEDECLPRINTER_H
#define PHASAR_SELECTIVEDECLPRINTER_H

#include <clang/AST/DeclBase.h>
#include <clang/AST/PrettyPrinter.h>
#include <llvm/Support/raw_ostream.h>

#include <set>

namespace selective_printer {
bool print(const clang::Decl *decl, const std::set<unsigned int> &lines,
           llvm::raw_ostream &Out, const clang::PrintingPolicy &Policy,
           unsigned Indentation = 0, bool PrintInstantiation = false);

bool print(const clang::Decl *decl, const std::set<unsigned int> &lines,
           llvm::raw_ostream &Out, unsigned Indentation = 0,
           bool PrintInstantiation = false);

bool printGroup(clang::Decl **Begin, unsigned NumDecls,
                const std::set<unsigned int> &lines, llvm::raw_ostream &Out,
                const clang::PrintingPolicy &Policy, unsigned Indentation = 0);
} // namespace selective_printer

#endif // PHASAR_SELECTIVEDECLPRINTER_H
