#include "nexium.hpp"

#include <sstream>
#include <thread>
#include <algorithm>
#include <cstring>
#include <cctype>

static std::string join_url(const std::string& base, const char* path) {
    if (base.empty()) return path;
    if (base.back() == '/') return base.substr(0, base.size() - 1) + path;
    return base + path;
}

const char* provider_name(AgentProvider p) {
    switch (p) {
    case AgentProvider::OpenAI: return "OpenAI";
    case AgentProvider::Anthropic: return "Anthropic";
    case AgentProvider::OpenRouter: return "OpenRouter";
    case AgentProvider::Groq: return "Groq";
    case AgentProvider::Ollama: return "Ollama";
    case AgentProvider::Custom: return "Custom";
    default: {
        AgentProvider unk = p;
        (void)unk;
        return "OpenAI";
    }
    }
}

const char* provider_id(AgentProvider p) {
    switch (p) {
    case AgentProvider::OpenAI: return "openai";
    case AgentProvider::Anthropic: return "anthropic";
    case AgentProvider::OpenRouter: return "openrouter";
    case AgentProvider::Groq: return "groq";
    case AgentProvider::Ollama: return "ollama";
    case AgentProvider::Custom: return "custom";
    default: {
        AgentProvider unk = p;
        (void)unk;
        return "openai";
    }
    }
}

const char* provider_default_url(AgentProvider p) {
    switch (p) {
    case AgentProvider::OpenAI: return "https://api.openai.com/v1";
    case AgentProvider::Anthropic: return "https://api.anthropic.com";
    case AgentProvider::OpenRouter: return "https://openrouter.ai/api/v1";
    case AgentProvider::Groq: return "https://api.groq.com/openai/v1";
    case AgentProvider::Ollama: return "http://127.0.0.1:11434/v1";
    case AgentProvider::Custom: return "https://api.openai.com/v1";
    default: {
        AgentProvider unk = p;
        (void)unk;
        return "https://api.openai.com/v1";
    }
    }
}

void provider_models(AgentProvider p, std::vector<std::pair<std::string, std::string>>& out) {
    out.clear();
    switch (p) {
    case AgentProvider::OpenAI:
        out = {
            {"gpt-4.1", "GPT-4.1"},
            {"gpt-4o", "GPT-4o"},
            {"gpt-4o-mini", "GPT-4o mini"},
            {"o3", "o3"},
            {"o4-mini", "o4-mini"},
            {"gpt-4.1-mini", "GPT-4.1 mini"},
        };
        break;
    case AgentProvider::Anthropic:
        out = {
            {"claude-sonnet-4-20250514", "Claude Sonnet 4"},
            {"claude-opus-4-20250514", "Claude Opus 4"},
            {"claude-3-5-haiku-20241022", "Claude 3.5 Haiku"},
            {"claude-3-7-sonnet-20250219", "Claude 3.7 Sonnet"},
        };
        break;
    case AgentProvider::OpenRouter:
        out = {
            {"anthropic/claude-sonnet-4", "Claude Sonnet 4"},
            {"openai/gpt-4o", "GPT-4o"},
            {"google/gemini-2.5-pro", "Gemini 2.5 Pro"},
            {"qwen/qwen3-coder", "Qwen3 Coder"},
            {"deepseek/deepseek-chat", "DeepSeek V3"},
        };
        break;
    case AgentProvider::Groq:
        out = {
            {"openai/gpt-oss-20b", "GPT OSS 20B"},
            {"openai/gpt-oss-120b", "GPT OSS 120B"},
            {"qwen/qwen3.6-27b", "Qwen3.6 27B"},
            {"qwen/qwen3.8-27b", "Qwen3.8 27B"},
            {"groq/compound", "Groq Compound"},
            {"groq/compound-mini", "Compound Mini"},
        };
        break;
    case AgentProvider::Ollama:
        out = {
            {"llama3.2", "Llama 3.2"},
            {"qwen2.5-coder", "Qwen2.5 Coder"},
            {"deepseek-coder-v2", "DeepSeek Coder V2"},
            {"codellama", "Code Llama"},
        };
        break;
    case AgentProvider::Custom:
        out = {{"custom", "Custom model"}};
        break;
    default: {
        AgentProvider unk = p;
        (void)unk;
        break;
    }
    }
}

static void migrate_groq_model(AgentSettings& a) {
    if (a.provider != AgentProvider::Groq) return;
    ProviderSlot& s = a.current();
    const char* id = s.custom_model.empty() ? s.model.c_str() : s.custom_model.c_str();
    const char* next = nullptr;
    if (std::strcmp(id, "qwen/qwen3-32b") == 0) next = "qwen/qwen3.6-27b";
    else if (std::strcmp(id, "llama-3.1-8b-instant") == 0) next = "openai/gpt-oss-20b";
    else if (std::strcmp(id, "llama-3.3-70b-versatile") == 0) next = "openai/gpt-oss-120b";
    else if (std::strcmp(id, "meta-llama/llama-4-scout-17b-16e-instruct") == 0) next = "openai/gpt-oss-120b";
    else if (std::strcmp(id, "meta-llama/llama-4-maverick-17b-128e-instruct") == 0) next = "openai/gpt-oss-120b";
    if (!next) return;
    s.model = next;
    s.custom_model.clear();
}

