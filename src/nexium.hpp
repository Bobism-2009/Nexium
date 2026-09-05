#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define IMGUI_DEFINE_MATH_OPERATORS

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <cstdint>
#include <utility>

#include "imgui.h"

// -----------------------------------------------------------------------------
// Segoe MDL2 Assets / Segoe Fluent Icons — private-use area codepoints,
// encoded as raw UTF-8 byte sequences (portable across compilers).
// See:  https://learn.microsoft.com/windows/apps/design/style/segoe-fluent-icons-font
// -----------------------------------------------------------------------------
#define IC_EXPLORER    "\xee\xa2\xb7"  // U+E8B7 FolderList
#define IC_SEARCH      "\xee\x9c\xa1"  // U+E721 Search
#define IC_OUTLINE     "\xee\xa3\xbd"  // U+E8FD LineStyle
#define IC_RUN         "\xee\x9d\xa8"  // U+E768 Play
#define IC_STOP        "\xee\x9d\xa9"  // U+E769 Pause (using stop-like glyph)
#define IC_SETTINGS    "\xee\x9c\x93"  // U+E713 Settings
#define IC_CHEV_R      "\xee\x9d\xac"  // U+E76C ChevronRight
#define IC_CHEV_D      "\xee\x9c\x8d"  // U+E70D ChevronDown
#define IC_FOLDER      "\xee\xa0\xb8"  // U+E838 Folder
#define IC_FOLDER_OPEN "\xee\xa3\x9a"  // U+E8DA List (open folder feel)
#define IC_FILE        "\xee\x9f\x83"  // U+E7C3 Page
#define IC_ADD         "\xee\x9c\x90"  // U+E710 Add
#define IC_ADD_FOLDER  "\xee\xa0\xb8"  // U+E838 Folder (used for New Folder action)
#define IC_REFRESH     "\xee\x9c\xac"  // U+E72C Refresh
#define IC_DELETE      "\xee\x9d\x8d"  // U+E74D Delete
#define IC_EDIT        "\xee\x9c\x8f"  // U+E70F Edit
#define IC_SAVE        "\xee\x9d\x8e"  // U+E74E Save
#define IC_CLOSE       "\xee\xa2\x94"  // U+E894 ChromeClose
#define IC_CLOSE_SMALL "\xee\x9c\x91"  // U+E711 Cancel
#define IC_MORE        "\xee\x9c\x92"  // U+E712 MoreLegacy (…)
#define IC_ERROR       "\xee\xa4\xb0"  // U+E930 StatusErrorCircle
#define IC_WARN        "\xee\x9e\xba"  // U+E7BA Warning
#define IC_INFO        "\xee\xa5\x86"  // U+E946 Info
#define IC_TERMINAL    "\xee\x9d\x96"  // U+E756 CommandPrompt
#define IC_SEND        "\xee\x9c\xa4"  // U+E724 Send
#define IC_SPARKLE     "\xee\xa5\x85"  // U+E945 LightBulb (represents AI)
#define IC_MIN         "\xee\xa4\xa1"  // U+E921 ChromeMinimize
#define IC_MAX         "\xee\xa4\xa2"  // U+E922 ChromeMaximize
#define IC_RESTORE     "\xee\xa4\xa3"  // U+E923 ChromeRestore
#define IC_XCLOSE      "\xee\xa2\xbb"  // U+E8BB ChromeClose
#define IC_ERROR_S     "\xee\xa4\xb0"  // U+E930 StatusErrorCircle
#define IC_WARN_S      "\xee\x9e\xba"  // U+E7BA Warning (triangle)
#define IC_BELL        "\xee\x9e\x81"  // U+E781 Ringer / Bell
#define IC_BRANCH      "\xee\x9c\x8f"  // U+E70F Edit as fallback (not all fonts have git branch)
#define IC_MENU        "\xee\x9c\x80"  // U+E700 GlobalNavButton (hamburger)

struct App;

enum class HighlightKind {
    Text,
    Comment,
    Keyword,
    Control,
    Type,
    Constant,
    String,
    Escape,
    Number,
    Function,
    Identifier,
    Module,
    Member,
    Preproc,
    IncludePath,
    Operator,
    Punct,
    Builtin,
    TypeName,
};

