#include "nexium.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "imgui_internal.h"

void scan_node(FsNode& node, bool force) {
    if (!node.is_dir) return;
    if (node.scanned && !force) return;
    node.children.clear();
    for (const auto& e : list_dir(node.path)) {
        FsNode child;
        child.name = e.first;
        child.path = path_join(node.path, e.first);
        child.is_dir = e.second;
        node.children.push_back(std::move(child));
    }
    node.scanned = true;
}

void set_folder(const std::string& folder) {
    g_app->settings.folder = path_norm(folder);
    g_app->root = FsNode{};
    g_app->root.name = path_filename(g_app->settings.folder);
    if (g_app->root.name.empty()) g_app->root.name = g_app->settings.folder;
    g_app->root.path = g_app->settings.folder;
    g_app->root.is_dir = true;
    g_app->root.expanded = true;
    scan_node(g_app->root, true);
    save_settings(g_app->settings);
    if (g_app->proc.is_shell && g_app->proc.running) {
        g_app->proc.shutdown = true;
        proc_stop();
        g_app->proc.shutdown = false;
        term_ensure_shell(g_app->settings.folder);
    }
    std::string title = g_app->root.name + " - Nexium";
    SetWindowTextW(g_app->hwnd, utf8_to_wide(title).c_str());
}

static bool skip_dir(const std::string& name) {
    return name == "node_modules" || name == "dist" || name == "build" || name == ".git" ||
           name == ".vs" || name == ".vscode" || name == "__pycache__" || name == ".cache" ||
           name == "out" || name == "obj" || name == ".nexium";
}

static void collect_files(const std::string& dir, std::vector<std::string>& out, bool text_only,
                          int depth = 0) {
    if (depth > 10 || out.size() >= 20000) return;
    for (const auto& e : list_dir(dir)) {
        std::string p = path_join(dir, e.first);
        if (e.second) {
            if (skip_dir(e.first)) continue;
            collect_files(p, out, text_only, depth + 1);
        } else if (!text_only || lang_for_path(p) != Lang::Plain || path_ext(p) == ".txt") {
            out.push_back(p);
        }
    }
}

void workspace_files(std::vector<std::string>& out) {
    out.clear();
    if (g_app->settings.folder.empty()) return;
    collect_files(g_app->settings.folder, out, false);
}

void workspace_search(const std::string& query) {
    g_app->search_hits.clear();
    if (query.empty() || g_app->settings.folder.empty()) return;
    std::vector<std::string> files;
    collect_files(g_app->settings.folder, files, true);
    for (const auto& path : files) {
        std::string text;
        if (!file_read(path, text)) continue;
        int line = 1;
        int col = 0;
        for (int i = 0; i < (int)text.size(); i++) {
            if (text.compare((size_t)i, query.size(), query) == 0) {
                int ls = i;
                while (ls > 0 && text[(size_t)ls - 1] != '\n') ls--;
                int le = i;
                while (le < (int)text.size() && text[(size_t)le] != '\n') le++;
                SearchHit h;
                h.path = path;
                h.line = line;
                h.col = col;
                h.preview = text.substr((size_t)ls, (size_t)(le - ls));
                g_app->search_hits.push_back(std::move(h));
                if (g_app->search_hits.size() > 400) return;
            }
            if (text[(size_t)i] == '\n') {
                line++;
                col = 0;
            } else {
                col++;
            }
        }
    }
}

void refresh_problems() {
    g_app->problems.clear();
    auto add_buf = [&](const TextBuffer& b) {
        if (b.lang != Lang::Nexa) return;
        std::vector<Diagnostic> ds;
        diagnose_nexa(b.text, b.untitled ? b.name : b.path, ds);
        for (auto& d : ds) g_app->problems.push_back(std::move(d));
    };
    if (g_app->active >= 0 && g_app->active < (int)g_app->buffers.size()) {
        add_buf(g_app->buffers[(size_t)g_app->active]);
    }
    g_app->diag_dirty = false;
    g_app->last_diag = ImGui::GetTime();
}

