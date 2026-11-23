// ast.hpp - Abstract syntax tree.
// One generic node type keeps the tree easy to traverse; the *kind* decides how
// `kids` is laid out (documented next to each kind below).
#pragma once
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>
#include "token.hpp"

namespace bminor {

enum class NodeKind {
    Program,      // kids: declarations (VarDecl | FuncDecl | Error)*
    VarDecl,      // text=name, tokIdx=name token; kids: [type, init?]  (init = expr | InitList)
    FuncDecl,     // text=name, tokIdx=name token; kids: [FuncType, Block?]  (no Block => prototype)
    Param,        // text=name; kids: [type]
    PrimType,     // text = integer|boolean|char|string|void
    ArrayType,    // kids: [elemType, sizeExpr?]
    FuncType,     // kids: [returnType, Param*]
    Block,        // kids: statements / local declarations
    ExprStmt,     // kids: [expr]
    PrintStmt,    // kids: expr*
    ReturnStmt,   // kids: [expr?]
    IfStmt,       // kids: [cond, then, else?]
    ForStmt,      // kids: [init, cond, step, body]  (missing parts are Empty)
    WhileStmt,    // kids: [cond, body]
    Empty,        // placeholder for an omitted for-clause
    AssignExpr,   // text="="; kids: [target, value]
    BinaryExpr,   // text=op; kids: [left, right]
    UnaryExpr,    // text=op (- !); kids: [operand]
    PostfixExpr,  // text=op (++ --); kids: [operand]
    IndexExpr,    // kids: [base, index]
    CallExpr,     // kids: [callee, arg*]
    Identifier,   // text=name
    IntLiteral, StringLiteral, CharLiteral, BoolLiteral,  // text=lexeme
    InitList,     // kids: expr*  ({1, 2, 3})
    Error         // placeholder produced by error recovery
};

struct Node;
using NodePtr = std::unique_ptr<Node>;

struct Node {
    NodeKind kind = NodeKind::Error;
    Loc loc;                 // line/column of the FIRST token of the construct
    std::string text;
    int tokIdx = -1;         // index of the "main" token in the full token stream
    std::string typeAnn;     // type annotation; empty == null (filled by the semantic phase)
    std::vector<NodePtr> kids;
};

const char* kindName(NodeKind k);
void dumpAst(const Node& n, std::ostream& os, int indent = 0);

}  // namespace bminor
