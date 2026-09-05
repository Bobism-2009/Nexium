#include "nexium.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

#include "imgui_internal.h"

void buffer_rebuild_lines(TextBuffer& b) {
    b.lines.clear();
    b.lines.push_back(0);
    for (int i = 0; i < (int)b.text.size(); i++) {
        if (b.text[(size_t)i] == '\n') b.lines.push_back(i + 1);
    }
}

void buffer_mark_nexa(TextBuffer& b) {
    b.is_nexa = b.untitled || path_is_nexa(b.path);
}

void buffer_refresh(TextBuffer& b) {
    buffer_rebuild_lines(b);
    buffer_mark_nexa(b);
    if (b.is_nexa) {
        highlight_nexa(b.text, b.spans);
        index_nexa(b.text, b.outline);
    } else {
        b.spans.clear();
        b.outline.clear();
    }
    b.spans_dirty = false;
    b.outline_dirty = false;
}

int buffer_line_at(const TextBuffer& b, int pos) {
    if (b.lines.empty()) return 0;
    pos = std::clamp(pos, 0, (int)b.text.size());
    auto it = std::upper_bound(b.lines.begin(), b.lines.end(), pos);
    int idx = (int)std::distance(b.lines.begin(), it) - 1;
    return std::max(0, idx);
}

int buffer_col_at(const TextBuffer& b, int pos) {
    int line = buffer_line_at(b, pos);
    return std::max(0, pos - b.lines[(size_t)line]);
}

int buffer_pos_at(const TextBuffer& b, int line, int col) {
    if (b.lines.empty()) return 0;
    line = std::clamp(line, 0, (int)b.lines.size() - 1);
    int start = b.lines[(size_t)line];
    int end = (line + 1 < (int)b.lines.size()) ? b.lines[(size_t)line + 1] - 1 : (int)b.text.size();
    return std::clamp(start + std::max(0, col), start, end);
}

static void clamp_cursor(TextBuffer& b) {
    int n = (int)b.text.size();
    b.cursor = std::clamp(b.cursor, 0, n);
    b.sel_anchor = std::clamp(b.sel_anchor, 0, n);
}

void buffer_apply(TextBuffer& b, int pos, int remove, const std::string& insert, bool coalesce) {
    pos = std::clamp(pos, 0, (int)b.text.size());
    remove = std::clamp(remove, 0, (int)b.text.size() - pos);
    UndoRec rec;
    rec.pos = pos;
    rec.removed = b.text.substr((size_t)pos, (size_t)remove);
    rec.inserted = insert;
    if ((int)b.undo.size() > b.undo_pos) b.undo.resize((size_t)b.undo_pos);
    double now = ImGui::GetTime();
    if (coalesce && !b.undo.empty() && now - b.last_type < 0.4 &&
        rec.removed.empty() && b.undo.back().removed.empty() &&
        b.undo.back().pos + (int)b.undo.back().inserted.size() == pos) {
        b.undo.back().inserted += insert;
    } else {
        b.undo.push_back(std::move(rec));
        if (b.undo.size() > 400) {
            b.undo.erase(b.undo.begin());
        }
    }
    b.undo_pos = (int)b.undo.size();
    b.last_type = now;
    b.text.replace((size_t)pos, (size_t)remove, insert);
    b.cursor = pos + (int)insert.size();
    b.sel_anchor = b.cursor;
    b.dirty = true;
    b.spans_dirty = true;
    b.outline_dirty = true;
    b.last_edit = now;
    if (g_app) g_app->diag_dirty = true;
    buffer_rebuild_lines(b);
}

void buffer_undo(TextBuffer& b) {
    if (b.undo_pos <= 0) return;
    b.undo_pos--;
    const UndoRec& rec = b.undo[(size_t)b.undo_pos];
    b.text.replace((size_t)rec.pos, rec.inserted.size(), rec.removed);
    b.cursor = rec.pos + (int)rec.removed.size();
    b.sel_anchor = b.cursor;
    b.dirty = true;
    b.spans_dirty = true;
    buffer_rebuild_lines(b);
}

void buffer_redo(TextBuffer& b) {
    if (b.undo_pos >= (int)b.undo.size()) return;
    const UndoRec& rec = b.undo[(size_t)b.undo_pos];
    b.text.replace((size_t)rec.pos, rec.removed.size(), rec.inserted);
    b.cursor = rec.pos + (int)rec.inserted.size();
    b.sel_anchor = b.cursor;
    b.undo_pos++;
    b.dirty = true;
    b.spans_dirty = true;
    buffer_rebuild_lines(b);
}

void buffer_ensure_sel(TextBuffer& b) {
    if (b.sel_anchor > b.cursor) std::swap(b.sel_anchor, b.cursor);
}

std::string buffer_selection(const TextBuffer& b) {
    int a = std::min(b.cursor, b.sel_anchor);
    int z = std::max(b.cursor, b.sel_anchor);
    if (z <= a) return {};
    return b.text.substr((size_t)a, (size_t)(z - a));
}

void buffer_replace_sel(TextBuffer& b, const std::string& text) {
    int a = std::min(b.cursor, b.sel_anchor);
    int z = std::max(b.cursor, b.sel_anchor);
    buffer_apply(b, a, z - a, text, false);
}