enum class ActivityView {
    Explorer,
    Search,
    Outline,
    Run,
    Settings,
};

enum class AgentProvider {
    OpenAI,
    Anthropic,
    OpenRouter,
    Groq,
    Ollama,
    Custom,
};

enum class AgentMode {
    Chat,
    Agent,
};

enum class BottomTab {
    Terminal,
    Problems,
    Output,
};

enum class DefKind {
    Function,
    Struct,
    Enum,
    Variable,
};

struct HighlightSpan {
    int start = 0;
    int end = 0;
    HighlightKind kind = HighlightKind::Text;
};

struct UndoRec {
    int pos = 0;
    std::string removed;
    std::string inserted;
};

struct IndexedDef {
    std::string name;
    DefKind kind = DefKind::Variable;
    int line = 0;
    int name_start = 0;
    int name_end = 0;
};

struct Diagnostic {
    std::string path;
    int line = 1;
    std::string message;
};

struct SearchHit {
    std::string path;
    int line = 1;
    int col = 0;
    std::string preview;
};

struct FsNode {
    std::string name;
    std::string path;
    bool is_dir = false;
    bool expanded = false;
    bool scanned = false;
    std::vector<FsNode> children;
};

struct TextBuffer {
    std::string path;
    std::string name;
    std::string text;
    std::vector<int> lines;
    std::vector<HighlightSpan> spans;
    std::vector<IndexedDef> outline;
    std::vector<UndoRec> undo;
    int undo_pos = 0;
    int cursor = 0;
    int sel_anchor = 0;
    int preferred_col = 0;
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
    bool dirty = false;
    bool untitled = true;
    bool spans_dirty = true;
    bool outline_dirty = true;
    bool is_nexa = false;
    double last_edit = 0.0;
    double last_type = 0.0;
};

struct ChatMessage {
    std::string role;
    std::string content;
    std::string tool_id;
    std::string tool_name;
    std::string tool_args;
};

struct ProviderSlot {
    std::string api_key;
    std::string base_url;
    std::string model;
    std::string custom_model;
};

struct AgentSettings {
    AgentProvider provider = AgentProvider::OpenAI;
    AgentMode mode = AgentMode::Agent;
    float temperature = 0.2f;
    ProviderSlot slots[6];

    ProviderSlot& current() { return slots[(int)provider]; }
    const ProviderSlot& current() const { return slots[(int)provider]; }
};

struct AgentRuntime {
    std::mutex mu;
    std::vector<ChatMessage> messages;
    std::string stream;
    std::string status;
    std::string error;
    std::atomic<bool> busy{false};
    std::atomic<bool> cancel{false};
    std::vector<std::string> tool_open;
};

struct TermCell {
    uint32_t ch = 32;
    ImU32 fg = IM_COL32(0xCC, 0xCC, 0xCC, 255);
};

struct TermScreen {
    int cols = 80;
    int rows = 24;
    int cx = 0;
    int cy = 0;
    int saved_cx = 0;
    int saved_cy = 0;
    bool cursor_vis = true;
    ImU32 fg = IM_COL32(0xCC, 0xCC, 0xCC, 255);
    std::vector<TermCell> cells;
    std::vector<std::vector<TermCell>> scrollback;
    int parse = 0;
    bool csi_q = false;
    std::string seq;
    uint32_t utf = 0;
    int utf_need = 0;
    uint64_t gen = 0;
};

struct ProcJob {
    HANDLE process = nullptr;
    HANDLE stdout_rd = nullptr;
    HANDLE stdin_wr = nullptr;
    HANDLE thread = nullptr;
    void* pty = nullptr;
    std::mutex mu;
    std::string output;
    std::string typed;
    std::string cwd;
    TermScreen screen;
    std::atomic<bool> running{false};
    DWORD exit_code = 0;
    bool is_shell = false;
    bool use_pty = false;
    bool shutdown = false;
    int cols = 80;
    int rows = 24;
};

