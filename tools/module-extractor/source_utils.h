#ifndef PHASAR_SOURCE_UTILS_H
#define PHASAR_SOURCE_UTILS_H

#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>

#include <cassert>

#include <set>

namespace utils {

/**
 * Return the location directy AFTER the end of the token
 * @param Loc
 * @param SM
 * @param LangOpts
 * @return
 */
inline clang::SourceLocation getEndOfToken(clang::SourceLocation Loc,
                                           const clang::SourceManager &SM,
                                           const clang::LangOptions &LangOpts) {
  return clang::Lexer::getLocForEndOfToken(Loc, 0, SM, LangOpts);
}

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

// Take from clang tidy
template <typename TokenKind>
clang::SourceLocation
findNextToken(clang::SourceLocation Start, const clang::SourceManager &SM,
              const clang::LangOptions &LangOpts, TokenKind TK) {
  while (true) {
    llvm::Optional<clang::Token> CurrentToken =
        clang::Lexer::findNextToken(Start, SM, LangOpts);

    if (!CurrentToken)
      return clang::SourceLocation();

    clang::Token PotentialMatch = *CurrentToken;
    if (PotentialMatch.getKind() == TK)
      return PotentialMatch.getLocation();

    // If we reach the end of the file, and eof is not the target token, we stop
    // the loop, otherwise we will get infinite loop (findNextToken will return
    // eof on eof).
    if (PotentialMatch.is(clang::tok::eof))
      return clang::SourceLocation();
    Start = PotentialMatch.getLastLoc();
  }
}

clang::SourceLocation getSemicolonAfterStmtEndLoc(
    const clang::SourceLocation &EndLoc, const clang::SourceManager &SM,
    const clang::LangOptions &LangOpts,
    clang::tok::TokenKind Kind = clang::tok::TokenKind::semi);

clang::SourceLocation
findPreviousTokenStart(clang::SourceLocation Start,
                       const clang::SourceManager &SM,
                       const clang::LangOptions &LangOpts);

} // namespace utils

#endif // PHASAR_SOURCE_UTILS_H