void buffer_move(TextBuffer& b, int pos, bool select) {
    b.cursor = std::clamp(pos, 0, (int)b.text.size());
    if (!select) b.sel_anchor = b.cursor;
    b.preferred_col = buffer_col_at(b, b.cursor);
}

void buffer_move_line_col(TextBuffer& b, int line, int col, bool select) {
    buffer_move(b, buffer_pos_at(b, line, col), select);
}

void buffer_delete_sel(TextBuffer& b) {
    if (b.cursor == b.sel_anchor) return;
    buffer_replace_sel(b, "");
}

static int word_left(const TextBuffer& b, int pos) {
    pos = std::clamp(pos, 0, (int)b.text.size());
    while (pos > 0 && std::isspace((unsigned char)b.text[(size_t)pos - 1])) pos--;
    while (pos > 0 && (std::isalnum((unsigned char)b.text[(size_t)pos - 1]) || b.text[(size_t)pos - 1] == '_')) pos--;
    return pos;
}

static int word_right(const TextBuffer& b, int pos) {
    pos = std::clamp(pos, 0, (int)b.text.size());
    int n = (int)b.text.size();
    while (pos < n && std::isspace((unsigned char)b.text[(size_t)pos])) pos++;
    while (pos < n && (std::isalnum((unsigned char)b.text[(size_t)pos]) || b.text[(size_t)pos] == '_')) pos++;
    return pos;
}

void buffer_indent(TextBuffer& b, bool back) {
    int a = buffer_line_at(b, std::min(b.cursor, b.sel_anchor));
    int z = buffer_line_at(b, std::max(b.cursor, b.sel_anchor));
    if (b.cursor != b.sel_anchor) {
        int max_pos = std::max(b.cursor, b.sel_anchor);
        if (max_pos > 0 && max_pos == buffer_pos_at(b, z, 0) && z > a) z--;
    }
    for (int line = z; line >= a; line--) {
        int start = b.lines[(size_t)line];
        if (back) {
            int rem = 0;
            while (rem < 4 && start + rem < (int)b.text.size() && b.text[(size_t)(start + rem)] == ' ') rem++;
            if (rem == 0 && start < (int)b.text.size() && b.text[(size_t)start] == '\t') rem = 1;
            if (rem > 0) buffer_apply(b, start, rem, "", false);
        } else {
            buffer_apply(b, start, 0, "    ", false);
        }
    }
}

void buffer_toggle_comment(TextBuffer& b) {
    int a = buffer_line_at(b, std::min(b.cursor, b.sel_anchor));
    int z = buffer_line_at(b, std::max(b.cursor, b.sel_anchor));
    bool all_commented = true;
    for (int line = a; line <= z; line++) {
        int start = b.lines[(size_t)line];
        int end = (line + 1 < (int)b.lines.size()) ? b.lines[(size_t)line + 1] - 1 : (int)b.text.size();
        int i = start;
        while (i < end && (b.text[(size_t)i] == ' ' || b.text[(size_t)i] == '\t')) i++;
        if (i + 1 >= end || b.text[(size_t)i] != '/' || b.text[(size_t)i + 1] != '/') {
            all_commented = false;
            break;
        }
    }
    for (int line = z; line >= a; line--) {
        int start = b.lines[(size_t)line];
        int end = (line + 1 < (int)b.lines.size()) ? b.lines[(size_t)line + 1] - 1 : (int)b.text.size();
        int i = start;
        while (i < end && (b.text[(size_t)i] == ' ' || b.text[(size_t)i] == '\t')) i++;
        if (all_commented) {
            if (i + 1 < (int)b.text.size() && b.text[(size_t)i] == '/' && b.text[(size_t)i + 1] == '/') {
                int rem = 2;
                if (i + 2 < (int)b.text.size() && b.text[(size_t)i + 2] == ' ') rem = 3;
                buffer_apply(b, i, rem, "", false);
            }
        } else {
            buffer_apply(b, start, 0, "// ", false);
        }
    }
}

void buffer_find_next(TextBuffer& b, const char* query, bool reverse) {
    if (!query || !query[0]) return;
    std::string q = query;
    if (reverse) {
        int from = std::max(0, std::min(b.cursor, b.sel_anchor) - 1);
        auto pos = b.text.rfind(q, (size_t)from);
        if (pos == std::string::npos) pos = b.text.rfind(q);
        if (pos != std::string::npos) {
            b.sel_anchor = (int)pos;
            b.cursor = (int)pos + (int)q.size();
        }
    } else {
        int from = std::max(b.cursor, b.sel_anchor);
        auto pos = b.text.find(q, (size_t)from);
        if (pos == std::string::npos) pos = b.text.find(q);
        if (pos != std::string::npos) {
            b.sel_anchor = (int)pos;
            b.cursor = (int)pos + (int)q.size();
        }
    }
}

static void insert_pair(TextBuffer& b, char open, char close) {
    std::string sel = buffer_selection(b);
    if (!sel.empty()) {
        buffer_replace_sel(b, std::string(1, open) + sel + std::string(1, close));
        return;
    }
    buffer_replace_sel(b, std::string(1, open) + std::string(1, close));
    b.cursor--;
    b.sel_anchor = b.cursor;
}

