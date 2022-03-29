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
 * Return the location direct AFTER the end of the token
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

/**
 * Returns the Location of Loc as it is written in the Source code. For example
 * this returns the expansion location of a macro and not the location where it
 * is defined
 * @param Loc
 * @param SM
 * @return
 */
inline clang::PresumedLoc getLocationAsWritten(clang::SourceLocation Loc,
                                               const clang::SourceManager &SM) {
  return SM.getPresumedLoc(Loc);
}

/**
 * Returns the first semicolon after the given end location
 * @param EndLoc The location after which to begin searching
 * @param SM
 * @param LangOpts
 * @param Kind The token to search. Defaults to a semicolon
 * @return The location of the found semicolon or an invalid location if none
 * could be found
 */
clang::SourceLocation getSemicolonAfterStmtEndLoc(
    const clang::SourceLocation &EndLoc, const clang::SourceManager &SM,
    const clang::LangOptions &LangOpts,
    clang::tok::TokenKind Kind = clang::tok::TokenKind::semi);

/**
 * Returns the location of the beginning of the previous token
 * @param Start Location of the current token
 * @param SM
 * @param LangOpts
 * @return
 */
clang::SourceLocation
findPreviousTokenStart(clang::SourceLocation Start,
                       const clang::SourceManager &SM,
                       const clang::LangOptions &LangOpts);

} // namespace utils

#endif // PHASAR_SOURCE_UTILS_H
