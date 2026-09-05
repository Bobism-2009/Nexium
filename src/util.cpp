#include "nexium.hpp"
#include "resource.h"

#include <shobjidl.h>
#include <shlobj.h>
#include <commdlg.h>
#include <winhttp.h>
#include <shellapi.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

App* g_app = nullptr;

std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string wide_to_utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::string path_join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char last = a.back();
    if (last == '\\' || last == '/') return a + b;
    return a + "\\" + b;
}

std::string path_filename(const std::string& p) {
    size_t i = p.find_last_of("\\/");
    return i == std::string::npos ? p : p.substr(i + 1);
}

std::string path_parent(const std::string& p) {
    size_t i = p.find_last_of("\\/");
    if (i == std::string::npos) return {};
    if (i == 2 && p.size() >= 2 && p[1] == ':') return p.substr(0, 3);
    return p.substr(0, i);
}

std::string path_ext(const std::string& p) {
    std::string name = path_filename(p);
    size_t i = name.find_last_of('.');
    if (i == std::string::npos) return {};
    std::string e = name.substr(i);
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}

std::string path_norm(const std::string& p) {
    std::error_code ec;
    auto abs = std::filesystem::weakly_canonical(std::filesystem::u8path(p), ec);
    if (ec) {
        std::string o = p;
        for (char& c : o) if (c == '/') c = '\\';
        return o;
    }
    return wide_to_utf8(abs.wstring());
}

bool path_is_nexa(const std::string& p) {
    return path_ext(p) == ".nxa";
}

bool file_read(const std::string& path, std::string& out, std::string* err) {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) {
        if (err) *err = "Could not open " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF) {
        out.erase(0, 3);
    }
    std::string norm;
    norm.reserve(out.size());
    for (size_t i = 0; i < out.size(); i++) {
        if (out[i] == '\r') {
            if (i + 1 < out.size() && out[i + 1] == '\n') continue;
            norm += '\n';
        } else {
            norm += out[i];
        }
    }
    out.swap(norm);
    return true;
}

bool file_write(const std::string& path, const std::string& data, std::string* err) {
    std::ofstream out(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
    if (!out) {
        if (err) *err = "Could not write " + path;
        return false;
    }
    out.write(data.data(), (std::streamsize)data.size());
    return (bool)out;
}

bool file_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::u8path(path), ec);
}

bool dir_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::u8path(path), ec);
}

bool make_dir(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(path), ec);
    return !ec;
}

bool remove_path(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::u8path(path), ec);
    return !ec;
}

bool rename_path(const std::string& from, const std::string& to) {
    std::error_code ec;
    std::filesystem::rename(std::filesystem::u8path(from), std::filesystem::u8path(to), ec);
    return !ec;
}

std::vector<std::pair<std::string, bool>> list_dir(const std::string& path) {
    std::vector<std::pair<std::string, bool>> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(std::filesystem::u8path(path), ec)) {
        std::string name = wide_to_utf8(e.path().filename().wstring());
        if (name.empty() || name[0] == '.') {
            if (name != ".nexa") continue;
        }
        bool is_dir = e.is_directory(ec);
        out.emplace_back(name, is_dir);
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        std::string la = a.first, lb = b.first;
        for (char& c : la) c = (char)std::tolower((unsigned char)c);
        for (char& c : lb) c = (char)std::tolower((unsigned char)c);
        return la < lb;
    });
    return out;
}

std::string appdata_dir() {
    wchar_t* w = nullptr;
    std::string dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &w)) && w) {
        dir = path_join(wide_to_utf8(w), "Nexium");
        CoTaskMemFree(w);
    } else {
        dir = path_join(read_env("APPDATA"), "Nexium");
    }
    make_dir(dir);
    return dir;
}

std::string settings_path() {
    return path_join(appdata_dir(), "settings.json");
}

std::string now_iso() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '\"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\b': o += "\\b"; break;
        case '\f': o += "\\f"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) {
                char hex[8];
                std::snprintf(hex, sizeof(hex), "\\u%04x", c);
                o += hex;
            } else {
                o += (char)c;
            }
            break;
        }
    }
    return o;
}