// ---------------------------------------------------------------------------
// File tree — custom drawn rows with folder/file icons and chevrons.
// ---------------------------------------------------------------------------
namespace vs_col {
    static const ImU32 row_hover    = IM_COL32(0x2A, 0x2D, 0x2E, 0xFF);
    static const ImU32 row_selected = IM_COL32(0x37, 0x37, 0x3D, 0xFF);
    static const ImU32 row_active   = IM_COL32(0x04, 0x39, 0x5E, 0xFF);
    static const ImU32 row_border   = IM_COL32(0x00, 0x78, 0xD4, 0xFF);
    static const ImU32 guide        = IM_COL32(0x58, 0x58, 0x58, 0x60);
    static const ImU32 folder_ic    = IM_COL32(0xC4, 0xA0, 0x58, 0xFF);
    static const ImU32 nexa_ic      = IM_COL32(0x7E, 0xC4, 0xE0, 0xFF);
    static const ImU32 file_ic      = IM_COL32(0x9A, 0x9A, 0x9A, 0xFF);
    static const ImU32 chevron      = IM_COL32(0x7A, 0x7A, 0x7A, 0xFF);
    static const ImU32 text         = IM_COL32(0xCC, 0xCC, 0xCC, 0xFF);
}

static ImU32 file_icon_color(const std::string& path) {
    std::string e = path_ext(path);
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    if (e == ".nxa" || e == ".nx") return vs_col::nexa_ic;
    if (e == ".md")                 return IM_COL32(0x9C, 0xDC, 0xFE, 0xFF);
    if (e == ".json" || e == ".yml" || e == ".yaml" || e == ".toml")
        return IM_COL32(0xE2, 0xC0, 0x8D, 0xFF);
    if (e == ".cpp" || e == ".cc" || e == ".hpp" || e == ".h" || e == ".c")
        return IM_COL32(0xC5, 0x93, 0xE7, 0xFF);
    return vs_col::file_ic;
}

struct FsRow {
    FsNode* node;
    int depth;
};

static void collect_rows(FsNode& node, int depth, std::vector<FsRow>& out) {
    out.push_back({&node, depth});
    if (node.is_dir && node.expanded && node.scanned) {
        for (auto& c : node.children) collect_rows(c, depth + 1, out);
    }
}

