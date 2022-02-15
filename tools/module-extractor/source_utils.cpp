#include "source_utils.h"

namespace utils {
// The functions here are taken and modified from clang tidy
llvm::Optional<clang::Token>
findNextTokenSkippingComments(clang::SourceLocation Start,
                              const clang::SourceManager &SM,
                              const clang::LangOptions &LangOpts) {
  llvm::Optional<clang::Token> CurrentToken;
  do {
    CurrentToken = clang::Lexer::findNextToken(Start, SM, LangOpts);
  } while (CurrentToken && CurrentToken->is(clang::tok::comment));
  return CurrentToken;
}

llvm::Optional<clang::Token> findNextToken(clang::SourceLocation Start,
                                           clang::tok::TokenKind Tok,
                                           const clang::SourceManager &SM,
                                           const clang::LangOptions &LangOpts) {
  llvm::Optional<clang::Token> CurrentToken;
  do {
    CurrentToken = clang::Lexer::findNextToken(Start, SM, LangOpts);
    if (CurrentToken) {
      Start = CurrentToken->getLocation();
    }
  } while (CurrentToken && !CurrentToken->is(Tok));
  return CurrentToken;
}

// Given a Stmt which does not include it's semicolon this method returns the
// SourceLocation of the semicolon.
// Taken from clang tidy
// The handling of macros may still not handle all edge cases correctly
clang::SourceLocation getSemicolonAfterStmtEndLoc(
    const clang::SourceLocation &EndLoc, const clang::SourceManager &SM,
    const clang::LangOptions &LangOpts, clang::tok::TokenKind Kind) {

  if (EndLoc.isMacroID()) {
    // Assuming EndLoc points to a function call foo within macro F.
    // This method is supposed to return location of the semicolon within
    // those macro arguments:
    //  F     (      foo()               ;   )
    //  ^ EndLoc         ^ SpellingLoc   ^ next token of SpellingLoc
    const clang::SourceLocation SpellingLoc = SM.getSpellingLoc(EndLoc);
    llvm::Optional<clang::Token> NextTok =
        findNextTokenSkippingComments(SpellingLoc, SM, LangOpts);

    // Was the next token found successfully?
    // All macro issues are simply resolved by ensuring it's a semicolon.
    if (NextTok && NextTok->is(Kind)) {
      // Ideally this would return `F` with spelling location `;` (NextTok)
      // following the example above. For now simply return NextTok location.
      return NextTok->getLocation();
    }

    // Fallthrough to 'normal handling'.
    //  F     (      foo()              ) ;
    //  ^ EndLoc         ^ SpellingLoc  ) ^ next token of EndLoc
  }

  llvm::Optional<clang::Token> NextTok =
      findNextToken(EndLoc, Kind, SM, LangOpts);

  // Testing for semicolon again avoids some issues with macros.
  if (NextTok && NextTok->is(Kind)) {
    return NextTok->getLocation();
  }

  return {};
}

clang::SourceLocation
findPreviousTokenStart(clang::SourceLocation Start,
                       const clang::SourceManager &SM,
                       const clang::LangOptions &LangOpts) {
  if (Start.isInvalid() || Start.isMacroID()) {
    return {};
  }

  clang::SourceLocation BeforeStart = Start.getLocWithOffset(-1);
  if (BeforeStart.isInvalid() || BeforeStart.isMacroID()) {
    return {};
  }

  return clang::Lexer::GetBeginningOfToken(BeforeStart, SM, LangOpts);
}
} // namespace utils