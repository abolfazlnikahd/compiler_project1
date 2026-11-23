#include "lexer.hpp"
#include <cctype>
#include <set>

namespace bminor {

static const std::set<std::string> kKeywords = {
    "array", "boolean", "char", "else", "for", "function", "if",
    "integer", "print", "return", "string", "void", "while"};

bool isKeyword(const std::string& s) { return kKeywords.count(s) > 0; }
bool isTypeKeyword(const std::string& s) {
    return s == "integer" || s == "boolean" || s == "char" || s == "string" || s == "void";
}

const char* tokTypeName(TokType t) {
    switch (t) {
        case TokType::KEYWORD: return "KW";
        case TokType::IDENT: return "IDENT";
        case TokType::INT_LIT: return "INT";
        case TokType::BOOL_LIT: return "BOOL";
        case TokType::STRING_LIT: return "STRING";
        case TokType::CHAR_LIT: return "CHAR";
        case TokType::OP: return "OP";
        case TokType::ASSIGN: return "ASSIGN";
        case TokType::LPAREN: return "LPAREN";
        case TokType::RPAREN: return "RPAREN";
        case TokType::LBRACE: return "LBRACE";
        case TokType::RBRACE: return "RBRACE";
        case TokType::LBRACKET: return "LBRACKET";
        case TokType::RBRACKET: return "RBRACKET";
        case TokType::COMMA: return "COMMA";
        case TokType::SEMI: return "SEMI";
        case TokType::COLON: return "COLON";
        case TokType::LINE_COMMENT: return "LINE_COMMENT";
        case TokType::BLOCK_COMMENT: return "BLOCK_COMMENT";
        case TokType::INVALID: return "INVALID";
        case TokType::END_OF_FILE: return "EOF";
    }
    return "?";
}

Lexer::Lexer(std::string src, std::string file) : src_(std::move(src)), file_(std::move(file)) {}

char Lexer::peek(size_t n) const { return pos_ + n < src_.size() ? src_[pos_ + n] : '\0'; }

void Lexer::advance() {
    if (pos_ >= src_.size()) return;
    unsigned char c = (unsigned char)src_[pos_];
    if (c == '\n') { line_++; col_ = 1; }
    else if ((c & 0xC0) != 0x80) col_++;   // UTF-8 continuation bytes do not move the column
    pos_++;
}

void Lexer::emit(TokType type, size_t start, const Loc& loc, const std::string& err) {
    Token t;
    t.type = type;
    t.offset = start;
    t.length = pos_ - start;
    t.lexeme = src_.substr(start, t.length);
    t.loc = loc;
    if (!err.empty()) {
        t.error = true;
        t.msg = err;
        diags_.push_back({loc, err, (int)out_.size(), true});
    }
    out_.push_back(std::move(t));
}

// Consumes '\' and the escaped character. Valid: \n \t \r \0 \\ \" \'
bool Lexer::scanEscape(std::string& err) {
    advance();  // backslash
    char c = peek();
    if (pos_ >= src_.size() || c == '\n') return false;
    if (std::string("ntr0\\\"'").find(c) == std::string::npos && err.empty())
        err = std::string("invalid escape sequence '\\") + c + "'";
    advance();
    return true;
}

void Lexer::scanString(size_t start, const Loc& loc) {
    advance();  // opening quote
    std::string err;
    bool closed = false;
    while (pos_ < src_.size()) {
        char c = peek();
        if (c == '"') { advance(); closed = true; break; }
        if (c == '\n') break;
        if (c == '\\') {
            if (pos_ + 1 >= src_.size() || peek(1) == '\n') { advance(); break; }
            scanEscape(err);
            continue;
        }
        advance();
    }
    if (!closed) emit(TokType::INVALID, start, loc, "unterminated string literal");
    else emit(TokType::STRING_LIT, start, loc, err);
}

void Lexer::scanChar(size_t start, const Loc& loc) {
    advance();  // opening quote
    if (pos_ >= src_.size() || peek() == '\n') {
        emit(TokType::INVALID, start, loc, "unterminated character literal");
        return;
    }
    if (peek() == '\'') {
        advance();
        emit(TokType::CHAR_LIT, start, loc, "empty character literal");
        return;
    }
    std::string err;
    if (peek() == '\\') scanEscape(err);
    else {
        advance();
        while ((peek() & 0xC0) == 0x80 && pos_ < src_.size()) advance();  // whole UTF-8 char
    }
    if (peek() == '\'') { advance(); emit(TokType::CHAR_LIT, start, loc, err); return; }
    while (pos_ < src_.size() && peek() != '\'' && peek() != '\n') advance();
    if (peek() == '\'') {
        advance();
        emit(TokType::CHAR_LIT, start, loc, "character literal must contain exactly one character");
    } else {
        emit(TokType::INVALID, start, loc, "unterminated character literal");
    }
}

void Lexer::scanBlockComment(size_t start, const Loc& loc) {
    advance(); advance();  // /*
    bool closed = false;
    while (pos_ < src_.size()) {
        if (peek() == '*' && peek(1) == '/') { advance(); advance(); closed = true; break; }
        advance();
    }
    emit(TokType::BLOCK_COMMENT, start, loc, closed ? "" : "unterminated block comment");
}

void Lexer::scanOperator(size_t start, const Loc& loc) {
    char c = peek(), d = peek(1);
    // Maximal munch: try the two-character operators first.
    static const char* two[] = {"<=", ">=", "==", "!=", "&&", "||", "++", "--"};
    for (const char* op : two) {
        if (c == op[0] && d == op[1]) { advance(); advance(); emit(TokType::OP, start, loc); return; }
    }
    switch (c) {
        case '+': case '-': case '*': case '/': case '%': case '^':
        case '<': case '>': case '!':
            advance(); emit(TokType::OP, start, loc); return;
        case '=': advance(); emit(TokType::ASSIGN, start, loc); return;
        case '(': advance(); emit(TokType::LPAREN, start, loc); return;
        case ')': advance(); emit(TokType::RPAREN, start, loc); return;
        case '{': advance(); emit(TokType::LBRACE, start, loc); return;
        case '}': advance(); emit(TokType::RBRACE, start, loc); return;
        case '[': advance(); emit(TokType::LBRACKET, start, loc); return;
        case ']': advance(); emit(TokType::RBRACKET, start, loc); return;
        case ',': advance(); emit(TokType::COMMA, start, loc); return;
        case ';': advance(); emit(TokType::SEMI, start, loc); return;
        case ':': advance(); emit(TokType::COLON, start, loc); return;
        default: break;
    }
    // Unknown character: report, skip exactly this (UTF-8) character and continue.
    advance();
    while (pos_ < src_.size() && ((unsigned char)peek() & 0xC0) == 0x80) advance();
    std::string shown = src_.substr(start, pos_ - start);
    std::string msg = "unexpected character '" + shown + "'";
    if (c == '&' || c == '|') msg += std::string(" (did you mean '") + c + c + "'?)";
    emit(TokType::INVALID, start, loc, msg);
}

std::vector<Token> Lexer::tokenize() {
    out_.clear();
    diags_.clear();
    while (true) {
        while (pos_ < src_.size() && std::isspace((unsigned char)peek())) advance();  // whitespace: only tracked for locations
        Loc loc{file_, line_, col_};
        size_t start = pos_;
        if (pos_ >= src_.size()) { emit(TokType::END_OF_FILE, start, loc); break; }
        char c = peek();
        if (std::isalpha((unsigned char)c) || c == '_') {
            while (std::isalnum((unsigned char)peek()) || peek() == '_') advance();
            std::string w = src_.substr(start, pos_ - start);
            // Priority rule: keywords / boolean constants win over IDENT.
            if (w == "true" || w == "false") emit(TokType::BOOL_LIT, start, loc);
            else if (isKeyword(w)) emit(TokType::KEYWORD, start, loc);
            else emit(TokType::IDENT, start, loc);
        } else if (std::isdigit((unsigned char)c)) {
            while (std::isdigit((unsigned char)peek())) advance();
            emit(TokType::INT_LIT, start, loc);
        } else if (c == '"') {
            scanString(start, loc);
        } else if (c == '\'') {
            scanChar(start, loc);
        } else if (c == '/' && peek(1) == '/') {
            while (pos_ < src_.size() && peek() != '\n') advance();
            emit(TokType::LINE_COMMENT, start, loc);
        } else if (c == '/' && peek(1) == '*') {
            scanBlockComment(start, loc);
        } else {
            scanOperator(start, loc);
        }
    }
    return out_;
}

}  // namespace bminor