void normalize_agent_settings(AgentSettings& a) {
    migrate_groq_model(a);
}

void apply_provider_defaults(AgentSettings& a) {
    ProviderSlot& s = a.current();
    if (s.base_url.empty()) s.base_url = provider_default_url(a.provider);
    if (s.model.empty() && s.custom_model.empty()) {
        std::vector<std::pair<std::string, std::string>> models;
        provider_models(a.provider, models);
        if (!models.empty()) s.model = models[0].first;
    }
    migrate_groq_model(a);
}

static bool skip_listed_model(const std::string& id) {
    if (id.find("whisper") != std::string::npos) return true;
    if (id.find("guard") != std::string::npos) return true;
    if (id.find("orpheus") != std::string::npos) return true;
    if (id.find("tts") != std::string::npos) return true;
    return false;
}

void agent_refresh_models() {
    if (!g_app) return;
    g_app->models_listed = true;
    const AgentSettings& a = g_app->settings.agent;
    if (a.current().api_key.empty()) return;
    if (a.provider != AgentProvider::Groq && a.provider != AgentProvider::OpenAI &&
        a.provider != AgentProvider::OpenRouter && a.provider != AgentProvider::Ollama)
        return;
    std::vector<std::pair<std::string, std::string>> headers = {
        {"Authorization", "Bearer " + a.current().api_key},
    };
    int status = 0;
    std::string err;
    std::string resp = http_request("GET", join_url(a.current().base_url, "/models"), headers, {}, &status, &err);
    Json j;
    if (resp.empty() || !json_parse(resp, j, nullptr)) return;
    const Json* data = j.get("data");
    if (!data) return;
    std::vector<std::string> ids;
    for (int i = 0; i < data->size(); i++) {
        const Json* m = data->at(i);
        if (!m) continue;
        std::string id = m->str("id");
        if (id.empty() || skip_listed_model(id)) continue;
        ids.push_back(id);
    }
    g_app->extra_models = std::move(ids);
}

void agent_new_chat() {
    std::lock_guard<std::mutex> lock(g_app->agent.mu);
    g_app->agent.messages.clear();
    g_app->agent.stream.clear();
    g_app->agent.error.clear();
    g_app->agent.status.clear();
    g_app->agent.tool_open.clear();
}

void agent_stop() {
    g_app->agent.cancel = true;
}

void agent_apply_write(const std::string& path, const std::string& content) {
    std::string abs = path;
    if (abs.size() < 2 || abs[1] != ':') {
        if (!g_app->settings.folder.empty()) abs = path_join(g_app->settings.folder, path);
    }
    abs = path_norm(abs);
    std::string parent = path_parent(abs);
    if (!parent.empty()) make_dir(parent);
    file_write(abs, content);
    for (auto& b : g_app->buffers) {
        if (!b.untitled && path_norm(b.path) == abs) {
            b.text = content;
            b.dirty = false;
            buffer_refresh(b);
        }
    }
}

static std::string resolve_workspace_path(const std::string& p) {
    if (p.size() >= 2 && p[1] == ':') return path_norm(p);
    if (!p.empty() && (p[0] == '/' || p[0] == '\\')) return path_norm(p);
    if (!g_app->settings.folder.empty()) return path_norm(path_join(g_app->settings.folder, p));
    return path_norm(p);
}

static bool skip_walk_dir(const std::string& name) {
    return name == ".git" || name == "node_modules" || name == "build" ||
           name == "dist" || name == ".vs" || name == "__pycache__" ||
           name == ".cache" || name == "out" || name == "bin" || name == "obj" ||
           name == ".nexium";
}

static bool looks_binary(const std::string& text) {
    size_t n = (std::min)(text.size(), (size_t)800);
    for (size_t i = 0; i < n; i++) {
        if (text[i] == 0) return true;
    }
    return false;
}

static bool glob_match(const char* p, const char* s) {
    while (*p) {
        if (p[0] == '*' && p[1] == '*') {
            p += 2;
            if (*p == '/' || *p == '\\') p++;
            if (!*p) return true;
            for (const char* t = s;; t++) {
                if (glob_match(p, t)) return true;
                if (!*t) break;
            }
            return false;
        }
        if (*p == '*') {
            p++;
            if (!*p) {
                while (*s && *s != '/' && *s != '\\') s++;
                return *s == 0;
            }
            for (const char* t = s;; t++) {
                if (glob_match(p, t)) return true;
                if (!*t || *t == '/' || *t == '\\') break;
            }
            return false;
        }
        if (*p == '?') {
            if (!*s || *s == '/' || *s == '\\') return false;
            p++;
            s++;
            continue;
        }
        if (!*s) return false;
        char a = *p;
        char b = *s;
        if (a == '/') a = '\\';
        if (b == '/') b = '\\';
        if ((char)std::tolower((unsigned char)a) != (char)std::tolower((unsigned char)b)) return false;
        p++;
        s++;
    }
    return *s == 0;
}

static bool path_glob(const std::string& pat, const std::string& rel, const std::string& name) {
    if (pat.empty()) return true;
    if (glob_match(pat.c_str(), rel.c_str())) return true;
    if (glob_match(pat.c_str(), name.c_str())) return true;
    return false;
}

