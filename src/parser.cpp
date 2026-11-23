#include "parser.hpp"
#include <algorithm>
#include <cstring>

namespace bminor {

namespace { struct SyntaxError {}; }  // thrown to unwind to the nearest recovery point

Parser::Parser(const std::vector<Token>& toks) : all_(toks) {
    for (size_t i = 0; i < all_.size(); ++i) {
        TokType t = all_[i].type;
        // Comments are trivia; INVALID tokens were already reported by the lexer and are skipped.
        if (t == TokType::LINE_COMMENT || t == TokType::BLOCK_COMMENT || t == TokType::INVALID) continue;
        sig_.push_back((int)i);
    }
}

const Token& Parser::peek(size_t n) const {
    return all_[sig_[std::min(p_ + n, sig_.size() - 1)]];
}

bool Parser::isOp(std::initializer_list<const char*> ops) const {
    if (cur().type != TokType::OP) return false;
    for (const char* o : ops) if (cur().lexeme == o) return true;
    return false;
}

std::string Parser::describe(const Token& t) {
    return t.type == TokType::END_OF_FILE ? "end of file" : "'" + t.lexeme + "'";
}

NodePtr Parser::mk(NodeKind k, const Loc& loc, std::string text, int tok) {
    auto n = std::make_unique<Node>();
    n->kind = k;
    n->loc = loc;
    n->text = std::move(text);
    n->tokIdx = tok;
    return n;
}

// ---------------------------------------------------------------- errors / recovery
void Parser::error(const std::string& msg, int ti) {
    if (ti == lastErrTok_) return;           // one diagnostic per token: avoids cascades
    lastErrTok_ = ti;
    if (all_[ti].error) return;              // the lexer already reported this token
    diags_.push_back({all_[ti].loc, msg, ti, false});
}

bool Parser::expectSoft(TokType t, const char* what) {
    if (cur().type == t) { advance(); return true; }
    error(std::string("expected ") + what + ", got " + describe(cur()), curIdx());
    return false;
}

int Parser::expectHard(TokType t, const char* what) {
    if (cur().type == t) { int i = curIdx(); advance(); return i; }
    error(std::string("expected ") + what + ", got " + describe(cur()), curIdx());
    throw SyntaxError{};
}

// Panic mode: skip tokens until a synchronization point.
//   ';'  -> consumed (statement finished)
//   '}'  -> kept (closes the enclosing block)
//   statement keywords (if/for/while/return/print) or `IDENT :` -> kept (a new statement starts)
void Parser::synchronize(bool topLevel) {
    while (!atEnd()) {
        TokType t = cur().type;
        if (t == TokType::SEMI) { advance(); return; }
        if (t == TokType::RBRACE) { if (topLevel) advance(); return; }
        if (t == TokType::IDENT && peek().type == TokType::COLON) return;
        if (!topLevel && t == TokType::KEYWORD &&
            (isKw("if") || isKw("for") || isKw("while") || isKw("return") || isKw("print")))
            return;
        advance();
    }
}

void Parser::recover(size_t start, bool topLevel) {
    synchronize(topLevel);
    // guarantee progress so that the enclosing loop can never spin forever
    if (p_ == start && !atEnd() && (topLevel || cur().type != TokType::RBRACE)) advance();
}

// ---------------------------------------------------------------- program / declarations
NodePtr Parser::parseProgram() {
    auto prog = mk(NodeKind::Program, cur().loc);
    while (!atEnd()) {
        size_t start = p_;
        try {
            prog->kids.push_back(parseTopDecl());
        } catch (SyntaxError&) {
            prog->kids.push_back(mk(NodeKind::Error, all_[sig_[start]].loc, "", sig_[start]));
            recover(start, true);
        }
    }
    return prog;
}

NodePtr Parser::parseTopDecl() {
    if (!(cur().type == TokType::IDENT && peek().type == TokType::COLON)) {
        error("expected a declaration ('name : type ...'), got " + describe(cur()), curIdx());
        throw SyntaxError{};
    }
    return parseDecl();
}

NodePtr Parser::parseDecl() {
    Loc loc = cur().loc;
    int nameTok = expectHard(TokType::IDENT, "identifier");
    std::string name = all_[nameTok].lexeme;
    expectHard(TokType::COLON, "':'");

    if (isKw("function")) {
        auto fn = mk(NodeKind::FuncDecl, loc, name, nameTok);
        Loc tl = cur().loc;
        advance();  // 'function'
        fn->kids.push_back(parseFuncTypeTail(tl));
        if (cur().type == TokType::ASSIGN) {
            advance();
            fn->kids.push_back(parseBlock());      // function body is mandatory after '='
        } else {
            expectSoft(TokType::SEMI, "';' or '= {' after function header");
        }
        return fn;
    }

    auto var = mk(NodeKind::VarDecl, loc, name, nameTok);
    var->kids.push_back(parseType());
    if (cur().type == TokType::ASSIGN) {
        advance();
        var->kids.push_back(cur().type == TokType::LBRACE ? parseInitList() : parseExpr());
    }
    expectSoft(TokType::SEMI, "';'");
    return var;
}

// after the 'function' keyword:  type '(' params ')'
NodePtr Parser::parseFuncTypeTail(const Loc& loc) {
    auto ft = mk(NodeKind::FuncType, loc);
    ft->kids.push_back(parseType());
    expectSoft(TokType::LPAREN, "'('");
    while (cur().type != TokType::RPAREN && !atEnd()) {
        Loc pl = cur().loc;
        int nt = expectHard(TokType::IDENT, "parameter name");
        expectHard(TokType::COLON, "':'");
        auto prm = mk(NodeKind::Param, pl, all_[nt].lexeme, nt);
        prm->kids.push_back(parseType());
        ft->kids.push_back(std::move(prm));
        if (cur().type == TokType::COMMA) advance(); else break;
    }
    expectSoft(TokType::RPAREN, "')'");
    return ft;
}

NodePtr Parser::parseType() {
    Loc loc = cur().loc;
    if (cur().type == TokType::KEYWORD && isTypeKeyword(cur().lexeme)) {
        auto t = mk(NodeKind::PrimType, loc, cur().lexeme, curIdx());
        advance();
        return t;
    }
    if (isKw("array")) {
        auto t = mk(NodeKind::ArrayType, loc, "", curIdx());
        advance();
        expectSoft(TokType::LBRACKET, "'['");
        NodePtr size;
        if (cur().type != TokType::RBRACKET) size = parseExpr();
        expectSoft(TokType::RBRACKET, "']'");
        t->kids.push_back(parseType());
        if (size) t->kids.push_back(std::move(size));
        return t;
    }
    error("expected a type (integer, boolean, char, string, void, array), got " + describe(cur()), curIdx());
    throw SyntaxError{};
}

NodePtr Parser::parseInitList() {
    auto il = mk(NodeKind::InitList, cur().loc, "", curIdx());
    expectHard(TokType::LBRACE, "'{'");
    if (cur().type != TokType::RBRACE) {
        while (true) {
            il->kids.push_back(cur().type == TokType::LBRACE ? parseInitList() : parseExpr());
            if (cur().type == TokType::COMMA) advance(); else break;
        }
    }
    expectSoft(TokType::RBRACE, "'}'");
    return il;
}

// ---------------------------------------------------------------- statements
NodePtr Parser::parseStmtSafe() {
    size_t start = p_;
    try {
        return parseStmt();
    } catch (SyntaxError&) {
        auto e = mk(NodeKind::Error, all_[sig_[start]].loc, "", sig_[start]);
        recover(start, false);
        return e;
    }
}

NodePtr Parser::parseStmt() {
    switch (cur().type) {
        case TokType::LBRACE: return parseBlock();
        case TokType::KEYWORD:
            if (isKw("if")) return parseIf();
            if (isKw("for")) return parseFor();
            if (isKw("while")) return parseWhile();
            if (isKw("return")) return parseReturn();
            if (isKw("print")) return parsePrint();
            break;
        case TokType::IDENT:
            if (peek().type == TokType::COLON) return parseDecl();  // the LL(2) decision point
            break;
        default: break;
    }
    auto st = mk(NodeKind::ExprStmt, cur().loc);
    st->kids.push_back(parseExpr());
    expectSoft(TokType::SEMI, "';'");
    return st;
}

NodePtr Parser::parseBlock() {
    auto b = mk(NodeKind::Block, cur().loc, "", curIdx());
    expectHard(TokType::LBRACE, "'{'");
    while (cur().type != TokType::RBRACE && !atEnd()) b->kids.push_back(parseStmtSafe());
    expectSoft(TokType::RBRACE, "'}'");
    return b;
}

NodePtr Parser::parseIf() {
    auto n = mk(NodeKind::IfStmt, cur().loc, "", curIdx());
    advance();
    expectSoft(TokType::LPAREN, "'('");
    n->kids.push_back(parseExpr());
    expectSoft(TokType::RPAREN, "')'");
    n->kids.push_back(parseStmtSafe());
    if (isKw("else")) {            // dangling else binds to the nearest 'if'
        advance();
        n->kids.push_back(parseStmtSafe());
    }
    return n;
}

NodePtr Parser::parseFor() {
    auto n = mk(NodeKind::ForStmt, cur().loc, "", curIdx());
    advance();
    expectSoft(TokType::LPAREN, "'('");
    auto clause = [&](TokType end, const char* what) {
        NodePtr c = cur().type == end ? mk(NodeKind::Empty, cur().loc) : parseExpr();
        expectSoft(end, what);
        n->kids.push_back(std::move(c));
    };
    clause(TokType::SEMI, "';'");
    clause(TokType::SEMI, "';'");
    clause(TokType::RPAREN, "')'");
    n->kids.push_back(parseStmtSafe());
    return n;
}

NodePtr Parser::parseWhile() {
    auto n = mk(NodeKind::WhileStmt, cur().loc, "", curIdx());
    advance();
    expectSoft(TokType::LPAREN, "'('");
    n->kids.push_back(parseExpr());
    expectSoft(TokType::RPAREN, "')'");
    n->kids.push_back(parseStmtSafe());
    return n;
}

NodePtr Parser::parseReturn() {
    auto n = mk(NodeKind::ReturnStmt, cur().loc, "", curIdx());
    advance();
    if (cur().type != TokType::SEMI) n->kids.push_back(parseExpr());
    expectSoft(TokType::SEMI, "';'");
    return n;
}

NodePtr Parser::parsePrint() {
    auto n = mk(NodeKind::PrintStmt, cur().loc, "", curIdx());
    advance();
    if (cur().type != TokType::SEMI) {
        while (true) {
            n->kids.push_back(parseExpr());
            if (cur().type == TokType::COMMA) advance(); else break;
        }
    }
    expectSoft(TokType::SEMI, "';'");
    return n;
}

// ---------------------------------------------------------------- expressions
NodePtr Parser::parseExpr() {
    auto lhs = parseOr();
    if (cur().type == TokType::ASSIGN) {          // right associative
        int opTok = curIdx();
        Loc loc = lhs->loc;
        if (lhs->kind != NodeKind::Identifier && lhs->kind != NodeKind::IndexExpr)
            error("invalid assignment target", opTok);
        advance();
        auto rhs = parseExpr();
        auto n = mk(NodeKind::AssignExpr, loc, "=", opTok);
        n->kids.push_back(std::move(lhs));
        n->kids.push_back(std::move(rhs));
        return n;
    }
    return lhs;
}

NodePtr Parser::leftAssoc(Fn next, std::initializer_list<const char*> ops) {
    NodePtr l = (this->*next)();
    while (isOp(ops)) {
        int opTok = curIdx();
        std::string op = cur().lexeme;
        Loc loc = l->loc;
        advance();
        NodePtr r = (this->*next)();
        auto n = mk(NodeKind::BinaryExpr, loc, op, opTok);
        n->kids.push_back(std::move(l));
        n->kids.push_back(std::move(r));
        l = std::move(n);
    }
    return l;
}

NodePtr Parser::parseOr()  { return leftAssoc(&Parser::parseAnd, {"||"}); }
NodePtr Parser::parseAnd() { return leftAssoc(&Parser::parseCmp, {"&&"}); }
NodePtr Parser::parseCmp() { return leftAssoc(&Parser::parseAdd, {"<", "<=", ">", ">=", "==", "!="}); }
NodePtr Parser::parseAdd() { return leftAssoc(&Parser::parseMul, {"+", "-"}); }
NodePtr Parser::parseMul() { return leftAssoc(&Parser::parsePow, {"*", "/", "%"}); }

NodePtr Parser::parsePow() {                       // right associative: a ^ b ^ c = a ^ (b ^ c)
    NodePtr l = parseUnary();
    if (isOp({"^"})) {
        int opTok = curIdx();
        Loc loc = l->loc;
        advance();
        NodePtr r = parsePow();
        auto n = mk(NodeKind::BinaryExpr, loc, "^", opTok);
        n->kids.push_back(std::move(l));
        n->kids.push_back(std::move(r));
        return n;
    }
    return l;
}

NodePtr Parser::parseUnary() {
    if (isOp({"-", "!"})) {
        int opTok = curIdx();
        std::string op = cur().lexeme;
        Loc loc = cur().loc;
        advance();
        auto n = mk(NodeKind::UnaryExpr, loc, op, opTok);
        n->kids.push_back(parseUnary());
        return n;
    }
    return parsePostfix();
}

NodePtr Parser::parsePostfix() {
    NodePtr e = parsePrimary();
    while (true) {
        Loc loc = e->loc;
        if (isOp({"++", "--"})) {
            auto n = mk(NodeKind::PostfixExpr, loc, cur().lexeme, curIdx());
            advance();
            n->kids.push_back(std::move(e));
            e = std::move(n);
        } else if (cur().type == TokType::LBRACKET) {
            auto n = mk(NodeKind::IndexExpr, loc, "", curIdx());
            advance();
            NodePtr idx = parseExpr();
            expectSoft(TokType::RBRACKET, "']'");
            n->kids.push_back(std::move(e));
            n->kids.push_back(std::move(idx));
            e = std::move(n);
        } else if (cur().type == TokType::LPAREN) {
            auto n = mk(NodeKind::CallExpr, loc, "", curIdx());
            advance();
            n->kids.push_back(std::move(e));
            if (cur().type != TokType::RPAREN) {
                while (true) {
                    n->kids.push_back(parseExpr());
                    if (cur().type == TokType::COMMA) advance(); else break;
                }
            }
            expectSoft(TokType::RPAREN, "')'");
            e = std::move(n);
        } else {
            break;
        }
    }
    return e;
}

NodePtr Parser::parsePrimary() {
    const Token& t = cur();
    NodeKind k;
    switch (t.type) {
        case TokType::INT_LIT: k = NodeKind::IntLiteral; break;
        case TokType::STRING_LIT: k = NodeKind::StringLiteral; break;
        case TokType::CHAR_LIT: k = NodeKind::CharLiteral; break;
        case TokType::BOOL_LIT: k = NodeKind::BoolLiteral; break;
        case TokType::IDENT: k = NodeKind::Identifier; break;
        case TokType::LPAREN: {
            advance();
            NodePtr e = parseExpr();
            expectSoft(TokType::RPAREN, "')'");
            return e;
        }
        default:
            error("expected expression, got " + describe(t), curIdx());
            throw SyntaxError{};
    }
    auto n = mk(k, t.loc, t.lexeme, curIdx());
    advance();
    return n;
}

}  // namespace bminor
