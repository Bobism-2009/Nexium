#include "nexium.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <regex>
#include <unordered_set>

#include "Lexer.hpp"
#include "Modules.hpp"
#include "Parser.hpp"

ImU32 highlight_color(HighlightKind k) {
    switch (k) {
    case HighlightKind::Text: return IM_COL32(212, 212, 212, 255);
    case HighlightKind::Comment: return IM_COL32(106, 153, 85, 255);
    case HighlightKind::Keyword: return IM_COL32(86, 156, 214, 255);
    case HighlightKind::Control: return IM_COL32(197, 134, 192, 255);
    case HighlightKind::Type: return IM_COL32(86, 156, 214, 255);
    case HighlightKind::Constant: return IM_COL32(86, 156, 214, 255);
    case HighlightKind::String: return IM_COL32(206, 145, 120, 255);
    case HighlightKind::Escape: return IM_COL32(215, 186, 125, 255);
    case HighlightKind::Number: return IM_COL32(181, 206, 168, 255);
    case HighlightKind::Function: return IM_COL32(220, 220, 170, 255);
    case HighlightKind::Identifier: return IM_COL32(156, 220, 254, 255);
    case HighlightKind::Module: return IM_COL32(79, 193, 255, 255);
    case HighlightKind::Member: return IM_COL32(220, 220, 170, 255);
    case HighlightKind::Preproc: return IM_COL32(197, 134, 192, 255);
    case HighlightKind::IncludePath: return IM_COL32(206, 145, 120, 255);
    case HighlightKind::Operator: return IM_COL32(212, 212, 212, 255);
    case HighlightKind::Punct: return IM_COL32(212, 212, 212, 255);
    case HighlightKind::Builtin: return IM_COL32(220, 220, 170, 255);
    case HighlightKind::TypeName: return IM_COL32(78, 201, 176, 255);
    case HighlightKind::Property: return IM_COL32(156, 220, 254, 255);
    case HighlightKind::EnumMember: return IM_COL32(79, 193, 255, 255);
    case HighlightKind::Invalid: return IM_COL32(244, 71, 71, 255);
    case HighlightKind::Todo: return IM_COL32(255, 176, 76, 255);
    case HighlightKind::Interp: return IM_COL32(86, 156, 214, 255);
    case HighlightKind::Bracket1: return IM_COL32(255, 215, 0, 255);
    case HighlightKind::Bracket2: return IM_COL32(218, 112, 214, 255);
    case HighlightKind::Bracket3: return IM_COL32(23, 159, 255, 255);
    case HighlightKind::Heading: return IM_COL32(86, 156, 214, 255);
    case HighlightKind::Strong: return IM_COL32(215, 186, 125, 255);
    case HighlightKind::Emphasis: return IM_COL32(197, 134, 192, 255);
    case HighlightKind::Link: return IM_COL32(55, 148, 255, 255);
    default: {
        HighlightKind unk = k;
        (void)unk;
        return IM_COL32(212, 212, 212, 255);
    }
    }
}

const char* highlight_name(HighlightKind k) {
    switch (k) {
    case HighlightKind::Text: return "text";
    case HighlightKind::Comment: return "comment";
    case HighlightKind::Keyword: return "keyword";
    case HighlightKind::Control: return "control";
    case HighlightKind::Type: return "type";
    case HighlightKind::Constant: return "constant";
    case HighlightKind::String: return "string";
    case HighlightKind::Escape: return "escape";
    case HighlightKind::Number: return "number";
    case HighlightKind::Function: return "function";
    case HighlightKind::Identifier: return "identifier";
    case HighlightKind::Module: return "module";
    case HighlightKind::Member: return "member";
    case HighlightKind::Preproc: return "preprocessor";
    case HighlightKind::IncludePath: return "include";
    case HighlightKind::Operator: return "operator";
    case HighlightKind::Punct: return "punctuation";
    case HighlightKind::Builtin: return "builtin";
    case HighlightKind::TypeName: return "typename";
    case HighlightKind::Property: return "property";
    case HighlightKind::EnumMember: return "enum member";
    case HighlightKind::Invalid: return "invalid";
    case HighlightKind::Todo: return "todo";
    case HighlightKind::Interp: return "interpolation";
    case HighlightKind::Bracket1:
    case HighlightKind::Bracket2:
    case HighlightKind::Bracket3: return "bracket";
    case HighlightKind::Heading: return "heading";
    case HighlightKind::Strong: return "strong";
    case HighlightKind::Emphasis: return "emphasis";
    case HighlightKind::Link: return "link";
    default: {
        HighlightKind unk = k;
        (void)unk;
        return "text";
    }
    }
}

// ---------------------------------------------------------------------------
// Languages
// ---------------------------------------------------------------------------
Lang lang_for_path(const std::string& path) {
    std::string name = path_filename(path);
    std::string lower = name;
    for (char& c : lower) c = (char)std::tolower((unsigned char)c);
    if (lower == "makefile" || lower == "gnumakefile") return Lang::Shell;
    if (lower == ".gitignore" || lower == ".gitattributes" || lower == ".editorconfig") return Lang::Ini;
    std::string e = path_ext(path);
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    if (e == ".nxa") return Lang::Nexa;
    if (e == ".json" || e == ".jsonc") return Lang::Json;
    if (e == ".c" || e == ".h" || e == ".cpp" || e == ".hpp" || e == ".cc" || e == ".cxx" ||
        e == ".hh" || e == ".hxx" || e == ".inl" || e == ".ipp" || e == ".rc")
        return Lang::Cpp;
    if (e == ".md" || e == ".markdown") return Lang::Markdown;
    if (e == ".ini" || e == ".toml" || e == ".cfg" || e == ".conf" || e == ".yaml" ||
        e == ".yml" || e == ".properties")
        return Lang::Ini;
    if (e == ".sh" || e == ".bash" || e == ".bat" || e == ".cmd" || e == ".ps1" ||
        e == ".psm1" || e == ".mk")
        return Lang::Shell;
    return Lang::Plain;
}

const char* lang_name(Lang l) {
    switch (l) {
    case Lang::Nexa: return "Nexa";
    case Lang::Json: return "JSON";
    case Lang::Cpp: return "C++";
    case Lang::Markdown: return "Markdown";
    case Lang::Ini: return "Config";
    case Lang::Shell: return "Shell";
    default: return "Plain Text";
    }
}