static void walk_files(const std::string& dir, const std::string& root,
                       const std::function<bool(const std::string& abs, const std::string& rel)>& fn,
                       int depth = 0) {
    if (depth > 14) return;
    for (const auto& e : list_dir(dir)) {
        std::string abs = path_join(dir, e.first);
        std::string rel = abs;
        if (abs.size() > root.size() + 1 && abs.rfind(root, 0) == 0)
            rel = abs.substr(root.size() + 1);
        if (e.second) {
            if (skip_walk_dir(e.first)) continue;
            walk_files(abs, root, fn, depth + 1);
        } else {
            if (!fn(abs, rel)) return;
        }
    }
}

static std::string tool_read_file(const Json& args) {
    std::string path = resolve_workspace_path(args.str("path"));
    std::string text, err;
    if (!file_read(path, text, &err)) return "ERROR: " + err;
    int offset = (int)args.num("offset", 0);
    int limit = (int)args.num("limit", 0);
    if (offset > 0 || limit > 0) {
        if (offset < 1) offset = 1;
        if (limit <= 0) limit = 400;
        std::ostringstream o;
        int line = 1;
        size_t i = 0;
        while (i < text.size() && line < offset) {
            if (text[i] == '\n') line++;
            i++;
        }
        int shown = 0;
        while (i < text.size() && shown < limit) {
            size_t e = text.find('\n', i);
            if (e == std::string::npos) e = text.size();
            o << line << "| " << text.substr(i, e - i) << "\n";
            i = (e < text.size()) ? e + 1 : e;
            line++;
            shown++;
        }
        if (i < text.size()) o << "... (" << (text.size() - i) << " bytes left)\n";
        return o.str();
    }
    if (text.size() > 120000) {
        text.resize(120000);
        text += "\n... [truncated]";
    }
    return text;
}

static std::string tool_write_file(const Json& args) {
    std::string path = resolve_workspace_path(args.str("path"));
    std::string content = args.str("content");
    std::string parent = path_parent(path);
    if (!parent.empty()) make_dir(parent);
    std::string err;
    if (!file_write(path, content, &err)) return "ERROR: " + err;
    for (auto& b : g_app->buffers) {
        if (!b.untitled && path_norm(b.path) == path) {
            b.text = content;
            b.dirty = false;
            buffer_refresh(b);
        }
    }
    if (g_app->settings.folder.size() && path.rfind(g_app->settings.folder, 0) == 0) {
        scan_node(g_app->root, true);
    }
    return "Wrote " + path + " (" + std::to_string(content.size()) + " bytes)";
}

static std::string tool_edit_file(const Json& args) {
    std::string path = resolve_workspace_path(args.str("path"));
    std::string olds = args.str("old_string");
    std::string news = args.str("new_string");
    if (olds.empty()) return "ERROR: old_string is empty";
    std::string text, err;
    bool from_disk = true;
    for (auto& b : g_app->buffers) {
        if (!b.untitled && path_norm(b.path) == path) {
            text = b.text;
            from_disk = false;
            break;
        }
    }
    if (from_disk && !file_read(path, text, &err)) return "ERROR: " + err;
    size_t pos = text.find(olds);
    if (pos == std::string::npos) return "ERROR: old_string not found in file";
    if (text.find(olds, pos + 1) != std::string::npos)
        return "ERROR: old_string matched more than once; make it unique";
    text.replace(pos, olds.size(), news);
    if (!file_write(path, text, &err)) return "ERROR: " + err;
    for (auto& b : g_app->buffers) {
        if (!b.untitled && path_norm(b.path) == path) {
            b.text = text;
            b.dirty = false;
            buffer_refresh(b);
        }
    }
    return "Edited " + path;
}

static std::string tool_list_dir(const Json& args) {
    std::string path = args.str("path");
    if (path.empty()) path = g_app->settings.folder;
    path = resolve_workspace_path(path);
    auto entries = list_dir(path);
    std::ostringstream o;
    for (const auto& e : entries) {
        o << (e.second ? "dir  " : "file ") << e.first << "\n";
    }
    return o.str().empty() ? "(empty)" : o.str();
}

static std::string tool_search(const Json& args) {
    std::string q = args.str("query");
    if (q.empty()) return "ERROR: query required";
    std::string glob = args.str("glob");
    std::string root = g_app->settings.folder;
    if (root.empty()) return "ERROR: no folder open";
    std::ostringstream o;
    int hits = 0;
    walk_files(root, root, [&](const std::string& abs, const std::string& rel) {
        if (!path_glob(glob, rel, path_filename(abs))) return true;
        std::string text;
        if (!file_read(abs, text) || looks_binary(text)) return true;
        int line = 1;
        size_t i = 0;
        while (i < text.size()) {
            size_t e = text.find('\n', i);
            if (e == std::string::npos) e = text.size();
            if (text.find(q, i) < e) {
                o << rel << ":" << line << ": " << text.substr(i, e - i) << "\n";
                hits++;
                if (hits >= 200) return false;
            }
            i = (e < text.size()) ? e + 1 : e;
            line++;
        }
        return true;
    });
    if (hits == 0) return "No matches";
    return o.str();
}