std::string json_unescape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] != '\\' || i + 1 >= s.size()) {
            o += s[i];
            continue;
        }
        char e = s[++i];
        switch (e) {
        case '"': o += '"'; break;
        case '\\': o += '\\'; break;
        case '/': o += '/'; break;
        case 'b': o += '\b'; break;
        case 'f': o += '\f'; break;
        case 'n': o += '\n'; break;
        case 'r': o += '\r'; break;
        case 't': o += '\t'; break;
        case 'u':
            if (i + 4 < s.size()) {
                int code = 0;
                for (int k = 0; k < 4; k++) {
                    char h = s[++i];
                    code <<= 4;
                    if (h >= '0' && h <= '9') code += h - '0';
                    else if (h >= 'a' && h <= 'f') code += h - 'a' + 10;
                    else if (h >= 'A' && h <= 'F') code += h - 'A' + 10;
                }
                if (code < 0x80) o += (char)code;
                else if (code < 0x800) {
                    o += (char)(0xC0 | (code >> 6));
                    o += (char)(0x80 | (code & 0x3F));
                } else {
                    o += (char)(0xE0 | (code >> 12));
                    o += (char)(0x80 | ((code >> 6) & 0x3F));
                    o += (char)(0x80 | (code & 0x3F));
                }
            }
            break;
        default:
            o += e;
            break;
        }
    }
    return o;
}

static void skip_ws(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
}

static bool parse_json_value(const std::string& s, size_t& i, Json& out, std::string* err);

static bool parse_json_string(const std::string& s, size_t& i, std::string& out, std::string* err) {
    if (i >= s.size() || s[i] != '"') {
        if (err) *err = "expected string";
        return false;
    }
    i++;
    std::string raw;
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') {
            out = json_unescape(raw);
            return true;
        }
        raw += c;
        if (c == '\\' && i < s.size()) raw += s[i++];
    }
    if (err) *err = "unterminated string";
    return false;
}

static bool parse_json_value(const std::string& s, size_t& i, Json& out, std::string* err) {
    skip_ws(s, i);
    if (i >= s.size()) {
        if (err) *err = "unexpected end of json";
        return false;
    }
    char c = s[i];
    if (c == '"') {
        out.type = Json::Type::String;
        return parse_json_string(s, i, out.s, err);
    }
    if (c == '{') {
        out.type = Json::Type::Object;
        i++;
        skip_ws(s, i);
        if (i < s.size() && s[i] == '}') {
            i++;
            return true;
        }
        while (i < s.size()) {
            skip_ws(s, i);
            std::string key;
            if (!parse_json_string(s, i, key, err)) return false;
            skip_ws(s, i);
            if (i >= s.size() || s[i] != ':') {
                if (err) *err = "expected ':'";
                return false;
            }
            i++;
            Json val;
            if (!parse_json_value(s, i, val, err)) return false;
            out.o.emplace_back(std::move(key), std::move(val));
            skip_ws(s, i);
            if (i < s.size() && s[i] == ',') {
                i++;
                continue;
            }
            if (i < s.size() && s[i] == '}') {
                i++;
                return true;
            }
            if (err) *err = "expected ',' or '}'";
            return false;
        }
        if (err) *err = "unterminated object";
        return false;
    }
    if (c == '[') {
        out.type = Json::Type::Array;
        i++;
        skip_ws(s, i);
        if (i < s.size() && s[i] == ']') {
            i++;
            return true;
        }
        while (i < s.size()) {
            Json val;
            if (!parse_json_value(s, i, val, err)) return false;
            out.a.push_back(std::move(val));
            skip_ws(s, i);
            if (i < s.size() && s[i] == ',') {
                i++;
                continue;
            }
            if (i < s.size() && s[i] == ']') {
                i++;
                return true;
            }
            if (err) *err = "expected ',' or ']'";
            return false;
        }
        if (err) *err = "unterminated array";
        return false;
    }
    if (s.compare(i, 4, "true") == 0) {
        out.type = Json::Type::Bool;
        out.b = true;
        i += 4;
        return true;
    }
    if (s.compare(i, 5, "false") == 0) {
        out.type = Json::Type::Bool;
        out.b = false;
        i += 5;
        return true;
    }
    if (s.compare(i, 4, "null") == 0) {
        out.type = Json::Type::Null;
        i += 4;
        return true;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        size_t start = i;
        if (s[i] == '-') i++;
        while (i < s.size() && std::isdigit((unsigned char)s[i])) i++;
        if (i < s.size() && s[i] == '.') {
            i++;
            while (i < s.size() && std::isdigit((unsigned char)s[i])) i++;
        }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            i++;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) i++;
            while (i < s.size() && std::isdigit((unsigned char)s[i])) i++;
        }
        out.type = Json::Type::Number;
        out.n = std::strtod(s.c_str() + start, nullptr);
        return true;
    }
    if (err) *err = std::string("unexpected '") + c + "'";
    return false;
}

