#include "highlighter.hpp"

namespace bminor {

static Cat baseCategory(TokType t) {
    switch (t) {
        case TokType::KEYWORD: return Cat::Keyword;
        case TokType::IDENT: return Cat::Variable;
        case TokType::INT_LIT: return Cat::Number;
        case TokType::BOOL_LIT: return Cat::Boolean;
        case TokType::STRING_LIT:
        case TokType::CHAR_LIT: return Cat::String;
        case TokType::LINE_COMMENT:
        case TokType::BLOCK_COMMENT: return Cat::Comment;
        case TokType::INVALID: return Cat::Error;
        default: return Cat::Operator;  // operators, '=', delimiters, EOF
    }
}

static void walk(const Node& n, std::vector<Cat>& c) {
    auto set = [&](int i, Cat k) { if (i >= 0 && i < (int)c.size()) c[i] = k; };
    for (const auto& k : n.kids) walk(*k, c);   // children first, so the rules below win
    switch (n.kind) {
        case NodeKind::FuncDecl: set(n.tokIdx, Cat::Function); break;
        case NodeKind::VarDecl:
        case NodeKind::Param:
        case NodeKind::Identifier: set(n.tokIdx, Cat::Variable); break;
        case NodeKind::PrimType: set(n.tokIdx, Cat::Type); break;
        case NodeKind::CallExpr:   // callee identifier is a function name, not a variable
            if (!n.kids.empty() && n.kids[0]->kind == NodeKind::Identifier)
                set(n.kids[0]->tokIdx, Cat::Function);
            break;
        default: break;
    }
}

std::vector<Cat> classify(const std::vector<Token>& toks, const Node& root,
                          const std::vector<Diagnostic>& diags,
                          std::map<int, std::string>& errMsgs) {
    std::vector<Cat> c(toks.size());
    for (size_t i = 0; i < toks.size(); ++i) c[i] = baseCategory(toks[i].type);
    walk(root, c);
    for (size_t i = 0; i < toks.size(); ++i)
        if (toks[i].error) { c[i] = Cat::Error; errMsgs[(int)i] = toks[i].msg; }
    for (const auto& d : diags)
        if (d.tokIdx >= 0 && d.tokIdx < (int)toks.size() && toks[d.tokIdx].type != TokType::END_OF_FILE) {
            c[d.tokIdx] = Cat::Error;
            if (!errMsgs.count(d.tokIdx)) errMsgs[d.tokIdx] = d.msg;
        }
    return c;
}

static const char* ansiStyle(Cat c) {
    switch (c) {
        case Cat::Keyword: return "\x1b[1;34m";         // bold blue
        case Cat::Type: return "\x1b[36m";              // cyan
        case Cat::Variable: return "";                  // default
        case Cat::Function: return "\x1b[33m";          // yellow
        case Cat::Number: return "\x1b[38;5;208m";      // orange
        case Cat::Boolean: return "\x1b[38;5;208m";     // orange
        case Cat::String: return "\x1b[32m";            // green
        case Cat::Operator: return "\x1b[37m";          // light gray
        case Cat::Comment: return "\x1b[2;3;90m";       // dim gray italic
        case Cat::Error: return "\x1b[4;31m";           // red underline
    }
    return "";
}

std::string renderAnsi(const std::string& src, const std::vector<Token>& toks, const std::vector<Cat>& cats) {
    std::string out;
    size_t pos = 0;
    for (size_t i = 0; i < toks.size(); ++i) {
        const Token& t = toks[i];
        if (t.type == TokType::END_OF_FILE) break;
        out += src.substr(pos, t.offset - pos);       // whitespace between tokens, verbatim
        const char* st = ansiStyle(cats[i]);
        std::string text = src.substr(t.offset, t.length);
        if (!*st) out += text;
        else {
            // re-open the style on every line so multi-line comments survive pagers
            size_t a = 0;
            while (a <= text.size()) {
                size_t nl = text.find('\n', a);
                std::string part = text.substr(a, nl == std::string::npos ? std::string::npos : nl - a);
                if (!part.empty()) out += std::string(st) + part + "\x1b[0m";
                if (nl == std::string::npos) break;
                out += '\n';
                a = nl + 1;
            }
        }
        pos = t.offset + t.length;
    }
    out += src.substr(pos);
    return out;
}

static std::string esc(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '&': r += "&amp;"; break;
            case '<': r += "&lt;"; break;
            case '>': r += "&gt;"; break;
            case '"': r += "&quot;"; break;
            default: r += c;
        }
    }
    return r;
}

static const char* htmlClass(Cat c) {
    switch (c) {
        case Cat::Keyword: return "kw";
        case Cat::Type: return "ty";
        case Cat::Variable: return "var";
        case Cat::Function: return "fn";
        case Cat::Number: return "lit num";
        case Cat::Boolean: return "lit bool";
        case Cat::String: return "lit str";
        case Cat::Operator: return "op";
        case Cat::Comment: return "cmt";
        case Cat::Error: return "err";
    }
    return "";
}

std::string renderHtml(const std::string& src, const std::vector<Token>& toks, const std::vector<Cat>& cats,
                       const std::map<int, std::string>& errMsgs, const std::string& title) {
    std::string out =
        "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<title>" + esc(title) +
        "</title>\n<style>\n"
        "body{background:#1e1e1e;margin:0;padding:1.5rem;}\n"
        "pre.code{font:14px/1.5 Consolas,\"DejaVu Sans Mono\",monospace;color:#e6e6e6;"
        "background:#252526;padding:1rem 1.25rem;border-radius:8px;overflow-x:auto;tab-size:4;}\n"
        ".kw{color:#4aa3ff;font-weight:bold}\n.ty{color:#2ec4b6}\n.var{color:#e6e6e6}\n.fn{color:#e6c200}\n"
        ".lit.num,.lit.bool{color:#ff9e3d}\n.lit.str{color:#7ec16e}\n.op{color:#b0b0b0}\n"
        ".cmt{color:#7a7a7a;font-style:italic}\n"
        ".err{color:#ff6b6b;text-decoration:underline;text-decoration-color:#ff3b3b;"
        "text-decoration-thickness:2px;text-underline-offset:3px}\n"
        "</style>\n</head>\n<body>\n<pre class=\"code\">";
    size_t pos = 0;
    for (size_t i = 0; i < toks.size(); ++i) {
        const Token& t = toks[i];
        if (t.type == TokType::END_OF_FILE) break;
        out += esc(src.substr(pos, t.offset - pos));
        out += std::string("<span class=\"") + htmlClass(cats[i]) + "\"";
        auto it = errMsgs.find((int)i);
        if (it != errMsgs.end()) out += " title=\"" + esc(it->second) + "\"";
        out += ">" + esc(src.substr(t.offset, t.length)) + "</span>";
        pos = t.offset + t.length;
    }
    out += esc(src.substr(pos));
    out += "</pre>\n</body>\n</html>\n";
    return out;
}

}  // namespace bminor
