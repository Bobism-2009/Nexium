#include "nexium.hpp"

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
    default: {
        HighlightKind unk = k;
        (void)unk;
        return "text";
    }
    }
}

static const std::unordered_set<std::string> kControl = {
    "fn", "extern", "if", "else", "while", "for", "switch", "case", "default",
    "return", "break", "continue", "goto", "try", "catch", "throw"
};

static const std::unordered_set<std::string> kKeyword = {
    "let", "const", "struct", "enum", "new", "delete", "sizeof", "in", "self"
};

static const std::unordered_set<std::string> kType = {
    "int", "short", "long", "size_t", "string", "bool", "float", "char", "void", "unsigned",
    "map", "Result", "result", "Json", "json", "HttpResponse", "HttpServer", "HttpRequest"
};

static const std::unordered_set<std::string> kConstant = {
    "true", "false", "null"
};

static const std::unordered_set<std::string> kModule = {
    "io", "os", "dll", "file", "random", "math", "crypto", "network",
    "http", "tcp", "udp", "json", "time", "thread", "gfx"
};

static const std::unordered_set<std::string> kBuiltin = {
    "len", "trim", "upper", "lower", "contains", "starts_with", "ends_with",
    "index_of", "replace", "substring", "repeat", "split", "push", "pop",
    "insert", "remove", "clear", "has", "sort", "sort_desc", "reverse",
    "min", "max", "sum", "join", "keys", "values", "ok", "err", "value",
    "error", "main", "__init__"
};

static bool is_ident_start(char c) {
    return std::isalpha((unsigned char)c) || c == '_';
}

static bool is_ident(char c) {
    return std::isalnum((unsigned char)c) || c == '_';
}

static void push_span(std::vector<HighlightSpan>& out, int start, int end, HighlightKind kind) {
    if (end <= start) return;
    if (!out.empty() && out.back().end == start && out.back().kind == kind) {
        out.back().end = end;
        return;
    }
    out.push_back({start, end, kind});
}

static bool is_nexapkg_key(const std::string& k) {
    static const char* keys[] = {
        "name", "version", "entry", "output", "dll", "dllOutput", "dependencies"
    };
    for (const char* key : keys) {
        if (k == key) return true;
    }
    return false;
}