bool json_parse(const std::string& src, Json& out, std::string* err) {
    size_t i = 0;
    out = Json{};
    if (!parse_json_value(src, i, out, err)) return false;
    skip_ws(src, i);
    return true;
}

const Json* Json::get(const char* key) const {
    if (type != Type::Object) return nullptr;
    for (const auto& kv : o) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

std::string Json::str(const char* key, const char* def) const {
    const Json* j = get(key);
    if (!j) return def;
    if (j->type == Type::String) return j->s;
    if (j->type == Type::Number) return std::to_string(j->n);
    if (j->type == Type::Bool) return j->b ? "true" : "false";
    return def;
}

double Json::num(const char* key, double def) const {
    const Json* j = get(key);
    if (!j) return def;
    if (j->type == Type::Number) return j->n;
    if (j->type == Type::String) return std::strtod(j->s.c_str(), nullptr);
    if (j->type == Type::Bool) return j->b ? 1.0 : 0.0;
    return def;
}

bool Json::boolean(const char* key, bool def) const {
    const Json* j = get(key);
    if (!j) return def;
    if (j->type == Type::Bool) return j->b;
    if (j->type == Type::Number) return j->n != 0;
    if (j->type == Type::String) return j->s == "true" || j->s == "1";
    return def;
}

const Json* Json::at(int i) const {
    if (type != Type::Array || i < 0 || i >= (int)a.size()) return nullptr;
    return &a[(size_t)i];
}

int Json::size() const {
    if (type == Type::Array) return (int)a.size();
    if (type == Type::Object) return (int)o.size();
    return 0;
}

std::string read_env(const char* name) {
    DWORD n = GetEnvironmentVariableA(name, nullptr, 0);
    if (n == 0) return {};
    std::string s(n, '\0');
    GetEnvironmentVariableA(name, s.data(), n);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

std::string which_exe(const std::string& name) {
    char buf[MAX_PATH];
    DWORD n = SearchPathA(nullptr, name.c_str(), ".exe", MAX_PATH, buf, nullptr);
    if (n > 0 && n < MAX_PATH) return buf;
    return {};
}

static bool looks_like_path(const std::string& s) {
    return s.find('\\') != std::string::npos || s.find('/') != std::string::npos ||
           s.find(':') != std::string::npos;
}

std::string nexa_compiler() {
    std::vector<std::string> names;
    if (g_app && !g_app->settings.nexac.empty() && !looks_like_path(g_app->settings.nexac)) {
        names.push_back(g_app->settings.nexac);
    }
    names.push_back("Nexa");
    names.push_back("NexaC");
    names.push_back("nexac");
    for (const std::string& name : names) {
        std::string found = which_exe(name);
        if (!found.empty()) return found;
    }
    return "Nexa";
}

static bool file_dialog(bool save, bool folder, std::string& out, const char* filter) {
    IFileDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
                                  nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return false;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM;
    if (folder) opts |= FOS_PICKFOLDERS;
    dlg->SetOptions(opts);
    if (!folder && filter) {
        COMDLG_FILTERSPEC spec[2];
        spec[0].pszName = L"Nexa files";
        spec[0].pszSpec = L"*.nxa";
        spec[1].pszName = L"All files";
        spec[1].pszSpec = L"*.*";
        dlg->SetFileTypes(2, spec);
    }
    hr = dlg->Show(g_app ? g_app->hwnd : nullptr);
    bool ok = false;
    if (SUCCEEDED(hr)) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR w = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &w)) && w) {
                out = wide_to_utf8(w);
                CoTaskMemFree(w);
                ok = true;
            }
            item->Release();
        }
    }
    dlg->Release();
    return ok;
}

