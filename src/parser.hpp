// parser.hpp - Recursive-descent (LL(1), one LL(2) spot) parser with panic-mode recovery.
#pragma once
#include <initializer_list>
#include "ast.hpp"
#include "token.hpp"

namespace bminor {

class Parser {
public:
    explicit Parser(const std::vector<Token>& toks);
    NodePtr parseProgram();
    const std::vector<Diagnostic>& diagnostics() const { return diags_; }

private:
    using Fn = NodePtr (Parser::*)();

    // token helpers
    const Token& cur() const { return all_[sig_[p_]]; }
    const Token& peek(size_t n = 1) const;
    int curIdx() const { return sig_[p_]; }
    bool atEnd() const { return cur().type == TokType::END_OF_FILE; }
    void advance() { if (!atEnd()) p_++; }
    bool isKw(const char* s) const { return cur().type == TokType::KEYWORD && cur().lexeme == s; }
    bool isOp(std::initializer_list<const char*> ops) const;
    static std::string describe(const Token& t);

    // errors
    void error(const std::string& msg, int tokIdx);
    bool expectSoft(TokType t, const char* what);   // report + continue (token insertion)
    int expectHard(TokType t, const char* what);    // report + throw (panic mode)
    void synchronize(bool topLevel);
    void recover(size_t start, bool topLevel);

    NodePtr mk(NodeKind k, const Loc& loc, std::string text = "", int tok = -1);

    // declarations & types
    NodePtr parseTopDecl();
    NodePtr parseDecl();
    NodePtr parseType();
    NodePtr parseFuncTypeTail(const Loc& loc);
    NodePtr parseInitList();

    // statements
    NodePtr parseStmtSafe();
    NodePtr parseStmt();
    NodePtr parseBlock();
    NodePtr parseIf();
    NodePtr parseFor();
    NodePtr parseWhile();
    NodePtr parseReturn();
    NodePtr parsePrint();

    // expressions (one function per precedence level, loops instead of left recursion)
    NodePtr parseExpr();
    NodePtr leftAssoc(Fn next, std::initializer_list<const char*> ops);
    NodePtr parseOr();
    NodePtr parseAnd();
    NodePtr parseCmp();
    NodePtr parseAdd();
    NodePtr parseMul();
    NodePtr parsePow();
    NodePtr parseUnary();
    NodePtr parsePostfix();
    NodePtr parsePrimary();

    const std::vector<Token>& all_;
    std::vector<int> sig_;  // indices of significant tokens (no comments, no INVALID)
    size_t p_ = 0;
    std::vector<Diagnostic> diags_;
    int lastErrTok_ = -1;
};

}  // namespace bminor
