#include "ast.hpp"
#include <ostream>

namespace bminor {

static const char* kNames[] = {
    "Program", "VarDecl", "FuncDecl", "Param", "PrimType", "ArrayType", "FuncType",
    "Block", "ExprStmt", "PrintStmt", "ReturnStmt", "IfStmt", "ForStmt", "WhileStmt",
    "Empty", "AssignExpr", "BinaryExpr", "UnaryExpr", "PostfixExpr", "IndexExpr",
    "CallExpr", "Identifier", "IntLiteral", "StringLiteral", "CharLiteral",
    "BoolLiteral", "InitList", "Error"};

const char* kindName(NodeKind k) { return kNames[(int)k]; }

void dumpAst(const Node& n, std::ostream& os, int indent) {
    os << std::string(indent * 2, ' ') << kindName(n.kind);
    if (!n.text.empty()) os << "(" << n.text << ")";
    os << "  @" << n.loc.line << ":" << n.loc.col
       << "  type=" << (n.typeAnn.empty() ? "null" : n.typeAnn) << "\n";
    for (const auto& k : n.kids) dumpAst(*k, os, indent + 1);
}

}  // namespace bminor