static void draw_fs_tree() {
    if (!g_app->root.scanned) scan_node(g_app->root, true);
    std::vector<FsRow> rows;
    for (auto& c : g_app->root.children) collect_rows(c, 0, rows);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float row_h = dp(22.0f);
    const float indent = dp(8.0f) + dp(8.0f);
    const float chev_w = dp(16.0f);
    const float icon_w = dp(20.0f);
    const float pad_l  = dp(8.0f);
    // The open editor's file is highlighted in the tree, as in VS Code.
    std::string open_path_norm;
    if (g_app->active >= 0 && !g_app->buffers[(size_t)g_app->active].untitled)
        open_path_norm = path_norm(g_app->buffers[(size_t)g_app->active].path);

    // Clip to visible rows for perf
    ImGuiListClipper clip;
    clip.Begin((int)rows.size(), row_h);
    while (clip.Step()) {
        for (int idx = clip.DisplayStart; idx < clip.DisplayEnd; idx++) {
            FsRow& r = rows[(size_t)idx];
            FsNode& n = *r.node;
            ImGui::PushID(n.path.c_str());
            ImVec2 c = ImGui::GetCursorScreenPos();
            bool selected = g_app->selected_path == n.path;
            ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowDoubleClick,
                              ImVec2(0, row_h));
            bool hovered = ImGui::IsItemHovered();
            bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            bool dbl = hovered && ImGui::IsMouseDoubleClicked(0);
            float row_w = ImGui::GetItemRectSize().x;

            // Row background: flat, full width.
            bool is_open = !n.is_dir && !open_path_norm.empty() && path_norm(n.path) == open_path_norm;
            ImU32 bg = 0;
            if (selected && g_app->editor_focused == false) bg = vs_col::row_active;
            else if (selected || is_open) bg = vs_col::row_selected;
            else if (hovered)   bg = vs_col::row_hover;
            if (bg) dl->AddRectFilled(ImVec2(c.x, c.y), ImVec2(c.x + row_w, c.y + row_h), bg);
            if (selected && !g_app->editor_focused)
                dl->AddRect(ImVec2(c.x, c.y), ImVec2(c.x + row_w, c.y + row_h), vs_col::row_border);

            // Indent guides, one per ancestor level.
            for (int d = 0; d < r.depth; d++) {
                float gx = (float)(int)(c.x + pad_l + d * indent + chev_w * 0.5f) + 0.5f;
                dl->AddLine(ImVec2(gx, c.y), ImVec2(gx, c.y + row_h), vs_col::guide);
            }

            // Content x
            float x = c.x + pad_l + r.depth * indent;

            // Chevron for folders (empty space for files, VS Code style)
            if (n.is_dir) {
                const char* chev = n.expanded ? IC_CHEV_D : IC_CHEV_R;
                ImVec2 ts = ImGui::CalcTextSize(chev);
                dl->AddText(ImVec2(x + (chev_w - ts.x) * 0.5f, c.y + (row_h - ts.y) * 0.5f),
                            vs_col::chevron, chev);
            }
            x += chev_w;

            const char* icon = n.is_dir
                                ? (n.expanded ? IC_FOLDER_OPEN : IC_FOLDER)
                                : IC_FILE;
            ImU32 ic_col = n.is_dir ? vs_col::folder_ic : file_icon_color(n.path);
            dl->AddText(ImVec2(x + 1, c.y + (row_h - ImGui::GetFontSize()) * 0.5f),
                        ic_col, icon);
            x += icon_w;

            // Label
            dl->AddText(ImVec2(x, c.y + (row_h - ImGui::GetFontSize()) * 0.5f),
                        vs_col::text, n.name.c_str());

            // Click handling
            if (clicked) {
                g_app->selected_path = n.path;
                if (n.is_dir) {
                    n.expanded = !n.expanded;
                    if (n.expanded) scan_node(n);
                } else {
                    open_path(n.path, true);
                }
            }
            if (dbl && !n.is_dir) open_path(n.path, false);

            // Context menu
            if (ImGui::BeginPopupContextItem("fsctx")) {
                g_app->selected_path = n.path;
                if (ImGui::MenuItem("New File")) {
                    g_app->new_file_modal = true;
                    g_app->modal_parent = n.is_dir ? n.path : path_parent(n.path);
                    std::strcpy(g_app->new_name, "untitled.nxa");
                }
                if (ImGui::MenuItem("New Folder")) {
                    g_app->new_folder_modal = true;
                    g_app->modal_parent = n.is_dir ? n.path : path_parent(n.path);
                    std::strcpy(g_app->new_name, "new-folder");
                }
                if (ImGui::MenuItem("Rename")) {
                    g_app->rename_modal = true;
                    g_app->modal_parent = n.path;
                    std::strcpy(g_app->new_name, n.name.c_str());
                }
                if (ImGui::MenuItem("Delete")) {
                    remove_path(n.path);
                    scan_node(g_app->root, true);
                }
                if (ImGui::MenuItem("Reveal in Explorer")) reveal_in_explorer(n.path);
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }
    clip.End();
}

void draw_explorer() {
    if (g_app->settings.folder.empty()) {
        ImGui::Indent(16);
        ui_empty_state(IC_FOLDER,
                       "Open a folder to start working in this workspace.",
                       "Nexium will show the files here. Opening a folder keeps your editors.");
        ImGui::Dummy(ImVec2(1, 12));
        if (ui_accent_button("Open Folder", ImVec2(-FLT_MIN, 32))) {
            std::string folder;
            if (open_folder_dialog(folder)) set_folder(folder);
        }
        ImGui::Unindent(16);
        return;
    }
    // Collapsible workspace section, titled with the folder name like VS Code.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 c = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    std::string folder_name = path_filename(g_app->settings.folder);
    if (folder_name.empty()) folder_name = g_app->settings.folder;
    for (char& ch : folder_name) ch = (char)std::toupper((unsigned char)ch);
    static bool expanded = true;
    const float hdr_h = dp(22.0f);
    ImGui::InvisibleButton("##wshdr", ImVec2(w, hdr_h));
    if (ImGui::IsItemClicked()) expanded = !expanded;
    if (ImGui::IsItemHovered()) dl->AddRectFilled(c, ImVec2(c.x + w, c.y + hdr_h), vs_col::row_hover);
    const char* chev = expanded ? IC_CHEV_D : IC_CHEV_R;
    ImVec2 cs = ImGui::CalcTextSize(chev);
    dl->AddText(ImVec2(c.x + dp(6.0f), c.y + (hdr_h - cs.y) * 0.5f), vs_col::text, chev);
    float fsz = ImGui::GetFontSize() * 0.85f;
    ImGui::RenderTextEllipsis(dl, ImVec2(c.x + dp(24.0f), c.y + (hdr_h - fsz) * 0.5f),
                              ImVec2(c.x + w - dp(8.0f), c.y + hdr_h), c.x + w - dp(8.0f), c.x + w - dp(8.0f),
                              folder_name.c_str(), nullptr, nullptr);
    if (!expanded) return;

    ImGui::BeginChild("fs_tree", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    draw_fs_tree();
    ImGui::EndChild();
}

void draw_search() {
    ImGui::Dummy(ImVec2(1, 8));
    ImGui::Indent(12);
    ui_push_field();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##search", "Search this folder", g_app->search_query,
                                 sizeof(g_app->search_query), ImGuiInputTextFlags_EnterReturnsTrue)) {
        workspace_search(g_app->search_query);
    }
    ui_pop_field();
    ImGui::Dummy(ImVec2(1, 4));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1));
    if (g_app->font_ui_small) ImGui::PushFont(g_app->font_ui_small);
    ImGui::Text("%d results", (int)g_app->search_hits.size());
    if (g_app->font_ui_small) ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Unindent(12);
    ImGui::BeginChild("hits", ImVec2(0, 0));
    ImGui::Dummy(ImVec2(1, 4));
    ImGui::Indent(8);
    if (g_app->search_hits.empty() && g_app->search_query[0] == 0) {
        ui_empty_state(IC_SEARCH, "Search the open folder.", "Enter a query and press Enter.");
    } else if (g_app->search_hits.empty()) {
        ui_empty_state(IC_SEARCH, "No matches in this folder.", "Try a different query.");
    }
    for (int i = 0; i < (int)g_app->search_hits.size(); i++) {
        const SearchHit& h = g_app->search_hits[(size_t)i];
        std::string label = path_filename(h.path);
        std::string loc = std::to_string(h.line);
        char id[32];
        std::snprintf(id, sizeof(id), "hit%d", i);
        if (ui_chip_row(id, IC_FILE, IM_COL32(0x7E, 0xC4, 0xE0, 255), label.c_str(), loc.c_str())) {
            if (open_path(h.path, false) && g_app->active >= 0) {
                TextBuffer& b = g_app->buffers[(size_t)g_app->active];
                int pos = buffer_pos_at(b, h.line - 1, h.col);
                buffer_move(b, pos, false);
                int qn = (int)std::strlen(g_app->search_query);
                b.sel_anchor = pos;
                b.cursor = pos + qn;
                g_app->editor_focused = true;
            }
        }
        if (!h.preview.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.50f, 1));
            if (g_app->font_ui_small) ImGui::PushFont(g_app->font_ui_small);
            ImGui::Indent(8);
            ImGui::TextUnformatted(h.preview.c_str());
            ImGui::Unindent(8);
            if (g_app->font_ui_small) ImGui::PopFont();
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(1, 4));
        }
    }
    ImGui::Unindent(8);
    ImGui::EndChild();
}