void highlight_json(const std::string& src, std::vector<HighlightSpan>& out, bool nexa_manifest) {
    out.clear();
    const int n = (int)src.size();
    int i = 0;
    auto peek = [&](int d = 0) -> char {
        int p = i + d;
        return (p >= 0 && p < n) ? src[(size_t)p] : '\0';
    };

    while (i < n) {
        char c = src[(size_t)i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
            continue;
        }
        if (c == '/' && peek(1) == '/') {
            int s = i;
            while (i < n && src[(size_t)i] != '\n') i++;
            push_span(out, s, i, HighlightKind::Comment);
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            int s = i;
            i += 2;
            while (i + 1 < n && !(src[(size_t)i] == '*' && src[(size_t)i + 1] == '/')) i++;
            if (i + 1 < n) i += 2;
            else i = n;
            push_span(out, s, i, HighlightKind::Comment);
            continue;
        }
        if (c == '"') {
            const int open = i;
            i++;
            std::string key;
            bool escaped = false;
            while (i < n && src[(size_t)i] != '"') {
                if (src[(size_t)i] == '\\') {
                    escaped = true;
                    i++;
                    if (i < n) {
                        if (src[(size_t)i] == 'u' && i + 4 < n) i += 5;
                        else i++;
                    }
                    continue;
                }
                key += src[(size_t)i];
                i++;
            }
            if (i < n) i++;
            int j = i;
            while (j < n && (src[(size_t)j] == ' ' || src[(size_t)j] == '\t' ||
                             src[(size_t)j] == '\n' || src[(size_t)j] == '\r')) j++;
            if (j < n && src[(size_t)j] == ':') {
                HighlightKind kind = (nexa_manifest && is_nexapkg_key(key))
                    ? HighlightKind::Keyword : HighlightKind::Identifier;
                push_span(out, open, i, kind);
                continue;
            }
            if (!escaped) {
                push_span(out, open, i, HighlightKind::String);
                continue;
            }
            int p = open;
            i = open + 1;
            while (i < n && src[(size_t)i] != '"') {
                if (src[(size_t)i] == '\\') {
                    if (i > p) push_span(out, p, i, HighlightKind::String);
                    int es = i;
                    i++;
                    if (i < n) {
                        if (src[(size_t)i] == 'u' && i + 4 < n) i += 5;
                        else i++;
                    }
                    push_span(out, es, i, HighlightKind::Escape);
                    p = i;
                    continue;
                }
                i++;
            }
            if (i < n) i++;
            push_span(out, p, i, HighlightKind::String);
            continue;
        }
        if (c == '-' || std::isdigit((unsigned char)c)) {
            int s = i;
            if (c == '-') i++;
            while (i < n && std::isdigit((unsigned char)src[(size_t)i])) i++;
            if (i < n && src[(size_t)i] == '.') {
                i++;
                while (i < n && std::isdigit((unsigned char)src[(size_t)i])) i++;
            }
            if (i < n && (src[(size_t)i] == 'e' || src[(size_t)i] == 'E')) {
                i++;
                if (i < n && (src[(size_t)i] == '+' || src[(size_t)i] == '-')) i++;
                while (i < n && std::isdigit((unsigned char)src[(size_t)i])) i++;
            }
            push_span(out, s, i, HighlightKind::Number);
            continue;
        }
        if (is_ident_start(c)) {
            int s = i++;
            while (i < n && is_ident(src[(size_t)i])) i++;
            std::string id = src.substr((size_t)s, (size_t)(i - s));
            HighlightKind kind = HighlightKind::Identifier;
            if (id == "true" || id == "false" || id == "null") kind = HighlightKind::Constant;
            push_span(out, s, i, kind);
            continue;
        }
        if (c == '{' || c == '}' || c == '[' || c == ']' || c == ',' || c == ':') {
            push_span(out, i, i + 1, HighlightKind::Punct);
            i++;
            continue;
        }
        i++;
    }
}