bool open_folder_dialog(std::string& out) {
    return file_dialog(false, true, out, nullptr);
}

bool open_file_dialog(std::string& out) {
    return file_dialog(false, false, out, "*.nxa");
}

bool save_file_dialog(std::string& out, const char* filter) {
    return file_dialog(true, false, out, filter ? filter : "*.nxa");
}

void reveal_in_explorer(const std::string& path) {
    std::wstring w = utf8_to_wide(path);
    std::wstring args = L"/select,\"" + w + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
}

static std::string json_get_string_raw(const std::string& src, const char* key, const std::string& def) {
    Json j;
    if (!json_parse(src, j)) return def;
    return j.str(key, def.c_str());
}

static AgentProvider parse_provider(const std::string& id) {
    if (id == "anthropic") return AgentProvider::Anthropic;
    if (id == "openrouter") return AgentProvider::OpenRouter;
    if (id == "groq") return AgentProvider::Groq;
    if (id == "ollama") return AgentProvider::Ollama;
    if (id == "custom") return AgentProvider::Custom;
    return AgentProvider::OpenAI;
}

static const char* env_key_for(AgentProvider p) {
    switch (p) {
    case AgentProvider::Anthropic: return "ANTHROPIC_API_KEY";
    case AgentProvider::OpenRouter: return "OPENROUTER_API_KEY";
    case AgentProvider::Groq: return "GROQ_API_KEY";
    case AgentProvider::OpenAI:
    case AgentProvider::Ollama:
    case AgentProvider::Custom:
    default:
        return "OPENAI_API_KEY";
    }
}

static const AgentProvider kAllProviders[] = {
    AgentProvider::OpenAI, AgentProvider::Anthropic, AgentProvider::OpenRouter,
    AgentProvider::Groq, AgentProvider::Ollama, AgentProvider::Custom
};

static void load_slot_json(const Json* obj, ProviderSlot& slot) {
    if (!obj) return;
    slot.api_key = obj->str("api_key");
    slot.base_url = obj->str("base_url");
    slot.model = obj->str("model");
    slot.custom_model = obj->str("custom_model");
}

static void finish_agent_slots(AgentSettings& a) {
    AgentProvider saved = a.provider;
    for (AgentProvider p : kAllProviders) {
        a.provider = p;
        apply_provider_defaults(a);
        if (a.current().api_key.empty())
            a.current().api_key = read_env(env_key_for(p));
    }
    a.provider = saved;
    apply_provider_defaults(a);
    normalize_agent_settings(a);
}

void load_settings(AppSettings& s) {
    std::string raw;
    if (!file_read(settings_path(), raw)) {
        if (!read_env("GROQ_API_KEY").empty() && read_env("OPENAI_API_KEY").empty())
            s.agent.provider = AgentProvider::Groq;
        else if (!read_env("ANTHROPIC_API_KEY").empty() && read_env("OPENAI_API_KEY").empty())
            s.agent.provider = AgentProvider::Anthropic;
        else if (!read_env("OPENROUTER_API_KEY").empty() && read_env("OPENAI_API_KEY").empty())
            s.agent.provider = AgentProvider::OpenRouter;
        finish_agent_slots(s.agent);
        return;
    }
    Json j;
    if (!json_parse(raw, j)) return;
    s.folder = j.str("folder");
    s.nexac = j.str("nexac", "Nexa");
    if (looks_like_path(s.nexac) || s.nexac.empty()) s.nexac = "Nexa";
    s.nexa_lang = j.str("nexa_lang", "D:\\Projects\\Nexa-Lang");
    s.sidebar_w = (float)j.num("sidebar_w", 268);
    s.agent_w = (float)j.num("agent_w", 400);
    s.panel_h = (float)j.num("panel_h", 220);
    s.show_sidebar = j.boolean("show_sidebar", true);
    s.show_agent = j.boolean("show_agent", true);
    s.show_panel = j.boolean("show_panel", true);
    s.agent.temperature = (float)j.num("temperature", 0.2);
    s.agent.provider = parse_provider(j.str("provider", "openai"));
    s.agent.mode = (j.str("mode", "agent") == "chat") ? AgentMode::Chat : AgentMode::Agent;

    if (const Json* providers = j.get("providers")) {
        for (AgentProvider p : kAllProviders) {
            if (const Json* slot = providers->get(provider_id(p)))
                load_slot_json(slot, s.agent.slots[(int)p]);
        }
    } else {
        ProviderSlot& cur = s.agent.current();
        cur.api_key = j.str("api_key");
        cur.base_url = j.str("base_url");
        cur.model = j.str("model");
        cur.custom_model = j.str("custom_model");
    }
    finish_agent_slots(s.agent);
    (void)json_get_string_raw;
}