void draw_outline() {
    if (g_app->active < 0) {
        ImGui::Indent(12);
        ui_empty_state(IC_OUTLINE, "No file open.", "Open a Nexa file to see its outline.");
        ImGui::Unindent(12);
        return;
    }
    TextBuffer& b = g_app->buffers[(size_t)g_app->active];
    if (b.outline_dirty) index_nexa(b.text, b.outline);
    ImGui::BeginChild("outline", ImVec2(0, 0));
    ImGui::Dummy(ImVec2(1, 8));
    ImGui::Indent(8);
    if (b.outline.empty()) {
        ui_empty_state(IC_OUTLINE, "Nothing to show in this file.", "Functions, structs, and lets show up here.");
    }
    for (int i = 0; i < (int)b.outline.size(); i++) {
        const IndexedDef& d = b.outline[(size_t)i];
        const char* kind = "var";
        const char* icon = IC_FILE;
        ImU32 icol = IM_COL32(0x9A, 0x9A, 0x9A, 255);
        switch (d.kind) {
        case DefKind::Function:
            kind = "fn";
            icon = IC_RUN;
            icol = IM_COL32(0x7E, 0xC4, 0xE0, 255);
            break;
        case DefKind::Struct:
            kind = "struct";
            icon = IC_FOLDER;
            icol = IM_COL32(0xC4, 0xA0, 0x58, 255);
            break;
        case DefKind::Enum:
            kind = "enum";
            icon = IC_OUTLINE;
            icol = IM_COL32(0xC5, 0x93, 0xE7, 255);
            break;
        case DefKind::Variable:
            kind = "let";
            icon = IC_FILE;
            icol = IM_COL32(0x9C, 0xDC, 0xFE, 255);
            break;
        default: {
            DefKind unk = d.kind;
            (void)unk;
            break;
        }
        }
        char id[32];
        std::snprintf(id, sizeof(id), "ol%d", i);
        if (ui_chip_row(id, icon, icol, d.name.c_str(), kind)) {
            buffer_move(b, d.name_start, false);
            b.sel_anchor = d.name_start;
            b.cursor = d.name_end;
            g_app->editor_focused = true;
        }
    }
    ImGui::Unindent(8);
    ImGui::EndChild();
}