static void handle_enter(TextBuffer& b) {
    int line = buffer_line_at(b, std::min(b.cursor, b.sel_anchor));
    int start = b.lines[(size_t)line];
    int i = start;
    while (i < (int)b.text.size() && (b.text[(size_t)i] == ' ' || b.text[(size_t)i] == '\t')) i++;
    std::string indent = b.text.substr((size_t)start, (size_t)(i - start));
    bool extra = false;
    int p = std::max(b.cursor, b.sel_anchor);
    if (p > 0) {
        char c = b.text[(size_t)p - 1];
        extra = (c == '{' || c == '(' || c == '[');
    }
    std::string ins = "\n" + indent;
    if (extra) ins += "    ";
    bool split_brace = extra && p < (int)b.text.size() && b.text[(size_t)p] == '}';
    buffer_replace_sel(b, ins);
    if (split_brace) {
        int cur = b.cursor;
        buffer_apply(b, cur, 0, "\n" + indent, false);
        b.cursor = cur;
        b.sel_anchor = cur;
    }
}

static void copy_sel(const TextBuffer& b) {
    std::string sel = buffer_selection(b);
    if (sel.empty()) {
        int line = buffer_line_at(b, b.cursor);
        int start = b.lines[(size_t)line];
        int end = (line + 1 < (int)b.lines.size()) ? b.lines[(size_t)line + 1] : (int)b.text.size();
        sel = b.text.substr((size_t)start, (size_t)(end - start));
        if (sel.empty() || sel.back() != '\n') sel += '\n';
    }
    ImGui::SetClipboardText(sel.c_str());
}

bool open_path(const std::string& path, bool preview) {
    std::string norm = path_norm(path);
    for (int i = 0; i < (int)g_app->buffers.size(); i++) {
        if (!g_app->buffers[(size_t)i].untitled && path_norm(g_app->buffers[(size_t)i].path) == norm) {
            g_app->active = i;
            return true;
        }
    }
    std::string text, err;
    if (!file_read(norm, text, &err)) return false;
    if (preview && g_app->preview_tab >= 0 && g_app->preview_tab < (int)g_app->buffers.size()) {
        TextBuffer& b = g_app->buffers[(size_t)g_app->preview_tab];
        if (!b.dirty) {
            b = TextBuffer{};
            b.path = norm;
            b.name = path_filename(norm);
            b.text = std::move(text);
            b.untitled = false;
            buffer_refresh(b);
            g_app->active = g_app->preview_tab;
            return true;
        }
    }
    TextBuffer b;
    b.path = norm;
    b.name = path_filename(norm);
    b.text = std::move(text);
    b.untitled = false;
    buffer_refresh(b);
    g_app->buffers.push_back(std::move(b));
    g_app->active = (int)g_app->buffers.size() - 1;
    if (preview) g_app->preview_tab = g_app->active;
    else g_app->preview_tab = -1;
    return true;
}

bool save_buffer(int index, bool save_as) {
    if (index < 0 || index >= (int)g_app->buffers.size()) return false;
    TextBuffer& b = g_app->buffers[(size_t)index];
    if (save_as || b.untitled || b.path.empty()) {
        std::string path;
        if (!save_file_dialog(path)) return false;
        if (path_ext(path).empty()) path += ".nxa";
        b.path = path_norm(path);
        b.name = path_filename(b.path);
        b.untitled = false;
        buffer_mark_nexa(b);
    }
    std::string err;
    if (!file_write(b.path, b.text, &err)) return false;
    b.dirty = false;
    b.spans_dirty = true;
    buffer_refresh(b);
    g_app->diag_dirty = true;
    refresh_problems();
    return true;
}

bool save_all() {
    bool ok = true;
    for (int i = 0; i < (int)g_app->buffers.size(); i++) {
        if (g_app->buffers[(size_t)i].dirty) ok = save_buffer(i) && ok;
    }
    return ok;
}

void close_buffer(int index, bool force) {
    if (index < 0 || index >= (int)g_app->buffers.size()) return;
    if (!force && g_app->buffers[(size_t)index].dirty) {
        g_app->confirm_close = true;
        g_app->close_index = index;
        return;
    }
    g_app->buffers.erase(g_app->buffers.begin() + index);
    if (g_app->preview_tab == index) g_app->preview_tab = -1;
    else if (g_app->preview_tab > index) g_app->preview_tab--;
    if (g_app->buffers.empty()) {
        g_app->active = -1;
        return;
    }
    if (g_app->active >= (int)g_app->buffers.size()) g_app->active = (int)g_app->buffers.size() - 1;
    if (g_app->active > index) g_app->active--;
}

void new_untitled() {
    TextBuffer b;
    b.name = "untitled.nxa";
    b.untitled = true;
    b.is_nexa = true;
    b.text = "#include <std/io>\n\nfn main(): void {\n    io.println(\"Hello, Nexa\");\n}\n";
    buffer_refresh(b);
    b.cursor = (int)b.text.size() - 3;
    b.sel_anchor = b.cursor;
    g_app->buffers.push_back(std::move(b));
    g_app->active = (int)g_app->buffers.size() - 1;
    g_app->preview_tab = -1;
}

void draw_editor_tabs(float strip_h);