struct AppSettings {
    std::string folder;
    std::string nexac = "Nexa";
    std::string nexa_lang = "D:\\Projects\\Nexa-Lang";
    float sidebar_w = 268.0f;
    float agent_w = 400.0f;
    float panel_h = 220.0f;
    bool show_sidebar = true;
    bool show_agent = true;
    bool show_panel = true;
    AgentSettings agent;
};

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;

    const Json* get(const char* key) const;
    std::string str(const char* key, const char* def = "") const;
    double num(const char* key, double def = 0.0) const;
    bool boolean(const char* key, bool def = false) const;
    const Json* at(int i) const;
    int size() const;
};

struct App {
    HWND hwnd = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swap = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    UINT resize_w = 0;
    UINT resize_h = 0;
    bool occluded = false;

    ImFont* font_ui = nullptr;
    ImFont* font_ui_small = nullptr;
    ImFont* font_title = nullptr;
    ImFont* font_agent = nullptr;
    ImFont* font_agent_bold = nullptr;
    ImFont* font_agent_code = nullptr;
    ImFont* font_icon = nullptr;
    ImFont* font_code = nullptr;
    ID3D11ShaderResourceView* icon_srv = nullptr;

    AppSettings settings;
    std::vector<TextBuffer> buffers;
    int active = -1;
    int preview_tab = -1;

    ActivityView activity = ActivityView::Explorer;
    BottomTab bottom = BottomTab::Terminal;
    FsNode root;
    std::string selected_path;
    char search_query[256] = {};
    std::vector<SearchHit> search_hits;
    std::vector<Diagnostic> problems;
    double last_diag = 0.0;
    bool diag_dirty = false;

    bool editor_focused = false;
    bool terminal_focused = false;
    bool find_open = false;
    bool replace_open = false;
    bool goto_open = false;
    bool palette_open = false;
    bool settings_modal = false;
    char find_text[256] = {};
    char replace_text[256] = {};
    char goto_text[64] = {};
    char palette_text[256] = {};
    char new_name[256] = {};
    bool new_file_modal = false;
    bool new_folder_modal = false;
    bool rename_modal = false;
    std::string modal_parent;

    char agent_input[8192] = {};
    AgentRuntime agent;
    std::vector<std::string> extra_models;
    bool models_listed = false;

    ProcJob proc;
    char run_args[512] = {};

    bool confirm_close = false;
    int close_index = -1;
    bool want_quit = false;

    // Custom title bar
    bool maximized = false;
    bool title_hover_blocks_drag = false;   // set true when hovering menu / window buttons
    int  open_menu = -1;                    // which top-level menu is open (-1 = none)
    int  want_open_menu = -1;               // menu requested to open next frame
    float dpi = 1.0f;
};

extern App* g_app;

inline float dp(float v) {
    if (!g_app) return v;
    return (float)(int)(v * g_app->dpi + 0.5f);
}

std::wstring utf8_to_wide(const std::string& s);
std::string wide_to_utf8(const std::wstring& w);
std::string path_join(const std::string& a, const std::string& b);
std::string path_filename(const std::string& p);
std::string path_parent(const std::string& p);
std::string path_ext(const std::string& p);
std::string path_norm(const std::string& p);
bool path_is_nexa(const std::string& p);
bool file_read(const std::string& path, std::string& out, std::string* err = nullptr);
bool file_write(const std::string& path, const std::string& data, std::string* err = nullptr);
bool file_exists(const std::string& path);
bool dir_exists(const std::string& path);
bool make_dir(const std::string& path);
bool remove_path(const std::string& path);
bool rename_path(const std::string& from, const std::string& to);
std::vector<std::pair<std::string, bool>> list_dir(const std::string& path);
std::string appdata_dir();
std::string settings_path();
std::string now_iso();
std::string json_escape(const std::string& s);
std::string json_unescape(const std::string& s);
bool json_parse(const std::string& src, Json& out, std::string* err = nullptr);
std::string read_env(const char* name);
std::string which_exe(const std::string& name);
std::string nexa_compiler();
bool open_folder_dialog(std::string& out);
bool open_file_dialog(std::string& out);
bool save_file_dialog(std::string& out, const char* filter = nullptr);
void reveal_in_explorer(const std::string& path);
ImU32 highlight_color(HighlightKind k);
const char* highlight_name(HighlightKind k);
void highlight_nexa(const std::string& src, std::vector<HighlightSpan>& out);
void index_nexa(const std::string& src, std::vector<IndexedDef>& out);
void diagnose_nexa(const std::string& src, const std::string& path, std::vector<Diagnostic>& out);
std::vector<std::string> completions_for(const TextBuffer& buf, int cursor, std::string& prefix);

