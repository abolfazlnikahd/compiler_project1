// lexer.hpp - Hand written DFA-style scanner for B-Minor.
#pragma once
#include "token.hpp"

namespace bminor {

class Lexer {
public:
    Lexer(std::string src, std::string file);
    // Never throws and never stops: invalid input yields INVALID tokens / error flags.
    std::vector<Token> tokenize();
    const std::vector<Diagnostic>& diagnostics() const { return diags_; }

private:
    char peek(size_t n = 0) const;
    void advance();
    void emit(TokType type, size_t start, const Loc& loc, const std::string& err = "");
    bool scanEscape(std::string& err);
    void scanString(size_t start, const Loc& loc);
    void scanChar(size_t start, const Loc& loc);
    void scanBlockComment(size_t start, const Loc& loc);
    void scanOperator(size_t start, const Loc& loc);

    std::string src_, file_;
    size_t pos_ = 0;
    int line_ = 1, col_ = 1;
    std::vector<Token> out_;
    std::vector<Diagnostic> diags_;
};

}  // namespace bminor