static std::string tool_glob(const Json& args) {
    std::string pat = args.str("pattern");
    if (pat.empty()) pat = args.str("glob");
    if (pat.empty()) return "ERROR: pattern required";
    std::string root = g_app->settings.folder;
    if (root.empty()) return "ERROR: no folder open";
    std::ostringstream o;
    int n = 0;
    walk_files(root, root, [&](const std::string& abs, const std::string& rel) {
        (void)abs;
        if (!path_glob(pat, rel, path_filename(rel))) return true;
        o << rel << "\n";
        n++;
        return n < 500;
    });
    if (n == 0) return "No files matched";
    return o.str();
}

static std::string tool_delete_file(const Json& args) {
    std::string path = resolve_workspace_path(args.str("path"));
    if (path.empty()) return "ERROR: path required";
    if (!file_exists(path) && !dir_exists(path)) return "ERROR: not found: " + path;
    for (int i = (int)g_app->buffers.size() - 1; i >= 0; i--) {
        if (!g_app->buffers[(size_t)i].untitled && path_norm(g_app->buffers[(size_t)i].path) == path)
            close_buffer(i, true);
    }
    if (!remove_path(path)) return "ERROR: could not delete " + path;
    if (!g_app->settings.folder.empty() && path.rfind(g_app->settings.folder, 0) == 0)
        scan_node(g_app->root, true);
    return "Deleted " + path;
}

static std::string tool_rename_file(const Json& args) {
    std::string from = resolve_workspace_path(args.str("from"));
    if (from.empty()) from = resolve_workspace_path(args.str("path"));
    std::string to = resolve_workspace_path(args.str("to"));
    if (to.empty()) to = resolve_workspace_path(args.str("new_path"));
    if (from.empty() || to.empty()) return "ERROR: from and to required";
    std::string parent = path_parent(to);
    if (!parent.empty()) make_dir(parent);
    if (!rename_path(from, to)) return "ERROR: rename failed";
    for (auto& b : g_app->buffers) {
        if (!b.untitled && path_norm(b.path) == from) {
            b.path = to;
            b.name = path_filename(to);
        }
    }
    if (!g_app->settings.folder.empty()) scan_node(g_app->root, true);
    return "Renamed " + from + " -> " + to;
}

static std::string tool_run_command(const Json& args) {
    std::string cmd = args.str("command");
    if (cmd.empty()) cmd = args.str("cmd");
    if (cmd.empty()) return "ERROR: command required";
    std::string cwd = args.str("cwd");
    if (cwd.empty()) cwd = args.str("working_directory");
    if (cwd.empty()) cwd = g_app->settings.folder;
    else cwd = resolve_workspace_path(cwd);
    if (cwd.empty() && g_app->active >= 0) {
        const TextBuffer& b = g_app->buffers[(size_t)g_app->active];
        if (!b.untitled) cwd = path_parent(b.path);
    }
    if (!cwd.empty() && !dir_exists(cwd)) return "ERROR: cwd not found: " + cwd;
    DWORD timeout = (DWORD)args.num("timeout_ms", 60000.0);
    if (g_app) {
        g_app->bottom = BottomTab::Terminal;
        g_app->settings.show_panel = true;
        term_write_local("$ " + cmd + "\n");
    }
    ProcCapture cap;
    if (!proc_run_capture(cmd, cwd, timeout, cap)) {
        if (g_app) term_write_local(cap.output + "\n");
        return cap.output;
    }
    if (g_app) {
        std::string echo = cap.output;
        if (echo.empty() || echo.back() != '\n') echo += "\n";
        echo += "[exit " + std::to_string(cap.exit_code) + "]\n";
        term_write_local(echo);
    }
    std::string body = cap.output;
    if (body.size() > 80000) {
        body = body.substr(body.size() - 80000);
        body = "... [truncated]\n" + body;
    }
    std::ostringstream o;
    o << "exit " << cap.exit_code;
    if (cap.timed_out) o << " (timed out)";
    if (cap.cancelled) o << " (cancelled)";
    o << "\ncwd: " << cwd << "\n";
    if (body.empty()) o << "(no output)";
    else o << body;
    return o.str();
}

static std::string tool_run_nexa(const Json& args) {
    bool run = args.boolean("run", true);
    std::string dir = g_app->settings.folder;
    std::string file = args.str("file");
    if (!file.empty()) {
        file = resolve_workspace_path(file);
        dir = path_parent(file);
    }
    if (dir.empty() && g_app->active >= 0) {
        const TextBuffer& b = g_app->buffers[(size_t)g_app->active];
        if (!b.untitled) dir = path_parent(b.path);
    }
    if (dir.empty()) return "ERROR: no project folder";
    if (g_app->active >= 0 && g_app->buffers[(size_t)g_app->active].dirty)
        save_buffer(g_app->active);
    std::string nexac = nexa_compiler();
    if (nexac.find(' ') != std::string::npos) nexac = "\"" + nexac + "\"";
    std::string cmd = nexac + (run ? " --run" : " build");
    proc_start(cmd, dir);
    Sleep(400);
    std::string dump = term_plain_text();
    return dump.empty() ? "Started Nexa" : dump;
}

