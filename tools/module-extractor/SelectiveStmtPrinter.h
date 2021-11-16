#ifndef PHASAR_SELECTIVESTMTPRINTER_H
#define PHASAR_SELECTIVESTMTPRINTER_H

#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Stmt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>

#include <set>

namespace selective_printer {
void printPretty(const clang::Stmt *stmt, const std::set<unsigned int> &lines,
                 llvm::raw_ostream &Out, clang::PrinterHelper *Helper,
                 const clang::PrintingPolicy &Policy,
                 const clang::ASTContext *Context, unsigned Indentation = 0,
                 llvm::StringRef NL = "\n");

} // namespace selective_printer

#endif // PHASAR_SELECTIVESTMTPRINTER_H
