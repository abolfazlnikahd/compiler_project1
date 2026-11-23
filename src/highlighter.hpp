// highlighter.hpp - AST-aware syntax highlighting with ANSI and HTML back ends.
#pragma once
#include <map>
#include <string>
#include <vector>
#include "ast.hpp"
#include "token.hpp"

namespace bminor {

enum class Cat { Keyword, Type, Variable, Function, Number, String, Boolean, Operator, Comment, Error };

// One category per token of the FULL token stream (comments included).
// Step 1: default category from the token type.  Step 2: AST pass refines identifiers
// (function name vs variable) and type keywords.  Step 3: lexical/syntax errors override.
std::vector<Cat> classify(const std::vector<Token>& toks, const Node& root,
                          const std::vector<Diagnostic>& diags,
                          std::map<int, std::string>& errMsgs);

std::string renderAnsi(const std::string& src, const std::vector<Token>& toks, const std::vector<Cat>& cats);
std::string renderHtml(const std::string& src, const std::vector<Token>& toks, const std::vector<Cat>& cats,
                       const std::map<int, std::string>& errMsgs, const std::string& title);

}  // namespace bminor