void save_settings(const AppSettings& s) {
    std::ostringstream o;
    o << "{\n"
      << "  \"folder\": \"" << json_escape(s.folder) << "\",\n"
      << "  \"nexac\": \"" << json_escape(s.nexac) << "\",\n"
      << "  \"nexa_lang\": \"" << json_escape(s.nexa_lang) << "\",\n"
      << "  \"sidebar_w\": " << s.sidebar_w << ",\n"
      << "  \"agent_w\": " << s.agent_w << ",\n"
      << "  \"panel_h\": " << s.panel_h << ",\n"
      << "  \"show_sidebar\": " << (s.show_sidebar ? "true" : "false") << ",\n"
      << "  \"show_agent\": " << (s.show_agent ? "true" : "false") << ",\n"
      << "  \"show_panel\": " << (s.show_panel ? "true" : "false") << ",\n"
      << "  \"provider\": \"" << provider_id(s.agent.provider) << "\",\n"
      << "  \"mode\": \"" << (s.agent.mode == AgentMode::Chat ? "chat" : "agent") << "\",\n"
      << "  \"temperature\": " << s.agent.temperature << ",\n"
      << "  \"providers\": {\n";
    for (int i = 0; i < 6; i++) {
        AgentProvider p = kAllProviders[i];
        const ProviderSlot& slot = s.agent.slots[i];
        o << "    \"" << provider_id(p) << "\": {"
          << "\"api_key\": \"" << json_escape(slot.api_key) << "\", "
          << "\"base_url\": \"" << json_escape(slot.base_url) << "\", "
          << "\"model\": \"" << json_escape(slot.model) << "\", "
          << "\"custom_model\": \"" << json_escape(slot.custom_model) << "\"}";
        o << (i < 5 ? ",\n" : "\n");
    }
    o << "  }\n"
      << "}\n";
    file_write(settings_path(), o.str());
}

static bool crack_url(const std::string& url, bool& https, std::wstring& host, INTERNET_PORT& port, std::wstring& path) {
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host_b[256];
    wchar_t path_b[2048];
    uc.lpszHostName = host_b;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path_b;
    uc.dwUrlPathLength = 2048;
    std::wstring wurl = utf8_to_wide(url);
    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) return false;
    https = uc.nScheme == INTERNET_SCHEME_HTTPS;
    host.assign(uc.lpszHostName, uc.dwHostNameLength);
    path.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
    port = uc.nPort;
    return true;
}