static std::string tool_open_info() {
    std::ostringstream o;
    o << "folder: " << g_app->settings.folder << "\n";
    if (g_app->active >= 0) {
        const TextBuffer& b = g_app->buffers[(size_t)g_app->active];
        o << "active: " << (b.untitled ? b.name : b.path) << "\n";
        o << "cursor_line: " << (buffer_line_at(b, b.cursor) + 1) << "\n";
        std::string sel = buffer_selection(b);
        if (!sel.empty()) {
            if (sel.size() > 4000) sel.resize(4000);
            o << "selection:\n" << sel << "\n";
        }
    }
    o << "open_tabs:\n";
    for (const auto& b : g_app->buffers) {
        o << "  " << (b.untitled ? b.name : b.path) << (b.dirty ? " *" : "") << "\n";
    }
    return o.str();
}

static std::string execute_tool(const std::string& name, const std::string& args_json) {
    Json args;
    if (!args_json.empty()) json_parse(args_json, args);
    if (name == "read_file") return tool_read_file(args);
    if (name == "write_file") return tool_write_file(args);
    if (name == "edit_file") return tool_edit_file(args);
    if (name == "list_dir") return tool_list_dir(args);
    if (name == "search_workspace") return tool_search(args);
    if (name == "glob_files") return tool_glob(args);
    if (name == "delete_file") return tool_delete_file(args);
    if (name == "rename_file") return tool_rename_file(args);
    if (name == "run_command") return tool_run_command(args);
    if (name == "run_nexa") return tool_run_nexa(args);
    if (name == "workspace_info") return tool_open_info();
    return "ERROR: unknown tool " + name;
}

