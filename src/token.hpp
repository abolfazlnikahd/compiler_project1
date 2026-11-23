// token.hpp - Token model shared by every phase of the front end.
#pragma once
#include <string>
#include <vector>

namespace bminor {

struct Loc {
    std::string file;
    int line = 1;  // 1-based
    int col = 1;   // 1-based (UTF-8 aware: counts characters, not bytes)
};

enum class TokType {
    KEYWORD, IDENT, INT_LIT, BOOL_LIT, STRING_LIT, CHAR_LIT,
    OP, ASSIGN,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, COMMA, SEMI, COLON,
    LINE_COMMENT, BLOCK_COMMENT,
    INVALID, END_OF_FILE
};

const char* tokTypeName(TokType t);
bool isKeyword(const std::string& s);
bool isTypeKeyword(const std::string& s);

struct Token {
    TokType type = TokType::INVALID;
    std::string lexeme;      // exact source substring
    Loc loc;                 // position of first character
    size_t offset = 0;       // byte offset in the source (used to re-print whitespace exactly)
    size_t length = 0;
    bool error = false;      // lexical error attached to this token
    std::string msg;         // error message when error == true
};

struct Diagnostic {
    Loc loc;
    std::string msg;
    int tokIdx = -1;         // index into the full token stream (for highlighting)
    bool lexical = false;
};

}  // namespace bminor