std::string http_request(const std::string& method,
                         const std::string& url,
                         const std::vector<std::pair<std::string, std::string>>& headers,
                         const std::string& body,
                         int* status,
                         std::string* err,
                         const std::function<bool(const char*, size_t)>& on_chunk) {
    if (status) *status = 0;
    bool https = false;
    std::wstring host, path;
    INTERNET_PORT port = 0;
    if (!crack_url(url, https, host, port, path)) {
        if (err) *err = "Invalid URL: " + url;
        return {};
    }
    HINTERNET session = WinHttpOpen(L"Nexium/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        if (err) *err = "WinHttpOpen failed";
        return {};
    }
    DWORD timeout = 120000;
    WinHttpSetTimeouts(session, timeout, timeout, timeout, timeout);
    HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
    if (!connect) {
        if (err) *err = "WinHttpConnect failed";
        WinHttpCloseHandle(session);
        return {};
    }
    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    std::wstring wmethod = utf8_to_wide(method);
    HINTERNET request = WinHttpOpenRequest(connect, wmethod.c_str(), path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        if (err) *err = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return {};
    }
    std::wstring hdr;
    for (const auto& h : headers) {
        hdr += utf8_to_wide(h.first);
        hdr += L": ";
        hdr += utf8_to_wide(h.second);
        hdr += L"\r\n";
    }
    if (!hdr.empty()) {
        WinHttpAddRequestHeaders(request, hdr.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    }
    BOOL sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
                                   (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        if (err) *err = "HTTP request failed";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return {};
    }
    DWORD code = 0, clen = sizeof(code);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &code, &clen, WINHTTP_NO_HEADER_INDEX);
    if (status) *status = (int)code;
    std::string out;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(request, &avail)) break;
        if (avail == 0) break;
        std::string chunk(avail, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), avail, &read) || read == 0) break;
        if (on_chunk) {
            if (!on_chunk(chunk.data(), read)) break;
        }
        out.append(chunk.data(), read);
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return out;
}

void load_app_icon() {
    release_app_icon();
    if (!g_app || !g_app->device) return;
    HICON icon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_NEXIUM),
                                   IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR);
    if (!icon) return;
    ICONINFO ii{};
    if (!GetIconInfo(icon, &ii)) {
        DestroyIcon(icon);
        return;
    }
    BITMAP bm{};
    GetObject(ii.hbmColor, sizeof(bm), &bm);
    const int w = bm.bmWidth;
    const int h = bm.bmHeight;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    std::vector<unsigned char> pixels((size_t)w * (size_t)h * 4);
    HDC dc = GetDC(nullptr);
    GetDIBits(dc, ii.hbmColor, 0, (UINT)h, pixels.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    DestroyIcon(icon);
    for (int i = 0; i < w * h; i++) {
        unsigned char* p = pixels.data() + (size_t)i * 4;
        unsigned char t = p[0];
        p[0] = p[2];
        p[2] = t;
    }
    D3D11_TEXTURE2D_DESC td{};
    td.Width = (UINT)w;
    td.Height = (UINT)h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sr{};
    sr.pSysMem = pixels.data();
    sr.SysMemPitch = (UINT)w * 4;
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(g_app->device->CreateTexture2D(&td, &sr, &tex))) return;
    g_app->device->CreateShaderResourceView(tex, nullptr, &g_app->icon_srv);
    tex->Release();
}

void release_app_icon() {
    if (!g_app || !g_app->icon_srv) return;
    g_app->icon_srv->Release();
    g_app->icon_srv = nullptr;
}

void draw_app_icon(ImDrawList* dl, ImVec2 p, float sz, float alpha) {
    if (!dl || sz < 4.0f) return;
    const int a = (int)(alpha * 255.0f);
    const ImU32 n = IM_COL32(0xCC, 0xCC, 0xCC, a);
    const ImU32 accent = IM_COL32(0x00, 0x7A, 0xCC, a);
    const float t = (std::max)(1.5f, sz * 0.16f);
    const float x0 = p.x + sz * 0.18f;
    const float x1 = p.x + sz - sz * 0.14f;
    const float y0 = p.y + sz * 0.10f;
    const float y1 = p.y + sz - sz * 0.10f;
    dl->AddRectFilled(ImVec2(x0 - t * 0.55f, y0), ImVec2(x0 - t * 0.12f, y1), accent, 0.4f);
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + t, y1), n, 0.4f);
    dl->AddRectFilled(ImVec2(x1 - t, y0), ImVec2(x1, y1), n, 0.4f);
    dl->AddQuadFilled(ImVec2(x0, y0), ImVec2(x0 + t, y0),
                      ImVec2(x1, y1), ImVec2(x1 - t, y1), n);
}