static void draw_breadcrumb(const TextBuffer& b, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    dl->AddRectFilled(origin, ImVec2(origin.x + w, origin.y + h),
                      IM_COL32(0x0E, 0x0E, 0x0E, 0xFF));
    dl->AddLine(ImVec2(origin.x, origin.y + h),
                ImVec2(origin.x + w, origin.y + h),
                IM_COL32(0x3C, 0x3C, 0x3C, 0xFF));

    std::string full = b.untitled ? b.name : b.path;
    std::string root = g_app && !g_app->settings.folder.empty() ? g_app->settings.folder : "";
    std::string rel;
    if (!root.empty() && full.size() > root.size() &&
        (full[root.size()] == '/' || full[root.size()] == '\\') &&
        std::equal(root.begin(), root.end(), full.begin())) {
        rel = full.substr(root.size() + 1);
    } else {
        rel = full;
    }
    std::vector<std::string> parts;
    if (!root.empty()) parts.push_back(path_filename(root));
    std::string cur;
    for (char ch : rel) {
        if (ch == '/' || ch == '\\') { if (!cur.empty()) parts.push_back(cur); cur.clear(); }
        else cur.push_back(ch);
    }
    if (!cur.empty()) parts.push_back(cur);

    const float chip_h = dp(22.0f);
    float x = origin.x + 10.0f;
    float cy = origin.y + (h - chip_h) * 0.5f;
    for (int i = 0; i < (int)parts.size(); i++) {
        bool last = i == (int)parts.size() - 1;
        const char* icon = last ? IC_FILE : IC_FOLDER;
        ImU32 icon_col = last ? IM_COL32(0x9C, 0xDC, 0xFE, 0xFF)
                              : IM_COL32(0xDC, 0xB6, 0x67, 0xFF);
        ImVec2 ts = ImGui::CalcTextSize(parts[(size_t)i].c_str());
        float chip_w = ts.x + 28.0f;
        ImVec2 r0(x, cy);
        ImVec2 r1(x + chip_w, cy + chip_h);
        dl->AddRectFilled(r0, r1, last ? IM_COL32(0x18, 0x18, 0x18, 255) : IM_COL32(0x14, 0x14, 0x14, 255), 11.0f);
        dl->AddRect(r0, r1, last ? IM_COL32(0x2A, 0x2A, 0x2A, 255) : IM_COL32(0x22, 0x22, 0x22, 255), 11.0f);
        dl->AddText(ImVec2(x + 8, cy + (chip_h - ImGui::GetFontSize()) * 0.5f), icon_col, icon);
        dl->AddText(ImVec2(x + 22, cy + (chip_h - ts.y) * 0.5f),
                    last ? IM_COL32(0xDC, 0xDC, 0xDC, 255) : IM_COL32(0x8A, 0x8A, 0x8A, 255),
                    parts[(size_t)i].c_str());
        x += chip_w + 4.0f;
        if (!last) {
            dl->AddText(ImVec2(x, cy + (chip_h - ImGui::GetFontSize()) * 0.5f),
                        IM_COL32(0x6B, 0x6B, 0x6B, 255), IC_CHEV_R);
            x += 16.0f;
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + h));
}

static void draw_empty_editor(const ImVec2& size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 c = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(c, ImVec2(c.x + size.x, c.y + size.y),
                      IM_COL32(0x12, 0x12, 0x12, 0xFF));

    struct Row { const char* label; const char* key; };
    const Row rows[] = {
        {"New File",           "Ctrl+N"},
        {"Open File",          "Ctrl+O"},
        {"Open Folder",        ""},
        {"Command Palette",    "Ctrl+Shift+P"},
        {"Run Nexa File",      "F5"},
        {"Toggle AI Agent",    "Ctrl+Shift+L"},
    };
    const int n = (int)(sizeof(rows) / sizeof(rows[0]));

    float max_label = 0, max_key = 0;
    for (const auto& r : rows) {
        max_label = std::max(max_label, ImGui::CalcTextSize(r.label).x);
        max_key   = std::max(max_key,   ImGui::CalcTextSize(r.key).x);
    }
    const float title_h = 48.0f;
    const float sub_h   = 24.0f;
    const float gap     = 28.0f;
    const float row_h   = 28.0f;
    const float col_gap = 48.0f;
    const float block_w = std::max(max_label + col_gap + max_key + 24.0f, 320.0f);
    const float block_h = title_h + sub_h + gap + row_h * n;

    float bx = c.x + (size.x - block_w) * 0.5f;
    float by = c.y + (size.y - block_h) * 0.5f;

    if (g_app->font_icon) ImGui::PushFont(g_app->font_icon);
    ImVec2 ic = ImGui::CalcTextSize(IC_SPARKLE);
    dl->AddText(ImVec2(c.x + (size.x - ic.x) * 0.5f, by - ic.y - 12.0f),
                IM_COL32(0x6C, 0xB6, 0xFF, 180), IC_SPARKLE);
    if (g_app->font_icon) ImGui::PopFont();

    const char* title = "Nexium";
    ImFont* title_f = g_app->font_title ? g_app->font_title : ImGui::GetFont();
    float title_sz = title_f->FontSize;
    ImVec2 title_ts = title_f->CalcTextSizeA(title_sz, FLT_MAX, 0.0f, title);
    dl->AddText(title_f, title_sz,
                ImVec2(c.x + (size.x - title_ts.x) * 0.5f, by),
                IM_COL32(0xE6, 0xE6, 0xE6, 0xFF), title);

    const char* subtitle = "Open a file or ask the agent to start.";
    ImFont* f = ImGui::GetFont();
    ImVec2 sub_ts = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.0f, subtitle);
    dl->AddText(f, f->FontSize,
                ImVec2(c.x + (size.x - sub_ts.x) * 0.5f, by + title_h + 4),
                IM_COL32(0x72, 0x72, 0x72, 0xFF), subtitle);

    float ly = by + title_h + sub_h + gap;
    for (int i = 0; i < n; i++) {
        ImVec2 r0(bx, ly + i * row_h);
        ImVec2 r1(bx + block_w, ly + i * row_h + 24.0f);
        dl->AddRectFilled(r0, r1, IM_COL32(0x18, 0x18, 0x18, 255), 6.0f);
        dl->AddRect(r0, r1, IM_COL32(0x2A, 0x2A, 0x2A, 255), 6.0f);
        dl->AddText(ImVec2(bx + 12, ly + i * row_h + 4),
                    IM_COL32(0xCC, 0xCC, 0xCC, 0xFF), rows[i].label);
        if (rows[i].key[0]) {
            ImVec2 ks = ImGui::CalcTextSize(rows[i].key);
            float kx = bx + block_w - ks.x - 12.0f;
            dl->AddRectFilled(ImVec2(kx - 6, ly + i * row_h + 3),
                              ImVec2(bx + block_w - 6, ly + i * row_h + 21),
                              IM_COL32(0x14, 0x14, 0x14, 255), 4.0f);
            dl->AddText(ImVec2(kx, ly + i * row_h + 4),
                        IM_COL32(0x6C, 0xB6, 0xFF, 255), rows[i].key);
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(bx, ly + 2 * row_h));
    ImGui::InvisibleButton("open_folder_click", ImVec2(block_w, 24.0f));
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked()) {
        std::string folder;
        if (open_folder_dialog(folder)) set_folder(folder);
    }

    ImGui::Dummy(size);
}