const char* lang_line_comment(Lang l, const std::string& path) {
    switch (l) {
    case Lang::Nexa:
    case Lang::Json:
    case Lang::Cpp: return "//";
    case Lang::Ini: {
        std::string e = path_ext(path);
        return e == ".ini" ? ";" : "#";
    }
    case Lang::Shell: {
        std::string e = path_ext(path);
        for (char& c : e) c = (char)std::tolower((unsigned char)c);
        if (e == ".bat" || e == ".cmd") return "::";
        return "#";
    }
    default: return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Shared scanning helpers
// ---------------------------------------------------------------------------
static const std::unordered_set<std::string> kControl = {
    "fn", "extern", "if", "else", "while", "for", "switch", "case", "default",
    "return", "break", "continue", "goto", "try", "catch", "throw"
};

static const std::unordered_set<std::string> kKeyword = {
    "let", "const", "struct", "enum", "new", "delete", "sizeof", "in", "self",
    "and", "or", "not"
};

static const std::unordered_set<std::string> kType = {
    "int", "short", "long", "size_t", "string", "bool", "float", "char", "void", "unsigned",
    "map", "Result", "result", "Json", "json", "HttpResponse", "HttpServer", "HttpRequest"
};

static const std::unordered_set<std::string> kConstant = {
    "true", "false", "null"
};

static const std::unordered_set<std::string> kModule = {
    "io", "os", "dll", "file", "random", "math", "crypto",
    "http", "tcp", "udp", "json", "time", "thread", "gfx", "gfx3d"
};

// Methods the runtime provides on strings, slices, maps, results and Json values.
static const std::unordered_set<std::string> kBuiltin = {
    "len", "trim", "upper", "lower", "contains", "starts_with", "ends_with",
    "index_of", "last_index_of", "count", "replace", "substring", "repeat", "split",
    "push", "pop", "insert", "remove", "clear", "has", "sort", "sort_desc", "reverse",
    "min", "max", "sum", "join", "keys", "values", "ok", "err", "value", "error",
    "kind", "is_null", "is_bool", "is_number", "is_string", "is_array", "is_object",
    "is_error", "as_bool", "as_int", "as_float", "as_string", "get", "set"
};

static const std::unordered_set<std::string> kStdIncludes = {
    "io", "os", "dll", "file", "random", "math", "crypto", "network", "json",
    "gfx", "gfx3d", "time", "thread", "inline"
};

static bool is_ident_start(char c) {
    return std::isalpha((unsigned char)c) || c == '_';
}

static bool is_ident(char c) {
    return std::isalnum((unsigned char)c) || c == '_';
}

static bool is_hex(char c) {
    return std::isxdigit((unsigned char)c) != 0;
}

static void push_span(std::vector<HighlightSpan>& out, int start, int end, HighlightKind kind) {
    if (end <= start) return;
    if (!out.empty() && out.back().end > start) {
        // Spans must stay sorted and disjoint; clip anything that would overlap.
        start = out.back().end;
        if (end <= start) return;
    }
    if (!out.empty() && out.back().end == start && out.back().kind == kind) {
        out.back().end = end;
        return;
    }
    out.push_back({start, end, kind});
}

// Colors a comment and picks out TODO-style markers inside it.
static void push_comment(std::vector<HighlightSpan>& out, const std::string& s, int b, int e) {
    static const char* tags[] = {"TODO", "FIXME", "NOTE", "HACK", "XXX", "BUG"};
    int p = b;
    int i = b;
    while (i < e) {
        bool hit = false;
        if (std::isupper((unsigned char)s[(size_t)i]) && (i == b || !is_ident(s[(size_t)i - 1]))) {
            for (const char* tag : tags) {
                int len = (int)std::strlen(tag);
                if (i + len <= e && s.compare((size_t)i, (size_t)len, tag) == 0 &&
                    (i + len == e || !is_ident(s[(size_t)(i + len)]))) {
                    push_span(out, p, i, HighlightKind::Comment);
                    push_span(out, i, i + len, HighlightKind::Todo);
                    i += len;
                    p = i;
                    hit = true;
                    break;
                }
            }
        }
        if (!hit) i++;
    }
    push_span(out, p, e, HighlightKind::Comment);
}

// Rainbow brackets: depth picks one of three colors, a closer with no opener is invalid.
struct BracketState {
    std::vector<char> stack;
};

static HighlightKind bracket_kind(size_t depth) {
    switch (depth % 3) {
    case 0: return HighlightKind::Bracket1;
    case 1: return HighlightKind::Bracket2;
    default: return HighlightKind::Bracket3;
    }
}

static void push_bracket(std::vector<HighlightSpan>& out, int pos, char c, BracketState& bs) {
    if (c == '(' || c == '[' || c == '{') {
        push_span(out, pos, pos + 1, bracket_kind(bs.stack.size()));
        bs.stack.push_back(c);
        return;
    }
    char want = c == ')' ? '(' : (c == ']' ? '[' : '{');
    if (!bs.stack.empty() && bs.stack.back() == want) {
        bs.stack.pop_back();
        push_span(out, pos, pos + 1, bracket_kind(bs.stack.size()));
    } else {
        push_span(out, pos, pos + 1, HighlightKind::Invalid);
    }
}

static bool is_bracket(char c) {
    return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
}

static int skip_blank(const std::string& s, int i, int end) {
    while (i < end && (s[(size_t)i] == ' ' || s[(size_t)i] == '\t')) i++;
    return i;
}

static int line_end(const std::string& s, int i, int end) {
    while (i < end && s[(size_t)i] != '\n') i++;
    return i;
}

static bool only_blank_before(const std::string& s, int i, int begin) {
    for (int j = i - 1; j >= begin; j--) {
        char c = s[(size_t)j];
        if (c == '\n') return true;
        if (c != ' ' && c != '\t') return false;
    }
    return true;
}

// Index of the previous non-whitespace character, or begin - 1.
static int prev_sig(const std::string& s, int i, int begin) {
    i--;
    while (i >= begin) {
        char c = s[(size_t)i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        i--;
    }
    return i;
}

static int next_sig(const std::string& s, int i, int end) {
    while (i < end) {
        char c = s[(size_t)i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        i++;
    }
    return i;
}

static int scan_ident(const std::string& s, int i, int end) {
    while (i < end && is_ident(s[(size_t)i])) i++;
    return i;
}

static bool operator_at(const std::string& s, int i, int end, int& len) {
    static const char* ops[] = {
        "<<=", ">>=", "...", "::", "<<", ">>", "==", "!=", "<=", ">=", "&&", "||",
        "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "++", "--", "->",
        "=", "<", ">", "+", "-", "*", "/", "%", "!", "&", "|", "^", "~", "?"
    };
    for (const char* o : ops) {
        int l = (int)std::strlen(o);
        if (i + l <= end && s.compare((size_t)i, (size_t)l, o) == 0) {
            len = l;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Nexa
// ---------------------------------------------------------------------------
static void hl_cpp(const std::string& s, int begin, int end, std::vector<HighlightSpan>& out);

// struct / enum names anywhere in the range, so a type used above its
// definition is still colored as one.
static void collect_nexa_types(const std::string& s, int begin, int end,
                               std::unordered_set<std::string>& types) {
    int i = begin;
    while (i < end) {
        char c = s[(size_t)i];
        if (c == '/' && i + 1 < end && s[(size_t)i + 1] == '/') {
            i = line_end(s, i, end);
            continue;
        }
        if (c == '/' && i + 1 < end && s[(size_t)i + 1] == '*') {
            auto p = s.find("*/", (size_t)i + 2);
            i = (p == std::string::npos || (int)p >= end) ? end : (int)p + 2;
            continue;
        }
        if (c == '"' || c == '\'') {
            i++;
            while (i < end && s[(size_t)i] != c && s[(size_t)i] != '\n') {
                if (s[(size_t)i] == '\\') i++;
                i++;
            }
            i++;
            continue;
        }
        if (is_ident_start(c) && (i == begin || !is_ident(s[(size_t)i - 1]))) {
            int e = scan_ident(s, i, end);
            size_t len = (size_t)(e - i);
            if ((len == 6 && s.compare((size_t)i, 6, "struct") == 0) ||
                (len == 4 && s.compare((size_t)i, 4, "enum") == 0)) {
                int k = skip_blank(s, e, end);
                int ke = scan_ident(s, k, end);
                if (ke > k && is_ident_start(s[(size_t)k])) {
                    types.insert(s.substr((size_t)k, (size_t)(ke - k)));
                }
            }
            i = e;
            continue;
        }
        i++;
    }
}

// Scans one escape starting at the backslash; ok is false for escapes the
// Nexa lexer rejects (unknown letters, \x without exactly two hex digits).
static int scan_nexa_escape(const std::string& s, int i, int end, bool& ok) {
    ok = true;
    i++;
    if (i >= end || s[(size_t)i] == '\n') {
        ok = false;
        return i;
    }
    char c = s[(size_t)i++];
    switch (c) {
    case 'n': case 't': case 'r': case 'a': case 'b': case 'f': case 'v': case '0':
    case '\\': case '"': case '\'':
        return i;
    default:
        break;
    }
    if (c == 'x' || c == 'X') {
        int d = 0;
        while (d < 2 && i < end && is_hex(s[(size_t)i])) {
            i++;
            d++;
        }
        ok = d == 2;
        return i;
    }
    ok = false;
    while (i < end && ((unsigned char)s[(size_t)i] & 0xC0) == 0x80) i++;
    return i;
}

namespace {

struct NexaHL {
    const std::string& s;
    std::vector<HighlightSpan>& out;
    const std::unordered_set<std::string>& user_types;
    BracketState br;
    int brace_depth = 0;
    int enum_body = -1;
    int struct_body = -1;
    bool pending_enum = false;
    bool pending_struct = false;
    int begin = 0;
    int end = 0;

    NexaHL(const std::string& src, std::vector<HighlightSpan>& o,
           const std::unordered_set<std::string>& types)
        : s(src), out(o), user_types(types) {}

    char at(int i) const { return (i >= begin && i < end) ? s[(size_t)i] : '\0'; }

    void push(int a, int b, HighlightKind k) { push_span(out, a, b, k); }

    int string_lit(int i, char q) {
        int p = i;
        i++;
        while (i < end && s[(size_t)i] != q && s[(size_t)i] != '\n') {
            if (s[(size_t)i] == '\\') {
                push(p, i, HighlightKind::String);
                bool ok = true;
                int e = scan_nexa_escape(s, i, end, ok);
                push(i, e, ok ? HighlightKind::Escape : HighlightKind::Invalid);
                i = e;
                p = i;
                continue;
            }
            i++;
        }
        if (i < end && s[(size_t)i] == q) i++;
        push(p, i, HighlightKind::String);
        return i;
    }

    int char_lit(int i) {
        int st = i;
        i++;
        bool ok = true;
        int esc_s = -1, esc_e = -1;
        if (i < end && s[(size_t)i] == '\\') {
            esc_s = i;
            esc_e = scan_nexa_escape(s, i, end, ok);
            i = esc_e;
        } else if (i < end && s[(size_t)i] != '\n' && s[(size_t)i] != '\'') {
            i++;
            while (i < end && ((unsigned char)s[(size_t)i] & 0xC0) == 0x80) i++;
        } else {
            ok = false;
        }
        if (i < end && s[(size_t)i] == '\'') {
            i++;
            if (esc_s >= 0) {
                push(st, esc_s, HighlightKind::String);
                push(esc_s, esc_e, ok ? HighlightKind::Escape : HighlightKind::Invalid);
                push(esc_e, i, HighlightKind::String);
            } else {
                push(st, i, ok ? HighlightKind::String : HighlightKind::Invalid);
            }
        } else {
            // A character literal must close right after its one character.
            push(st, i, HighlightKind::Invalid);
        }
        return i;
    }

    int raw_string(int i) {
        int st = i;
        i += 2;
        int ds = i;
        while (i < end && s[(size_t)i] != '(' && s[(size_t)i] != '\n' && s[(size_t)i] != '"') i++;
        if (i >= end || s[(size_t)i] != '(') {
            push(st, i, HighlightKind::Invalid);
            return i;
        }
        std::string close = ")" + s.substr((size_t)ds, (size_t)(i - ds)) + "\"";
        i++;
        auto p = s.find(close, (size_t)i);
        i = (p == std::string::npos || (int)(p + close.size()) > end) ? end : (int)(p + close.size());
        push(st, i, HighlightKind::String);
        return i;
    }

    // f"text {value:spec} {{literal}}" -- values are highlighted as Nexa.
    int fstring(int i) {
        int p = i;
        i += 2;
        while (i < end && s[(size_t)i] != '"' && s[(size_t)i] != '\n') {
            char c = s[(size_t)i];
            if (c == '\\') {
                push(p, i, HighlightKind::String);
                bool ok = true;
                int e = scan_nexa_escape(s, i, end, ok);
                push(i, e, ok ? HighlightKind::Escape : HighlightKind::Invalid);
                i = e;
                p = i;
                continue;
            }
            if ((c == '{' || c == '}') && at(i + 1) == c) {
                push(p, i, HighlightKind::String);
                push(i, i + 2, HighlightKind::Escape);
                i += 2;
                p = i;
                continue;
            }
            if (c == '}') {
                push(p, i, HighlightKind::String);
                push(i, i + 1, HighlightKind::Invalid);
                i++;
                p = i;
                continue;
            }
            if (c != '{') {
                i++;
                continue;
            }
            push(p, i, HighlightKind::String);
            push(i, i + 1, HighlightKind::Interp);
            i++;
            int es = i;
            int depth = 0;
            int ternaries = 0;
            while (i < end) {
                char v = s[(size_t)i];
                if (v == '\n') break;
                if (v == '"' || v == '\'') {
                    i++;
                    while (i < end && s[(size_t)i] != v && s[(size_t)i] != '\n') {
                        if (s[(size_t)i] == '\\') i++;
                        i++;
                    }
                    if (i < end && s[(size_t)i] == v) i++;
                    continue;
                }
                if (v == '(' || v == '[' || v == '{') {
                    depth++;
                } else if (v == ')' || v == ']') {
                    if (depth > 0) depth--;
                } else if (v == '}') {
                    if (depth == 0) break;
                    depth--;
                } else if (depth == 0 && v == '?') {
                    ternaries++;
                } else if (depth == 0 && v == ':') {
                    if (at(i + 1) == ':') {
                        i += 2;
                        continue;
                    }
                    if (ternaries == 0) break;
                    ternaries--;
                }
                i++;
            }
            i = std::min(i, end);
            if (i > es) {
                NexaHL sub(s, out, user_types);
                sub.run(es, i);
            }
            if (i < end && s[(size_t)i] == ':') {
                int ss = i;
                i++;
                while (i < end && s[(size_t)i] != '}' && s[(size_t)i] != '\n' && s[(size_t)i] != '"') i++;
                push(ss, i, HighlightKind::Escape);
            }
            if (i < end && s[(size_t)i] == '}') {
                push(i, i + 1, HighlightKind::Interp);
                i++;
            }
            p = i;
        }
        if (i < end && s[(size_t)i] == '"') i++;
        push(p, i, HighlightKind::String);
        return i;
    }

    int directive(int i) {
        int st = i;
        int we = scan_ident(s, i + 1, end);
        std::string word = s.substr((size_t)i + 1, (size_t)(we - i - 1));
        int le = line_end(s, i, end);
        if (word != "include") {
            push(st, le, HighlightKind::Preproc);
            return le;
        }
        push(st, we, HighlightKind::Preproc);
        i = skip_blank(s, we, le);
        if (i < le && (s[(size_t)i] == '<' || s[(size_t)i] == '"')) {
            char closer = s[(size_t)i] == '<' ? '>' : '"';
            int ps = i;
            i++;
            while (i < le && s[(size_t)i] != closer) i++;
            std::string path = s.substr((size_t)ps + 1, (size_t)(i - ps - 1));
            bool closed = i < le;
            if (closed) i++;
            bool ok = closed;
            if (closer == '>' && path.rfind("std/", 0) == 0) ok = ok && kStdIncludes.count(path.substr(4)) > 0;
            push(ps, i, ok ? HighlightKind::IncludePath : HighlightKind::Invalid);
        }
        i = skip_blank(s, i, le);
        if (i + 1 < le && s[(size_t)i] == '/' && s[(size_t)i + 1] == '/') push_comment(out, s, i, le);
        else push(i, le, HighlightKind::Preproc);
        return le;
    }

    int inline_cpp(int i) {
        // i is after `inline_cpp`; highlight `!`, `{`, the C++ body and `}`.
        int j = next_sig(s, i, end);
        if (j < end && s[(size_t)j] == '!') {
            push(j, j + 1, HighlightKind::Keyword);
            j = next_sig(s, j + 1, end);
        } else {
            return i;
        }
        if (j >= end || s[(size_t)j] != '{') return j;
        push_bracket(out, j, '{', br);
        int body = j + 1;
        int depth = 1;
        int k = body;
        while (k < end) {
            char c = s[(size_t)k];
            if (c == '{') depth++;
            else if (c == '}' && --depth == 0) break;
            k++;
        }
        hl_cpp(s, body, k, out);
        if (k < end) {
            push_bracket(out, k, '}', br);
            k++;
        }
        return k;
    }

    bool type_position(int st) const {
        int j = prev_sig(s, st, begin);
        if (j < begin) return false;
        char c = s[(size_t)j];
        if (c == ':' && (j == begin || s[(size_t)j - 1] != ':')) return true;
        if (c == ']' && j > begin && s[(size_t)j - 1] == '[') return true;
        if (is_ident(c)) {
            int ws = j;
            while (ws > begin && is_ident(s[(size_t)ws - 1])) ws--;
            if (j - ws + 1 == 3 && s.compare((size_t)ws, 3, "new") == 0) return true;
        }
        return false;
    }

    bool statement_start(int st) const {
        int j = prev_sig(s, st, begin);
        if (j < begin) return true;
        char c = s[(size_t)j];
        return c == '{' || c == ';' || c == '}';
    }

    void ident(int st, int e) {
        std::string id = s.substr((size_t)st, (size_t)(e - st));
        int j = skip_blank(s, e, end);
        char nx = at(j);
        bool call = nx == '(';
        bool dotted = st > begin && s[(size_t)st - 1] == '.';
        bool before_scope = nx == ':' && at(j + 1) == ':';
        bool after_scope = st >= begin + 2 && s[(size_t)st - 1] == ':' && s[(size_t)st - 2] == ':';

        if (id == "inline_cpp") {
            push(st, e, HighlightKind::Keyword);
            pos = inline_cpp(e);
            return;
        }
        if (id == "struct" || id == "enum") {
            push(st, e, HighlightKind::Keyword);
            int k = skip_blank(s, e, end);
            int ke = scan_ident(s, k, end);
            if (ke > k && is_ident_start(s[(size_t)k])) {
                push(k, ke, HighlightKind::TypeName);
                pos = ke;
            }
            if (id == "struct") pending_struct = true;
            else pending_enum = true;
            return;
        }
        if (id == "fn" || id == "extern") {
            push(st, e, HighlightKind::Control);
            if (id == "extern") return;
            int k = skip_blank(s, e, end);
            int ke = scan_ident(s, k, end);
            if (ke > k && is_ident_start(s[(size_t)k])) {
                push(k, ke, HighlightKind::Function);
                pos = ke;
            }
            return;
        }
        if (dotted) {
            if (call && kBuiltin.count(id)) push(st, e, HighlightKind::Builtin);
            else push(st, e, call ? HighlightKind::Member : HighlightKind::Property);
            return;
        }
        if (kControl.count(id)) {
            push(st, e, HighlightKind::Control);
            return;
        }
        if (kKeyword.count(id)) {
            push(st, e, HighlightKind::Keyword);
            return;
        }
        if (kConstant.count(id)) {
            push(st, e, HighlightKind::Constant);
            return;
        }
        if (kModule.count(id) && nx == '.') {
            push(st, e, HighlightKind::Module);
            return;
        }
        if (kType.count(id)) {
            push(st, e, HighlightKind::Type);
            return;
        }
        if (after_scope) {
            push(st, e, call ? HighlightKind::Function : HighlightKind::EnumMember);
            return;
        }
        if (before_scope || user_types.count(id)) {
            push(st, e, HighlightKind::TypeName);
            return;
        }
        if (enum_body >= 0 && brace_depth == enum_body) {
            push(st, e, HighlightKind::EnumMember);
            return;
        }
        if (struct_body >= 0 && brace_depth == struct_body && nx == ':' && at(j + 1) != ':' &&
            statement_start(st)) {
            push(st, e, HighlightKind::Property);
            return;
        }
        if ((id == "ok" || id == "err" || id == "len" || id == "trim") && call) {
            push(st, e, HighlightKind::Builtin);
            return;
        }
        if (std::isupper((unsigned char)id[0]) && type_position(st)) {
            push(st, e, HighlightKind::TypeName);
            return;
        }
        if (call || id == "main" || id == "__init__") {
            push(st, e, HighlightKind::Function);
            return;
        }
        push(st, e, HighlightKind::Identifier);
    }

    int pos = 0;

    void run(int b, int e) {
        begin = b;
        end = e;
        pos = b;
        while (pos < end) {
            int i = pos;
            char c = s[(size_t)i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                pos++;
                continue;
            }
            char c1 = at(i + 1);
            if (c == '/' && c1 == '/') {
                int le = line_end(s, i, end);
                push_comment(out, s, i, le);
                pos = le;
                continue;
            }
            if (c == '/' && c1 == '*') {
                auto p = s.find("*/", (size_t)i + 2);
                int ce = (p == std::string::npos || (int)p + 2 > end) ? end : (int)p + 2;
                push_comment(out, s, i, ce);
                pos = ce;
                continue;
            }
            if (c == '#') {
                if (only_blank_before(s, i, begin)) {
                    pos = directive(i);
                } else {
                    push(i, i + 1, HighlightKind::Invalid);
                    pos = i + 1;
                }
                continue;
            }
            bool word_edge = i == begin || !is_ident(s[(size_t)i - 1]);
            if (c == 'R' && c1 == '"' && word_edge) {
                pos = raw_string(i);
                continue;
            }
            if (c == 'f' && c1 == '"' && word_edge) {
                pos = fstring(i);
                continue;
            }
            if (c == '"') {
                pos = string_lit(i, '"');
                continue;
            }
            if (c == '\'') {
                pos = char_lit(i);
                continue;
            }
            if (std::isdigit((unsigned char)c) || (c == '.' && std::isdigit((unsigned char)c1))) {
                int st = i;
                bool bad = false;
                if (c == '0' && (c1 == 'x' || c1 == 'X')) {
                    i += 2;
                    int ds = i;
                    while (i < end && is_hex(s[(size_t)i])) i++;
                    bad = i == ds;
                } else {
                    if (c == '.') i++;
                    while (i < end && std::isdigit((unsigned char)s[(size_t)i])) i++;
                    bool is_float = c == '.';
                    if (i + 1 < end && s[(size_t)i] == '.' && std::isdigit((unsigned char)s[(size_t)i + 1])) {
                        i++;
                        is_float = true;
                        while (i < end && std::isdigit((unsigned char)s[(size_t)i])) i++;
                    }
                    // A leading 0 makes an integer octal, so 8 and 9 are errors.
                    if (!is_float && c == '0' && i - st > 1) {
                        for (int k = st; k < i; k++) {
                            if (s[(size_t)k] == '8' || s[(size_t)k] == '9') bad = true;
                        }
                    }
                }
                if (i < end && is_ident(s[(size_t)i])) {
                    i = scan_ident(s, i, end);
                    bad = true;
                }
                push(st, i, bad ? HighlightKind::Invalid : HighlightKind::Number);
                pos = i;
                continue;
            }
            if (is_ident_start(c)) {
                int e2 = scan_ident(s, i, end);
                pos = e2;
                ident(i, e2);
                continue;
            }
            if (is_bracket(c)) {
                push_bracket(out, i, c, br);
                if (c == '{') {
                    brace_depth++;
                    if (pending_enum) enum_body = brace_depth;
                    if (pending_struct) struct_body = brace_depth;
                    pending_enum = pending_struct = false;
                } else if (c == '}') {
                    if (brace_depth > 0) brace_depth--;
                    if (enum_body > brace_depth) enum_body = -1;
                    if (struct_body > brace_depth) struct_body = -1;
                }
                pos = i + 1;
                continue;
            }
            if (c == ';') pending_enum = pending_struct = false;
            int len = 0;
            if (operator_at(s, i, end, len)) {
                push(i, i + len, HighlightKind::Operator);
                pos = i + len;
                continue;
            }
            if (c == ',' || c == ';' || c == ':' || c == '.') {
                push(i, i + 1, HighlightKind::Punct);
                pos = i + 1;
                continue;
            }
            if (c == '@' || c == '$' || c == '`' || c == '\\') {
                push(i, i + 1, HighlightKind::Invalid);
            }
            pos = i + 1;
        }
    }
};

}  // namespace

static void hl_nexa(const std::string& s, int begin, int end, std::vector<HighlightSpan>& out) {
    std::unordered_set<std::string> types;
    collect_nexa_types(s, begin, end, types);
    NexaHL h(s, out, types);
    h.run(begin, end);
}

void highlight_nexa(const std::string& src, std::vector<HighlightSpan>& out) {
    out.clear();
    hl_nexa(src, 0, (int)src.size(), out);
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------
static bool is_nexapkg_key(const std::string& k) {
    static const char* keys[] = {
        "name", "version", "entry", "output", "dll", "dllOutput", "dependencies"
    };
    for (const char* key : keys) {
        if (k == key) return true;
    }
    return false;
}

static void hl_json(const std::string& s, int begin, int end, std::vector<HighlightSpan>& out,
                    bool nexa_manifest) {
    BracketState br;
    int i = begin;
    auto at = [&](int p) -> char { return (p >= begin && p < end) ? s[(size_t)p] : '\0'; };
    while (i < end) {
        char c = s[(size_t)i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
            continue;
        }
        if (c == '/' && at(i + 1) == '/') {
            int le = line_end(s, i, end);
            push_comment(out, s, i, le);
            i = le;
            continue;
        }
        if (c == '/' && at(i + 1) == '*') {
            auto p = s.find("*/", (size_t)i + 2);
            int ce = (p == std::string::npos || (int)p + 2 > end) ? end : (int)p + 2;
            push_comment(out, s, i, ce);
            i = ce;
            continue;
        }
        if (c == '"') {
            const int open = i;
            i++;
            std::string key;
            while (i < end && s[(size_t)i] != '"' && s[(size_t)i] != '\n') {
                if (s[(size_t)i] == '\\') {
                    i++;
                    if (i < end && s[(size_t)i] != '\n') i++;
                    continue;
                }
                key += s[(size_t)i];
                i++;
            }
            bool closed = i < end && s[(size_t)i] == '"';
            if (closed) i++;
            int close = i;
            int j = next_sig(s, i, end);
            if (closed && j < end && s[(size_t)j] == ':') {
                push_span(out, open, close, (nexa_manifest && is_nexapkg_key(key))
                                                ? HighlightKind::Keyword : HighlightKind::Property);
                continue;
            }
            int p = open;
            int k = open + 1;
            while (k < close) {
                if (s[(size_t)k] == '\\') {
                    push_span(out, p, k, HighlightKind::String);
                    int es = k;
                    k++;
                    bool ok = true;
                    if (k < close) {
                        char e = s[(size_t)k];
                        if (e == 'u') {
                            int d = 0;
                            k++;
                            while (d < 4 && k < close && is_hex(s[(size_t)k])) {
                                k++;
                                d++;
                            }
                            ok = d == 4;
                        } else {
                            ok = std::strchr("\"\\/bfnrt", e) != nullptr;
                            k++;
                        }
                    }
                    push_span(out, es, k, ok ? HighlightKind::Escape : HighlightKind::Invalid);
                    p = k;
                    continue;
                }
                k++;
            }
            push_span(out, p, close, HighlightKind::String);
            continue;
        }
        if (c == '-' || std::isdigit((unsigned char)c)) {
            int st = i;
            if (c == '-') i++;
            while (i < end && std::isdigit((unsigned char)s[(size_t)i])) i++;
            if (i < end && s[(size_t)i] == '.') {
                i++;
                while (i < end && std::isdigit((unsigned char)s[(size_t)i])) i++;
            }
            if (i < end && (s[(size_t)i] == 'e' || s[(size_t)i] == 'E')) {
                i++;
                if (i < end && (s[(size_t)i] == '+' || s[(size_t)i] == '-')) i++;
                while (i < end && std::isdigit((unsigned char)s[(size_t)i])) i++;
            }
            push_span(out, st, i, HighlightKind::Number);
            continue;
        }
        if (is_ident_start(c)) {
            int st = i;
            i = scan_ident(s, i, end);
            std::string id = s.substr((size_t)st, (size_t)(i - st));
            bool lit = id == "true" || id == "false" || id == "null";
            push_span(out, st, i, lit ? HighlightKind::Constant : HighlightKind::Invalid);
            continue;
        }
        if (is_bracket(c)) {
            push_bracket(out, i, c, br);
            i++;
            continue;
        }
        if (c == ',' || c == ':') {
            push_span(out, i, i + 1, HighlightKind::Punct);
            i++;
            continue;
        }
        push_span(out, i, i + 1, HighlightKind::Invalid);
        i++;
    }
}

void highlight_json(const std::string& src, std::vector<HighlightSpan>& out, bool nexa_manifest) {
    out.clear();
    hl_json(src, 0, (int)src.size(), out, nexa_manifest);
}

// ---------------------------------------------------------------------------
// C / C++ (also used for inline_cpp! bodies)
// ---------------------------------------------------------------------------
static const std::unordered_set<std::string> kCppControl = {
    "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue",
    "return", "goto", "try", "catch", "throw", "co_await", "co_return", "co_yield"
};

static const std::unordered_set<std::string> kCppKeyword = {
    "alignas", "alignof", "asm", "auto", "class", "concept", "const", "consteval", "constexpr",
    "constinit", "const_cast", "decltype", "delete", "dynamic_cast", "enum", "explicit",
    "export", "extern", "final", "friend", "inline", "mutable", "namespace", "new", "noexcept",
    "operator", "override", "private", "protected", "public", "register", "reinterpret_cast",
    "requires", "sizeof", "static", "static_assert", "static_cast", "struct", "template",
    "this", "thread_local", "typedef", "typeid", "typename", "union", "using", "virtual",
    "volatile", "and", "or", "not", "xor", "bitand", "bitor", "compl", "and_eq", "or_eq",
    "xor_eq", "not_eq"
};

static const std::unordered_set<std::string> kCppType = {
    "void", "bool", "char", "char8_t", "char16_t", "char32_t", "wchar_t", "short", "int",
    "long", "float", "double", "signed", "unsigned", "size_t", "ssize_t", "ptrdiff_t",
    "intptr_t", "uintptr_t", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t",
    "uint32_t", "uint64_t"
};

static const std::unordered_set<std::string> kCppConstant = {
    "true", "false", "nullptr", "NULL"
};

static void collect_cpp_types(const std::string& s, int begin, int end,
                              std::unordered_set<std::string>& types) {
    static const char* kw[] = {"struct", "class", "enum", "union"};
    int i = begin;
    while (i < end) {
        if (is_ident_start(s[(size_t)i]) && (i == begin || !is_ident(s[(size_t)i - 1]))) {
            int e = scan_ident(s, i, end);
            bool hit = false;
            for (const char* k : kw) {
                size_t len = std::strlen(k);
                if ((size_t)(e - i) == len && s.compare((size_t)i, len, k) == 0) hit = true;
            }
            if (hit) {
                int k = skip_blank(s, e, end);
                if (k + 5 <= end && s.compare((size_t)k, 5, "class") == 0) k = skip_blank(s, k + 5, end);
                int ke = scan_ident(s, k, end);
                if (ke > k && is_ident_start(s[(size_t)k])) types.insert(s.substr((size_t)k, (size_t)(ke - k)));
            }
            i = e;
            continue;
        }
        i++;
    }
}

static void hl_cpp(const std::string& s, int begin, int end, std::vector<HighlightSpan>& out) {
    std::unordered_set<std::string> types;
    collect_cpp_types(s, begin, end, types);
    BracketState br;
    auto at = [&](int p) -> char { return (p >= begin && p < end) ? s[(size_t)p] : '\0'; };
    int i = begin;

    auto quoted = [&](int st, int q_at, char q) -> int {
        // st: literal start (prefix included), q_at: the opening quote.
        int p = st;
        int k = q_at + 1;
        while (k < end && s[(size_t)k] != q && s[(size_t)k] != '\n') {
            if (s[(size_t)k] == '\\') {
                push_span(out, p, k, HighlightKind::String);
                int es = k;
                k++;
                if (k < end) {
                    char e = s[(size_t)k++];
                    if (e == 'x') while (k < end && is_hex(s[(size_t)k])) k++;
                    else if (e == 'u' || e == 'U') {
                        int want = e == 'u' ? 4 : 8;
                        while (want-- > 0 && k < end && is_hex(s[(size_t)k])) k++;
                    } else if (e >= '0' && e <= '7') {
                        int d = 1;
                        while (d < 3 && k < end && s[(size_t)k] >= '0' && s[(size_t)k] <= '7') {
                            k++;
                            d++;
                        }
                    }
                }
                push_span(out, es, k, HighlightKind::Escape);
                p = k;
                continue;
            }
            k++;
        }
        if (k < end && s[(size_t)k] == q) k++;
        push_span(out, p, k, HighlightKind::String);
        return k;
    };

    while (i < end) {
        char c = s[(size_t)i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
            continue;
        }
        if (c == '/' && at(i + 1) == '/') {
            int le = line_end(s, i, end);
            push_comment(out, s, i, le);
            i = le;
            continue;
        }
        if (c == '/' && at(i + 1) == '*') {
            auto p = s.find("*/", (size_t)i + 2);
            int ce = (p == std::string::npos || (int)p + 2 > end) ? end : (int)p + 2;
            push_comment(out, s, i, ce);
            i = ce;
            continue;
        }
        if (c == '#' && only_blank_before(s, i, begin)) {
            int k = skip_blank(s, i + 1, end);
            int we = scan_ident(s, k, end);
            std::string word = s.substr((size_t)k, (size_t)(we - k));
            push_span(out, i, we, HighlightKind::Preproc);
            i = we;
            if (word == "include" || word == "include_next" || word == "import") {
                int k2 = skip_blank(s, i, end);
                char q = at(k2);
                if (q == '<' || q == '"') {
                    char closer = q == '<' ? '>' : '"';
                    int le = line_end(s, k2, end);
                    int pe = k2 + 1;
                    while (pe < le && s[(size_t)pe] != closer) pe++;
                    if (pe < le) pe++;
                    push_span(out, k2, pe, HighlightKind::IncludePath);
                    i = pe;
                }
            } else if (word == "define" || word == "undef" || word == "ifdef" || word == "ifndef") {
                int k2 = skip_blank(s, i, end);
                int ne = scan_ident(s, k2, end);
                if (ne > k2) {
                    bool fn_like = at(ne) == '(';
                    push_span(out, k2, ne, fn_like ? HighlightKind::Function : HighlightKind::Constant);
                    i = ne;
                }
            }
            continue;
        }
        bool word_edge = i == begin || !is_ident(s[(size_t)i - 1]);
        if (c == '"') {
            i = quoted(i, i, '"');
            continue;
        }
        if (c == '\'') {
            i = quoted(i, i, '\'');
            continue;
        }
        if (std::isdigit((unsigned char)c) || (c == '.' && std::isdigit((unsigned char)at(i + 1)))) {
            int st = i;
            if (c == '0' && (at(i + 1) == 'x' || at(i + 1) == 'X')) {
                i += 2;
                while (i < end && (is_hex(s[(size_t)i]) || s[(size_t)i] == '\'')) i++;
            } else if (c == '0' && (at(i + 1) == 'b' || at(i + 1) == 'B')) {
                i += 2;
                while (i < end && (s[(size_t)i] == '0' || s[(size_t)i] == '1' || s[(size_t)i] == '\'')) i++;
            } else {
                while (i < end && (std::isdigit((unsigned char)s[(size_t)i]) || s[(size_t)i] == '\'' ||
                                   s[(size_t)i] == '.')) i++;
                if (i < end && (s[(size_t)i] == 'e' || s[(size_t)i] == 'E')) {
                    i++;
                    if (i < end && (s[(size_t)i] == '+' || s[(size_t)i] == '-')) i++;
                    while (i < end && std::isdigit((unsigned char)s[(size_t)i])) i++;
                }
            }
            while (i < end && std::strchr("uUlLfFzZ", s[(size_t)i]) && s[(size_t)i] != '\0') i++;
            push_span(out, st, i, HighlightKind::Number);
            continue;
        }
        if (is_ident_start(c) && word_edge) {
            int st = i;
            int e = scan_ident(s, i, end);
            std::string id = s.substr((size_t)st, (size_t)(e - st));
            char after = at(e);
            // String / character literal prefixes: L"", u8"", R"()", u8R"()" ...
            if ((after == '"' || after == '\'') &&
                (id == "L" || id == "u" || id == "U" || id == "u8" || id == "R" || id == "LR" ||
                 id == "uR" || id == "UR" || id == "u8R")) {
                if (id.back() == 'R' && after == '"') {
                    int ds = e + 1;
                    int k = ds;
                    while (k < end && s[(size_t)k] != '(' && s[(size_t)k] != '\n') k++;
                    std::string close = ")" + s.substr((size_t)ds, (size_t)(k - ds)) + "\"";
                    auto p = s.find(close, (size_t)k);
                    int re = (p == std::string::npos || (int)(p + close.size()) > end) ? end
                                                                                      : (int)(p + close.size());
                    push_span(out, st, re, HighlightKind::String);
                    i = re;
                } else {
                    i = quoted(st, e, after);
                }
                continue;
            }
            i = e;
            int j = skip_blank(s, e, end);
            bool call = at(j) == '(';
            bool member = (st > begin && s[(size_t)st - 1] == '.') ||
                          (st > begin + 1 && s[(size_t)st - 1] == '>' && s[(size_t)st - 2] == '-');
            bool scope_next = at(j) == ':' && at(j + 1) == ':';
            HighlightKind k = HighlightKind::Identifier;
            if (member) k = call ? HighlightKind::Member : HighlightKind::Property;
            else if (kCppControl.count(id)) k = HighlightKind::Control;
            else if (kCppKeyword.count(id)) k = HighlightKind::Keyword;
            else if (kCppType.count(id)) k = HighlightKind::Type;
            else if (kCppConstant.count(id)) k = HighlightKind::Constant;
            else if (id == "std" || (scope_next && std::islower((unsigned char)id[0]))) k = HighlightKind::Module;
            else if (types.count(id) || scope_next) k = HighlightKind::TypeName;
            else if (call) k = HighlightKind::Function;
            else if (id.size() > 1) {
                bool caps = true;
                for (char ch : id) {
                    if (std::islower((unsigned char)ch)) {
                        caps = false;
                        break;
                    }
                }
                if (caps) k = HighlightKind::Constant;
            }
            push_span(out, st, e, k);
            continue;
        }
        if (is_ident(c)) {
            i = scan_ident(s, i, end);
            continue;
        }
        if (is_bracket(c)) {
            push_bracket(out, i, c, br);
            i++;
            continue;
        }
        int len = 0;
        if (operator_at(s, i, end, len)) {
            push_span(out, i, i + len, HighlightKind::Operator);
            i += len;
            continue;
        }
        if (c == ',' || c == ';' || c == ':' || c == '.') {
            push_span(out, i, i + 1, HighlightKind::Punct);
        }
        i++;
    }
}

void highlight_cpp(const std::string& src, std::vector<HighlightSpan>& out) {
    out.clear();
    hl_cpp(src, 0, (int)src.size(), out);
}

// ---------------------------------------------------------------------------
// Shell scripts, batch files, PowerShell and Makefiles
// ---------------------------------------------------------------------------
static void hl_shell(const std::string& s, int begin, int end, std::vector<HighlightSpan>& out,
                     const std::string& path) {
    static const std::unordered_set<std::string> kw = {
        "if", "then", "else", "elif", "fi", "for", "in", "do", "done", "while", "until", "case",
        "esac", "function", "return", "exit", "local", "export", "echo", "set", "call", "goto",
        "shift", "setlocal", "endlocal", "not", "exist", "defined", "errorlevel", "equ", "neq",
        "lss", "leq", "gtr", "geq", "ifeq", "ifneq", "ifdef", "ifndef", "endif", "include",
        "define", "endef", "foreach", "param", "try", "catch", "finally", "throw", "switch",
        "begin", "process", "end", "elseif", "cd", "pushd", "popd", "source", "unset", "readonly"
    };
    std::string ext = path_ext(path);
    for (char& ch : ext) ch = (char)std::tolower((unsigned char)ch);
    std::string fname = path_filename(path);
    for (char& ch : fname) ch = (char)std::tolower((unsigned char)ch);
    const bool bat = ext == ".bat" || ext == ".cmd";
    const bool make = fname == "makefile" || fname == "gnumakefile" || ext == ".mk";
    BracketState br;

    int i = begin;
    while (i < end) {
        int ls = i;
        int le = line_end(s, i, end);
        int t = skip_blank(s, ls, le);
        // Whole-line comments and labels.
        if (bat) {
            bool rem = t + 3 <= le && (s.compare((size_t)t, 3, "rem") == 0 || s.compare((size_t)t, 3, "REM") == 0 ||
                                       s.compare((size_t)t, 3, "Rem") == 0) &&
                       (t + 3 == le || s[(size_t)t + 3] == ' ' || s[(size_t)t + 3] == '\t');
            if (rem || (t + 1 < le && s[(size_t)t] == ':' && s[(size_t)t + 1] == ':')) {
                push_comment(out, s, t, le);
                i = le + 1;
                continue;
            }
            if (t < le && s[(size_t)t] == ':') {
                push_span(out, t, le, HighlightKind::Function);
                i = le + 1;
                continue;
            }
            if (t < le && s[(size_t)t] == '@') {
                push_span(out, t, t + 1, HighlightKind::Operator);
                t++;
            }
        }
        if (make && t == ls && t < le && s[(size_t)t] != '\t') {
            // target: prerequisites
            int k = t;
            while (k < le && s[(size_t)k] != ':' && s[(size_t)k] != '=' && s[(size_t)k] != '#') k++;
            if (k < le && s[(size_t)k] == ':' && (k + 1 >= le || s[(size_t)k + 1] != '=')) {
                push_span(out, t, k, s[(size_t)t] == '.' ? HighlightKind::Keyword : HighlightKind::Function);
                push_span(out, k, k + 1, HighlightKind::Punct);
                t = k + 1;
            } else if (k < le && (s[(size_t)k] == '=' || (s[(size_t)k] == ':' && s[(size_t)k + 1] == '='))) {
                int ke = k;
                while (ke > t && (s[(size_t)ke - 1] == ' ' || s[(size_t)ke - 1] == '?' ||
                                  s[(size_t)ke - 1] == '+' || s[(size_t)ke - 1] == '\t')) ke--;
                if (ke > t) push_span(out, t, ke, HighlightKind::Property);
                t = ke;
            }
        }
        int k = t;
        while (k < le) {
            char c = s[(size_t)k];
            if (c == '#' && !bat && (k == ls || s[(size_t)k - 1] == ' ' || s[(size_t)k - 1] == '\t')) {
                push_comment(out, s, k, le);
                k = le;
                break;
            }
            if (c == '"' || (c == '\'' && !bat)) {
                int st = k;
                k++;
                while (k < le && s[(size_t)k] != c) {
                    if (s[(size_t)k] == '\\' && !bat && c == '"') k++;
                    k++;
                }
                k = std::min(le, k + 1);
                push_span(out, st, k, HighlightKind::String);
                continue;
            }
            if (c == '$' && k + 1 < le) {
                int st = k;
                char o = s[(size_t)k + 1];
                if (o == '(' || o == '{') {
                    char cl = o == '(' ? ')' : '}';
                    int depth = 0;
                    k++;
                    while (k < le) {
                        if (s[(size_t)k] == o) depth++;
                        else if (s[(size_t)k] == cl && --depth == 0) break;
                        k++;
                    }
                    k = std::min(le, k + 1);
                } else {
                    k++;
                    if (k < le && (is_ident(s[(size_t)k]) || std::strchr("@<^?*#$!", s[(size_t)k]))) {
                        if (is_ident(s[(size_t)k])) k = scan_ident(s, k, le);
                        else k++;
                    }
                }
                push_span(out, st, k, HighlightKind::Property);
                continue;
            }
            if (c == '%' && bat) {
                int st = k;
                k++;
                if (k < le && s[(size_t)k] == '%') {
                    k++;
                    k = scan_ident(s, k, le);
                } else if (k < le && s[(size_t)k] == '~') {
                    while (k < le && (std::isalpha((unsigned char)s[(size_t)k]) || s[(size_t)k] == '~')) k++;
                    if (k < le && std::isdigit((unsigned char)s[(size_t)k])) k++;
                } else if (k < le && std::isdigit((unsigned char)s[(size_t)k])) {
                    k++;
                } else {
                    int ve = k;
                    while (ve < le && s[(size_t)ve] != '%' && s[(size_t)ve] != ' ') ve++;
                    if (ve < le && s[(size_t)ve] == '%') k = ve + 1;
                }
                push_span(out, st, k, HighlightKind::Property);
                continue;
            }
            if (std::isdigit((unsigned char)c) && (k == ls || !is_ident(s[(size_t)k - 1]))) {
                int st = k;
                while (k < le && (is_ident(s[(size_t)k]) || s[(size_t)k] == '.')) k++;
                push_span(out, st, k, HighlightKind::Number);
                continue;
            }
            if (is_ident_start(c) && (k == ls || (!is_ident(s[(size_t)k - 1]) && s[(size_t)k - 1] != '-'))) {
                int st = k;
                k = scan_ident(s, k, le);
                std::string w = s.substr((size_t)st, (size_t)(k - st));
                std::string lw = w;
                for (char& ch : lw) ch = (char)std::tolower((unsigned char)ch);
                if (kw.count(bat || ext == ".ps1" || ext == ".psm1" ? lw : w)) {
                    push_span(out, st, k, HighlightKind::Keyword);
                } else if (k < le && s[(size_t)k] == '-' && ext.rfind(".ps", 0) == 0 &&
                           std::isupper((unsigned char)w[0])) {
                    // PowerShell Verb-Noun cmdlets.
                    int ce = k + 1;
                    ce = scan_ident(s, ce, le);
                    push_span(out, st, ce, HighlightKind::Function);
                    k = ce;
                }
                continue;
            }
            if (is_bracket(c)) {
                push_bracket(out, k, c, br);
                k++;
                continue;
            }
            if (std::strchr("|&;<>=!", c) && c != '\0') {
                push_span(out, k, k + 1, HighlightKind::Operator);
            }
            k++;
        }
        i = le + 1;
    }
}

void highlight_shell(const std::string& src, const std::string& path, std::vector<HighlightSpan>& out) {
    out.clear();
    hl_shell(src, 0, (int)src.size(), out, path);
}

// ---------------------------------------------------------------------------
// INI / TOML / YAML style config
// ---------------------------------------------------------------------------
static void config_value(const std::string& s, int i, int le, std::vector<HighlightSpan>& out) {
    auto prev = [&](int p) -> char { return p > 0 ? s[(size_t)p - 1] : ' '; };
    while (i < le) {
        char c = s[(size_t)i];
        if ((c == '#' || c == ';') && (prev(i) == ' ' || prev(i) == '\t')) {
            push_comment(out, s, i, le);
            return;
        }
        if (c == '"' || c == '\'') {
            int st = i;
            i++;
            while (i < le && s[(size_t)i] != c) {
                if (s[(size_t)i] == '\\' && c == '"') {
                    push_span(out, st, i, HighlightKind::String);
                    push_span(out, i, std::min(le, i + 2), HighlightKind::Escape);
                    i += 2;
                    st = i;
                    continue;
                }
                i++;
            }
            i = std::min(le, i + 1);
            push_span(out, st, i, HighlightKind::String);
            continue;
        }
        if ((std::isdigit((unsigned char)c) || ((c == '-' || c == '+') && i + 1 < le &&
                                                std::isdigit((unsigned char)s[(size_t)i + 1]))) &&
            !is_ident(prev(i))) {
            int st = i++;
            while (i < le && (std::isalnum((unsigned char)s[(size_t)i]) || s[(size_t)i] == '.' ||
                              s[(size_t)i] == '_' || s[(size_t)i] == ':' || s[(size_t)i] == '-')) i++;
            push_span(out, st, i, HighlightKind::Number);
            continue;
        }
        if (is_ident_start(c) && !is_ident(prev(i))) {
            int st = i;
            i = scan_ident(s, i, le);
            std::string w = s.substr((size_t)st, (size_t)(i - st));
            for (char& ch : w) ch = (char)std::tolower((unsigned char)ch);
            if (w == "true" || w == "false" || w == "yes" || w == "no" || w == "on" || w == "off" ||
                w == "null" || w == "none")
                push_span(out, st, i, HighlightKind::Constant);
            continue;
        }
        if (c == '[' || c == ']' || c == '{' || c == '}' || c == ',') {
            push_span(out, i, i + 1, HighlightKind::Punct);
        }
        i++;
    }
}

static void hl_ini(const std::string& s, int begin, int end, std::vector<HighlightSpan>& out) {
    int i = begin;
    while (i < end) {
        int ls = i;
        int le = line_end(s, i, end);
        int t = skip_blank(s, ls, le);
        i = le + 1;
        if (t >= le) continue;
        char c = s[(size_t)t];
        if (c == '#' || c == ';') {
            push_comment(out, s, t, le);
            continue;
        }
        if (c == '[') {
            int e = le;
            while (e > t && (s[(size_t)e - 1] == ' ' || s[(size_t)e - 1] == '\t' || s[(size_t)e - 1] == '\r')) e--;
            if (e > t && s[(size_t)e - 1] == ']') {
                push_span(out, t, e, HighlightKind::TypeName);
                continue;
            }
        }
        if (t + 3 <= le && (s.compare((size_t)t, 3, "---") == 0 || s.compare((size_t)t, 3, "...") == 0)) {
            push_span(out, t, le, HighlightKind::Punct);
            continue;
        }
        if (c == '-' && t + 1 < le && (s[(size_t)t + 1] == ' ')) {
            push_span(out, t, t + 1, HighlightKind::Keyword);
            t = skip_blank(s, t + 1, le);
        }
        // key = value / key: value
        int k = t;
        char q = s[(size_t)k];
        if (q == '"' || q == '\'') {
            k++;
            while (k < le && s[(size_t)k] != q) k++;
            if (k < le) k++;
        } else {
            while (k < le && s[(size_t)k] != '=' && s[(size_t)k] != ':' && s[(size_t)k] != ' ' &&
                   s[(size_t)k] != '\t') k++;
        }
        int sep = skip_blank(s, k, le);
        if (sep < le && (s[(size_t)sep] == '=' || (s[(size_t)sep] == ':' &&
                                                   (sep + 1 >= le || s[(size_t)sep + 1] == ' ' ||
                                                    s[(size_t)sep + 1] == '\t')))) {
            push_span(out, t, k, HighlightKind::Property);
            push_span(out, sep, sep + 1, HighlightKind::Punct);
            config_value(s, sep + 1, le, out);
        } else {
            config_value(s, t, le, out);
        }
    }
}

void highlight_ini(const std::string& src, std::vector<HighlightSpan>& out) {
    out.clear();
    hl_ini(src, 0, (int)src.size(), out);
}

// ---------------------------------------------------------------------------
// Markdown (fenced code blocks are highlighted in their own language)
// ---------------------------------------------------------------------------
static void md_code(const std::string& s, int b, int e, const std::string& lang,
                    std::vector<HighlightSpan>& out) {
    if (e <= b) return;
    if (lang == "nexa" || lang == "nxa") hl_nexa(s, b, e, out);
    else if (lang == "c" || lang == "cpp" || lang == "c++" || lang == "h" || lang == "hpp" || lang == "cc")
        hl_cpp(s, b, e, out);
    else if (lang == "json" || lang == "jsonc") hl_json(s, b, e, out, false);
    else if (lang == "sh" || lang == "bash" || lang == "shell" || lang == "console") hl_shell(s, b, e, out, "x.sh");
    else if (lang == "bat" || lang == "cmd" || lang == "batch") hl_shell(s, b, e, out, "x.bat");
    else if (lang == "ps1" || lang == "powershell" || lang == "pwsh") hl_shell(s, b, e, out, "x.ps1");
    else if (lang == "make" || lang == "makefile") hl_shell(s, b, e, out, "Makefile");
    else if (lang == "ini" || lang == "toml" || lang == "yaml" || lang == "yml") hl_ini(s, b, e, out);
    else push_span(out, b, e, HighlightKind::String);
}

static void md_inline(const std::string& s, int i, int end, std::vector<HighlightSpan>& out) {
    auto find_on_line = [&](const char* needle, int from) -> int {
        auto p = s.find(needle, (size_t)from);
        return (p == std::string::npos || (int)p >= end) ? -1 : (int)p;
    };
    while (i < end) {
        char c = s[(size_t)i];
        bool edge = i == 0 || !std::isalnum((unsigned char)s[(size_t)i - 1]);
        if (c == '\\' && i + 1 < end && std::ispunct((unsigned char)s[(size_t)i + 1])) {
            push_span(out, i, i + 2, HighlightKind::Escape);
            i += 2;
            continue;
        }
        if (c == '`') {
            int k = i;
            while (k < end && s[(size_t)k] == '`') k++;
            std::string ticks = s.substr((size_t)i, (size_t)(k - i));
            int close = find_on_line(ticks.c_str(), k);
            if (close >= 0) {
                push_span(out, i, close + (int)ticks.size(), HighlightKind::String);
                i = close + (int)ticks.size();
            } else {
                i = k;
            }
            continue;
        }
        if ((c == '*' || c == '_') && (c == '*' || edge)) {
            int k = i;
            while (k < end && s[(size_t)k] == c) k++;
            int run = k - i;
            if (run >= 2) {
                char pair[3] = {c, c, 0};
                int close = find_on_line(pair, k);
                if (close > k) {
                    push_span(out, i, close + 2, HighlightKind::Strong);
                    i = close + 2;
                    continue;
                }
            } else if (k < end && s[(size_t)k] != ' ') {
                int close = k;
                while (close < end && !(s[(size_t)close] == c && s[(size_t)close - 1] != ' ' &&
                                        (close + 1 >= end || s[(size_t)close + 1] != c))) close++;
                if (close < end && close > k) {
                    push_span(out, i, close + 1, HighlightKind::Emphasis);
                    i = close + 1;
                    continue;
                }
            }
            i = k;
            continue;
        }
        if (c == '[' || (c == '!' && i + 1 < end && s[(size_t)i + 1] == '[')) {
            int open = c == '!' ? i + 1 : i;
            int close = find_on_line("]", open + 1);
            if (close > 0 && close + 1 < end && s[(size_t)close + 1] == '(') {
                int pe = find_on_line(")", close + 2);
                if (pe > 0) {
                    push_span(out, i, close + 1, HighlightKind::Link);
                    push_span(out, close + 1, pe + 1, HighlightKind::IncludePath);
                    i = pe + 1;
                    continue;
                }
            }
            i++;
            continue;
        }
        if (c == '<' && i + 1 < end) {
            char n1 = s[(size_t)i + 1];
            if (n1 == '!' && i + 3 < end && s.compare((size_t)i, 4, "<!--") == 0) {
                auto p = s.find("-->", (size_t)i + 4);
                int ce = p == std::string::npos ? end : std::min(end, (int)p + 3);
                push_comment(out, s, i, ce);
                i = ce;
                continue;
            }
            if (std::isalpha((unsigned char)n1) || n1 == '/') {
                int close = find_on_line(">", i + 1);
                if (close > 0) {
                    bool url = s.compare((size_t)i + 1, 4, "http") == 0;
                    push_span(out, i, close + 1, url ? HighlightKind::Link : HighlightKind::Keyword);
                    i = close + 1;
                    continue;
                }
            }
        }
        if ((c == 'h') && edge && (s.compare((size_t)i, 7, "http://") == 0 || s.compare((size_t)i, 8, "https://") == 0)) {
            int k = i;
            while (k < end && s[(size_t)k] != ' ' && s[(size_t)k] != '\t' && s[(size_t)k] != ')' &&
                   s[(size_t)k] != '>') k++;
            push_span(out, i, k, HighlightKind::Link);
            i = k;
            continue;
        }
        if (c == '|') push_span(out, i, i + 1, HighlightKind::Punct);
        i++;
    }
}

void highlight_markdown(const std::string& s, std::vector<HighlightSpan>& out) {
    out.clear();
    const int n = (int)s.size();
    int i = 0;
    bool fence = false;
    char fence_ch = '`';
    int fence_len = 0;
    std::string fence_lang;
    int code_start = 0;
    while (i < n) {
        int ls = i;
        int le = line_end(s, i, n);
        int nx = le < n ? le + 1 : n;
        int t = skip_blank(s, ls, le);
        char c = t < le ? s[(size_t)t] : '\0';
        if (fence) {
            int k = t;
            while (k < le && s[(size_t)k] == fence_ch) k++;
            if (k - t >= fence_len && skip_blank(s, k, le) == le) {
                md_code(s, code_start, ls, fence_lang, out);
                push_span(out, t, le, HighlightKind::Comment);
                fence = false;
            }
            i = nx;
            continue;
        }
        if (c == '`' || c == '~') {
            int k = t;
            while (k < le && s[(size_t)k] == c) k++;
            if (k - t >= 3) {
                fence = true;
                fence_ch = c;
                fence_len = k - t;
                int ls2 = skip_blank(s, k, le);
                int le2 = ls2;
                while (le2 < le && s[(size_t)le2] != ' ' && s[(size_t)le2] != '{') le2++;
                fence_lang = s.substr((size_t)ls2, (size_t)(le2 - ls2));
                for (char& ch : fence_lang) ch = (char)std::tolower((unsigned char)ch);
                push_span(out, t, k, HighlightKind::Comment);
                push_span(out, ls2, le, HighlightKind::Keyword);
                code_start = nx;
                i = nx;
                continue;
            }
        }
        if (c == '#') {
            int k = t;
            while (k < le && s[(size_t)k] == '#') k++;
            if (k - t <= 6 && (k == le || s[(size_t)k] == ' ')) {
                push_span(out, t, le, HighlightKind::Heading);
                i = nx;
                continue;
            }
        }
        if (c == '>') {
            push_span(out, t, t + 1, HighlightKind::Keyword);
            push_span(out, t + 1, le, HighlightKind::Comment);
            i = nx;
            continue;
        }
        if (c == '-' || c == '*' || c == '_' || c == '=') {
            int count = 0;
            bool only = true;
            for (int k = t; k < le; k++) {
                if (s[(size_t)k] == c) count++;
                else if (s[(size_t)k] != ' ' && s[(size_t)k] != '\t') {
                    only = false;
                    break;
                }
            }
            if (only && count >= 3) {
                push_span(out, t, le, HighlightKind::Punct);
                i = nx;
                continue;
            }
        }
        if ((c == '-' || c == '*' || c == '+') && t + 1 < le && s[(size_t)t + 1] == ' ') {
            push_span(out, t, t + 1, HighlightKind::Keyword);
            t += 2;
            if (t + 2 < le && s[(size_t)t] == '[' && s[(size_t)t + 2] == ']') {
                push_span(out, t, t + 3, HighlightKind::Keyword);
                t += 3;
            }
        } else if (std::isdigit((unsigned char)c)) {
            int k = t;
            while (k < le && std::isdigit((unsigned char)s[(size_t)k])) k++;
            if (k + 1 < le && (s[(size_t)k] == '.' || s[(size_t)k] == ')') && s[(size_t)k + 1] == ' ') {
                push_span(out, t, k + 1, HighlightKind::Keyword);
                t = k + 2;
            }
        }
        md_inline(s, t, le, out);
        i = nx;
    }
    if (fence) md_code(s, code_start, n, fence_lang, out);
}

void highlight_source(Lang lang, const std::string& path, const std::string& src,
                      std::vector<HighlightSpan>& out) {
    switch (lang) {
    case Lang::Nexa: highlight_nexa(src, out); break;
    case Lang::Json: highlight_json(src, out, path_filename(path) == "nexapkg.json"); break;
    case Lang::Cpp: highlight_cpp(src, out); break;
    case Lang::Markdown: highlight_markdown(src, out); break;
    case Lang::Ini: highlight_ini(src, out); break;
    case Lang::Shell: highlight_shell(src, path, out); break;
    default: out.clear(); break;
    }
}

// ---------------------------------------------------------------------------
// Outline, diagnostics, completion
// ---------------------------------------------------------------------------
void index_nexa(const std::string& src, std::vector<IndexedDef>& out) {
    out.clear();
    static const std::regex fn_re(R"(\b(?:extern\s+)?fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\()");
    static const std::regex struct_re(R"(\bstruct\s+([A-Za-z_][A-Za-z0-9_]*))");
    static const std::regex enum_re(R"(\benum\s+([A-Za-z_][A-Za-z0-9_]*))");
    static const std::regex let_re(R"(^\s*let\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:[:=;]|$))",
                                   std::regex::multiline);

    std::vector<int> starts{0};
    for (int i = 0; i < (int)src.size(); i++) {
        if (src[(size_t)i] == '\n') starts.push_back(i + 1);
    }
    auto line_of = [&](int pos) {
        return (int)(std::upper_bound(starts.begin(), starts.end(), pos) - starts.begin()) - 1;
    };

    auto add = [&](const std::regex& re, DefKind kind) {
        std::sregex_iterator it(src.begin(), src.end(), re);
        std::sregex_iterator end;
        for (; it != end; ++it) {
            IndexedDef d;
            d.name = (*it)[1].str();
            d.kind = kind;
            d.name_start = (int)it->position(1);
            d.name_end = d.name_start + (int)d.name.size();
            d.line = line_of(d.name_start);
            out.push_back(std::move(d));
        }
    };
    add(fn_re, DefKind::Function);
    add(struct_re, DefKind::Struct);
    add(enum_re, DefKind::Enum);
    add(let_re, DefKind::Variable);
    std::sort(out.begin(), out.end(), [](const IndexedDef& a, const IndexedDef& b) {
        return a.name_start < b.name_start;
    });
}

void diagnose_nexa(const std::string& src, const std::string& path, std::vector<Diagnostic>& out) {
    out.clear();
    try {
        nexa::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        nexa::Modules modules;
        nexa::Parser parser(std::move(tokens), modules, path);
        parser.parse();
    } catch (const std::exception& e) {
        Diagnostic d;
        d.path = path;
        d.message = e.what();
        d.line = 1;
        std::string msg = d.message;
        auto sep = msg.find(": ");
        if (sep != std::string::npos) {
            std::string prefix = msg.substr(0, sep);
            if (prefix.find(".nxa") != std::string::npos ||
                prefix.find('\\') != std::string::npos ||
                prefix.find('/') != std::string::npos) {
                d.path = prefix;
                d.message = msg.substr(sep + 2);
                msg = d.message;
            }
        }
        auto pos = msg.rfind("line ");
        if (pos != std::string::npos) {
            d.line = std::max(1, std::atoi(msg.c_str() + pos + 5));
        }
        out.push_back(std::move(d));
    }
}

static const char* kKeywords[] = {
    "fn", "extern", "let", "const", "struct", "enum", "if", "else", "while", "for",
    "switch", "case", "default", "return", "break", "continue", "goto", "try", "catch",
    "throw", "new", "delete", "sizeof", "true", "false", "null", "in", "self", "ok", "err",
    "and", "or", "not", "inline_cpp"
};

static const char* kTypes[] = {
    "int", "unsigned int", "unsigned char", "short", "unsigned short", "long",
    "unsigned long", "size_t", "string", "bool", "float", "char", "void",
    "*int", "*char", "*void", "*size_t", "[]int", "[]string", "[]float",
    "map", "fn", "Json", "json", "Result", "result",
    "HttpResponse", "HttpServer", "HttpRequest"
};

struct ModuleMember {
    const char* mod;
    const char* name;
};

static const ModuleMember kMembers[] = {
    {"io", "print"}, {"io", "println"}, {"io", "flush"}, {"io", "readln"},
    {"io", "read_int"}, {"io", "to_int"}, {"io", "getline"}, {"io", "trim"},

    {"os", "system"}, {"os", "spawn"}, {"os", "spawn_wait"}, {"os", "spawn_at"},
    {"os", "wait"}, {"os", "kill"}, {"os", "platform"}, {"os", "arch"},
    {"os", "cpu_count"}, {"os", "getenv"}, {"os", "setenv"}, {"os", "unsetenv"},
    {"os", "hostname"}, {"os", "username"}, {"os", "user"}, {"os", "home"},
    {"os", "tempdir"}, {"os", "cwd"}, {"os", "chdir"}, {"os", "executable"},
    {"os", "which"}, {"os", "total_mem"}, {"os", "avail_mem"}, {"os", "page_size"},
    {"os", "uptime"}, {"os", "shell"}, {"os", "newline"}, {"os", "path_sep"},
    {"os", "lang"}, {"os", "isatty"}, {"os", "environ"}, {"os", "env"},
    {"os", "config_dir"}, {"os", "cache_dir"}, {"os", "desktop"}, {"os", "endian"},
    {"os", "exit"}, {"os", "getpid"}, {"os", "getprocessid"}, {"os", "exe_dir"},
    {"os", "clip_get"}, {"os", "clip_set"}, {"os", "notify"}, {"os", "open"},
    {"os", "load"}, {"os", "save"}, {"os", "play"}, {"os", "lock"},
    {"os", "shutdown"}, {"os", "reboot"}, {"os", "suspend"}, {"os", "logout"},
    {"os", "set_volume"}, {"os", "get_volume"}, {"os", "mute"}, {"os", "unmute"},
    {"os", "toggle_mute"}, {"os", "set_brightness"}, {"os", "get_brightness"},
    {"os", "type"}, {"os", "hideconsolewindow"}, {"os", "showconsolewindow"},
    {"os", "minimizeconsolewindow"}, {"os", "maximizeconsolewindow"},
    {"os", "messagebox"}, {"os", "grepkeys"}, {"os", "getkey"}, {"os", "keypressed"},

    {"file", "read"}, {"file", "write"}, {"file", "append"}, {"file", "exists"},
    {"file", "mkdir"}, {"file", "remove"}, {"file", "delete"}, {"file", "remove_all"},
    {"file", "rename"}, {"file", "move"}, {"file", "copy"}, {"file", "list"},
    {"file", "listdir"}, {"file", "isdir"}, {"file", "isfile"}, {"file", "size"},
    {"file", "cwd"}, {"file", "chdir"}, {"file", "abspath"}, {"file", "join"},
    {"file", "dirname"}, {"file", "parent"}, {"file", "basename"}, {"file", "name"},
    {"file", "extension"},

    {"random", "int"}, {"random", "seed"},

    {"math", "abs"}, {"math", "min"}, {"math", "max"}, {"math", "pow"}, {"math", "sqrt"},
    {"math", "floor"}, {"math", "ceil"}, {"math", "round"}, {"math", "sin"}, {"math", "cos"},
    {"math", "tan"}, {"math", "log"}, {"math", "log10"}, {"math", "exp"},
    {"math", "pi"}, {"math", "e"},

    {"crypto", "xor"}, {"crypto", "sha256"}, {"crypto", "sha1"}, {"crypto", "hmac_sha256"},
    {"crypto", "hex_encode"}, {"crypto", "hex_decode"},
    {"crypto", "base64_encode"}, {"crypto", "base64_decode"}, {"crypto", "random_bytes"},

    {"http", "get"}, {"http", "post"}, {"http", "put"}, {"http", "patch"}, {"http", "delete"},
    {"http", "request"}, {"http", "localhost"}, {"http", "accept"}, {"http", "reply"},
    {"http", "raw"}, {"http", "close"},

    {"tcp", "connect"}, {"tcp", "listen"}, {"tcp", "accept"}, {"tcp", "send"},
    {"tcp", "recv"}, {"tcp", "port"}, {"tcp", "close"},

    {"udp", "open"}, {"udp", "port"}, {"udp", "send"}, {"udp", "recv"},
    {"udp", "sender"}, {"udp", "sender_port"}, {"udp", "close"},

    {"json", "parse"}, {"json", "stringify"}, {"json", "of"}, {"json", "null"},
    {"json", "bool"}, {"json", "int"}, {"json", "float"}, {"json", "string"},
    {"json", "array"}, {"json", "object"},

    {"time", "sleep"}, {"time", "seconds"}, {"time", "milliseconds"}, {"time", "now_ms"},

    {"thread", "spawn"}, {"thread", "join"}, {"thread", "worker"}, {"thread", "run"},
    {"thread", "worker_join"},

    {"dll", "load"}, {"dll", "call"},

    {"gfx", "open"}, {"gfx", "resize"}, {"gfx", "width"}, {"gfx", "height"}, {"gfx", "scale"},
    {"gfx", "title"}, {"gfx", "close"}, {"gfx", "poll"}, {"gfx", "closed"},
    {"gfx", "clear"}, {"gfx", "plot"}, {"gfx", "get"}, {"gfx", "fill"},
    {"gfx", "rect"}, {"gfx", "round_rect"}, {"gfx", "fill_round_rect"},
    {"gfx", "line"}, {"gfx", "circle"}, {"gfx", "fill_circle"},
    {"gfx", "ellipse"}, {"gfx", "fill_ellipse"}, {"gfx", "arc"}, {"gfx", "pie"},
    {"gfx", "tri"}, {"gfx", "fill_tri"}, {"gfx", "poly"}, {"gfx", "fill_poly"},
    {"gfx", "alpha"}, {"gfx", "save"}, {"gfx", "text"}, {"gfx", "text_size"},
    {"gfx", "text_width"}, {"gfx", "text_height"}, {"gfx", "present"}, {"gfx", "maxfps"},
    {"gfx", "fullscreen"}, {"gfx", "borderless"}, {"gfx", "ontop"}, {"gfx", "transparent"},
    {"gfx", "audio"}, {"gfx", "sample"}, {"gfx", "audio_queued"}, {"gfx", "audio_flush"},
    {"gfx", "sound"}, {"gfx", "play"}, {"gfx", "loop"}, {"gfx", "stop"}, {"gfx", "volume"},
    {"gfx", "image"}, {"gfx", "decode"}, {"gfx", "image_w"}, {"gfx", "image_h"},
    {"gfx", "blit"}, {"gfx", "blit_rot"}, {"gfx", "icon"}, {"gfx", "cursor"},
    {"gfx", "key"}, {"gfx", "pressed"}, {"gfx", "released"},
    {"gfx", "wheel"}, {"gfx", "wheel_x"}, {"gfx", "typed"},
    {"gfx", "mouse_x"}, {"gfx", "mouse_y"}, {"gfx", "mouse"},
    {"gfx", "drop"}, {"gfx", "opendialog"}, {"gfx", "openfile"},

    {"gfx3d", "open"}, {"gfx3d", "close"}, {"gfx3d", "closed"}, {"gfx3d", "poll"},
    {"gfx3d", "present"}, {"gfx3d", "clear"}, {"gfx3d", "width"}, {"gfx3d", "height"},
    {"gfx3d", "maxfps"}, {"gfx3d", "backend"}, {"gfx3d", "renderer"}, {"gfx3d", "camera"},
    {"gfx3d", "perspective"}, {"gfx3d", "ambient"}, {"gfx3d", "light"}, {"gfx3d", "reset"},
    {"gfx3d", "translate"}, {"gfx3d", "rotate"}, {"gfx3d", "scale"}, {"gfx3d", "draw"},
    {"gfx3d", "cube"}, {"gfx3d", "box"}, {"gfx3d", "sphere"}, {"gfx3d", "cylinder"},
    {"gfx3d", "cone"}, {"gfx3d", "capsule"}, {"gfx3d", "teapot"}, {"gfx3d", "grid"},
    {"gfx3d", "tri"}, {"gfx3d", "line3"}, {"gfx3d", "model"}, {"gfx3d", "model_tris"},
    {"gfx3d", "get"}, {"gfx3d", "save"}, {"gfx3d", "key"}, {"gfx3d", "pressed"},
    {"gfx3d", "released"}, {"gfx3d", "typed"}, {"gfx3d", "mouse"}, {"gfx3d", "mouse_x"},
    {"gfx3d", "mouse_y"}, {"gfx3d", "wheel"}, {"gfx3d", "wheel_x"},
    {"gfx3d", "audio"}, {"gfx3d", "sample"}, {"gfx3d", "audio_queued"},
    {"gfx3d", "audio_flush"}, {"gfx3d", "sound"}, {"gfx3d", "sound_at"}, {"gfx3d", "play"},
    {"gfx3d", "loop"}, {"gfx3d", "stop"}, {"gfx3d", "volume"},
};

static const char* kStringMethods[] = {
    "upper", "lower", "trim", "len", "contains", "starts_with", "ends_with",
    "index_of", "last_index_of", "count", "replace", "substring", "repeat", "split",
    "push", "pop", "insert", "remove", "clear", "has",
    "sort", "sort_desc", "reverse", "min", "max", "sum", "join",
    "keys", "values", "ok", "value", "error",
    "kind", "is_null", "is_bool", "is_number", "is_string", "is_array",
    "is_object", "is_error", "as_bool", "as_int", "as_float", "as_string",
    "get", "set"
};

static const char* kIncludes[] = {
    "std/io", "std/os", "std/file", "std/dll", "std/random", "std/math",
    "std/crypto", "std/network", "std/json", "std/gfx", "std/gfx3d", "std/time",
    "std/thread", "std/inline"
};

static const char* kModuleNames[] = {
    "io", "os", "dll", "file", "random", "math", "crypto",
    "http", "tcp", "udp", "json", "time", "thread", "gfx", "gfx3d"
};

static bool starts_with_ci(const std::string& w, const std::string& prefix) {
    if (prefix.size() > w.size()) return false;
    for (size_t i = 0; i < prefix.size(); i++) {
        if (std::tolower((unsigned char)w[i]) != std::tolower((unsigned char)prefix[i])) return false;
    }
    return true;
}

// Collects candidates, drops duplicates and the word already typed, and puts
// case-exact prefix matches ahead of case-insensitive ones.
namespace {
struct CompletionSink {
    std::vector<Completion>& out;
    const std::string& prefix;
    std::unordered_set<std::string> seen;
    std::vector<Completion> loose;

    CompletionSink(std::vector<Completion>& o, const std::string& p) : out(o), prefix(p) {}

    void add(const std::string& w, HighlightKind k) {
        if (w.empty() || w == prefix || seen.count(w)) return;
        if (w.rfind(prefix, 0) == 0) {
            seen.insert(w);
            out.push_back({w, k});
        } else if (starts_with_ci(w, prefix)) {
            seen.insert(w);
            loose.push_back({w, k});
        }
    }

    void finish() {
        for (auto& c : loose) out.push_back(std::move(c));
        if (out.size() > 200) out.resize(200);
    }
};
}  // namespace

std::vector<Completion> completions_for(const TextBuffer& buf, int cursor, std::string& prefix) {
    std::vector<Completion> out;
    prefix.clear();
    const std::string& t = buf.text;
    int i = std::max(0, std::min(cursor, (int)t.size()));
    int s = i;
    while (s > 0 && is_ident(t[(size_t)s - 1])) s--;

    if (buf.lang == Lang::Json) {
        prefix = t.substr((size_t)s, (size_t)(i - s));
        CompletionSink sink(out, prefix);
        if (path_filename(buf.path) == "nexapkg.json") {
            for (const char* k : {"name", "version", "entry", "output", "dll", "dllOutput", "dependencies"}) {
                sink.add(k, HighlightKind::Keyword);
            }
        }
        for (const char* k : {"true", "false", "null"}) sink.add(k, HighlightKind::Constant);
        sink.finish();
        return out;
    }
    if (buf.lang != Lang::Nexa) return out;

    int line_start = i;
    while (line_start > 0 && t[(size_t)line_start - 1] != '\n') line_start--;
    std::string line = t.substr((size_t)line_start, (size_t)(i - line_start));

    if (line.find("#include") != std::string::npos) {
        size_t lt = line.find_last_of("<\"");
        if (lt == std::string::npos) return out;
        prefix = line.substr(lt + 1);
        CompletionSink sink(out, prefix);
        for (const char* inc : kIncludes) sink.add(inc, HighlightKind::IncludePath);
        sink.finish();
        return out;
    }

    prefix = t.substr((size_t)s, (size_t)(i - s));
    if (s > 0 && t[(size_t)s - 1] == '.') {
        int ms = s - 1;
        while (ms > 0 && is_ident(t[(size_t)ms - 1])) ms--;
        std::string mod = t.substr((size_t)ms, (size_t)(s - 1 - ms));
        CompletionSink sink(out, prefix);
        bool known = false;
        for (const auto& m : kMembers) {
            if (mod == m.mod) {
                known = true;
                sink.add(m.name, HighlightKind::Member);
            }
        }
        if (!known) {
            for (const char* sm : kStringMethods) sink.add(sm, HighlightKind::Builtin);
        }
        sink.finish();
        return out;
    }
    if (prefix.empty()) return out;

    CompletionSink sink(out, prefix);
    for (const auto& d : buf.outline) {
        HighlightKind k = HighlightKind::Identifier;
        if (d.kind == DefKind::Function) k = HighlightKind::Function;
        else if (d.kind == DefKind::Struct || d.kind == DefKind::Enum) k = HighlightKind::TypeName;
        sink.add(d.name, k);
    }
    for (const char* w : kKeywords) sink.add(w, kControl.count(w) ? HighlightKind::Control : HighlightKind::Keyword);
    for (const char* w : kTypes) sink.add(w, HighlightKind::Type);
    for (const char* w : kModuleNames) sink.add(w, HighlightKind::Module);
    // Words already in the file, skipping comments and strings is not worth
    // the cost here: they are cheap hints, ranked after everything else.
    int n = (int)t.size();
    int k = 0;
    int words = 0;
    while (k < n && words < 4000) {
        if (is_ident_start(t[(size_t)k]) && (k == 0 || !is_ident(t[(size_t)k - 1]))) {
            int e = scan_ident(t, k, n);
            if (e - k >= 3 && !(k <= i && i <= e)) {
                sink.add(t.substr((size_t)k, (size_t)(e - k)), HighlightKind::Text);
                words++;
            }
            k = e;
            continue;
        }
        k++;
    }
    sink.finish();
    return out;
}
