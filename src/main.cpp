// bminor - Phase 1: lexer + parser + AST + syntax highlighter for B-Minor.
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include "ast.hpp"
#include "highlighter.hpp"
#include "lexer.hpp"
#include "parser.hpp"

using namespace bminor;

static void usage() {
    std::cerr <<
        "usage: bminor [options] <file.bm | ->\n"
        "  --ansi          print ANSI-colored source to stdout (default if nothing else is chosen)\n"
        "  --html <file>   write a standalone HTML/CSS page\n"
        "  --tokens        print the token stream\n"
        "  --ast           print the abstract syntax tree\n"
        "exit status: 0 = no diagnostics, 1 = lexical/syntax errors reported, 2 = usage / I/O error\n";
}

int main(int argc, char** argv) {
    bool ansi = false, tokens = false, ast = false;
    std::string htmlOut, path;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--ansi") ansi = true;
        else if (a == "--tokens") tokens = true;
        else if (a == "--ast") ast = true;
        else if (a == "--html" && i + 1 < argc) htmlOut = argv[++i];
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (path.empty() && (a == "-" || a[0] != '-')) path = a;
        else { usage(); return 2; }
    }
    if (path.empty()) { usage(); return 2; }
    if (!ansi && !tokens && !ast && htmlOut.empty()) ansi = true;

    std::string src;
    {
        std::ostringstream ss;
        if (path == "-") ss << std::cin.rdbuf();
        else {
            std::ifstream in(path, std::ios::binary);
            if (!in) { std::cerr << "bminor: cannot open '" << path << "'\n"; return 2; }
            ss << in.rdbuf();
        }
        src = ss.str();
    }

    Lexer lexer(src, path);
    std::vector<Token> toks = lexer.tokenize();
    Parser parser(toks);
    NodePtr root = parser.parseProgram();

    std::vector<Diagnostic> diags = lexer.diagnostics();
    diags.insert(diags.end(), parser.diagnostics().begin(), parser.diagnostics().end());
    std::stable_sort(diags.begin(), diags.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return a.loc.line != b.loc.line ? a.loc.line < b.loc.line : a.loc.col < b.loc.col;
    });

    std::map<int, std::string> errMsgs;
    std::vector<Cat> cats = classify(toks, *root, diags, errMsgs);

    if (tokens)
        for (const auto& t : toks) {
            std::cout << t.loc.line << ":" << t.loc.col << "\t" << tokTypeName(t.type);
            if (t.type != TokType::END_OF_FILE) std::cout << "(" << t.lexeme << ")";
            if (t.error) std::cout << "   <-- " << t.msg;
            std::cout << "\n";
        }
    if (ast) dumpAst(*root, std::cout);
    if (ansi) std::cout << renderAnsi(src, toks, cats);
    if (!htmlOut.empty()) {
        std::ofstream out(htmlOut, std::ios::binary);
        if (!out) { std::cerr << "bminor: cannot write '" << htmlOut << "'\n"; return 2; }
        out << renderHtml(src, toks, cats, errMsgs, path);
    }

    for (const auto& d : diags)
        std::cerr << d.loc.file << ":" << d.loc.line << ":" << d.loc.col << ": "
                  << (d.lexical ? "lexical error: " : "syntax error: ") << d.msg << "\n";
    return diags.empty() ? 0 : 1;
}