void highlight_nexa(const std::string& src, std::vector<HighlightSpan>& out) {
    out.clear();
    const int n = (int)src.size();
    int i = 0;
    std::unordered_set<std::string> user_types;

    auto peek = [&](int d = 0) -> char {
        int p = i + d;
        return (p >= 0 && p < n) ? src[(size_t)p] : '\0';
    };

    auto starts_with = [&](const char* s) {
        size_t len = std::strlen(s);
        if (i + (int)len > n) return false;
        return src.compare((size_t)i, len, s) == 0;
    };

    while (i < n) {
        char c = src[(size_t)i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
            continue;
        }

        if (c == '/' && peek(1) == '/') {
            int s = i;
            i += 2;
            while (i < n && src[(size_t)i] != '\n') i++;
            push_span(out, s, i, HighlightKind::Comment);
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            int s = i;
            i += 2;
            while (i + 1 < n && !(src[(size_t)i] == '*' && src[(size_t)i + 1] == '/')) i++;
            if (i + 1 < n) i += 2;
            else i = n;
            push_span(out, s, i, HighlightKind::Comment);
            continue;
        }

        if (c == '#' && (i == 0 || src[(size_t)i - 1] == '\n')) {
            int s = i;
            if (starts_with("#include")) {
                push_span(out, i, i + 8, HighlightKind::Preproc);
                i += 8;
                while (i < n && (src[(size_t)i] == ' ' || src[(size_t)i] == '\t')) i++;
                if (i < n && (src[(size_t)i] == '<' || src[(size_t)i] == '"')) {
                    char endc = src[(size_t)i] == '<' ? '>' : '"';
                    int ps = i;
                    i++;
                    while (i < n && src[(size_t)i] != endc && src[(size_t)i] != '\n') i++;
                    if (i < n && src[(size_t)i] == endc) i++;
                    push_span(out, ps, i, HighlightKind::IncludePath);
                }
                while (i < n && src[(size_t)i] != '\n') i++;
            } else {
                while (i < n && src[(size_t)i] != '\n') i++;
                push_span(out, s, i, HighlightKind::Preproc);
            }
            continue;
        }

        if (c == 'R' && peek(1) == '"') {
            int s = i;
            i += 2;
            int delim_s = i;
            while (i < n && src[(size_t)i] != '(' && src[(size_t)i] != '\n') i++;
            std::string delim = src.substr((size_t)delim_s, (size_t)(i - delim_s));
            if (i < n && src[(size_t)i] == '(') i++;
            std::string close = ")" + delim + "\"";
            while (i + (int)close.size() <= n) {
                if (src.compare((size_t)i, close.size(), close) == 0) {
                    i += (int)close.size();
                    break;
                }
                i++;
            }
            push_span(out, s, i, HighlightKind::String);
            continue;
        }

        if (c == '"') {
            int s = i++;
            while (i < n && src[(size_t)i] != '"') {
                if (src[(size_t)i] == '\\') {
                    if (i > s) push_span(out, s, i, HighlightKind::String);
                    int es = i;
                    i++;
                    if (i < n) {
                        if (src[(size_t)i] == 'x' || src[(size_t)i] == 'X') {
                            i++;
                            int digits = 0;
                            while (i < n && digits < 2 && std::isxdigit((unsigned char)src[(size_t)i])) {
                                i++;
                                digits++;
                            }
                        } else {
                            i++;
                        }
                    }
                    push_span(out, es, i, HighlightKind::Escape);
                    s = i;
                    continue;
                }
                i++;
            }
            if (i < n) i++;
            push_span(out, s, i, HighlightKind::String);
            continue;
        }

        if (c == '\'') {
            int s = i++;
            if (i < n && src[(size_t)i] == '\\') {
                i++;
                if (i < n && (src[(size_t)i] == 'x' || src[(size_t)i] == 'X')) {
                    i++;
                    int digits = 0;
                    while (i < n && digits < 2 && std::isxdigit((unsigned char)src[(size_t)i])) {
                        i++;
                        digits++;
                    }
                } else if (i < n) {
                    i++;
                }
            } else if (i < n) {
                i++;
            }
            if (i < n && src[(size_t)i] == '\'') i++;
            push_span(out, s, i, HighlightKind::String);
            continue;
        }

        if (std::isdigit((unsigned char)c) || (c == '.' && std::isdigit((unsigned char)peek(1)))) {
            int s = i;
            if (c == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
                i += 2;
                while (i < n && std::isxdigit((unsigned char)src[(size_t)i])) i++;
            } else {
                if (c == '.') i++;
                while (i < n && std::isdigit((unsigned char)src[(size_t)i])) i++;
                if (i < n && src[(size_t)i] == '.') {
                    i++;
                    while (i < n && std::isdigit((unsigned char)src[(size_t)i])) i++;
                }
            }
            push_span(out, s, i, HighlightKind::Number);
            continue;
        }

        if (is_ident_start(c)) {
            int s = i++;
            while (i < n && is_ident(src[(size_t)i])) i++;
            std::string id = src.substr((size_t)s, (size_t)(i - s));

            int j = i;
            while (j < n && (src[(size_t)j] == ' ' || src[(size_t)j] == '\t')) j++;
            bool call = j < n && src[(size_t)j] == '(';
            bool dotted = s > 0 && src[(size_t)s - 1] == '.';

            if (id == "inline_cpp") {
                push_span(out, s, i, HighlightKind::Keyword);
                if (j < n && src[(size_t)j] == '!') {
                    push_span(out, j, j + 1, HighlightKind::Keyword);
                }
                continue;
            }
            if (id == "struct" || id == "enum") {
                push_span(out, s, i, HighlightKind::Keyword);
                int k = i;
                while (k < n && (src[(size_t)k] == ' ' || src[(size_t)k] == '\t')) k++;
                if (k < n && is_ident_start(src[(size_t)k])) {
                    int ns = k++;
                    while (k < n && is_ident(src[(size_t)k])) k++;
                    std::string name = src.substr((size_t)ns, (size_t)(k - ns));
                    user_types.insert(name);
                    push_span(out, ns, k, HighlightKind::TypeName);
                    i = k;
                }
                continue;
            }
            if (id == "fn" || id == "extern") {
                push_span(out, s, i, HighlightKind::Control);
                if (id == "extern") continue;
                int k = i;
                while (k < n && (src[(size_t)k] == ' ' || src[(size_t)k] == '\t')) k++;
                if (k < n && is_ident_start(src[(size_t)k])) {
                    int ns = k++;
                    while (k < n && is_ident(src[(size_t)k])) k++;
                    push_span(out, ns, k, HighlightKind::Function);
                    i = k;
                }
                continue;
            }
            if (kControl.count(id)) {
                push_span(out, s, i, HighlightKind::Control);
                continue;
            }
            if (kKeyword.count(id)) {
                push_span(out, s, i, HighlightKind::Keyword);
                continue;
            }
            if (kType.count(id)) {
                push_span(out, s, i, HighlightKind::Type);
                continue;
            }
            if (kConstant.count(id)) {
                push_span(out, s, i, HighlightKind::Constant);
                continue;
            }
            if (kModule.count(id) && j < n && src[(size_t)j] == '.') {
                push_span(out, s, i, HighlightKind::Module);
                continue;
            }
            if ((id == "ok" || id == "err") && call) {
                push_span(out, s, i, HighlightKind::Builtin);
                continue;
            }
            if (dotted && kBuiltin.count(id)) {
                push_span(out, s, i, HighlightKind::Builtin);
                continue;
            }
            if (user_types.count(id)) {
                push_span(out, s, i, HighlightKind::TypeName);
                continue;
            }
            if (dotted) {
                push_span(out, s, i, HighlightKind::Member);
                continue;
            }
            if (call || kBuiltin.count(id)) {
                push_span(out, s, i, HighlightKind::Function);
                continue;
            }
            push_span(out, s, i, HighlightKind::Identifier);
            continue;
        }

        static const char* ops[] = {
            "<<=", ">>=", "...", "::", "<<", ">>", "==", "!=", "<=", ">=", "&&", "||",
            "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "++", "--", "->",
            "=", "<", ">", "+", "-", "*", "/", "%", "!", "&", "|", "^", "~", "?"
        };
        bool op = false;
        for (const char* o : ops) {
            if (starts_with(o)) {
                int len = (int)std::strlen(o);
                push_span(out, i, i + len, HighlightKind::Operator);
                i += len;
                op = true;
                break;
            }
        }
        if (op) continue;

        if (c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' ||
            c == ',' || c == ';' || c == ':' || c == '.') {
            push_span(out, i, i + 1, HighlightKind::Punct);
            i++;
            continue;
        }
        i++;
    }
}