static int pos_from_mouse(const TextBuffer& b, ImVec2 origin, float char_w, float line_h, float gutter) {
    // origin is GetCursorScreenPos() at the start of the child, which is already
    // (window pos + padding - scroll). Adding scroll again maps clicks to the
    // wrong line once the file is taller than the viewport.
    ImVec2 mouse = ImGui::GetIO().MousePos;
    float x = mouse.x - origin.x - gutter;
    float y = mouse.y - origin.y;
    int line = (int)std::floor(y / line_h);
    int col = (int)std::floor((x / char_w) + 0.3f);
    return buffer_pos_at(b, line, std::max(0, col));
}

void draw_editor(const ImVec2& size) {
    float strip_h = dp(36.0f);
    if (!g_app->buffers.empty()) draw_editor_tabs(strip_h);
    else strip_h = 0.0f;

    if (g_app->active < 0 || g_app->active >= (int)g_app->buffers.size()) {
        ImVec2 remain(size.x, size.y - strip_h);
        draw_empty_editor(remain);
        return;
    }

    TextBuffer& b = g_app->buffers[(size_t)g_app->active];
    float bc_h = dp(26.0f);
    draw_breadcrumb(b, bc_h);
    ImVec2 remain(size.x, size.y - strip_h - bc_h);
    if (b.spans_dirty || b.lines.empty()) buffer_refresh(b);

    static std::vector<std::string> last_comps;
    static int last_complete_sel = 0;
    static bool last_show_comp = false;

    ImFont* code = g_app->font_code ? g_app->font_code : ImGui::GetFont();
    ImGui::PushFont(code);
    float font_sz = ImGui::GetFontSize();
    float char_w = ImGui::GetFont()->CalcTextSizeA(font_sz, FLT_MAX, 0.0f, "M").x;
    char_w = (float)(int)(char_w + 0.5f);
    float line_h = (float)(int)(font_sz + dp(4.0f) + 0.5f);
    int line_count = (int)b.lines.size();
    float gutter = 12.0f + ImGui::CalcTextSize(std::to_string(std::max(1, line_count)).c_str()).x;

    ImGui::BeginChild("editor_view", remain, ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiWindow* ed_win = ImGui::GetCurrentWindow();

    // Restore per-buffer scroll when switching tabs (once, not every frame).
    static int scroll_tab = -999;
    if (scroll_tab != g_app->active) {
        ImGui::SetScrollX(b.scroll_x);
        ImGui::SetScrollY(b.scroll_y);
        scroll_tab = g_app->active;
    }

    {
        float sy = ImGui::GetScrollY();
        float sx = ImGui::GetScrollX();
        float nsy = (float)(int)(sy + 0.5f);
        float nsx = (float)(int)(sx + 0.5f);
        if (nsy != sy) ImGui::SetScrollY(nsy);
        if (nsx != sx) ImGui::SetScrollX(nsx);
    }
    ImVec2 origin = ImGui::GetCursorScreenPos();
    origin.x = (float)(int)(origin.x + 0.5f);
    origin.y = (float)(int)(origin.y + 0.5f);
    ImVec2 view = ImGui::GetContentRegionAvail();
    float content_h = (float)line_count * line_h + 40.0f;
    float max_cols = 80.0f;
    for (int i = 0; i < line_count; i++) {
        int start = b.lines[(size_t)i];
        int end = (i + 1 < line_count) ? b.lines[(size_t)i + 1] - 1 : (int)b.text.size();
        max_cols = std::max(max_cols, (float)(end - start));
    }
    float content_w = gutter + max_cols * char_w + 80.0f;
    // Dummy sizes the scroll region without eating scrollbar / wheel input.
    ImGui::Dummy(ImVec2(content_w, content_h));

    auto over_scrollbar = [&]() {
        if (ed_win->ScrollbarY &&
            ImGui::IsMouseHoveringRect(ImVec2(ed_win->InnerRect.Max.x, ed_win->InnerRect.Min.y),
                                       ImVec2(ed_win->Rect().Max.x, ed_win->InnerRect.Max.y)))
            return true;
        if (ed_win->ScrollbarX &&
            ImGui::IsMouseHoveringRect(ImVec2(ed_win->InnerRect.Min.x, ed_win->InnerRect.Max.y),
                                       ImVec2(ed_win->InnerRect.Max.x, ed_win->Rect().Max.y)))
            return true;
        return false;
    };
    int cursor_before_input = b.cursor;
    bool hovered = ImGui::IsWindowHovered() && !over_scrollbar();
    bool dragging_sel = hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
                        ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if ((hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) || dragging_sel) {
        g_app->editor_focused = true;
        g_app->terminal_focused = false;
        bool select = ImGui::GetIO().KeyShift || dragging_sel;
        int pos = pos_from_mouse(b, origin, char_w, line_h, gutter);
        if (!dragging_sel && ImGui::IsMouseDoubleClicked(0)) {
            b.sel_anchor = word_left(b, pos + 1);
            b.cursor = word_right(b, pos);
        } else {
            buffer_move(b, pos, select);
        }
    }
    if (g_app->editor_focused) {
        ImGuiIO& io = ImGui::GetIO();
        io.WantTextInput = true;
        io.WantCaptureKeyboard = true;
        bool shift = io.KeyShift;
        bool ctrl = io.KeyCtrl;
        bool alt = io.KeyAlt;

        auto move_h = [&](int dir) {
            if (ctrl) buffer_move(b, dir < 0 ? word_left(b, b.cursor) : word_right(b, b.cursor), shift);
            else buffer_move(b, b.cursor + dir, shift);
        };
        auto move_v = [&](int dir) {
            int line = buffer_line_at(b, b.cursor) + dir;
            buffer_move_line_col(b, line, b.preferred_col, shift);
            b.preferred_col = b.preferred_col;
        };

        if (!ctrl && !alt) {
            for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
                unsigned int c = io.InputQueueCharacters[n];
                if (c < 32 || c == 127) continue;
                if (c == '(') { insert_pair(b, '(', ')'); continue; }
                if (c == '{') { insert_pair(b, '{', '}'); continue; }
                if (c == '[') { insert_pair(b, '[', ']'); continue; }
                if (c == '"' || c == '\'') {
                    char q = (char)c;
                    insert_pair(b, q, q);
                    continue;
                }
                char utf[8];
                int len = 0;
                if (c < 0x80) utf[len++] = (char)c;
                else if (c < 0x800) {
                    utf[len++] = (char)(0xC0 | (c >> 6));
                    utf[len++] = (char)(0x80 | (c & 0x3F));
                } else {
                    utf[len++] = (char)(0xE0 | (c >> 12));
                    utf[len++] = (char)(0x80 | ((c >> 6) & 0x3F));
                    utf[len++] = (char)(0x80 | (c & 0x3F));
                }
                buffer_replace_sel(b, std::string(utf, utf + len));
            }
        }

        static int complete_sel = 0;
        std::string prefix;
        std::vector<std::string> comps = completions_for(b, b.cursor, prefix);
        bool show_comp = b.is_nexa && !comps.empty() && prefix.size() >= 1 && !ctrl;
        if (show_comp && complete_sel >= (int)comps.size()) complete_sel = 0;

        if (show_comp && ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            complete_sel = (complete_sel + 1) % (int)comps.size();
        } else if (show_comp && ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            complete_sel = (complete_sel - 1 + (int)comps.size()) % (int)comps.size();
        } else if (show_comp && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Tab))) {
            int start = b.cursor - (int)prefix.size();
            if (start < 0) start = 0;
            buffer_apply(b, start, (int)prefix.size(), comps[(size_t)complete_sel], false);
            show_comp = false;
        } else if (show_comp && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            show_comp = false;
            comps.clear();
        } else if (!show_comp) {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) move_v(-1);
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) move_v(1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) move_h(-1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) move_h(1);
        if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
            int line = buffer_line_at(b, b.cursor);
            int start = b.lines[(size_t)line];
            int i = start;
            while (i < (int)b.text.size() && (b.text[(size_t)i] == ' ' || b.text[(size_t)i] == '\t')) i++;
            buffer_move(b, (b.cursor == i || ctrl) ? start : i, shift);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_End)) {
            int line = buffer_line_at(b, b.cursor);
            int end = (line + 1 < (int)b.lines.size()) ? b.lines[(size_t)line + 1] - 1 : (int)b.text.size();
            buffer_move(b, end, shift);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) move_v(-(int)(view.y / line_h) + 2);
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) move_v((int)(view.y / line_h) - 2);
        if (!show_comp && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) handle_enter(b);
        if (!show_comp && ImGui::IsKeyPressed(ImGuiKey_Tab)) buffer_indent(b, shift);
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
            if (b.cursor != b.sel_anchor) buffer_delete_sel(b);
            else if (b.cursor > 0) {
                int from = ctrl ? word_left(b, b.cursor) : b.cursor - 1;
                buffer_apply(b, from, b.cursor - from, "", false);
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            if (b.cursor != b.sel_anchor) buffer_delete_sel(b);
            else if (b.cursor < (int)b.text.size()) {
                int to = ctrl ? word_right(b, b.cursor) : b.cursor + 1;
                buffer_apply(b, b.cursor, to - b.cursor, "", false);
            }
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
            b.sel_anchor = 0;
            b.cursor = (int)b.text.size();
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) copy_sel(b);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
            copy_sel(b);
            if (b.cursor == b.sel_anchor) {
                int line = buffer_line_at(b, b.cursor);
                int start = b.lines[(size_t)line];
                int end = (line + 1 < (int)b.lines.size()) ? b.lines[(size_t)line + 1] : (int)b.text.size();
                buffer_apply(b, start, end - start, "", false);
            } else {
                buffer_delete_sel(b);
            }
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
            const char* clip = ImGui::GetClipboardText();
            if (clip) buffer_replace_sel(b, clip);
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !shift) buffer_undo(b);
        if (ctrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (shift && ImGui::IsKeyPressed(ImGuiKey_Z)))) buffer_redo(b);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Slash)) buffer_toggle_comment(b);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
            int line = buffer_line_at(b, b.cursor);
            int start = b.lines[(size_t)line];
            int end = (line + 1 < (int)b.lines.size()) ? b.lines[(size_t)line + 1] : (int)b.text.size();
            std::string row = b.text.substr((size_t)start, (size_t)(end - start));
            if (row.empty() || row.back() != '\n') row += '\n';
            buffer_apply(b, end, 0, row, false);
        }

        // Only chase the caret when it actually moved. Doing this every frame
        // pins the viewport to the cursor and makes wheel / scrollbar scrolling
        // snap straight back.
        if (b.cursor != cursor_before_input) {
            int cline = buffer_line_at(b, b.cursor);
            float caret_y = (float)cline * line_h;
            float caret_x = gutter + (float)buffer_col_at(b, b.cursor) * char_w;
            if (caret_y < ImGui::GetScrollY()) ImGui::SetScrollY(caret_y);
            if (caret_y + line_h > ImGui::GetScrollY() + view.y)
                ImGui::SetScrollY(caret_y + line_h - view.y);
            if (caret_x < ImGui::GetScrollX() + gutter)
                ImGui::SetScrollX(std::max(0.0f, caret_x - gutter));
            if (caret_x + char_w > ImGui::GetScrollX() + view.x)
                ImGui::SetScrollX(caret_x + char_w - view.x);
        }

        last_show_comp = show_comp;
        last_comps = std::move(comps);
        last_complete_sel = complete_sel;
    } else {
        last_show_comp = false;
        last_comps.clear();
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float scroll_x = ImGui::GetScrollX();
    float scroll_y = ImGui::GetScrollY();
    int first = std::max(0, (int)std::floor(scroll_y / line_h) - 1);
    int last = std::min(line_count - 1, (int)std::ceil((scroll_y + view.y) / line_h) + 1);
    int cur_line = buffer_line_at(b, b.cursor);
    int sel_a = std::min(b.cursor, b.sel_anchor);
    int sel_z = std::max(b.cursor, b.sel_anchor);

    size_t span_i = 0;
    while (span_i < b.spans.size() && b.spans[span_i].end <= b.lines[(size_t)std::max(0, first)]) span_i++;

    for (int line = first; line <= last; line++) {
        float y = origin.y + (float)line * line_h;
        int ls = b.lines[(size_t)line];
        int le = (line + 1 < line_count) ? b.lines[(size_t)line + 1] - 1 : (int)b.text.size();
        if (line == cur_line && g_app->editor_focused) {
            dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + std::max(view.x, content_w), y + line_h),
                              IM_COL32(0x1A, 0x1C, 0x1E, 255));
        }
        if (sel_z > sel_a) {
            int a = std::max(sel_a, ls);
            int z = std::min(sel_z, le);
            if (z > a || (sel_a <= ls && sel_z >= le && ls < le)) {
                if (sel_z > ls && sel_a < (le == ls ? le + 1 : le + 1)) {
                    int draw_a = std::max(sel_a, ls);
                    int draw_z = std::min(sel_z, le);
                    if (draw_z < draw_a) draw_z = draw_a;
                    float x0 = origin.x + gutter + (float)(draw_a - ls) * char_w;
                    float x1 = origin.x + gutter + (float)std::max(draw_z - ls, 1) * char_w;
                    if (sel_z > le && le >= ls) x1 = origin.x + gutter + (float)(le - ls) * char_w + char_w;
                    dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + line_h), IM_COL32(38, 79, 120, 255));
                }
            }
        }
        char num[16];
        std::snprintf(num, sizeof(num), "%d", line + 1);
        ImU32 num_col = (line == cur_line) ? IM_COL32(200, 200, 200, 255) : IM_COL32(110, 110, 110, 255);
        float nw = ImGui::CalcTextSize(num).x;
        dl->AddText(ImVec2(origin.x + gutter - 8.0f - nw, y + 1.0f), num_col, num);

        if (ls >= le) continue;
        if (b.is_nexa && !b.spans.empty()) {
            int p = ls;
            size_t si = span_i;
            while (si < b.spans.size() && b.spans[si].end <= ls) si++;
            span_i = si;
            while (p < le) {
                HighlightKind kind = HighlightKind::Text;
                int next = le;
                while (si < b.spans.size() && b.spans[si].end <= p) si++;
                if (si < b.spans.size()) {
                    const HighlightSpan& sp = b.spans[si];
                    if (sp.start > p && sp.start < le) next = sp.start;
                    else if (p >= sp.start && p < sp.end) {
                        kind = sp.kind;
                        next = std::min(le, sp.end);
                    }
                }
                if (next <= p) next = p + 1;
                std::string chunk = b.text.substr((size_t)p, (size_t)(next - p));
                float x = origin.x + gutter + (float)(p - ls) * char_w;
                dl->AddText(ImVec2(x, y + 1.0f), highlight_color(kind), chunk.c_str());
                p = next;
            }
        } else {
            std::string chunk = b.text.substr((size_t)ls, (size_t)(le - ls));
            dl->AddText(ImVec2(origin.x + gutter, y + 1.0f), IM_COL32(212, 212, 212, 255), chunk.c_str());
        }
    }

    if (g_app->editor_focused && (int)(ImGui::GetTime() * 2.0) % 2 == 0) {
        float cx = origin.x + gutter + (float)buffer_col_at(b, b.cursor) * char_w;
        float cy = origin.y + (float)cur_line * line_h;
        dl->AddRectFilled(ImVec2(cx, cy + 1.0f), ImVec2(cx + 1.5f, cy + line_h - 1.0f), IM_COL32(220, 220, 220, 255));
    }

    if (last_show_comp && !last_comps.empty()) {
        float px = origin.x + gutter + (float)buffer_col_at(b, b.cursor) * char_w;
        float py = origin.y + (float)(cur_line + 1) * line_h;
        int shown = std::min(10, (int)last_comps.size());
        float pw = 220.0f;
        for (int i = 0; i < shown; i++) {
            pw = std::max(pw, ImGui::CalcTextSize(last_comps[(size_t)i].c_str()).x + 16.0f);
        }
        float ph = (float)shown * line_h + 4.0f;
        dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + ph), IM_COL32(0x16, 0x16, 0x16, 250));
        dl->AddRect(ImVec2(px, py), ImVec2(px + pw, py + ph), IM_COL32(0, 122, 204, 255));
        for (int i = 0; i < shown; i++) {
            if (i == last_complete_sel) {
                dl->AddRectFilled(ImVec2(px + 1, py + 2 + i * line_h),
                                  ImVec2(px + pw - 1, py + 2 + (i + 1) * line_h),
                                  IM_COL32(9, 71, 113, 255));
            }
            dl->AddText(ImVec2(px + 8, py + 3 + i * line_h), IM_COL32(220, 220, 220, 255),
                        last_comps[(size_t)i].c_str());
        }
    }

    if (g_app->find_open) {
        ImGui::SetNextWindowPos(ImVec2(origin.x + view.x - 360.0f, origin.y + 8.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(340, g_app->replace_open ? 110.0f : 64.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.078f, 0.078f, 0.078f, 1));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.180f, 0.180f, 0.180f, 1));
        ImGui::Begin("Find", &g_app->find_open,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        ui_push_field();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##find", "Find", g_app->find_text, sizeof(g_app->find_text),
                                     ImGuiInputTextFlags_EnterReturnsTrue)) {
            buffer_find_next(b, g_app->find_text, ImGui::GetIO().KeyShift);
        }
        if (g_app->replace_open) {
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##repl", "Replace", g_app->replace_text, sizeof(g_app->replace_text));
            if (ui_accent_button("Replace", ImVec2(0, 0))) {
                if (buffer_selection(b) == g_app->find_text) buffer_replace_sel(b, g_app->replace_text);
                buffer_find_next(b, g_app->find_text, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Replace All")) {
                std::string q = g_app->find_text;
                std::string r = g_app->replace_text;
                if (!q.empty()) {
                    size_t pos = 0;
                    while ((pos = b.text.find(q, pos)) != std::string::npos) {
                        buffer_apply(b, (int)pos, (int)q.size(), r, false);
                        pos += r.size();
                    }
                }
            }
        }
        ui_pop_field();
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    if (g_app->goto_open) {
        ImGui::SetNextWindowPos(ImVec2(origin.x + 80, origin.y + 40), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(240, 64));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.078f, 0.078f, 0.078f, 1));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.180f, 0.180f, 0.180f, 1));
        ImGui::Begin("Go to Line", &g_app->goto_open,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ui_push_field();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##goto", "Line number", g_app->goto_text, sizeof(g_app->goto_text),
                                     ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
            int line = std::max(1, std::atoi(g_app->goto_text));
            buffer_move_line_col(b, line - 1, 0, false);
            g_app->goto_open = false;
            g_app->editor_focused = true;
        }
        ui_pop_field();
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    b.scroll_x = ImGui::GetScrollX();
    b.scroll_y = ImGui::GetScrollY();
    ImGui::EndChild();
    ImGui::PopFont();
}