void draw_problems() {
    if (g_app->problems.empty()) {
        ui_empty_state(IC_INFO, "No problems in the open file.", "Diagnostics show up here after you edit or build.");
        return;
    }
    ImGui::Dummy(ImVec2(1, 4));
    for (int i = 0; i < (int)g_app->problems.size(); i++) {
        const Diagnostic& d = g_app->problems[(size_t)i];
        std::string label = path_filename(d.path);
        if (label.empty()) label = d.message;
        std::string loc = std::to_string(d.line);
        char id[32];
        std::snprintf(id, sizeof(id), "pr%d", i);
        bool err = d.error;
        if (ui_chip_row(id, err ? IC_ERROR : IC_WARN,
                        err ? IM_COL32(0xD0, 0x3C, 0x3C, 255) : IM_COL32(0xC8, 0xA8, 0x78, 255),
                        d.message.c_str(), loc.c_str())) {
            if (!d.path.empty() && file_exists(d.path)) open_path(d.path, false);
            if (g_app->active >= 0) {
                buffer_move_line_col(g_app->buffers[(size_t)g_app->active], d.line - 1, 0, false);
                g_app->editor_focused = true;
            }
        }
        if (!label.empty() && label != d.message) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1));
            if (g_app->font_ui_small) ImGui::PushFont(g_app->font_ui_small);
            ImGui::Indent(8);
            ImGui::TextUnformatted(label.c_str());
            ImGui::Unindent(8);
            if (g_app->font_ui_small) ImGui::PopFont();
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(1, 4));
        }
    }
}

void draw_run_view() {
    ImGui::Dummy(ImVec2(1, 10));
    ImGui::Indent(12);
    ui_begin_card("run_card");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.82f, 1));
    ImGui::TextWrapped("Builds with NexaC in the open folder, then launches the program. Arguments go to your program, not the compiler.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(1, 8));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.72f, 1));
    ImGui::TextUnformatted("Program arguments");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(1, 4));
    ui_push_field();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##args", "Optional args passed to the program",
                             g_app->settings.run_args, sizeof(g_app->settings.run_args));
    ui_pop_field();
    ImGui::Dummy(ImVec2(1, 10));
    if (ui_accent_button("Run  ·  F5", ImVec2(-FLT_MIN, 32))) run_active(true);
    ImGui::Dummy(ImVec2(1, 6));
    if (ImGui::Button("Build  ·  Ctrl+Alt+B", ImVec2(-FLT_MIN, 30))) run_active(false);
    if (g_app->proc.running) {
        ImGui::Dummy(ImVec2(1, 6));
        if (ImGui::Button("Stop", ImVec2(-FLT_MIN, 30))) proc_stop();
    }
    ui_end_card();
    ImGui::Unindent(12);
}

void draw_settings_panel() {
    ImGui::BeginChild("settings_body", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::Dummy(ImVec2(1, 10));
    ImGui::Indent(12);

    ui_begin_card("agent_card");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.82f, 1));
    ImGui::TextUnformatted("Agent");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(1, 2));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1));
    ImGui::TextWrapped("Provider, model, and API key. The agent panel is just the conversation.");
    ImGui::PopStyleColor();
    ui_push_field();
    draw_agent_settings();
    ui_pop_field();
    ui_end_card();

    ImGui::Dummy(ImVec2(1, 12));
    ui_begin_card("nexa_card");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.82f, 1));
    ImGui::TextUnformatted("Nexa");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(1, 6));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.72f, 1));
    ImGui::TextUnformatted("Language root");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(1, 4));
    char lang[512];
    std::snprintf(lang, sizeof(lang), "%s", g_app->settings.nexa_lang.c_str());
    ui_push_field();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputText("##lang", lang, sizeof(lang))) {
        g_app->settings.nexa_lang = lang;
        save_settings(g_app->settings);
    }
    ui_pop_field();
    ImGui::Dummy(ImVec2(1, 6));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1));
    ImGui::TextWrapped("Compiler is NexaC on PATH. This folder is where syntax notes are loaded from.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(1, 10));
    if (ui_accent_button("Save Settings", ImVec2(-FLT_MIN, 32))) save_settings(g_app->settings);
    ui_end_card();

    ImGui::Unindent(12);
    ImGui::Dummy(ImVec2(1, 16));
    ImGui::EndChild();
}