static const char* kOpenaiTools = R"JSON(
[{"type":"function","function":{"name":"read_file","description":"Read a file. Optional 1-based offset and limit in lines.","parameters":{"type":"object","properties":{"path":{"type":"string"},"offset":{"type":"integer"},"limit":{"type":"integer"}},"required":["path"]}}},
{"type":"function","function":{"name":"write_file","description":"Create or overwrite a file.","parameters":{"type":"object","properties":{"path":{"type":"string"},"content":{"type":"string"}},"required":["path","content"]}}},
{"type":"function","function":{"name":"edit_file","description":"Replace one unique old_string with new_string.","parameters":{"type":"object","properties":{"path":{"type":"string"},"old_string":{"type":"string"},"new_string":{"type":"string"}},"required":["path","old_string","new_string"]}}},
{"type":"function","function":{"name":"list_dir","description":"List a directory.","parameters":{"type":"object","properties":{"path":{"type":"string"}}}}},
{"type":"function","function":{"name":"search_workspace","description":"Search workspace files for a string. Optional glob like *.nxa.","parameters":{"type":"object","properties":{"query":{"type":"string"},"glob":{"type":"string"}},"required":["query"]}}},
{"type":"function","function":{"name":"glob_files","description":"Find files by glob (*.nxa, **/*.cpp).","parameters":{"type":"object","properties":{"pattern":{"type":"string"}},"required":["pattern"]}}},
{"type":"function","function":{"name":"delete_file","description":"Delete a file or folder.","parameters":{"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}}},
{"type":"function","function":{"name":"rename_file","description":"Rename or move a file.","parameters":{"type":"object","properties":{"from":{"type":"string"},"to":{"type":"string"}},"required":["from","to"]}}},
{"type":"function","function":{"name":"run_command","description":"Run a Windows cmd.exe command. Captures stdout/stderr and exit code. Use for git, builds, tests, installs, dir, powershell -Command, anything the user could type in a terminal. cwd defaults to the open folder. timeout_ms defaults to 60000.","parameters":{"type":"object","properties":{"command":{"type":"string"},"cwd":{"type":"string"},"timeout_ms":{"type":"integer"}},"required":["command"]}}},
{"type":"function","function":{"name":"run_nexa","description":"Run NexaC --run or NexaC build in the project folder.","parameters":{"type":"object","properties":{"file":{"type":"string"},"run":{"type":"boolean"}}}}},
{"type":"function","function":{"name":"workspace_info","description":"Open tabs, folder, selection.","parameters":{"type":"object","properties":{}}}}]
)JSON";

static const char* kAnthropicTools = R"JSON(
[{"name":"read_file","description":"Read a file. Optional offset and limit in lines.","input_schema":{"type":"object","properties":{"path":{"type":"string"},"offset":{"type":"integer"},"limit":{"type":"integer"}},"required":["path"]}},
{"name":"write_file","description":"Create or overwrite a file.","input_schema":{"type":"object","properties":{"path":{"type":"string"},"content":{"type":"string"}},"required":["path","content"]}},
{"name":"edit_file","description":"Replace one unique old_string with new_string.","input_schema":{"type":"object","properties":{"path":{"type":"string"},"old_string":{"type":"string"},"new_string":{"type":"string"}},"required":["path","old_string","new_string"]}},
{"name":"list_dir","description":"List a directory.","input_schema":{"type":"object","properties":{"path":{"type":"string"}}}},
{"name":"search_workspace","description":"Search workspace files. Optional glob.","input_schema":{"type":"object","properties":{"query":{"type":"string"},"glob":{"type":"string"}},"required":["query"]}},
{"name":"glob_files","description":"Find files by glob.","input_schema":{"type":"object","properties":{"pattern":{"type":"string"}},"required":["pattern"]}},
{"name":"delete_file","description":"Delete a file or folder.","input_schema":{"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}},
{"name":"rename_file","description":"Rename or move a file.","input_schema":{"type":"object","properties":{"from":{"type":"string"},"to":{"type":"string"}},"required":["from","to"]}},
{"name":"run_command","description":"Run a Windows cmd.exe command and capture output. cwd defaults to the open folder.","input_schema":{"type":"object","properties":{"command":{"type":"string"},"cwd":{"type":"string"},"timeout_ms":{"type":"integer"}},"required":["command"]}},
{"name":"run_nexa","description":"Run NexaC --run or NexaC build.","input_schema":{"type":"object","properties":{"file":{"type":"string"},"run":{"type":"boolean"}}}},
{"name":"workspace_info","description":"Open tabs and selection.","input_schema":{"type":"object","properties":{}}}]
)JSON";

std::string agent_system_prompt() {
    const AgentSettings& a = g_app->settings.agent;
    const bool agent_mode = a.mode == AgentMode::Agent;
    const bool groq = a.provider == AgentProvider::Groq;

    std::ostringstream o;
    o << "You are Nexium, an AI assistant in a native Nexa IDE.\n"
      << "Nexa is a systems language (.nxa) that transpiles to C++ via NexaC.\n"
      << "Write real Nexa, not C++, unless the user is in inline_cpp! or asking about the compiler.\n"
      << "The chat UI renders Markdown: headings, **bold**, *italic*, lists, `inline code`, and fenced code blocks.\n";
    if (agent_mode) {
        o << "You have tools. Use them instead of telling the user what to run.\n"
          << "run_command executes a real Windows cmd.exe command on this machine and returns stdout, stderr, and the exit code. "
          << "Use it for git, builds, tests, installs, dir, where, powershell -Command, and any other shell work.\n"
          << "glob_files finds files. search_workspace greps the tree. delete_file and rename_file change the disk. "
          << "Prefer edit_file for small edits. Prefer write_file for new files.\n"
          << "Do not invent Nexa APIs that are not in the syntax notes.\n";
    }
    if (!g_app->settings.folder.empty())
        o << "Open folder: " << g_app->settings.folder << "\n";
    if (g_app->active >= 0) {
        const TextBuffer& b = g_app->buffers[(size_t)g_app->active];
        o << "Active file: " << (b.untitled ? b.name : b.path) << "\n";
    }

    // Groq on-demand TPM is small; Chat mode does not need the syntax dump.
    if (!groq || agent_mode) {
        const size_t budget = groq ? 3500 : 8000;
        const size_t per_file = groq ? 1000 : 2500;
        std::string syntax;
        std::string root = g_app->settings.nexa_lang;
        const char* files[] = {"SYNTAX/Core.txt", "SYNTAX/ControlFlow.txt", "SYNTAX/Modules.txt", "SYNTAX/CLI.txt"};
        for (const char* rel : files) {
            std::string text;
            if (!file_read(path_join(root, rel), text)) continue;
            if (text.size() > per_file) text.resize(per_file);
            syntax += "\n\n===== ";
            syntax += rel;
            syntax += " =====\n";
            syntax += text;
            if (syntax.size() >= budget) break;
        }
        if (syntax.size() > budget) syntax.resize(budget);
        if (!syntax.empty()) o << syntax;
    }
    return o.str();
}

static std::string effective_model(const AgentSettings& a) {
    const ProviderSlot& s = a.current();
    if (!s.custom_model.empty()) return s.custom_model;
    return s.model;
}

static std::string openai_messages_json(const std::vector<ChatMessage>& msgs, const std::string& system) {
    std::ostringstream o;
    o << "[{\"role\":\"system\",\"content\":\"" << json_escape(system) << "\"}";
    for (const auto& m : msgs) {
        o << ",{";
        if (m.role == "tool") {
            o << "\"role\":\"tool\",\"tool_call_id\":\"" << json_escape(m.tool_id)
              << "\",\"content\":\"" << json_escape(m.content) << "\"";
        } else if (m.role == "assistant" && !m.tool_name.empty()) {
            o << "\"role\":\"assistant\",\"content\":"
              << (m.content.empty() ? "null" : ("\"" + json_escape(m.content) + "\""))
              << ",\"tool_calls\":[{\"id\":\"" << json_escape(m.tool_id)
              << "\",\"type\":\"function\",\"function\":{\"name\":\"" << json_escape(m.tool_name)
              << "\",\"arguments\":\"" << json_escape(m.tool_args) << "\"}}]";
        } else {
            o << "\"role\":\"" << json_escape(m.role) << "\",\"content\":\"" << json_escape(m.content) << "\"";
        }
        o << "}";
    }
    o << "]";
    return o.str();
}

static bool openai_request(const AgentSettings& cfg, std::vector<ChatMessage>& msgs,
                           const std::string& system, bool use_tools, std::string& err) {
    std::ostringstream body;
    body << "{\"model\":\"" << json_escape(effective_model(cfg)) << "\""
         << ",\"temperature\":" << cfg.temperature
         << ",\"max_tokens\":" << (cfg.provider == AgentProvider::Groq ? 1000 : 4096)
         << ",\"messages\":" << openai_messages_json(msgs, system);
    if (use_tools) body << ",\"tools\":" << kOpenaiTools;
    body << "}";
    std::vector<std::pair<std::string, std::string>> headers = {
        {"Content-Type", "application/json"},
    };
    if (!cfg.current().api_key.empty()) headers.push_back({"Authorization", "Bearer " + cfg.current().api_key});
    if (cfg.provider == AgentProvider::OpenRouter) {
        headers.push_back({"HTTP-Referer", "https://nexium.local"});
        headers.push_back({"X-Title", "Nexium"});
    }
    int status = 0;
    std::string resp = http_request("POST", join_url(cfg.current().base_url, "/chat/completions"),
                                    headers, body.str(), &status, &err);
    if (resp.empty() && !err.empty()) return false;
    Json j;
    if (!json_parse(resp, j, &err)) {
        err = "Bad JSON (" + std::to_string(status) + "): " + resp.substr(0, 400);
        return false;
    }
    if (const Json* e = j.get("error")) {
        err = e->str("message", "API error");
        return false;
    }
    const Json* choices = j.get("choices");
    if (!choices || choices->size() == 0) {
        err = "No choices in response: " + resp.substr(0, 400);
        return false;
    }
    const Json* msg = choices->at(0)->get("message");
    if (!msg) {
        err = "No message";
        return false;
    }
    ChatMessage out;
    out.role = "assistant";
    out.content = msg->str("content");
    const Json* tools = msg->get("tool_calls");
    if (tools && tools->size() > 0) {
        for (int i = 0; i < tools->size(); i++) {
            const Json* tc = tools->at(i);
            ChatMessage call;
            call.role = "assistant";
            call.tool_id = tc->str("id");
            if (const Json* fn = tc->get("function")) {
                call.tool_name = fn->str("name");
                call.tool_args = fn->str("arguments");
            }
            msgs.push_back(call);
            {
                std::lock_guard<std::mutex> lock(g_app->agent.mu);
                g_app->agent.status = "Running " + call.tool_name + "...";
                g_app->agent.messages.push_back(call);
            }
            std::string result = execute_tool(call.tool_name, call.tool_args);
            ChatMessage tool;
            tool.role = "tool";
            tool.tool_id = call.tool_id;
            tool.tool_name = call.tool_name;
            tool.content = result;
            msgs.push_back(tool);
            {
                std::lock_guard<std::mutex> lock(g_app->agent.mu);
                g_app->agent.messages.push_back(tool);
            }
        }
        return true;
    }
    msgs.push_back(out);
    {
        std::lock_guard<std::mutex> lock(g_app->agent.mu);
        g_app->agent.messages.push_back(out);
        g_app->agent.stream.clear();
    }
    return true;
}

static std::string anthropic_messages_json(const std::vector<ChatMessage>& msgs) {
    std::ostringstream o;
    o << "[";
    bool first = true;
    for (size_t i = 0; i < msgs.size(); i++) {
        const ChatMessage& m = msgs[i];
        if (m.role == "system") continue;
        if (!first) o << ",";
        first = false;
        if (m.role == "tool") {
            o << "{\"role\":\"user\",\"content\":[{\"type\":\"tool_result\",\"tool_use_id\":\""
              << json_escape(m.tool_id) << "\",\"content\":\"" << json_escape(m.content) << "\"}]}";
        } else if (m.role == "assistant" && !m.tool_name.empty()) {
            o << "{\"role\":\"assistant\",\"content\":[";
            if (!m.content.empty())
                o << "{\"type\":\"text\",\"text\":\"" << json_escape(m.content) << "\"},";
            o << "{\"type\":\"tool_use\",\"id\":\"" << json_escape(m.tool_id)
              << "\",\"name\":\"" << json_escape(m.tool_name)
              << "\",\"input\":" << (m.tool_args.empty() ? "{}" : m.tool_args) << "}]}";
        } else {
            o << "{\"role\":\"" << (m.role == "assistant" ? "assistant" : "user")
              << "\",\"content\":\"" << json_escape(m.content) << "\"}";
        }
    }
    o << "]";
    return o.str();
}

static bool anthropic_request(const AgentSettings& cfg, std::vector<ChatMessage>& msgs,
                              const std::string& system, bool use_tools, std::string& err) {
    std::ostringstream body;
    body << "{\"model\":\"" << json_escape(effective_model(cfg)) << "\""
         << ",\"max_tokens\":8192"
         << ",\"temperature\":" << cfg.temperature
         << ",\"system\":\"" << json_escape(system) << "\""
         << ",\"messages\":" << anthropic_messages_json(msgs);
    if (use_tools) body << ",\"tools\":" << kAnthropicTools;
    body << "}";
    std::vector<std::pair<std::string, std::string>> headers = {
        {"Content-Type", "application/json"},
        {"x-api-key", cfg.current().api_key},
        {"anthropic-version", "2023-06-01"},
    };
    int status = 0;
    std::string resp = http_request("POST", join_url(cfg.current().base_url, "/v1/messages"),
                                    headers, body.str(), &status, &err);
    Json j;
    if (!json_parse(resp, j, &err)) {
        err = "Bad JSON (" + std::to_string(status) + "): " + resp.substr(0, 400);
        return false;
    }
    if (const Json* e = j.get("error")) {
        err = e->str("message", "API error");
        return false;
    }
    const Json* content = j.get("content");
    if (!content) {
        err = "No content: " + resp.substr(0, 400);
        return false;
    }
    bool used_tool = false;
    std::string text;
    for (int i = 0; i < content->size(); i++) {
        const Json* block = content->at(i);
        std::string type = block->str("type");
        if (type == "text") text += block->str("text");
        if (type == "tool_use") {
            used_tool = true;
            ChatMessage call;
            call.role = "assistant";
            call.content = text;
            call.tool_id = block->str("id");
            call.tool_name = block->str("name");
            if (const Json* input = block->get("input")) {
                std::ostringstream raw;
                // reconstruct minimal json object
                raw << "{";
                bool f = true;
                for (const auto& kv : input->o) {
                    if (!f) raw << ",";
                    f = false;
                    raw << "\"" << json_escape(kv.first) << "\":";
                    if (kv.second.type == Json::Type::String) raw << "\"" << json_escape(kv.second.s) << "\"";
                    else if (kv.second.type == Json::Type::Bool) raw << (kv.second.b ? "true" : "false");
                    else if (kv.second.type == Json::Type::Number) raw << kv.second.n;
                    else raw << "null";
                }
                raw << "}";
                call.tool_args = raw.str();
            }
            msgs.push_back(call);
            {
                std::lock_guard<std::mutex> lock(g_app->agent.mu);
                g_app->agent.status = "Running " + call.tool_name + "...";
                g_app->agent.messages.push_back(call);
            }
            std::string result = execute_tool(call.tool_name, call.tool_args);
            ChatMessage tool;
            tool.role = "tool";
            tool.tool_id = call.tool_id;
            tool.tool_name = call.tool_name;
            tool.content = result;
            msgs.push_back(tool);
            {
                std::lock_guard<std::mutex> lock(g_app->agent.mu);
                g_app->agent.messages.push_back(tool);
            }
            text.clear();
        }
    }
    if (!used_tool) {
        ChatMessage out;
        out.role = "assistant";
        out.content = text;
        msgs.push_back(out);
        std::lock_guard<std::mutex> lock(g_app->agent.mu);
        g_app->agent.messages.push_back(out);
        g_app->agent.stream.clear();
    }
    return true;
}

static void agent_thread(AgentSettings cfg, std::string user_text, std::string system, bool agent_mode) {
    std::vector<ChatMessage> msgs;
    {
        std::lock_guard<std::mutex> lock(g_app->agent.mu);
        msgs = g_app->agent.messages;
    }
    ChatMessage user;
    user.role = "user";
    user.content = std::move(user_text);
    msgs.push_back(user);
    {
        std::lock_guard<std::mutex> lock(g_app->agent.mu);
        g_app->agent.messages.push_back(user);
        g_app->agent.status = "Thinking...";
        g_app->agent.error.clear();
        g_app->agent.stream.clear();
    }

    std::string err;
    bool ok = true;
    int hops = 0;
    while (ok && hops < 24 && !g_app->agent.cancel) {
        hops++;
        bool used_tools_before = false;
        size_t before = msgs.size();
        if (cfg.provider == AgentProvider::Anthropic) {
            ok = anthropic_request(cfg, msgs, system, agent_mode, err);
        } else {
            ok = openai_request(cfg, msgs, system, agent_mode, err);
        }
        if (!ok) break;
        used_tools_before = false;
        for (size_t i = before; i < msgs.size(); i++) {
            if (msgs[i].role == "tool") used_tools_before = true;
        }
        if (!used_tools_before) break;
        {
            std::lock_guard<std::mutex> lock(g_app->agent.mu);
            g_app->agent.status = "Continuing...";
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_app->agent.mu);
        if (!ok) g_app->agent.error = err;
        g_app->agent.status.clear();
        g_app->agent.busy = false;
        g_app->agent.cancel = false;
    }
}

void agent_send() {
    if (g_app->agent.busy) return;
    std::string text = g_app->agent_input;
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
    if (text.empty()) return;
    g_app->agent_input[0] = 0;
    g_app->agent.busy = true;
    g_app->agent.cancel = false;
    migrate_groq_model(g_app->settings.agent);
    AgentSettings cfg = g_app->settings.agent;
    std::string system = agent_system_prompt();
    if (cfg.mode == AgentMode::Agent && g_app->active >= 0) {
        const TextBuffer& b = g_app->buffers[(size_t)g_app->active];
        const size_t cap = (cfg.provider == AgentProvider::Groq) ? 2500 : 8000;
        std::string clip = b.text;
        if (clip.size() > cap) clip.resize(cap);
        system += "\n\n===== Active buffer (truncated) =====\n";
        system += clip;
    }
    bool agent_mode = cfg.mode == AgentMode::Agent;
    std::thread(agent_thread, cfg, text, system, agent_mode).detach();
}

