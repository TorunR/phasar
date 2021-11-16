#ifndef PHASAR_SOURCE_UTILS_H
#define PHASAR_SOURCE_UTILS_H

#include <clang/AST/Decl.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>

#include <cassert>
#include <clang/AST/Expr.h>
#include <set>

namespace utils {

inline clang::PresumedLoc getLocationAsWritten(clang::SourceLocation loc,
                                               const clang::SourceManager &sm) {
  return sm.getPresumedLoc(loc);
}

// https://github.com/llvm/llvm-project/blob/main/clang/lib/CodeGen/CGDebugInfo.cpp#L470
inline unsigned int getLineFromSourceLocation(const clang::PresumedLoc &loc) {
  return loc.getLine();
}

inline bool isInTargetFile(const clang::PresumedLoc &loc,
                           const clang::SourceManager &sm) {
  return sm.getMainFileID() == loc.getFileID();
}

inline bool isInWhitelist(unsigned int line,
                          const std::set<unsigned int> &lines) {
  return lines.find(line) != lines.end();
}

/**
 * inclusive
 * @param lineBegin
 * @param lineEnd
 * @param lines
 * @return
 */
inline bool isAnyInRangeInWhitelist(unsigned int lineBegin,
                                    unsigned int lineEnd,
                                    const std::set<unsigned int> &lines) {
  assert(lineBegin <= lineEnd);
  return lines.lower_bound(lineBegin) != lines.upper_bound(lineEnd);
}

inline bool shouldBeSliced(const clang::Decl *decl,
                           const clang::SourceManager &sm,
                           const std::set<unsigned int> &lines) {
  if (llvm::isa<clang::TranslationUnitDecl>(decl)) {
    return true;
  }
  if (sm.getFileID(sm.getSpellingLoc(decl->getBeginLoc())) !=
      sm.getMainFileID()) {
    return false;
  }
  // return true;
  return lines.lower_bound(sm.getPresumedLineNumber(decl->getBeginLoc())) !=
         lines.upper_bound(sm.getPresumedLineNumber(decl->getEndLoc()));
}

inline bool shouldBeSliced(const clang::Stmt *stmt,
                           const clang::SourceManager &sm,
                           const std::set<unsigned int> &lines) {
  //  if (sm.getFileID(sm.getSpellingLoc(stmt->getBeginLoc())) !=
  //      sm.getMainFileID()) {
  //    return false;
  //  }
  if (llvm::isa<clang::ConstantExpr>(stmt) ||
      llvm::isa<clang::CharacterLiteral>(stmt) ||
      llvm::isa<clang::IntegerLiteral>(stmt) ||
      llvm::isa<clang::FloatingLiteral>(stmt) ||
      llvm::isa<clang::StringLiteral>(stmt) ||
      llvm::isa<clang::DeclRefExpr>(stmt)) {
    return true;
  }

  return lines.lower_bound(sm.getPresumedLineNumber(stmt->getBeginLoc())) !=
         lines.upper_bound(sm.getPresumedLineNumber(stmt->getEndLoc()));
}

} // namespace utils

#endif // PHASAR_SOURCE_UTILS_H