void index_nexa(const std::string& src, std::vector<IndexedDef>& out) {
    out.clear();
    static const std::regex fn_re(R"(\b(?:extern\s+)?fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\()");
    static const std::regex struct_re(R"(\bstruct\s+([A-Za-z_][A-Za-z0-9_]*))");
    static const std::regex enum_re(R"(\benum\s+([A-Za-z_][A-Za-z0-9_]*))");
    static const std::regex let_re(R"(^\s*let\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:[:=;]|$))",
                                   std::regex::multiline);

    auto line_of = [&](int pos) {
        int line = 0;
        for (int i = 0; i < pos && i < (int)src.size(); i++) {
            if (src[(size_t)i] == '\n') line++;
        }
        return line;
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
    "throw", "new", "delete", "sizeof", "true", "false", "null", "in", "self", "ok", "err"
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
};

static const char* kStringMethods[] = {
    "upper", "lower", "trim", "len", "contains", "starts_with", "ends_with",
    "index_of", "replace", "substring", "repeat", "split",
    "push", "pop", "insert", "remove", "clear", "has",
    "sort", "sort_desc", "reverse", "min", "max", "sum", "join",
    "keys", "values", "ok", "value", "error",
    "kind", "is_null", "is_bool", "is_number", "is_string", "is_array",
    "is_object", "is_error", "as_bool", "as_int", "as_float", "as_string",
    "get", "set"
};

static const char* kIncludes[] = {
    "std/io", "std/os", "std/file", "std/dll", "std/random", "std/math",
    "std/crypto", "std/network", "std/json", "std/gfx", "std/time",
    "std/thread", "std/inline"
};

std::vector<std::string> completions_for(const TextBuffer& buf, int cursor, std::string& prefix) {
    std::vector<std::string> out;
    prefix.clear();
    const std::string& t = buf.text;
    if (buf.is_json) {
        int i = std::max(0, std::min(cursor, (int)t.size()));
        int s = i;
        while (s > 0 && is_ident(t[(size_t)s - 1])) s--;
        prefix = t.substr((size_t)s, (size_t)(i - s));
        auto add_if = [&](const char* w) {
            if (prefix.empty() || std::strncmp(w, prefix.c_str(), prefix.size()) == 0) out.push_back(w);
        };
        if (path_filename(buf.path) == "nexapkg.json") {
            add_if("name");
            add_if("version");
            add_if("entry");
            add_if("output");
            add_if("dll");
            add_if("dllOutput");
            add_if("dependencies");
        }
        add_if("true");
        add_if("false");
        add_if("null");
        return out;
    }
    int i = std::max(0, std::min(cursor, (int)t.size()));
    int line_start = i;
    while (line_start > 0 && t[(size_t)line_start - 1] != '\n') line_start--;
    std::string line = t.substr((size_t)line_start, (size_t)(i - line_start));

    if (line.find("#include") != std::string::npos) {
        size_t lt = line.find_last_of("<\"");
        prefix = (lt == std::string::npos) ? "" : line.substr(lt + 1);
        for (const char* inc : kIncludes) {
            if (std::strncmp(inc, prefix.c_str(), prefix.size()) == 0) out.push_back(inc);
        }
        return out;
    }

    if (!line.empty() && line.back() == '.') {
        prefix.clear();
        size_t a = line.size();
        while (a > 1 && is_ident(line[a - 2])) a--;
        std::string mod = line.substr(a - 1, line.size() - a);
        if (!mod.empty() && is_ident_start(mod[0])) {
            for (const auto& m : kMembers) {
                if (mod == m.mod) out.push_back(m.name);
            }
            if (out.empty()) {
                for (const char* sm : kStringMethods) out.push_back(sm);
            }
            return out;
        }
        for (const char* sm : kStringMethods) out.push_back(sm);
        return out;
    }

    int s = i;
    while (s > 0 && is_ident(t[(size_t)s - 1])) s--;
    prefix = t.substr((size_t)s, (size_t)(i - s));
    if (s > 0 && t[(size_t)s - 1] == '.') {
        int ms = s - 1;
        while (ms > 0 && is_ident(t[(size_t)ms - 1])) ms--;
        std::string mod = t.substr((size_t)ms, (size_t)(s - 1 - ms));
        for (const auto& m : kMembers) {
            if (mod == m.mod && std::strncmp(m.name, prefix.c_str(), prefix.size()) == 0) {
                out.push_back(m.name);
            }
        }
        if (out.empty()) {
            for (const char* sm : kStringMethods) {
                if (std::strncmp(sm, prefix.c_str(), prefix.size()) == 0) out.push_back(sm);
            }
        }
        return out;
    }

    auto add_if = [&](const char* w) {
        if (prefix.empty() || std::strncmp(w, prefix.c_str(), prefix.size()) == 0) out.push_back(w);
    };
    static const char* kModuleNames[] = {
        "io", "os", "dll", "file", "random", "math", "crypto",
        "http", "tcp", "udp", "json", "time", "thread", "gfx"
    };
    for (const char* w : kKeywords) add_if(w);
    for (const char* w : kTypes) add_if(w);
    for (const char* w : kModuleNames) add_if(w);
    for (const char* w : kIncludes) add_if(w);
    for (const auto& d : buf.outline) {
        if (prefix.empty() || d.name.rfind(prefix, 0) == 0) out.push_back(d.name);
    }
    return out;
}