void buffer_rebuild_lines(TextBuffer& b);
void buffer_mark_nexa(TextBuffer& b);
void buffer_refresh(TextBuffer& b);
int buffer_line_at(const TextBuffer& b, int pos);
int buffer_col_at(const TextBuffer& b, int pos);
int buffer_pos_at(const TextBuffer& b, int line, int col);
void buffer_apply(TextBuffer& b, int pos, int remove, const std::string& insert, bool coalesce);
void buffer_undo(TextBuffer& b);
void buffer_redo(TextBuffer& b);
void buffer_ensure_sel(TextBuffer& b);
std::string buffer_selection(const TextBuffer& b);
void buffer_replace_sel(TextBuffer& b, const std::string& text);
void buffer_move(TextBuffer& b, int pos, bool select);
void buffer_move_line_col(TextBuffer& b, int line, int col, bool select);
void buffer_delete_sel(TextBuffer& b);
void buffer_indent(TextBuffer& b, bool back);
void buffer_toggle_comment(TextBuffer& b);
void buffer_find_next(TextBuffer& b, const char* query, bool reverse);
bool open_path(const std::string& path, bool preview = false);
bool save_buffer(int index, bool save_as = false);
bool save_all();
void close_buffer(int index, bool force = false);
void new_untitled();
void set_folder(const std::string& folder);
void scan_node(FsNode& node, bool force = false);
void draw_editor(const ImVec2& size);
void draw_explorer();
void draw_search();
void draw_outline();
void draw_problems();
void draw_run_view();
void draw_settings_panel();
void draw_agent_settings();

void ui_empty_state(const char* icon, const char* title, const char* hint);
bool ui_accent_button(const char* label, const ImVec2& size);
void ui_push_field();
void ui_pop_field();
void ui_begin_card(const char* id);
void ui_end_card();
bool ui_chip_row(const char* id, const char* icon, ImU32 icon_col,
                 const char* title, const char* detail = nullptr);
void refresh_problems();
void workspace_search(const std::string& query);

void proc_start(const std::string& cmdline, const std::string& cwd);
void proc_stop();
void proc_tick();
struct ProcCapture {
    std::string output;
    int exit_code = 0;
    bool timed_out = false;
    bool cancelled = false;
};
bool proc_run_capture(const std::string& command, const std::string& cwd,
                      DWORD timeout_ms, ProcCapture& out);
void term_ensure_shell(const std::string& cwd = {});
void term_write(const void* data, size_t n);
void term_write_local(const std::string& s);
void term_clear();
void term_poll_input();
void term_resize(int cols, int rows);
void draw_terminal(const ImVec2& size);
std::string term_plain_text();
void run_active(bool execute);
void build_folder();

void load_app_icon();
void release_app_icon();
void draw_app_icon(ImDrawList* dl, ImVec2 p, float sz, float alpha = 1.0f);

void agent_send();
void agent_stop();
void agent_new_chat();
void agent_apply_write(const std::string& path, const std::string& content);
std::string agent_system_prompt();
const char* provider_name(AgentProvider p);
const char* provider_id(AgentProvider p);
const char* provider_default_url(AgentProvider p);
void provider_models(AgentProvider p, std::vector<std::pair<std::string, std::string>>& out);
void apply_provider_defaults(AgentSettings& a);
void normalize_agent_settings(AgentSettings& a);
void agent_refresh_models();

void load_settings(AppSettings& s);
void save_settings(const AppSettings& s);
void apply_vscode_theme();
void draw_ide();
void ide_shortcuts();

std::string http_request(const std::string& method,
                         const std::string& url,
                         const std::vector<std::pair<std::string, std::string>>& headers,
                         const std::string& body,
                         int* status,
                         std::string* err,
                         const std::function<bool(const char*, size_t)>& on_chunk = nullptr);
