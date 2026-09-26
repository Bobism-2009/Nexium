#include "nexium.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstring>

#include "imgui_internal.h"

static const int kTab = 4;

static bool is_word_char(char c) {
    return std::isalnum((unsigned char)c) || c == '_';
}

static bool is_cont_byte(char c) {
    return ((unsigned char)c & 0xC0) == 0x80;
}

// ---------------------------------------------------------------------------
// Lines, columns and language
// ---------------------------------------------------------------------------
void buffer_rebuild_lines(TextBuffer& b) {
    b.lines.clear();
    b.lines.push_back(0);
    const int n = (int)b.text.size();
    int start = 0;
    int longest = 0;
    for (int i = 0; i < n; i++) {
        if (b.text[(size_t)i] == '\n') {
            longest = std::max(longest, i - start);
            b.lines.push_back(i + 1);
            start = i + 1;
        }
    }
    b.max_line_len = std::max(longest, n - start);
}

void buffer_mark_nexa(TextBuffer& b) {
    b.lang = b.untitled ? Lang::Nexa : lang_for_path(b.path);
    b.is_nexa = b.lang == Lang::Nexa;
    b.is_json = b.lang == Lang::Json;
}

void buffer_refresh(TextBuffer& b) {
    buffer_rebuild_lines(b);
    buffer_mark_nexa(b);
    highlight_source(b.lang, b.path, b.text, b.spans);
    if (b.is_nexa) {
        // The outline is regex based and much slower than highlighting, so it
        // waits until typing pauses (draw_ide picks it up) unless there is none yet.
        if (b.outline_dirty && (b.outline.empty() || ImGui::GetTime() - b.last_edit > 0.3)) {
            index_nexa(b.text, b.outline);
            b.outline_dirty = false;
        }
    } else {
        b.outline.clear();
        b.outline_dirty = false;
    }
    b.spans_dirty = false;
}

int buffer_line_at(const TextBuffer& b, int pos) {
    if (b.lines.empty()) return 0;
    pos = std::clamp(pos, 0, (int)b.text.size());
    auto it = std::upper_bound(b.lines.begin(), b.lines.end(), pos);
    int idx = (int)std::distance(b.lines.begin(), it) - 1;
    return std::max(0, idx);
}

static int line_start(const TextBuffer& b, int line) {
    if (b.lines.empty()) return 0;
    line = std::clamp(line, 0, (int)b.lines.size() - 1);
    return b.lines[(size_t)line];
}

// End of the line's text, excluding its newline.
static int line_end(const TextBuffer& b, int line) {
    if (b.lines.empty()) return (int)b.text.size();
    line = std::clamp(line, 0, (int)b.lines.size() - 1);
    return (line + 1 < (int)b.lines.size()) ? b.lines[(size_t)line + 1] - 1 : (int)b.text.size();
}

// Start of the next line, or the end of the text for the last line.
static int line_next(const TextBuffer& b, int line) {
    if (line + 1 < (int)b.lines.size()) return b.lines[(size_t)line + 1];
    return (int)b.text.size();
}

int buffer_col_at(const TextBuffer& b, int pos) {
    if (b.lines.empty()) return 0;
    int line = buffer_line_at(b, pos);
    return std::max(0, pos - line_start(b, line));
}

int buffer_pos_at(const TextBuffer& b, int line, int col) {
    if (b.lines.empty()) return 0;
    int start = line_start(b, line);
    int end = line_end(b, line);
    return std::clamp(start + std::max(0, col), start, end);
}

// Visual column: tabs advance to the next stop and UTF-8 sequences count once.
static int vcol_between(const std::string& t, int from, int pos) {
    int v = 0;
    for (int i = from; i < pos; i++) {
        char c = t[(size_t)i];
        if (c == '\t') v = (v / kTab + 1) * kTab;
        else if (!is_cont_byte(c)) v++;
    }
    return v;
}

int buffer_vcol_at(const TextBuffer& b, int pos) {
    if (b.lines.empty()) return 0;
    pos = std::clamp(pos, 0, (int)b.text.size());
    return vcol_between(b.text, line_start(b, buffer_line_at(b, pos)), pos);
}

int buffer_pos_at_vcol(const TextBuffer& b, int line, int vcol) {
    if (b.lines.empty()) return 0;
    int i = line_start(b, line);
    int le = line_end(b, line);
    int v = 0;
    while (i < le) {
        char c = b.text[(size_t)i];
        int nv = c == '\t' ? (v / kTab + 1) * kTab : v + 1;
        if (nv > vcol) break;
        i++;
        while (i < le && is_cont_byte(b.text[(size_t)i])) i++;
        v = nv;
    }
    return i;
}

// Nearest character boundary to a fractional visual column (mouse hits).
static int pos_at_x(const TextBuffer& b, int line, float cols) {
    int i = line_start(b, line);
    int le = line_end(b, line);
    int v = 0;
    while (i < le) {
        char c = b.text[(size_t)i];
        int nv = c == '\t' ? (v / kTab + 1) * kTab : v + 1;
        if (cols < (float)(v + nv) * 0.5f) break;
        i++;
        while (i < le && is_cont_byte(b.text[(size_t)i])) i++;
        v = nv;
    }
    return i;
}

static void clamp_cursor(TextBuffer& b) {
    int n = (int)b.text.size();
    b.cursor = std::clamp(b.cursor, 0, n);
    b.sel_anchor = std::clamp(b.sel_anchor, 0, n);
}

static int next_char(const TextBuffer& b, int pos) {
    int n = (int)b.text.size();
    if (pos >= n) return n;
    pos++;
    while (pos < n && is_cont_byte(b.text[(size_t)pos])) pos++;
    return pos;
}

static int prev_char(const TextBuffer& b, int pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && is_cont_byte(b.text[(size_t)pos])) pos--;
    return pos;
}

// ---------------------------------------------------------------------------
// Editing and undo
// ---------------------------------------------------------------------------
static void mark_changed(TextBuffer& b) {
    b.dirty = b.undo_pos != b.saved_undo_pos;
    b.spans_dirty = true;
    b.outline_dirty = true;
    b.version++;
    b.last_edit = ImGui::GetTime();
    if (g_app) g_app->diag_dirty = true;
    buffer_rebuild_lines(b);
}

void buffer_apply(TextBuffer& b, int pos, int remove, const std::string& insert, bool coalesce) {
    pos = std::clamp(pos, 0, (int)b.text.size());
    remove = std::clamp(remove, 0, (int)b.text.size() - pos);
    if (remove == 0 && insert.empty()) return;
    UndoRec rec;
    rec.pos = pos;
    rec.removed = b.text.substr((size_t)pos, (size_t)remove);
    rec.inserted = insert;
    rec.group = b.undo_group;
    rec.cursor_before = b.cursor;
    rec.anchor_before = b.sel_anchor;
    if ((int)b.undo.size() > b.undo_pos) {
        b.undo.resize((size_t)b.undo_pos);
        if (b.saved_undo_pos > b.undo_pos) b.saved_undo_pos = -1;
    }
    double now = ImGui::GetTime();
    bool merged = false;
    // Consecutive typing merges into one undo step, broken at word boundaries
    // and never merged into the step that matches the saved file.
    if (coalesce && b.undo_group == 0 && !b.undo.empty() && now - b.last_type < 1.0 &&
        b.saved_undo_pos != (int)b.undo.size()) {
        UndoRec& back = b.undo.back();
        bool boundary = !insert.empty() && !back.inserted.empty() &&
                        std::isspace((unsigned char)insert[0]) &&
                        !std::isspace((unsigned char)back.inserted.back());
        if (back.group == 0 && rec.removed.empty() && back.removed.empty() &&
            back.pos + (int)back.inserted.size() == pos && !boundary) {
            back.inserted += insert;
            merged = true;
        }
    }
    if (!merged) {
        b.undo.push_back(std::move(rec));
        if (b.undo.size() > 2000) {
            b.undo.erase(b.undo.begin());
            if (b.saved_undo_pos >= 0) b.saved_undo_pos--;
        }
    }
    b.undo_pos = (int)b.undo.size();
    b.last_type = now;
    b.text.replace((size_t)pos, (size_t)remove, insert);
    b.cursor = pos + (int)insert.size();
    b.sel_anchor = b.cursor;
    mark_changed(b);
}

void buffer_begin_group(TextBuffer& b) {
    if (b.undo_group == 0) b.undo_group = ++b.undo_group_seq;
}

void buffer_end_group(TextBuffer& b) {
    b.undo_group = 0;
}

namespace {
// Groups every edit made in its scope into one undo step. Nested scopes join
// the outer group.
struct UndoGroup {
    TextBuffer& b;
    bool owner;
    explicit UndoGroup(TextBuffer& buf) : b(buf), owner(buf.undo_group == 0) {
        if (owner) buffer_begin_group(b);
    }
    ~UndoGroup() {
        if (owner) buffer_end_group(b);
    }
};
}  // namespace

static bool undo_rec_fits(const TextBuffer& b, const UndoRec& rec, bool redo) {
    size_t len = redo ? rec.removed.size() : rec.inserted.size();
    return rec.pos >= 0 && (size_t)rec.pos + len <= b.text.size();
}

void buffer_undo(TextBuffer& b) {
    if (b.undo_pos <= 0) return;
    int group = b.undo[(size_t)b.undo_pos - 1].group;
    do {
        const UndoRec& rec = b.undo[(size_t)b.undo_pos - 1];
        if (!undo_rec_fits(b, rec, false)) {
            // History no longer matches the text; drop it rather than corrupt.
            b.undo.clear();
            b.undo_pos = 0;
            b.saved_undo_pos = -1;
            break;
        }
        b.undo_pos--;
        b.text.replace((size_t)rec.pos, rec.inserted.size(), rec.removed);
        b.cursor = rec.cursor_before;
        b.sel_anchor = rec.anchor_before;
    } while (group != 0 && b.undo_pos > 0 && b.undo[(size_t)b.undo_pos - 1].group == group);
    clamp_cursor(b);
    mark_changed(b);
}

void buffer_redo(TextBuffer& b) {
    if (b.undo_pos >= (int)b.undo.size()) return;
    int group = b.undo[(size_t)b.undo_pos].group;
    do {
        const UndoRec& rec = b.undo[(size_t)b.undo_pos];
        if (!undo_rec_fits(b, rec, true)) {
            b.undo.resize((size_t)b.undo_pos);
            break;
        }
        b.text.replace((size_t)rec.pos, rec.removed.size(), rec.inserted);
        b.cursor = rec.pos + (int)rec.inserted.size();
        b.undo_pos++;
    } while (group != 0 && b.undo_pos < (int)b.undo.size() && b.undo[(size_t)b.undo_pos].group == group);
    b.sel_anchor = b.cursor;
    clamp_cursor(b);
    mark_changed(b);
}

void buffer_set_text(TextBuffer& b, const std::string& text) {
    if (text == b.text) return;
    int cursor = b.cursor;
    int anchor = b.sel_anchor;
    buffer_apply(b, 0, (int)b.text.size(), text, false);
    b.cursor = cursor;
    b.sel_anchor = anchor;
    clamp_cursor(b);
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
    b.preferred_col = buffer_vcol_at(b, b.cursor);
}

void buffer_move_line_col(TextBuffer& b, int line, int col, bool select) {
    buffer_move(b, buffer_pos_at(b, line, col), select);
}

void buffer_delete_sel(TextBuffer& b) {
    if (b.cursor == b.sel_anchor) return;
    buffer_replace_sel(b, "");
}

void buffer_reveal(TextBuffer& b, bool center) {
    b.last_cursor = -1;
    b.reveal_center = center;
}

static int char_class(char c) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') return 0;
    if (is_word_char(c) || ((unsigned char)c & 0x80)) return 1;
    return 2;
}

static int word_left(const TextBuffer& b, int pos) {
    const std::string& t = b.text;
    pos = std::clamp(pos, 0, (int)t.size());
    if (pos > 0 && t[(size_t)pos - 1] == '\n') return pos - 1;
    while (pos > 0 && (t[(size_t)pos - 1] == ' ' || t[(size_t)pos - 1] == '\t')) pos--;
    if (pos > 0) {
        int cls = char_class(t[(size_t)pos - 1]);
        if (cls != 0) {
            while (pos > 0 && char_class(t[(size_t)pos - 1]) == cls) pos--;
        }
    }
    return pos;
}

static int word_right(const TextBuffer& b, int pos) {
    const std::string& t = b.text;
    int n = (int)t.size();
    pos = std::clamp(pos, 0, n);
    if (pos < n && t[(size_t)pos] == '\n') return pos + 1;
    while (pos < n && (t[(size_t)pos] == ' ' || t[(size_t)pos] == '\t')) pos++;
    if (pos < n) {
        int cls = char_class(t[(size_t)pos]);
        if (cls != 0) {
            while (pos < n && char_class(t[(size_t)pos]) == cls) pos++;
        }
    }
    return pos;
}

// The identifier touching pos, if any.
static bool word_at(const TextBuffer& b, int pos, int& a, int& z) {
    const std::string& t = b.text;
    int n = (int)t.size();
    pos = std::clamp(pos, 0, n);
    a = z = pos;
    while (a > 0 && is_word_char(t[(size_t)a - 1])) a--;
    while (z < n && is_word_char(t[(size_t)z])) z++;
    return z > a;
}

static void selection_lines(const TextBuffer& b, int& la, int& lz) {
    int a = std::min(b.cursor, b.sel_anchor);
    int z = std::max(b.cursor, b.sel_anchor);
    la = buffer_line_at(b, a);
    lz = buffer_line_at(b, z);
    // A selection that ends at column 0 does not include that line.
    if (z > a && lz > la && z == line_start(b, lz)) lz--;
}

static std::string leading_ws(const TextBuffer& b, int line) {
    int s = line_start(b, line);
    int e = line_end(b, line);
    int i = s;
    while (i < e && (b.text[(size_t)i] == ' ' || b.text[(size_t)i] == '\t')) i++;
    return b.text.substr((size_t)s, (size_t)(i - s));
}

void buffer_indent(TextBuffer& b, bool back) {
    int la = 0, lz = 0;
    selection_lines(b, la, lz);
    int cur = b.cursor;
    int anc = b.sel_anchor;
    // A selection that starts at column 0 keeps starting there, as in VS Code.
    const int sel_start = b.cursor != b.sel_anchor ? std::min(cur, anc) : -1;
    UndoGroup group(b);
    for (int line = lz; line >= la; line--) {
        int start = line_start(b, line);
        int n = (int)b.text.size();
        if (back) {
            int rem = 0;
            while (rem < kTab && start + rem < n && b.text[(size_t)(start + rem)] == ' ') rem++;
            if (rem == 0 && start < n && b.text[(size_t)start] == '\t') rem = 1;
            if (rem == 0) continue;
            buffer_apply(b, start, rem, "", false);
            for (int* p : {&cur, &anc}) {
                if (*p > start) *p = std::max(start, *p - rem);
            }
        } else {
            if (lz > la && line_end(b, line) == start) continue;
            buffer_apply(b, start, 0, std::string((size_t)kTab, ' '), false);
            for (int* p : {&cur, &anc}) {
                if (*p > start || (*p == start && *p != sel_start)) *p += kTab;
            }
        }
    }
    b.cursor = cur;
    b.sel_anchor = anc;
    clamp_cursor(b);
}

void buffer_toggle_comment(TextBuffer& b) {
    const char* tok = lang_line_comment(b.lang, b.path);
    if (!tok) return;
    const std::string mark = tok;
    int la = 0, lz = 0;
    selection_lines(b, la, lz);
    bool any = false;
    bool all = true;
    int min_indent = INT_MAX;
    for (int line = la; line <= lz; line++) {
        int ls = line_start(b, line);
        int le = line_end(b, line);
        int i = ls;
        while (i < le && (b.text[(size_t)i] == ' ' || b.text[(size_t)i] == '\t')) i++;
        if (i == le) continue;
        any = true;
        min_indent = std::min(min_indent, i - ls);
        if (b.text.compare((size_t)i, mark.size(), mark) != 0) all = false;
    }
    int cur = b.cursor;
    int anc = b.sel_anchor;
    UndoGroup group(b);
    if (!any) {
        // Only blank lines: comment the cursor line where the cursor is.
        buffer_apply(b, b.cursor, 0, mark + " ", false);
        return;
    }
    for (int line = lz; line >= la; line--) {
        int ls = line_start(b, line);
        int le = line_end(b, line);
        int i = ls;
        while (i < le && (b.text[(size_t)i] == ' ' || b.text[(size_t)i] == '\t')) i++;
        if (i == le) continue;
        if (all) {
            int rem = (int)mark.size();
            if (i + rem < le && b.text[(size_t)(i + rem)] == ' ') rem++;
            buffer_apply(b, i, rem, "", false);
            for (int* p : {&cur, &anc}) {
                if (*p > i) *p = std::max(i, *p - rem);
            }
        } else {
            int at = ls + min_indent;
            std::string ins = mark + " ";
            buffer_apply(b, at, 0, ins, false);
            for (int* p : {&cur, &anc}) {
                if (*p >= at) *p += (int)ins.size();
            }
        }
    }
    b.cursor = cur;
    b.sel_anchor = anc;
    clamp_cursor(b);
}

// ---------------------------------------------------------------------------
// Find / replace
// ---------------------------------------------------------------------------
bool buffer_match_at(const TextBuffer& b, int pos, const std::string& q, bool match_case, bool whole_word) {
    const std::string& t = b.text;
    int qn = (int)q.size();
    if (qn == 0 || pos < 0 || pos + qn > (int)t.size()) return false;
    if (match_case) {
        if (t.compare((size_t)pos, (size_t)qn, q) != 0) return false;
    } else {
        for (int i = 0; i < qn; i++) {
            if (std::tolower((unsigned char)t[(size_t)(pos + i)]) != std::tolower((unsigned char)q[(size_t)i]))
                return false;
        }
    }
    if (whole_word) {
        if (pos > 0 && is_word_char(t[(size_t)pos - 1]) && is_word_char(q.front())) return false;
        int e = pos + qn;
        if (e < (int)t.size() && is_word_char(t[(size_t)e]) && is_word_char(q.back())) return false;
    }
    return true;
}

static int find_from(const TextBuffer& b, const std::string& q, int from, bool reverse,
                     bool match_case, bool whole_word) {
    int n = (int)b.text.size();
    int qn = (int)q.size();
    if (qn == 0 || qn > n) return -1;
    int last = n - qn;
    if (reverse) {
        from = std::clamp(from, 0, last);
        for (int p = from; p >= 0; p--) {
            if (buffer_match_at(b, p, q, match_case, whole_word)) return p;
        }
        for (int p = last; p > from; p--) {
            if (buffer_match_at(b, p, q, match_case, whole_word)) return p;
        }
    } else {
        // A start past the last possible match goes straight to the wrap-around.
        from = std::max(0, from);
        for (int p = from; p <= last; p++) {
            if (buffer_match_at(b, p, q, match_case, whole_word)) return p;
        }
        for (int p = 0; p < std::min(from, last + 1); p++) {
            if (buffer_match_at(b, p, q, match_case, whole_word)) return p;
        }
    }
    return -1;
}

void buffer_find_next(TextBuffer& b, const char* query, bool reverse) {
    if (!query || !query[0]) return;
    std::string q = query;
    int a = std::min(b.cursor, b.sel_anchor);
    int z = std::max(b.cursor, b.sel_anchor);
    int from = reverse ? a - 1 : z;
    if (reverse && from < 0) from = (int)b.text.size();
    int p = find_from(b, q, from, reverse, g_app->find_case, g_app->find_word);
    if (p < 0) return;
    b.sel_anchor = p;
    b.cursor = p + (int)q.size();
    b.preferred_col = buffer_vcol_at(b, b.cursor);
    buffer_reveal(b, true);
}

static void collect_matches(const TextBuffer& b, const std::string& q, bool match_case, bool whole_word,
                            std::vector<int>& out, size_t limit) {
    out.clear();
    int n = (int)b.text.size();
    int qn = (int)q.size();
    if (qn == 0) return;
    for (int p = 0; p + qn <= n && out.size() < limit; p++) {
        if (buffer_match_at(b, p, q, match_case, whole_word)) {
            out.push_back(p);
            p += qn - 1;
        }
    }
}

int buffer_replace_all(TextBuffer& b, const std::string& q, const std::string& r,
                       bool match_case, bool whole_word) {
    std::vector<int> hits;
    collect_matches(b, q, match_case, whole_word, hits, SIZE_MAX);
    if (hits.empty()) return 0;
    // One edit spanning first..last match keeps this a single undo step.
    int first = hits.front();
    int last_end = hits.back() + (int)q.size();
    std::string seg;
    int p = first;
    for (int h : hits) {
        seg.append(b.text, (size_t)p, (size_t)(h - p));
        seg += r;
        p = h + (int)q.size();
    }
    buffer_apply(b, first, last_end - first, seg, false);
    b.cursor = b.sel_anchor = first;
    buffer_reveal(b, true);
    return (int)hits.size();
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------
static void init_file_buffer(TextBuffer& b, const std::string& norm, std::string&& text, bool crlf) {
    b = TextBuffer{};
    b.path = norm;
    b.name = path_filename(norm);
    b.text = std::move(text);
    b.untitled = false;
    b.crlf = crlf;
    buffer_refresh(b);
}

bool open_path(const std::string& path, bool preview) {
    std::string norm = path_norm(path);
    for (int i = 0; i < (int)g_app->buffers.size(); i++) {
        if (!g_app->buffers[(size_t)i].untitled && path_norm(g_app->buffers[(size_t)i].path) == norm) {
            g_app->active = i;
            if (!preview && g_app->preview_tab == i) g_app->preview_tab = -1;
            return true;
        }
    }
    std::string text, err;
    bool crlf = false;
    if (!file_read(norm, text, &err, &crlf)) return false;
    if (preview && g_app->preview_tab >= 0 && g_app->preview_tab < (int)g_app->buffers.size()) {
        TextBuffer& b = g_app->buffers[(size_t)g_app->preview_tab];
        if (!b.dirty) {
            init_file_buffer(b, norm, std::move(text), crlf);
            g_app->active = g_app->preview_tab;
            return true;
        }
    }
    TextBuffer b;
    init_file_buffer(b, norm, std::move(text), crlf);
    g_app->buffers.push_back(std::move(b));
    g_app->active = (int)g_app->buffers.size() - 1;
    if (preview) {
        g_app->preview_tab = g_app->active;
    } else {
        g_app->preview_tab = -1;
        g_app->editor_focused = true;
        g_app->terminal_focused = false;
    }
    return true;
}

bool save_buffer(int index, bool save_as) {
    if (index < 0 || index >= (int)g_app->buffers.size()) return false;
    TextBuffer& b = g_app->buffers[(size_t)index];
    if (save_as || b.untitled || b.path.empty()) {
        std::string path;
        if (!save_file_dialog(path)) return false;
        if (path_ext(path).empty()) path += b.is_json ? ".json" : ".nxa";
        b.path = path_norm(path);
        b.name = path_filename(b.path);
        b.untitled = false;
        buffer_mark_nexa(b);
    }
    std::string data;
    if (b.crlf) {
        data.reserve(b.text.size() + b.lines.size());
        for (char c : b.text) {
            if (c == '\n') data += '\r';
            data += c;
        }
    }
    std::string err;
    if (!file_write(b.path, b.crlf ? data : b.text, &err)) return false;
    b.saved_undo_pos = b.undo_pos;
    b.dirty = false;
    b.spans_dirty = true;
    b.outline_dirty = true;
    buffer_refresh(b);
    if (g_app->preview_tab == index) g_app->preview_tab = -1;
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
    if (g_app->active > index) g_app->active--;
    if (g_app->active >= (int)g_app->buffers.size()) g_app->active = (int)g_app->buffers.size() - 1;
}

void new_untitled() {
    int n = 1;
    for (const auto& ob : g_app->buffers) {
        if (ob.untitled) n++;
    }
    TextBuffer b;
    b.name = n == 1 ? "untitled.nxa" : "untitled-" + std::to_string(n) + ".nxa";
    b.untitled = true;
    b.text = "#include <std/io>\n\nfn main(): void {\n    io.println(\"Hello, Nexa\");\n}\n";
    buffer_refresh(b);
    b.cursor = (int)b.text.size() - 3;
    b.sel_anchor = b.cursor;
    b.last_cursor = b.cursor;
    g_app->buffers.push_back(std::move(b));
    g_app->active = (int)g_app->buffers.size() - 1;
    g_app->preview_tab = -1;
    g_app->editor_focused = true;
}

// ---------------------------------------------------------------------------
// Brackets and spans
// ---------------------------------------------------------------------------
static HighlightKind kind_at(const TextBuffer& b, int pos) {
    auto it = std::upper_bound(b.spans.begin(), b.spans.end(), pos,
                               [](int p, const HighlightSpan& s) { return p < s.start; });
    if (it == b.spans.begin()) return HighlightKind::Text;
    --it;
    return pos < it->end ? it->kind : HighlightKind::Text;
}

static bool is_bracket(char c) {
    return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
}

// Brackets inside strings and comments do not pair with code brackets.
static bool code_bracket(const TextBuffer& b, int pos) {
    if (!is_bracket(b.text[(size_t)pos])) return false;
    switch (kind_at(b, pos)) {
    case HighlightKind::Bracket1:
    case HighlightKind::Bracket2:
    case HighlightKind::Bracket3:
    case HighlightKind::Invalid:
    case HighlightKind::Punct:
    case HighlightKind::Text:
        return true;
    default:
        return false;
    }
}

static int match_bracket(const TextBuffer& b, int pos, bool skip_first_check = false) {
    const std::string& t = b.text;
    int n = (int)t.size();
    if (pos < 0 || pos >= n) return -1;
    char c = t[(size_t)pos];
    if (!skip_first_check && !code_bracket(b, pos)) return -1;
    char open, close;
    int dir;
    switch (c) {
    case '(': open = '('; close = ')'; dir = 1; break;
    case '[': open = '['; close = ']'; dir = 1; break;
    case '{': open = '{'; close = '}'; dir = 1; break;
    case ')': open = '('; close = ')'; dir = -1; break;
    case ']': open = '['; close = ']'; dir = -1; break;
    case '}': open = '{'; close = '}'; dir = -1; break;
    default: return -1;
    }
    int depth = 0;
    int steps = 0;
    for (int p = pos; p >= 0 && p < n && steps < 400000; p += dir, steps++) {
        char ch = t[(size_t)p];
        if (ch != open && ch != close) continue;
        if (p != pos && !code_bracket(b, p)) continue;
        depth += (ch == open) == (dir > 0) ? 1 : -1;
        if (depth == 0) return p;
    }
    return -1;
}

// The bracket next to the caret (left side first) and its partner.
static bool caret_brackets(const TextBuffer& b, int& a, int& z) {
    int c = b.cursor;
    int n = (int)b.text.size();
    for (int p : {c - 1, c}) {
        if (p < 0 || p >= n || !is_bracket(b.text[(size_t)p])) continue;
        int m = match_bracket(b, p);
        if (m >= 0) {
            a = p;
            z = m;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Editor state shared by the frame and editor_exec()
// ---------------------------------------------------------------------------
namespace {
struct CompletionState {
    bool active = false;
    int buffer = -1;
    std::vector<Completion> items;
    std::string prefix;
    int sel = 0;
    int top = 0;
    ImVec2 rmin, rmax;
    float row_h = 0.0f;
};
}  // namespace

static CompletionState g_comp;
static std::string g_line_copy;  // text copied as a whole line (no selection)

static void comp_close() {
    g_comp.active = false;
    g_comp.items.clear();
}

static void comp_update(TextBuffer& b, bool force, char trigger) {
    std::string prefix;
    std::vector<Completion> items = completions_for(b, b.cursor, prefix);
    bool opener = trigger == '.' || trigger == '<' || trigger == '"' || trigger == '/';
    if (items.empty() || (!force && prefix.empty() && !opener)) {
        comp_close();
        return;
    }
    std::string keep = (g_comp.active && g_comp.sel < (int)g_comp.items.size())
                           ? g_comp.items[(size_t)g_comp.sel].text : std::string();
    g_comp.active = true;
    g_comp.buffer = g_app->active;
    g_comp.items = std::move(items);
    g_comp.prefix = prefix;
    g_comp.sel = 0;
    g_comp.top = 0;
    for (int i = 0; i < (int)g_comp.items.size(); i++) {
        if (!keep.empty() && g_comp.items[(size_t)i].text == keep) {
            g_comp.sel = i;
            break;
        }
    }
}

static void comp_accept(TextBuffer& b) {
    if (!g_comp.active || g_comp.sel < 0 || g_comp.sel >= (int)g_comp.items.size()) return;
    std::string text = g_comp.items[(size_t)g_comp.sel].text;
    int start = std::max(0, b.cursor - (int)g_comp.prefix.size());
    buffer_apply(b, start, b.cursor - start, text, false);
    comp_close();
}

static void copy_sel(const TextBuffer& b) {
    std::string sel = buffer_selection(b);
    g_line_copy.clear();
    if (sel.empty()) {
        int line = buffer_line_at(b, b.cursor);
        sel = b.text.substr((size_t)line_start(b, line), (size_t)(line_next(b, line) - line_start(b, line)));
        if (sel.empty() || sel.back() != '\n') sel += '\n';
        g_line_copy = sel;
    }
    ImGui::SetClipboardText(sel.c_str());
}

static void delete_lines(TextBuffer& b) {
    int la = 0, lz = 0;
    selection_lines(b, la, lz);
    int vcol = buffer_vcol_at(b, b.cursor);
    int s = line_start(b, la);
    int e = line_next(b, lz);
    // The last line has no newline of its own; take the one before it instead.
    if (e == (int)b.text.size() && s > 0 && (lz + 1 >= (int)b.lines.size())) s--;
    buffer_apply(b, s, e - s, "", false);
    int line = std::min(la, (int)b.lines.size() - 1);
    b.cursor = b.sel_anchor = buffer_pos_at_vcol(b, line, vcol);
}

static void move_lines(TextBuffer& b, int dir) {
    int la = 0, lz = 0;
    selection_lines(b, la, lz);
    if (dir < 0 && la == 0) return;
    if (dir > 0 && lz + 1 >= (int)b.lines.size()) return;
    int bs = line_start(b, la);
    int be = line_end(b, lz);
    std::string block = b.text.substr((size_t)bs, (size_t)(be - bs));
    int cur = b.cursor;
    int anc = b.sel_anchor;
    if (dir < 0) {
        int ps = line_start(b, la - 1);
        std::string prev = b.text.substr((size_t)ps, (size_t)(bs - 1 - ps));
        buffer_apply(b, ps, be - ps, block + "\n" + prev, false);
        cur -= (int)prev.size() + 1;
        anc -= (int)prev.size() + 1;
    } else {
        int ne = line_end(b, lz + 1);
        std::string next = b.text.substr((size_t)be + 1, (size_t)(ne - be - 1));
        buffer_apply(b, bs, ne - bs, next + "\n" + block, false);
        cur += (int)next.size() + 1;
        anc += (int)next.size() + 1;
    }
    b.cursor = cur;
    b.sel_anchor = anc;
    clamp_cursor(b);
}

static void copy_lines(TextBuffer& b, int dir) {
    int la = 0, lz = 0;
    selection_lines(b, la, lz);
    int bs = line_start(b, la);
    int be = line_end(b, lz);
    std::string block = b.text.substr((size_t)bs, (size_t)(be - bs));
    int cur = b.cursor;
    int anc = b.sel_anchor;
    if (dir < 0) {
        buffer_apply(b, bs, 0, block + "\n", false);
    } else {
        buffer_apply(b, be, 0, "\n" + block, false);
        cur += (int)block.size() + 1;
        anc += (int)block.size() + 1;
    }
    b.cursor = cur;
    b.sel_anchor = anc;
    clamp_cursor(b);
}

static bool opens_block(const TextBuffer& b, int pos) {
    int p = pos - 1;
    while (p >= 0 && (b.text[(size_t)p] == ' ' || b.text[(size_t)p] == '\t')) p--;
    if (p < 0) return false;
    char c = b.text[(size_t)p];
    return c == '{' || c == '(' || c == '[';
}

static void insert_line(TextBuffer& b, bool above) {
    int line = buffer_line_at(b, b.cursor);
    std::string indent = leading_ws(b, line);
    if (above) {
        int s = line_start(b, line);
        buffer_apply(b, s, 0, indent + "\n", false);
        b.cursor = b.sel_anchor = s + (int)indent.size();
    } else {
        int e = line_end(b, line);
        if (opens_block(b, e)) indent += std::string((size_t)kTab, ' ');
        buffer_apply(b, e, 0, "\n" + indent, false);
    }
}

static void select_line(TextBuffer& b) {
    int a = std::min(b.cursor, b.sel_anchor);
    int z = std::max(b.cursor, b.sel_anchor);
    int la = buffer_line_at(b, a);
    bool full = z > a && a == line_start(b, la) && (z == line_start(b, buffer_line_at(b, z)) || z == (int)b.text.size());
    if (full) {
        b.sel_anchor = a;
        b.cursor = line_next(b, buffer_line_at(b, z));
    } else {
        int lz = buffer_line_at(b, z);
        b.sel_anchor = line_start(b, la);
        b.cursor = line_next(b, lz);
    }
}

static void trim_trailing(TextBuffer& b) {
    UndoGroup group(b);
    int cur = b.cursor;
    for (int line = (int)b.lines.size() - 1; line >= 0; line--) {
        int s = line_start(b, line);
        int e = line_end(b, line);
        int k = e;
        while (k > s && (b.text[(size_t)k - 1] == ' ' || b.text[(size_t)k - 1] == '\t')) k--;
        if (k < e) {
            buffer_apply(b, k, e - k, "", false);
            if (cur > k) cur = std::max(k, cur - (e - k));
        }
    }
    b.cursor = b.sel_anchor = cur;
    clamp_cursor(b);
}

static void transform_case(TextBuffer& b, bool upper) {
    int a = std::min(b.cursor, b.sel_anchor);
    int z = std::max(b.cursor, b.sel_anchor);
    if (z <= a && !word_at(b, b.cursor, a, z)) return;
    std::string s = b.text.substr((size_t)a, (size_t)(z - a));
    for (char& c : s) c = (char)(upper ? std::toupper((unsigned char)c) : std::tolower((unsigned char)c));
    int cur = b.cursor;
    int anc = b.sel_anchor;
    buffer_apply(b, a, z - a, s, false);
    b.cursor = cur;
    b.sel_anchor = anc;
}

static bool goto_definition(TextBuffer& b) {
    int a = 0, z = 0;
    if (!word_at(b, b.cursor, a, z)) return false;
    std::string name = b.text.substr((size_t)a, (size_t)(z - a));
    auto jump = [&](TextBuffer& tb, const IndexedDef& d) {
        tb.sel_anchor = d.name_start;
        tb.cursor = d.name_end;
        tb.preferred_col = buffer_vcol_at(tb, tb.cursor);
        buffer_reveal(tb, true);
    };
    if (b.outline_dirty && b.is_nexa) {
        index_nexa(b.text, b.outline);
        b.outline_dirty = false;
    }
    for (const auto& d : b.outline) {
        if (d.name == name && !(d.name_start <= b.cursor && b.cursor <= d.name_end)) {
            jump(b, d);
            return true;
        }
    }
    for (int i = 0; i < (int)g_app->buffers.size(); i++) {
        TextBuffer& ob = g_app->buffers[(size_t)i];
        if (&ob == &b) continue;
        for (const auto& d : ob.outline) {
            if (d.name == name) {
                g_app->active = i;
                jump(ob, d);
                return true;
            }
        }
    }
    return false;
}

static void paste(TextBuffer& b) {
    const char* clip = ImGui::GetClipboardText();
    if (!clip || !clip[0]) return;
    std::string text;
    for (const char* p = clip; *p; p++) {
        if (*p == '\r') {
            if (p[1] != '\n') text += '\n';
            continue;
        }
        text += *p;
    }
    if (b.cursor == b.sel_anchor && !g_line_copy.empty() && text == g_line_copy) {
        // A whole line copied without a selection pastes above the current line.
        int s = line_start(b, buffer_line_at(b, b.cursor));
        int cur = b.cursor;
        buffer_apply(b, s, 0, text, false);
        b.cursor = b.sel_anchor = cur + (int)text.size();
        return;
    }
    buffer_replace_sel(b, text);
}

void editor_exec(EdCmd cmd) {
    if (g_app->active < 0 || g_app->active >= (int)g_app->buffers.size()) return;
    TextBuffer& b = g_app->buffers[(size_t)g_app->active];
    switch (cmd) {
    case EdCmd::Undo: buffer_undo(b); break;
    case EdCmd::Redo: buffer_redo(b); break;
    case EdCmd::Copy: copy_sel(b); break;
    case EdCmd::Cut:
        copy_sel(b);
        if (b.cursor == b.sel_anchor) {
            int line = buffer_line_at(b, b.cursor);
            int s = line_start(b, line);
            buffer_apply(b, s, line_next(b, line) - s, "", false);
        } else {
            buffer_delete_sel(b);
        }
        break;
    case EdCmd::Paste: paste(b); break;
    case EdCmd::SelectAll:
        b.sel_anchor = 0;
        b.cursor = (int)b.text.size();
        break;
    case EdCmd::SelectLine: select_line(b); break;
    case EdCmd::ToggleComment: buffer_toggle_comment(b); break;
    case EdCmd::DuplicateLine: copy_lines(b, 1); break;
    case EdCmd::DeleteLine: delete_lines(b); break;
    case EdCmd::MoveLineUp: move_lines(b, -1); break;
    case EdCmd::MoveLineDown: move_lines(b, 1); break;
    case EdCmd::CopyLineUp: copy_lines(b, -1); break;
    case EdCmd::CopyLineDown: copy_lines(b, 1); break;
    case EdCmd::InsertLineBelow: insert_line(b, false); break;
    case EdCmd::InsertLineAbove: insert_line(b, true); break;
    case EdCmd::Indent: buffer_indent(b, false); break;
    case EdCmd::Outdent: buffer_indent(b, true); break;
    case EdCmd::GotoDefinition: goto_definition(b); break;
    case EdCmd::JumpToBracket: {
        int a = 0, z = 0;
        if (caret_brackets(b, a, z)) buffer_move(b, z, false);
        break;
    }
    case EdCmd::FindNext: buffer_find_next(b, g_app->find_text, false); break;
    case EdCmd::FindPrev: buffer_find_next(b, g_app->find_text, true); break;
    case EdCmd::TrimTrailingWhitespace: trim_trailing(b); break;
    case EdCmd::UpperCase: transform_case(b, true); break;
    case EdCmd::LowerCase: transform_case(b, false); break;
    case EdCmd::TriggerSuggest:
        g_app->editor_focused = true;
        comp_update(b, true, 0);
        break;
    }
}

// ---------------------------------------------------------------------------
// Typing
// ---------------------------------------------------------------------------
static bool is_closer(char c) {
    return c == ')' || c == ']' || c == '}';
}

static char closer_for(char c) {
    switch (c) {
    case '(': return ')';
    case '[': return ']';
    case '{': return '}';
    default: return c;
    }
}

// Brackets and quotes auto-close only before whitespace, closers or separators.
static bool auto_close_ok(const TextBuffer& b) {
    int n = (int)b.text.size();
    if (b.cursor >= n) return true;
    char next = b.text[(size_t)b.cursor];
    return next == ' ' || next == '\t' || next == '\n' || next == '\r' || is_closer(next) ||
           next == ';' || next == ',' || next == ':' || next == '.';
}

// After typing `}` on a line that was only indentation, line it up with its `{`.
static void reindent_closer(TextBuffer& b, int brace) {
    int line = buffer_line_at(b, brace);
    int ls = line_start(b, line);
    for (int i = ls; i < brace; i++) {
        if (b.text[(size_t)i] != ' ' && b.text[(size_t)i] != '\t') return;
    }
    int m = match_bracket(b, brace, true);
    if (m < 0) return;
    std::string want = leading_ws(b, buffer_line_at(b, m));
    if (b.text.compare((size_t)ls, (size_t)(brace - ls), want) == 0 && (int)want.size() == brace - ls) return;
    UndoGroup group(b);
    buffer_apply(b, ls, brace - ls, want, false);
    b.cursor = b.sel_anchor = ls + (int)want.size() + 1;
}

static void type_char(TextBuffer& b, unsigned int c) {
    char utf[8];
    int len = 0;
    if (c < 0x80) {
        utf[len++] = (char)c;
    } else if (c < 0x800) {
        utf[len++] = (char)(0xC0 | (c >> 6));
        utf[len++] = (char)(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        utf[len++] = (char)(0xE0 | (c >> 12));
        utf[len++] = (char)(0x80 | ((c >> 6) & 0x3F));
        utf[len++] = (char)(0x80 | (c & 0x3F));
    } else {
        utf[len++] = (char)(0xF0 | (c >> 18));
        utf[len++] = (char)(0x80 | ((c >> 12) & 0x3F));
        utf[len++] = (char)(0x80 | ((c >> 6) & 0x3F));
        utf[len++] = (char)(0x80 | (c & 0x3F));
    }
    const std::string s(utf, utf + len);
    const bool code = b.lang != Lang::Plain && b.lang != Lang::Markdown;
    const bool pairs = b.lang != Lang::Plain;
    const bool has_sel = b.cursor != b.sel_anchor;
    const int n = (int)b.text.size();
    const char next = b.cursor < n ? b.text[(size_t)b.cursor] : '\0';
    const char prev = b.cursor > 0 ? b.text[(size_t)b.cursor - 1] : '\0';
    const char ch = (char)c;

    if (c < 0x80 && pairs) {
        bool quote = ch == '"' || (ch == '\'' && code) || (ch == '`' && b.lang == Lang::Markdown);
        bool opener = ch == '(' || ch == '[' || ch == '{';
        // Typing a closer or quote that is already there steps over it.
        if (!has_sel && (is_closer(ch) || quote) && next == ch) {
            b.cursor = b.sel_anchor = b.cursor + 1;
            return;
        }
        if (has_sel && (opener || quote)) {
            int a = std::min(b.cursor, b.sel_anchor);
            int z = std::max(b.cursor, b.sel_anchor);
            std::string inner = b.text.substr((size_t)a, (size_t)(z - a));
            buffer_apply(b, a, z - a, s + inner + std::string(1, closer_for(ch)), false);
            b.sel_anchor = a + 1;
            b.cursor = a + 1 + (int)inner.size();
            return;
        }
        if (!has_sel && opener && auto_close_ok(b)) {
            buffer_apply(b, b.cursor, 0, s + std::string(1, closer_for(ch)), true);
            b.cursor = b.sel_anchor = b.cursor - 1;
            return;
        }
        if (!has_sel && quote && auto_close_ok(b) && prev != '\\') {
            // f"..." and R"(...)" prefixes are the only word characters a quote may follow.
            char prev2 = b.cursor > 1 ? b.text[(size_t)b.cursor - 2] : '\0';
            bool prefix = (prev == 'f' || prev == 'R') && !is_word_char(prev2);
            if (!is_word_char(prev) || prefix) {
                buffer_apply(b, b.cursor, 0, s + s, true);
                b.cursor = b.sel_anchor = b.cursor - 1;
                return;
            }
        }
    }
    if (has_sel) buffer_replace_sel(b, s);
    else buffer_apply(b, b.cursor, 0, s, true);
    if (ch == '}' && code) reindent_closer(b, b.cursor - 1);
}

static void handle_enter(TextBuffer& b) {
    if (b.cursor != b.sel_anchor) buffer_delete_sel(b);
    int cur = b.cursor;
    int line = buffer_line_at(b, cur);
    int ls = line_start(b, line);
    int le = line_end(b, line);
    int i = ls;
    while (i < cur && (b.text[(size_t)i] == ' ' || b.text[(size_t)i] == '\t')) i++;
    std::string indent = b.text.substr((size_t)ls, (size_t)(i - ls));
    bool extra = opens_block(b, cur);
    int after = cur;
    while (after < le && (b.text[(size_t)after] == ' ' || b.text[(size_t)after] == '\t')) after++;
    bool split = extra && after < le && is_closer(b.text[(size_t)after]);
    UndoGroup group(b);
    // A line that is nothing but auto-indent is cleared rather than left behind.
    if (i == cur && cur == le && cur > ls) {
        buffer_apply(b, ls, cur - ls, "", false);
        cur = ls;
        after = ls;
    }
    std::string ins = "\n" + indent;
    if (extra) ins += std::string((size_t)kTab, ' ');
    buffer_apply(b, cur, after - cur, ins, false);
    if (split) {
        int caret = b.cursor;
        buffer_apply(b, caret, 0, "\n" + indent, false);
        b.cursor = b.sel_anchor = caret;
    }
}

static void handle_backspace(TextBuffer& b, bool word) {
    if (b.cursor != b.sel_anchor) {
        buffer_delete_sel(b);
        return;
    }
    if (b.cursor <= 0) return;
    int cur = b.cursor;
    if (word) {
        int from = word_left(b, cur);
        buffer_apply(b, from, cur - from, "", false);
        return;
    }
    int n = (int)b.text.size();
    char prev = b.text[(size_t)cur - 1];
    char next = cur < n ? b.text[(size_t)cur] : '\0';
    if (next != '\0' && ((prev == '(' && next == ')') || (prev == '[' && next == ']') ||
                         (prev == '{' && next == '}') || (prev == '"' && next == '"') ||
                         (prev == '\'' && next == '\'') || (prev == '`' && next == '`'))) {
        buffer_apply(b, cur - 1, 2, "", false);
        return;
    }
    // Inside leading spaces, back up to the previous tab stop.
    int ls = line_start(b, buffer_line_at(b, cur));
    bool spaces = cur > ls;
    for (int i = ls; i < cur && spaces; i++) spaces = b.text[(size_t)i] == ' ';
    if (spaces) {
        int v = cur - ls;
        int rem = v % kTab == 0 ? kTab : v % kTab;
        buffer_apply(b, cur - rem, rem, "", false);
        return;
    }
    int from = prev_char(b, cur);
    buffer_apply(b, from, cur - from, "", false);
}

// ---------------------------------------------------------------------------
// Chrome: breadcrumb, empty state
// ---------------------------------------------------------------------------
void draw_editor_tabs(float strip_h);

static void draw_breadcrumb(const TextBuffer& b, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    dl->AddRectFilled(origin, ImVec2(origin.x + w, origin.y + h), IM_COL32(0x1F, 0x1F, 0x1F, 255));

    std::string full = b.untitled ? b.name : b.path;
    std::string root = g_app && !g_app->settings.folder.empty() ? g_app->settings.folder : "";
    std::string rel;
    if (!root.empty() && full.size() > root.size() &&
        (full[root.size()] == '/' || full[root.size()] == '\\') &&
        std::equal(root.begin(), root.end(), full.begin())) {
        rel = full.substr(root.size() + 1);
    } else {
        rel = b.untitled ? full : path_filename(full);
    }
    std::vector<std::string> parts;
    std::string cur;
    for (char ch : rel) {
        if (ch == '/' || ch == '\\') { if (!cur.empty()) parts.push_back(cur); cur.clear(); }
        else cur.push_back(ch);
    }
    if (!cur.empty()) parts.push_back(cur);

    // The definition the caret is in, like VS Code's symbol breadcrumb.
    std::string symbol;
    if (!b.outline.empty()) {
        int line = buffer_line_at(b, b.cursor);
        for (const auto& d : b.outline) {
            if (d.line > line) break;
            if (d.kind == DefKind::Function || d.kind == DefKind::Struct || d.kind == DefKind::Enum) {
                symbol = d.name;
            }
        }
    }

    const ImU32 dim = IM_COL32(0xA9, 0xA9, 0xA9, 255);
    const float ty = origin.y + (h - ImGui::GetFontSize()) * 0.5f;
    float x = origin.x + dp(16.0f);
    int count = (int)parts.size() + (symbol.empty() ? 0 : 1);
    for (int i = 0; i < count; i++) {
        bool is_symbol = i == (int)parts.size();
        bool file = i == (int)parts.size() - 1;
        const std::string& label = is_symbol ? symbol : parts[(size_t)i];
        if (file || is_symbol) {
            const char* icon = is_symbol ? IC_OUTLINE : IC_FILE;
            ImU32 icol = is_symbol ? IM_COL32(0xB1, 0x80, 0xD7, 255) : IM_COL32(0x7E, 0xC4, 0xE0, 255);
            dl->AddText(ImVec2(x, ty), icol, icon);
            x += ImGui::CalcTextSize(icon).x + dp(5.0f);
        }
        dl->AddText(ImVec2(x, ty), dim, label.c_str());
        x += ImGui::CalcTextSize(label.c_str()).x;
        if (i + 1 < count) {
            x += dp(4.0f);
            dl->AddText(ImVec2(x, ty), IM_COL32(0x80, 0x80, 0x80, 255), IC_CHEV_R);
            x += ImGui::CalcTextSize(IC_CHEV_R).x + dp(4.0f);
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + h));
}

static void draw_empty_editor(const ImVec2& size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 c = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(c, ImVec2(c.x + size.x, c.y + size.y), IM_COL32(0x1F, 0x1F, 0x1F, 255));

    struct Row { const char* label; const char* key; };
    const Row rows[] = {
        {"Show All Commands", "Ctrl+Shift+P"},
        {"Go to File",        "Ctrl+P"},
        {"New File",          "Ctrl+N"},
        {"Open File",         "Ctrl+O"},
        {"Open Folder",       "Click"},
        {"Run Nexa File",     "F5"},
        {"Toggle Terminal",   "Ctrl+`"},
        {"Toggle AI Agent",   "Ctrl+Shift+L"},
    };
    const int n = (int)(sizeof(rows) / sizeof(rows[0]));
    const float row_h = dp(30.0f);
    const float logo = std::min(dp(160.0f), size.y * 0.28f);
    const float block_h = logo + dp(40.0f) + row_h * (float)n;
    float cy = c.y + std::max(dp(24.0f), (size.y - block_h) * 0.5f);
    float mid = c.x + size.x * 0.5f;

    draw_app_icon(dl, ImVec2(mid - logo * 0.5f, cy), logo, 0.22f);
    float y = cy + logo + dp(40.0f);
    for (int i = 0; i < n; i++) {
        float ry = y + row_h * (float)i;
        ImVec2 ls = ImGui::CalcTextSize(rows[i].label);
        dl->AddText(ImVec2(mid - dp(10.0f) - ls.x, ry + (row_h - ls.y) * 0.5f), IM_COL32(0x9D, 0x9D, 0x9D, 255), rows[i].label);
        // Keys as keycaps, split on '+'.
        float kx = mid + dp(10.0f);
        std::string key = rows[i].key;
        size_t start = 0;
        while (start <= key.size()) {
            size_t plus = key.find('+', start);
            std::string part = key.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
            ImVec2 ks = ImGui::CalcTextSize(part.c_str());
            ImVec2 k0(kx, ry + (row_h - ks.y) * 0.5f - dp(3.0f));
            ImVec2 k1(kx + ks.x + dp(10.0f), k0.y + ks.y + dp(6.0f));
            dl->AddRectFilled(k0, k1, IM_COL32(0x2A, 0x2A, 0x2A, 255), 3.0f);
            dl->AddRect(k0, k1, IM_COL32(0x44, 0x44, 0x44, 255), 3.0f);
            dl->AddLine(ImVec2(k0.x + 2.0f, k1.y), ImVec2(k1.x - 2.0f, k1.y), IM_COL32(0x55, 0x55, 0x55, 255));
            dl->AddText(ImVec2(kx + dp(5.0f), ry + (row_h - ks.y) * 0.5f), IM_COL32(0xCC, 0xCC, 0xCC, 255), part.c_str());
            kx = k1.x + dp(4.0f);
            if (plus == std::string::npos) break;
            start = plus + 1;
        }
        if (std::strcmp(rows[i].label, "Open Folder") == 0) {
            ImGui::SetCursorScreenPos(ImVec2(mid - dp(10.0f) - ls.x, ry));
            ImGui::InvisibleButton("open_folder_click", ImVec2(kx - (mid - dp(10.0f) - ls.x), row_h));
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (ImGui::IsItemClicked()) {
                std::string folder;
                if (open_folder_dialog(folder)) set_folder(folder);
            }
        }
    }
    ImGui::SetCursorScreenPos(c);
    ImGui::Dummy(size);
}

// ---------------------------------------------------------------------------
// Find and Go to Line widgets
// ---------------------------------------------------------------------------
namespace {
struct FindCache {
    int buffer = -1;
    uint64_t version = UINT64_MAX;
    std::string query;
    bool match_case = false;
    bool whole_word = false;
    std::vector<int> hits;
};
}  // namespace

static FindCache g_find;

static const std::vector<int>& find_hits(const TextBuffer& b) {
    std::string q = g_app->find_text;
    if (g_find.buffer != g_app->active || g_find.version != b.version || g_find.query != q ||
        g_find.match_case != g_app->find_case || g_find.whole_word != g_app->find_word) {
        g_find.buffer = g_app->active;
        g_find.version = b.version;
        g_find.query = q;
        g_find.match_case = g_app->find_case;
        g_find.whole_word = g_app->find_word;
        collect_matches(b, q, g_find.match_case, g_find.whole_word, g_find.hits, 20000);
    }
    return g_find.hits;
}

static bool toggle_button(const char* label, bool* v, const char* tip) {
    ImGui::PushStyleColor(ImGuiCol_Button, *v ? ImVec4(0.0f, 0.35f, 0.6f, 1.0f) : ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    bool clicked = ImGui::Button(label, ImVec2(dp(26.0f), 0));
    if (clicked) *v = !*v;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    ImGui::PopStyleColor(2);
    return clicked;
}

static void draw_find_widget(TextBuffer& b, ImVec2 view_min, float view_w) {
    static bool was_open = false;
    static std::string last_query;
    if (g_app->find_open && !was_open) {
        // Seed the query from a one-line selection or the word under the caret.
        std::string seed = buffer_selection(b);
        if (seed.empty()) {
            int a = 0, z = 0;
            if (word_at(b, b.cursor, a, z)) seed = b.text.substr((size_t)a, (size_t)(z - a));
        }
        if (!seed.empty() && seed.find('\n') == std::string::npos && seed.size() < sizeof(g_app->find_text)) {
            std::snprintf(g_app->find_text, sizeof(g_app->find_text), "%s", seed.c_str());
        }
        g_app->find_focus = true;
        last_query = g_app->find_text;
    }
    was_open = g_app->find_open;
    if (!g_app->find_open) return;

    const float w = dp(460.0f);
    ImGui::SetNextWindowPos(ImVec2(view_min.x + view_w - w - dp(20.0f), view_min.y + dp(6.0f)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.098f, 0.098f, 0.098f, 1));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.22f, 0.22f, 1));
    ImGui::Begin("##find_widget", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_AlwaysAutoResize);

    ImGuiIO& io = ImGui::GetIO();
    bool close = false;
    bool find_active = false;

    if (ImGui::ArrowButton("##repl_toggle", g_app->replace_open ? ImGuiDir_Down : ImGuiDir_Right)) {
        g_app->replace_open = !g_app->replace_open;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Replace (Ctrl+H)");
    ImGui::SameLine();
    ui_push_field();
    ImGui::SetNextItemWidth(dp(210.0f));
    if (g_app->find_focus) {
        ImGui::SetKeyboardFocusHere();
        g_app->find_focus = false;
    }
    bool keep = io.ConfigInputTextEnterKeepActive;
    io.ConfigInputTextEnterKeepActive = true;
    bool enter = ImGui::InputTextWithHint("##find", "Find", g_app->find_text, sizeof(g_app->find_text),
                                          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    io.ConfigInputTextEnterKeepActive = keep;
    find_active = ImGui::IsItemActive();
    ui_pop_field();
    if (enter) buffer_find_next(b, g_app->find_text, io.KeyShift);
    if (last_query != g_app->find_text) {
        // Incremental: jump to the first match at or after the selection start.
        last_query = g_app->find_text;
        if (g_app->find_text[0]) {
            int from = std::min(b.cursor, b.sel_anchor);
            int p = find_from(b, g_app->find_text, from, false, g_app->find_case, g_app->find_word);
            if (p >= 0) {
                b.sel_anchor = p;
                b.cursor = p + (int)std::strlen(g_app->find_text);
                buffer_reveal(b, true);
            }
        }
    }
    ImGui::SameLine();
    toggle_button("Aa", &g_app->find_case, "Match Case (Alt+C)");
    ImGui::SameLine();
    toggle_button("ab", &g_app->find_word, "Match Whole Word (Alt+W)");
    ImGui::SameLine();

    const std::vector<int>& hits = find_hits(b);
    char count[48];
    if (!g_app->find_text[0]) {
        std::snprintf(count, sizeof(count), "No results");
    } else if (hits.empty()) {
        std::snprintf(count, sizeof(count), "No results");
    } else {
        int a = std::min(b.cursor, b.sel_anchor);
        int z = std::max(b.cursor, b.sel_anchor);
        auto it = std::lower_bound(hits.begin(), hits.end(), a);
        int qn = (int)std::strlen(g_app->find_text);
        if (it != hits.end() && *it == a && z - a == qn) {
            std::snprintf(count, sizeof(count), "%d of %d%s", (int)(it - hits.begin()) + 1, (int)hits.size(),
                          hits.size() >= 20000 ? "+" : "");
        } else {
            std::snprintf(count, sizeof(count), "? of %d%s", (int)hits.size(), hits.size() >= 20000 ? "+" : "");
        }
    }
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, (g_app->find_text[0] && hits.empty())
                                             ? ImVec4(0.95f, 0.45f, 0.45f, 1) : ImVec4(0.6f, 0.6f, 0.6f, 1));
    ImGui::TextUnformatted(count);
    ImGui::PopStyleColor();
    ImGui::SameLine(w - dp(94.0f));
    if (ImGui::ArrowButton("##prev", ImGuiDir_Up)) buffer_find_next(b, g_app->find_text, true);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Previous Match (Shift+F3)");
    ImGui::SameLine();
    if (ImGui::ArrowButton("##next", ImGuiDir_Down)) buffer_find_next(b, g_app->find_text, false);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Next Match (F3)");
    ImGui::SameLine();
    if (ImGui::Button("x##close", ImVec2(dp(22.0f), 0))) close = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close (Escape)");

    bool repl_active = false;
    if (g_app->replace_open) {
        ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), 1));
        ImGui::SameLine();
        ui_push_field();
        ImGui::SetNextItemWidth(dp(210.0f));
        bool r_enter = ImGui::InputTextWithHint("##repl", "Replace", g_app->replace_text,
                                                sizeof(g_app->replace_text), ImGuiInputTextFlags_EnterReturnsTrue);
        repl_active = ImGui::IsItemActive();
        ui_pop_field();
        ImGui::SameLine();
        bool do_one = ImGui::Button("Replace") || r_enter;
        if (do_one && g_app->find_text[0]) {
            int a = std::min(b.cursor, b.sel_anchor);
            int z = std::max(b.cursor, b.sel_anchor);
            std::string q = g_app->find_text;
            if (z - a == (int)q.size() && buffer_match_at(b, a, q, g_app->find_case, g_app->find_word)) {
                buffer_replace_sel(b, g_app->replace_text);
            }
            buffer_find_next(b, g_app->find_text, false);
        }
        ImGui::SameLine();
        if (ImGui::Button("All") && g_app->find_text[0]) {
            buffer_replace_all(b, g_app->find_text, g_app->replace_text, g_app->find_case, g_app->find_word);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Replace All (Ctrl+Alt+Enter)");
    }
    if ((find_active || repl_active) && ImGui::IsKeyPressed(ImGuiKey_Escape)) close = true;
    if ((find_active || repl_active) && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_C)) g_app->find_case = !g_app->find_case;
    if ((find_active || repl_active) && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_W)) g_app->find_word = !g_app->find_word;
    if ((find_active || repl_active) && io.KeyCtrl && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Enter) &&
        g_app->find_text[0]) {
        buffer_replace_all(b, g_app->find_text, g_app->replace_text, g_app->find_case, g_app->find_word);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    if (close) {
        g_app->find_open = false;
        g_app->replace_open = false;
        g_app->editor_focused = true;
    }
}

static void draw_goto_widget(TextBuffer& b, ImVec2 view_min, float view_w) {
    static bool was_open = false;
    bool opened = g_app->goto_open && !was_open;
    was_open = g_app->goto_open;
    if (!g_app->goto_open) return;
    if (opened) g_app->goto_text[0] = 0;
    const float w = dp(320.0f);
    ImGui::SetNextWindowPos(ImVec2(view_min.x + (view_w - w) * 0.5f, view_min.y + dp(6.0f)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.098f, 0.098f, 0.098f, 1));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.22f, 0.22f, 1));
    ImGui::Begin("##goto_widget", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ui_push_field();
    ImGui::SetNextItemWidth(-1);
    if (opened) ImGui::SetKeyboardFocusHere();
    char hint[96];
    std::snprintf(hint, sizeof(hint), "Line or line:column  (current %d of %d)",
                  buffer_line_at(b, b.cursor) + 1, (int)b.lines.size());
    bool enter = ImGui::InputTextWithHint("##goto", hint, g_app->goto_text, sizeof(g_app->goto_text),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
    bool active = ImGui::IsItemActive();
    ui_pop_field();
    if (enter && g_app->goto_text[0]) {
        int line = std::max(1, std::atoi(g_app->goto_text));
        int col = 1;
        if (const char* colon = std::strchr(g_app->goto_text, ':')) col = std::max(1, std::atoi(colon + 1));
        b.cursor = b.sel_anchor = buffer_pos_at_vcol(b, line - 1, col - 1);
        b.preferred_col = buffer_vcol_at(b, b.cursor);
        buffer_reveal(b, true);
        g_app->goto_open = false;
        g_app->editor_focused = true;
    }
    if ((enter && !g_app->goto_text[0]) || (active && ImGui::IsKeyPressed(ImGuiKey_Escape)) ||
        (!active && !opened && !ImGui::IsWindowFocused())) {
        g_app->goto_open = false;
        g_app->editor_focused = true;
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------
// Editor view
// ---------------------------------------------------------------------------
static const char* completion_label(HighlightKind k) {
    switch (k) {
    case HighlightKind::Function: return "fn";
    case HighlightKind::Member: return "member";
    case HighlightKind::Builtin: return "method";
    case HighlightKind::Keyword:
    case HighlightKind::Control: return "keyword";
    case HighlightKind::Type:
    case HighlightKind::TypeName: return "type";
    case HighlightKind::Module: return "module";
    case HighlightKind::IncludePath: return "include";
    case HighlightKind::Constant: return "const";
    case HighlightKind::Identifier: return "var";
    default: return "word";
    }
}

static void draw_squiggle(ImDrawList* dl, float x0, float x1, float y, ImU32 col) {
    const float step = 2.5f;
    ImVec2 pts[512];
    int n = 0;
    for (float x = x0; x <= x1 && n < 512; x += step, n++) {
        pts[n] = ImVec2(x, y + ((n & 1) ? -1.5f : 1.0f));
    }
    if (n >= 2) dl->AddPolyline(pts, n, col, 0, 1.0f);
}

static bool diag_matches(const Diagnostic& d, const TextBuffer& b) {
    if (b.untitled) return d.path == b.name;
    return path_norm(d.path) == path_norm(b.path);
}

void draw_editor(const ImVec2& size) {
    float strip_h = dp(35.0f);
    if (!g_app->buffers.empty()) draw_editor_tabs(strip_h);
    else strip_h = 0.0f;

    if (g_app->active < 0 || g_app->active >= (int)g_app->buffers.size()) {
        comp_close();
        ImVec2 remain(size.x, size.y - strip_h);
        draw_empty_editor(remain);
        return;
    }

    TextBuffer& b = g_app->buffers[(size_t)g_app->active];
    if (b.spans_dirty || b.lines.empty()) buffer_refresh(b);
    float bc_h = dp(22.0f);
    draw_breadcrumb(b, bc_h);
    ImVec2 remain(size.x, size.y - strip_h - bc_h);
    if (g_comp.active && g_comp.buffer != g_app->active) comp_close();

    ImFont* code = g_app->font_code ? g_app->font_code : ImGui::GetFont();
    ImGui::PushFont(code);
    const float font_sz = ImGui::GetFontSize();
    const float char_w = code->CalcTextSizeA(font_sz, FLT_MAX, 0.0f, "M").x;
    const float line_h = (float)(int)(font_sz + dp(4.0f) + 0.5f);
    int line_count = std::max(1, (int)b.lines.size());
    char digits[16];
    std::snprintf(digits, sizeof(digits), "%d", std::max(99, line_count));
    const float marker_w = dp(14.0f);
    const float gutter = marker_w + ImGui::CalcTextSize(digits).x + dp(18.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0x1F / 255.0f, 0x1F / 255.0f, 0x1F / 255.0f, 1.0f));
    ImGui::BeginChild("editor_view", remain, ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoNavInputs);
    ImGuiWindow* ed_win = ImGui::GetCurrentWindow();

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
    const ImVec2 view = ImGui::GetContentRegionAvail();
    const ImVec2 view_min = ed_win->InnerRect.Min;
    const float content_h = (float)line_count * line_h + std::max(40.0f, view.y - line_h * 3.0f);
    // The minimap takes a strip on the right; text scrolls under it, so it adds to the content width.
    const float minimap_w = (b.lang != Lang::Plain || line_count > 20) && view.x > dp(520.0f) ? dp(90.0f) : 0.0f;
    const float content_w = gutter + (float)(b.max_line_len + 8) * char_w * 1.15f + 80.0f + minimap_w;
    ImGui::Dummy(ImVec2(content_w, content_h));

    // Text x of column 0; the gutter itself stays pinned while scrolling sideways.
    const float text_x0 = origin.x + gutter;
    const float gutter_x = view_min.x;
    const float scroll_y = ImGui::GetScrollY();
    int first_vis = std::max(0, (int)std::floor(scroll_y / line_h));
    int last_vis = std::min(line_count - 1, (int)std::floor((scroll_y + view.y) / line_h));

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
    auto mouse_line = [&](bool clamp_view) {
        float y = ImGui::GetIO().MousePos.y - origin.y;
        int line = (int)std::floor(y / line_h);
        if (clamp_view) line = std::clamp(line, first_vis - 1, last_vis + 1);
        return std::clamp(line, 0, line_count - 1);
    };
    auto mouse_pos = [&](bool clamp_view) {
        int line = mouse_line(clamp_view);
        float cols = (ImGui::GetIO().MousePos.x - text_x0) / char_w;
        return pos_at_x(b, line, std::max(0.0f, cols));
    };

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mm_min(view_min.x + view.x - minimap_w, view_min.y);
    const ImVec2 mm_max(view_min.x + view.x, view_min.y + view.y);
    const bool over_minimap = minimap_w > 0.0f && ImGui::IsMouseHoveringRect(mm_min, mm_max, false);
    const bool hovered = ImGui::IsWindowHovered() && !over_scrollbar() && !over_minimap;
    const bool in_gutter = io.MousePos.x < gutter_x + gutter;
    static bool drag_sel = false;
    static bool drag_lines = false;
    static int drag_origin = 0;
    bool consumed_click = false;

    // Clicks on the completion list.
    if (g_comp.active && ImGui::IsMouseClicked(0) &&
        ImGui::IsMouseHoveringRect(g_comp.rmin, g_comp.rmax, false) && g_comp.row_h > 0.0f) {
        int row = g_comp.top + (int)((io.MousePos.y - g_comp.rmin.y - 3.0f) / g_comp.row_h);
        if (row >= 0 && row < (int)g_comp.items.size()) {
            g_comp.sel = row;
            comp_accept(b);
        }
        consumed_click = true;
    }

    // Mouse: click, double-click word, triple-click line, gutter line select, drag.
    if (!consumed_click && hovered && ImGui::IsMouseClicked(0)) {
        g_app->editor_focused = true;
        g_app->terminal_focused = false;
        comp_close();
        int pos = mouse_pos(false);
        int clicks = ImGui::GetMouseClickedCount(0);
        if (in_gutter) {
            int line = mouse_line(false);
            if (io.KeyShift) {
                b.cursor = line_next(b, line);
            } else {
                b.sel_anchor = line_start(b, line);
                b.cursor = line_next(b, line);
            }
            drag_lines = true;
            drag_origin = b.sel_anchor;
        } else if (clicks >= 3) {
            int line = buffer_line_at(b, pos);
            b.sel_anchor = line_start(b, line);
            b.cursor = line_next(b, line);
        } else if (clicks == 2) {
            int a = 0, z = 0;
            if (word_at(b, pos, a, z)) {
                b.sel_anchor = a;
                b.cursor = z;
            }
        } else {
            buffer_move(b, pos, io.KeyShift);
            drag_sel = true;
            if (io.KeyCtrl) goto_definition(b);
        }
    }
    if (!ImGui::IsMouseDown(0)) {
        drag_sel = false;
        drag_lines = false;
    }
    if ((drag_sel || drag_lines) && ImGui::IsMouseDragging(0, 2.0f)) {
        if (drag_lines) {
            int line = mouse_line(true);
            int ol = buffer_line_at(b, drag_origin);
            if (line >= ol) {
                b.sel_anchor = line_start(b, ol);
                b.cursor = line_next(b, line);
            } else {
                b.sel_anchor = line_next(b, ol);
                b.cursor = line_start(b, line);
            }
        } else {
            buffer_move(b, mouse_pos(true), true);
        }
    }
    // Right-click keeps an existing selection it lands in, otherwise moves the caret.
    if (hovered && ImGui::IsMouseClicked(1)) {
        g_app->editor_focused = true;
        g_app->terminal_focused = false;
        comp_close();
        int pos = mouse_pos(false);
        int a = std::min(b.cursor, b.sel_anchor);
        int z = std::max(b.cursor, b.sel_anchor);
        if (!(a < z && pos >= a && pos <= z)) buffer_move(b, pos, false);
        ImGui::OpenPopup("##editor_ctx");
    }

    // Keyboard. Skipped while any widget (find box, palette, modal) owns input.
    const bool kb = g_app->editor_focused && !ImGui::IsAnyItemActive() && !g_app->palette_open &&
                    !g_app->quick_open && !g_app->goto_open;
    if (kb) {
        io.WantTextInput = true;
        io.WantCaptureKeyboard = true;
        const bool shift = io.KeyShift;
        const bool ctrl = io.KeyCtrl;
        const bool alt = io.KeyAlt;
        const int page = std::max(1, (int)(view.y / line_h) - 1);
        const uint64_t ver_before = b.version;
        const int cursor_before = b.cursor;
        char typed_trigger = 0;
        bool typed = false;
        bool comp_nav = false;

        if (g_comp.active) {
            int n = (int)g_comp.items.size();
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                g_comp.sel = (g_comp.sel + 1) % n;
                comp_nav = true;
            } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                g_comp.sel = (g_comp.sel - 1 + n) % n;
                comp_nav = true;
            } else if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
                g_comp.sel = std::min(n - 1, g_comp.sel + 10);
                comp_nav = true;
            } else if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
                g_comp.sel = std::max(0, g_comp.sel - 10);
                comp_nav = true;
            } else if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) ||
                       (ImGui::IsKeyPressed(ImGuiKey_Tab) && !shift)) {
                comp_accept(b);
                comp_nav = true;
            } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                comp_close();
                comp_nav = true;
            }
        }

        // AltGr arrives as Ctrl+Alt; plain Ctrl or Alt combinations are shortcuts.
        if (!(ctrl ^ alt)) {
            for (int q = 0; q < io.InputQueueCharacters.Size; q++) {
                unsigned int c = io.InputQueueCharacters[q];
                if (c < 32 || c == 127) continue;
                type_char(b, c);
                typed = true;
                typed_trigger = c < 0x80 ? (char)c : 'a';
            }
        }

        auto move_h = [&](int dir) {
            if (!shift && b.cursor != b.sel_anchor && !ctrl) {
                int edge = dir < 0 ? std::min(b.cursor, b.sel_anchor) : std::max(b.cursor, b.sel_anchor);
                buffer_move(b, edge, false);
                return;
            }
            int to = ctrl ? (dir < 0 ? word_left(b, b.cursor) : word_right(b, b.cursor))
                          : (dir < 0 ? prev_char(b, b.cursor) : next_char(b, b.cursor));
            buffer_move(b, to, shift);
        };
        auto move_v = [&](int dir) {
            int line = buffer_line_at(b, b.cursor);
            int target = line + dir;
            int col = b.preferred_col;
            if (target < 0) {
                b.cursor = 0;
            } else if (target >= line_count) {
                b.cursor = (int)b.text.size();
            } else {
                b.cursor = buffer_pos_at_vcol(b, target, col);
            }
            if (!shift) b.sel_anchor = b.cursor;
            b.preferred_col = col;
        };

        if (!comp_nav) {
            if (alt && !ctrl && ImGui::IsKeyPressed(ImGuiKey_UpArrow)) editor_exec(shift ? EdCmd::CopyLineUp : EdCmd::MoveLineUp);
            else if (alt && !ctrl && ImGui::IsKeyPressed(ImGuiKey_DownArrow)) editor_exec(shift ? EdCmd::CopyLineDown : EdCmd::MoveLineDown);
            else if (ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_UpArrow)) ImGui::SetScrollY(std::max(0.0f, scroll_y - line_h));
            else if (ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_DownArrow)) ImGui::SetScrollY(scroll_y + line_h);
            else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) move_v(-1);
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) move_v(1);
            // Paging scrolls the view with the caret instead of recentering on it.
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
                move_v(-page);
                ImGui::SetScrollY(std::max(0.0f, scroll_y - page * line_h));
                b.last_cursor = b.cursor;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
                move_v(page);
                ImGui::SetScrollY(scroll_y + page * line_h);
                b.last_cursor = b.cursor;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                if (ctrl && shift) editor_exec(EdCmd::InsertLineAbove);
                else if (ctrl) editor_exec(EdCmd::InsertLineBelow);
                else handle_enter(b);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Tab) && !ctrl) {
                bool multi = buffer_line_at(b, b.cursor) != buffer_line_at(b, b.sel_anchor);
                if (shift) {
                    buffer_indent(b, true);
                } else if (multi) {
                    buffer_indent(b, false);
                } else {
                    int v = buffer_vcol_at(b, std::min(b.cursor, b.sel_anchor));
                    buffer_replace_sel(b, std::string((size_t)(kTab - v % kTab), ' '));
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                if (g_app->find_open) {
                    g_app->find_open = false;
                    g_app->replace_open = false;
                } else {
                    b.sel_anchor = b.cursor;
                }
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) move_h(-1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) move_h(1);
        if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
            if (ctrl) {
                buffer_move(b, 0, shift);
            } else {
                int line = buffer_line_at(b, b.cursor);
                int start = line_start(b, line);
                int i = start;
                while (i < line_end(b, line) && (b.text[(size_t)i] == ' ' || b.text[(size_t)i] == '\t')) i++;
                buffer_move(b, b.cursor == i ? start : i, shift);
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_End)) {
            if (ctrl) buffer_move(b, (int)b.text.size(), shift);
            else buffer_move(b, line_end(b, buffer_line_at(b, b.cursor)), shift);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) handle_backspace(b, ctrl);
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            if (shift && !ctrl && b.cursor == b.sel_anchor) {
                editor_exec(EdCmd::Cut);
            } else if (b.cursor != b.sel_anchor) {
                if (shift) editor_exec(EdCmd::Cut);
                else buffer_delete_sel(b);
            } else if (b.cursor < (int)b.text.size()) {
                int to = ctrl ? word_right(b, b.cursor) : next_char(b, b.cursor);
                buffer_apply(b, b.cursor, to - b.cursor, "", false);
            }
        }
        if (ctrl && !alt) {
            if (ImGui::IsKeyPressed(ImGuiKey_A)) editor_exec(EdCmd::SelectAll);
            if (ImGui::IsKeyPressed(ImGuiKey_C) || ImGui::IsKeyPressed(ImGuiKey_Insert)) editor_exec(EdCmd::Copy);
            if (ImGui::IsKeyPressed(ImGuiKey_X)) editor_exec(EdCmd::Cut);
            if (ImGui::IsKeyPressed(ImGuiKey_V)) editor_exec(EdCmd::Paste);
            if (ImGui::IsKeyPressed(ImGuiKey_Z) && !shift) editor_exec(EdCmd::Undo);
            if (ImGui::IsKeyPressed(ImGuiKey_Y) || (shift && ImGui::IsKeyPressed(ImGuiKey_Z))) editor_exec(EdCmd::Redo);
            if (ImGui::IsKeyPressed(ImGuiKey_Slash)) editor_exec(EdCmd::ToggleComment);
            if (ImGui::IsKeyPressed(ImGuiKey_D)) editor_exec(EdCmd::DuplicateLine);
            if (ImGui::IsKeyPressed(ImGuiKey_L) && !shift) editor_exec(EdCmd::SelectLine);
            if (ImGui::IsKeyPressed(ImGuiKey_K) && shift) editor_exec(EdCmd::DeleteLine);
            if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) editor_exec(EdCmd::Indent);
            if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) editor_exec(EdCmd::Outdent);
            if (ImGui::IsKeyPressed(ImGuiKey_Backslash) && shift) editor_exec(EdCmd::JumpToBracket);
            if (ImGui::IsKeyPressed(ImGuiKey_Space)) editor_exec(EdCmd::TriggerSuggest);
        }
        if (shift && !ctrl && ImGui::IsKeyPressed(ImGuiKey_Insert)) editor_exec(EdCmd::Paste);
        if (ImGui::IsKeyPressed(ImGuiKey_F12)) editor_exec(EdCmd::GotoDefinition);
        if (ImGui::IsKeyPressed(ImGuiKey_F3)) editor_exec(shift ? EdCmd::FindPrev : EdCmd::FindNext);

        // Completion follows typing; anything else that moves the caret closes it.
        if (typed && b.version != ver_before) {
            bool trigger = is_word_char(typed_trigger) || typed_trigger == '.' ||
                           typed_trigger == '<' || typed_trigger == '/' || typed_trigger == '"';
            if (trigger && (b.is_nexa || b.is_json)) comp_update(b, false, typed_trigger);
            else comp_close();
        } else if (g_comp.active && b.version != ver_before && ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
            comp_update(b, false, 0);
        } else if (g_comp.active && !comp_nav && b.cursor != cursor_before) {
            comp_close();
        } else if (g_comp.active && b.version != ver_before && !comp_nav) {
            comp_close();
        }
    } else if (!g_app->editor_focused) {
        comp_close();
    }

    if (b.spans_dirty || b.lines.empty()) buffer_refresh(b);
    line_count = std::max(1, (int)b.lines.size());
    last_vis = std::min(line_count - 1, last_vis);

    // Keep the caret on screen whenever it moved (typing, jumps, find, outline...).
    if (b.cursor != b.last_cursor) {
        int cline = buffer_line_at(b, b.cursor);
        float caret_y = (float)cline * line_h;
        float caret_x = gutter + (float)buffer_vcol_at(b, b.cursor) * char_w;
        bool far_jump = cline < first_vis - 1 || cline > last_vis + 1;
        if ((b.reveal_center || far_jump) && !drag_sel && !drag_lines) {
            if (cline < first_vis || cline > last_vis - 1) {
                ImGui::SetScrollY(std::max(0.0f, caret_y - view.y * 0.4f));
            }
        } else {
            if (caret_y < scroll_y) ImGui::SetScrollY(caret_y);
            else if (caret_y + line_h > scroll_y + view.y) ImGui::SetScrollY(caret_y + line_h - view.y);
        }
        float sx = ImGui::GetScrollX();
        if (caret_x - gutter < sx) ImGui::SetScrollX(std::max(0.0f, caret_x - gutter - char_w * 4.0f));
        else if (caret_x + char_w * 2.0f > sx + view.x - minimap_w)
            ImGui::SetScrollX(caret_x + char_w * 6.0f - (view.x - minimap_w));
        b.last_cursor = b.cursor;
        b.reveal_center = false;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const std::string& t = b.text;
    const int text_n = (int)t.size();
    const int first = std::max(0, first_vis - 1);
    const int last = std::min(line_count - 1, last_vis + 1);
    const int cur_line = buffer_line_at(b, b.cursor);
    const int sel_a = std::min(b.cursor, b.sel_anchor);
    const int sel_z = std::max(b.cursor, b.sel_anchor);
    const bool focused = g_app->editor_focused;
    const float view_right = view_min.x + std::max(view.x, 0.0f) + 1.0f;

    // Line-level data for the visible range.
    auto vx = [&](int ls, int pos) { return text_x0 + (float)vcol_between(t, ls, pos) * char_w; };

    std::vector<int> err_lines;
    std::vector<std::string> err_msgs;
    for (const auto& d : g_app->problems) {
        if (!diag_matches(d, b)) continue;
        err_lines.push_back(std::clamp(d.line - 1, 0, line_count - 1));
        err_msgs.push_back(d.message);
    }

    // Word under the caret, or a one-line selection: its other occurrences get a box.
    std::string occ;
    bool occ_word = false;
    if (focused) {
        if (sel_z > sel_a) {
            if (sel_z - sel_a <= 200 && t.find('\n', (size_t)sel_a) >= (size_t)sel_z) {
                occ = t.substr((size_t)sel_a, (size_t)(sel_z - sel_a));
                if (occ.find_first_not_of(" \t") == std::string::npos) occ.clear();
            }
        } else if (b.lang != Lang::Plain) {
            int a = 0, z = 0;
            if (word_at(b, b.cursor, a, z) && !std::isdigit((unsigned char)t[(size_t)a])) {
                HighlightKind k = kind_at(b, a);
                if (k != HighlightKind::Comment && k != HighlightKind::String) {
                    occ = t.substr((size_t)a, (size_t)(z - a));
                    occ_word = true;
                }
            }
        }
    }

    int br_a = -1, br_z = -1;
    if (focused) caret_brackets(b, br_a, br_z);

    const bool show_find = g_app->find_open && g_app->find_text[0];
    const std::vector<int>* hits = show_find ? &find_hits(b) : nullptr;
    const int find_len = (int)std::strlen(g_app->find_text);

    // Indent level (in columns) of each visible line; blank lines borrow the
    // deeper of their neighbours so guides run through them.
    auto indent_of = [&](int line, bool& blank) {
        int ls = line_start(b, line);
        int le = line_end(b, line);
        int i = ls;
        while (i < le && (t[(size_t)i] == ' ' || t[(size_t)i] == '\t')) i++;
        blank = i == le;
        return vcol_between(t, ls, i);
    };

    const ImU32 col_cur_line = IM_COL32(0x28, 0x28, 0x28, 255);
    const ImU32 col_sel = focused ? IM_COL32(38, 79, 120, 255) : IM_COL32(0x3A, 0x3D, 0x41, 255);
    const ImU32 col_occ = IM_COL32(0x57, 0x57, 0x57, 0x90);
    const ImU32 col_find = IM_COL32(234, 92, 0, 90);
    const ImU32 col_guide = IM_COL32(0x40, 0x40, 0x40, 255);

    for (int line = first; line <= last; line++) {
        const float y = origin.y + (float)line * line_h;
        const int ls = std::clamp(line_start(b, line), 0, text_n);
        const int le = std::clamp(line_end(b, line), ls, text_n);

        if (line == cur_line && sel_a == sel_z) {
            if (focused) {
                // VS Code draws the current line as a 2px border rather than a fill.
                dl->AddRect(ImVec2(view_min.x + gutter - dp(6.0f), y + 0.5f), ImVec2(view_right - 1.0f, y + line_h - 0.5f),
                            col_cur_line, 0.0f, 0, 2.0f);
            } else {
                dl->AddRect(ImVec2(view_min.x + gutter - dp(6.0f), y + 0.5f), ImVec2(view_right - 1.0f, y + line_h - 0.5f),
                            IM_COL32(0x24, 0x24, 0x24, 255));
            }
        }

        if (hits && !hits->empty()) {
            auto it = std::lower_bound(hits->begin(), hits->end(), ls);
            for (; it != hits->end() && *it < le; ++it) {
                int hz = std::min(*it + find_len, le);
                dl->AddRectFilled(ImVec2(vx(ls, *it), y), ImVec2(vx(ls, hz), y + line_h), col_find);
            }
        }

        if (!occ.empty() && le - ls >= (int)occ.size()) {
            size_t p = (size_t)ls;
            while ((p = t.find(occ, p)) != std::string::npos && (int)(p + occ.size()) <= le) {
                bool ok = true;
                if (occ_word) {
                    if (p > 0 && is_word_char(t[p - 1])) ok = false;
                    if (p + occ.size() < t.size() && is_word_char(t[p + occ.size()])) ok = false;
                }
                if (ok && !((int)p == sel_a && (int)(p + occ.size()) == sel_z)) {
                    ImVec2 a0(vx(ls, (int)p), y);
                    ImVec2 a1(vx(ls, (int)(p + occ.size())), y + line_h);
                    dl->AddRectFilled(a0, a1, col_occ, 2.0f);
                }
                p += occ.size();
            }
        }

        if (sel_z > sel_a && sel_z >= ls && sel_a <= le) {
            int a = std::max(sel_a, ls);
            int z = std::min(sel_z, le);
            float x0 = vx(ls, a);
            float x1 = vx(ls, z);
            if (sel_z > le) x1 += char_w * 0.6f;  // the newline is part of the selection
            if (x1 > x0) dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + line_h), col_sel);
        }

        // Indent guides.
        bool blank = false;
        int indent = indent_of(line, blank);
        if (blank) {
            int up = 0, down = 0;
            bool bl = true;
            for (int k = line - 1; k >= 0 && k >= line - 200; k--) {
                up = indent_of(k, bl);
                if (!bl) break;
                up = 0;
            }
            for (int k = line + 1; k < line_count && k <= line + 200; k++) {
                down = indent_of(k, bl);
                if (!bl) break;
                down = 0;
            }
            indent = std::max(up, down);
        }
        for (int stop = 0; stop < indent; stop += kTab) {
            float gx = (float)(int)(text_x0 + (float)stop * char_w) + 0.5f;
            dl->AddLine(ImVec2(gx, y), ImVec2(gx, y + line_h), col_guide);
        }

        // Matching bracket pair box.
        for (int p : {br_a, br_z}) {
            if (p >= ls && p < le) {
                ImVec2 a0(vx(ls, p), y + 1.0f);
                ImVec2 a1(a0.x + char_w, y + line_h - 1.0f);
                dl->AddRectFilled(a0, a1, IM_COL32(0x00, 0x64, 0x00, 0x40));
                dl->AddRect(a0, a1, IM_COL32(0x88, 0x88, 0x88, 0xFF));
            }
        }

        // Text runs, colored by span; tabs advance to the next tab stop.
        if (le > ls) {
            int v = 0;
            auto draw_run = [&](int a, int z, ImU32 col) {
                int seg = a;
                int seg_v = v;
                for (int q = a; q < z; q++) {
                    char c = t[(size_t)q];
                    if (c == '\t') {
                        if (q > seg) {
                            dl->AddText(code, font_sz, ImVec2(text_x0 + (float)seg_v * char_w, y + 1.0f), col,
                                        t.data() + seg, t.data() + q);
                        }
                        v = (v / kTab + 1) * kTab;
                        seg = q + 1;
                        seg_v = v;
                        continue;
                    }
                    if (!is_cont_byte(c)) v++;
                }
                if (z > seg) {
                    dl->AddText(code, font_sz, ImVec2(text_x0 + (float)seg_v * char_w, y + 1.0f), col,
                                t.data() + seg, t.data() + z);
                }
            };
            auto it = std::upper_bound(b.spans.begin(), b.spans.end(), ls,
                                       [](int p, const HighlightSpan& s) { return p < s.end; });
            int p = ls;
            while (p < le) {
                HighlightKind kind = HighlightKind::Text;
                int next = le;
                while (it != b.spans.end() && it->end <= p) ++it;
                if (it != b.spans.end()) {
                    if (it->start > p) next = std::min(le, it->start);
                    else {
                        kind = it->kind;
                        next = std::min(le, it->end);
                    }
                }
                if (next <= p) next = p + 1;
                draw_run(p, next, highlight_color(kind));
                p = next;
            }
        }

        // Error squiggles.
        for (size_t e = 0; e < err_lines.size(); e++) {
            if (err_lines[e] != line) continue;
            int i = ls;
            while (i < le && (t[(size_t)i] == ' ' || t[(size_t)i] == '\t')) i++;
            float x0 = vx(ls, i);
            float x1 = std::max(vx(ls, le), x0 + char_w * 2.0f);
            draw_squiggle(dl, x0, x1, y + line_h - 2.0f, IM_COL32(0xF1, 0x4C, 0x4C, 0xFF));
            break;
        }
    }

    // Caret, with the blink restarting whenever it moves.
    static double caret_moved = 0.0;
    static int caret_last = -1;
    static uint64_t caret_ver = 0;
    if (caret_last != b.cursor || caret_ver != b.version) {
        caret_last = b.cursor;
        caret_ver = b.version;
        caret_moved = ImGui::GetTime();
    }
    const float caret_x = text_x0 + (float)buffer_vcol_at(b, b.cursor) * char_w;
    const float caret_y = origin.y + (float)cur_line * line_h;
    if (focused && std::fmod(ImGui::GetTime() - caret_moved, 1.0) < 0.55) {
        dl->AddRectFilled(ImVec2(caret_x, caret_y + 1.0f), ImVec2(caret_x + 2.0f, caret_y + line_h - 1.0f),
                          IM_COL32(0xAE, 0xAF, 0xAD, 255));
    }

    // Pinned gutter: background, line numbers, error markers.
    {
        float gy0 = view_min.y;
        float gy1 = ed_win->InnerRect.Max.y;
        dl->AddRectFilled(ImVec2(gutter_x, gy0), ImVec2(gutter_x + gutter - dp(6.0f), gy1), IM_COL32(0x1F, 0x1F, 0x1F, 255));
        for (int line = first; line <= last; line++) {
            float y = origin.y + (float)line * line_h;
            char num[16];
            std::snprintf(num, sizeof(num), "%d", line + 1);
            ImU32 num_col = (line == cur_line) ? IM_COL32(0xCC, 0xCC, 0xCC, 255) : IM_COL32(0x6E, 0x76, 0x81, 255);
            float nw = ImGui::CalcTextSize(num).x;
            dl->AddText(ImVec2(gutter_x + gutter - dp(14.0f) - nw, y + 1.0f), num_col, num);
            if (std::find(err_lines.begin(), err_lines.end(), line) != err_lines.end()) {
                dl->AddCircleFilled(ImVec2(gutter_x + marker_w * 0.5f + 2.0f, y + line_h * 0.5f), 4.0f,
                                    IM_COL32(0xF1, 0x4C, 0x4C, 0xFF), 12);
            }
        }
    }

    // Minimap: two pixels per line, one per column, coloured by token.
    if (minimap_w > 0.0f) {
        static bool mm_drag = false;
        const float mm_line = 2.0f;
        const int mm_rows = std::max(1, (int)(view.y / mm_line));
        const float max_scroll = std::max(1.0f, content_h - view.y);
        int mm_first = 0;
        if (line_count > mm_rows) {
            mm_first = (int)((scroll_y / max_scroll) * (float)(line_count - mm_rows) + 0.5f);
            mm_first = std::clamp(mm_first, 0, line_count - mm_rows);
        }
        dl->AddRectFilled(mm_min, mm_max, IM_COL32(0x1F, 0x1F, 0x1F, 255));
        dl->AddLine(ImVec2(mm_min.x + 0.5f, mm_min.y), ImVec2(mm_min.x + 0.5f, mm_max.y), IM_COL32(0x2B, 0x2B, 0x2B, 255));
        dl->PushClipRect(mm_min, mm_max, true);
        const float mx0 = mm_min.x + dp(6.0f);
        const int max_cols = (int)(minimap_w - dp(10.0f));
        auto it = b.spans.begin();
        for (int line = mm_first; line < std::min(line_count, mm_first + mm_rows); line++) {
            const int ls = line_start(b, line);
            const int le = line_end(b, line);
            const float my0 = mm_min.y + (float)(line - mm_first) * mm_line;
            it = std::upper_bound(it, b.spans.end(), ls, [](int p, const HighlightSpan& sp) { return p < sp.end; });
            auto sit = it;
            int v = 0;
            int q = ls;
            while (q < le && v < max_cols) {
                char ch = t[(size_t)q];
                if (ch == ' ' || ch == '\t') {
                    v = ch == '\t' ? (v / 4 + 1) * 4 : v + 1;
                    q++;
                    continue;
                }
                // A run of non-space characters sharing one span colour.
                while (sit != b.spans.end() && sit->end <= q) ++sit;
                HighlightKind kind = (sit != b.spans.end() && sit->start <= q) ? sit->kind : HighlightKind::Text;
                int run_end = q;
                int run_v = v;
                while (run_end < le && t[(size_t)run_end] != ' ' && t[(size_t)run_end] != '\t' &&
                       !(sit != b.spans.end() && run_end >= sit->end) &&
                       !(sit != b.spans.end() && sit->start > q && run_end >= sit->start)) {
                    if (!is_cont_byte(t[(size_t)run_end])) run_v++;
                    run_end++;
                }
                if (run_end == q) run_end = q + 1, run_v = v + 1;
                ImU32 col = (highlight_color(kind) & 0x00FFFFFF) | 0xA0000000;
                dl->AddRectFilled(ImVec2(mx0 + (float)v, my0), ImVec2(mx0 + (float)std::min(run_v, max_cols), my0 + 1.5f), col);
                v = run_v;
                q = run_end;
            }
        }
        for (int el : err_lines) {
            if (el >= mm_first && el < mm_first + mm_rows) {
                float ey = mm_min.y + (float)(el - mm_first) * mm_line;
                dl->AddRectFilled(ImVec2(mm_min.x + 1.0f, ey - 1.0f), ImVec2(mm_min.x + 4.0f, ey + 3.0f), IM_COL32(0xF1, 0x4C, 0x4C, 255));
            }
        }
        // Viewport slider.
        float sy0 = mm_min.y + (float)(first_vis - mm_first) * mm_line;
        float sy1 = sy0 + std::max(4.0f, view.y / line_h * mm_line);
        bool over_slider = over_minimap && io.MousePos.y >= sy0 && io.MousePos.y <= sy1;
        dl->AddRectFilled(ImVec2(mm_min.x + 1.0f, sy0), ImVec2(mm_max.x, sy1),
                          (mm_drag || over_slider) ? IM_COL32(0x79, 0x79, 0x79, 0x40) : IM_COL32(0x79, 0x79, 0x79, 0x22));
        dl->PopClipRect();
        if (over_minimap && ImGui::IsMouseClicked(0)) mm_drag = true;
        if (!ImGui::IsMouseDown(0)) mm_drag = false;
        if (mm_drag) {
            // Centre the view on the line under the pointer.
            int target = mm_first + (int)((io.MousePos.y - mm_min.y) / mm_line);
            target = std::clamp(target, 0, line_count - 1);
            ImGui::SetScrollY(std::clamp((float)target * line_h - view.y * 0.5f, 0.0f, max_scroll));
        }
    }

    // Overview ruler inside the vertical scrollbar track.
    if (ed_win->ScrollbarY) {
        ImRect track = ImGui::GetWindowScrollbarRect(ed_win, ImGuiAxis_Y);
        float th = track.GetHeight();
        dl->PushClipRect(track.Min, track.Max, false);
        auto mark_y = [&](int line) { return track.Min.y + ((float)line + 0.5f) / (float)line_count * th; };
        if (hits) {
            float last_y = -1000.0f;
            for (int h : *hits) {
                float my = mark_y(buffer_line_at(b, h));
                if (my - last_y < 2.0f) continue;
                last_y = my;
                dl->AddRectFilled(ImVec2(track.Min.x + 1.0f, my - 1.0f), ImVec2(track.Max.x - 1.0f, my + 1.0f),
                                  IM_COL32(234, 92, 0, 200));
            }
        }
        for (int el : err_lines) {
            float my = mark_y(el);
            dl->AddRectFilled(ImVec2(track.Max.x - 5.0f, my - 2.0f), ImVec2(track.Max.x - 1.0f, my + 2.0f),
                              IM_COL32(0xF1, 0x4C, 0x4C, 0xFF));
        }
        float cy = mark_y(cur_line);
        dl->AddLine(ImVec2(track.Min.x, cy), ImVec2(track.Max.x, cy), IM_COL32(0xA0, 0xA0, 0xA0, 0xC0), 2.0f);
        dl->PopClipRect();
    }

    // Hover: diagnostic message, and Ctrl+hover definition link.
    if (hovered && !ImGui::IsMouseDown(0) && !g_comp.active) {
        int hl = mouse_line(false);
        for (size_t e = 0; e < err_lines.size(); e++) {
            if (err_lines[e] == hl && io.MousePos.y >= origin.y + hl * line_h &&
                io.MousePos.y < origin.y + (hl + 1) * line_h) {
                ImGui::SetTooltip("%s", err_msgs[e].c_str());
                break;
            }
        }
        if (io.KeyCtrl && !in_gutter) {
            int pos = mouse_pos(false);
            int a = 0, z = 0;
            if (word_at(b, pos, a, z)) {
                std::string w = t.substr((size_t)a, (size_t)(z - a));
                bool def = false;
                for (const auto& d : b.outline) def = def || d.name == w;
                if (def) {
                    int line = buffer_line_at(b, a);
                    float uy = origin.y + (float)(line + 1) * line_h - 2.0f;
                    int ls = line_start(b, line);
                    dl->AddLine(ImVec2(vx(ls, a), uy), ImVec2(vx(ls, z), uy), highlight_color(kind_at(b, a)));
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
            }
        } else if (!in_gutter) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
        }
    }

    // Completion list, drawn above everything and flipped above the caret near the bottom.
    if (g_comp.active && focused && !g_comp.items.empty()) {
        ImDrawList* fl = ImGui::GetForegroundDrawList();
        const int n = (int)g_comp.items.size();
        const int rows = std::min(10, n);
        g_comp.sel = std::clamp(g_comp.sel, 0, n - 1);
        if (g_comp.sel < g_comp.top) g_comp.top = g_comp.sel;
        if (g_comp.sel >= g_comp.top + rows) g_comp.top = g_comp.sel - rows + 1;
        g_comp.top = std::clamp(g_comp.top, 0, std::max(0, n - rows));
        float pw = dp(300.0f);
        for (int i = g_comp.top; i < g_comp.top + rows; i++) {
            pw = std::max(pw, ImGui::CalcTextSize(g_comp.items[(size_t)i].text.c_str()).x + dp(90.0f));
        }
        float ph = (float)rows * line_h + 6.0f;
        float px = caret_x - ImGui::CalcTextSize(g_comp.prefix.c_str()).x - dp(4.0f);
        float py = caret_y + line_h + 2.0f;
        ImVec2 vp_max = ImGui::GetMainViewport()->WorkPos + ImGui::GetMainViewport()->WorkSize;
        if (py + ph > vp_max.y) py = caret_y - ph - 2.0f;
        if (px + pw > vp_max.x) px = vp_max.x - pw - 4.0f;
        g_comp.rmin = ImVec2(px, py);
        g_comp.rmax = ImVec2(px + pw, py + ph);
        g_comp.row_h = line_h;
        fl->AddRectFilled(ImVec2(px + 3, py + 4), ImVec2(px + pw + 3, py + ph + 4), IM_COL32(0, 0, 0, 90), 4.0f);
        fl->AddRectFilled(g_comp.rmin, g_comp.rmax, IM_COL32(0x1B, 0x1B, 0x1C, 252), 4.0f);
        fl->AddRect(g_comp.rmin, g_comp.rmax, IM_COL32(0x45, 0x45, 0x45, 255), 4.0f);
        for (int r = 0; r < rows; r++) {
            int i = g_comp.top + r;
            const Completion& c = g_comp.items[(size_t)i];
            float ry = py + 3.0f + (float)r * line_h;
            if (i == g_comp.sel) {
                fl->AddRectFilled(ImVec2(px + 2, ry), ImVec2(px + pw - 2, ry + line_h), IM_COL32(0x04, 0x39, 0x5E, 255), 3.0f);
            }
            ImU32 col = c.kind == HighlightKind::Text ? IM_COL32(0xB0, 0xB0, 0xB0, 255) : highlight_color(c.kind);
            // The typed prefix is emphasised in the accent color.
            size_t plen = std::min(g_comp.prefix.size(), c.text.size());
            fl->AddText(code, font_sz, ImVec2(px + 10.0f, ry + 1.0f), IM_COL32(0x2A, 0xAA, 0xFF, 255),
                        c.text.c_str(), c.text.c_str() + plen);
            float pxw = code->CalcTextSizeA(font_sz, FLT_MAX, 0.0f, c.text.c_str(), c.text.c_str() + plen).x;
            fl->AddText(code, font_sz, ImVec2(px + 10.0f + pxw, ry + 1.0f), col, c.text.c_str() + plen);
            const char* label = completion_label(c.kind);
            float lw = ImGui::CalcTextSize(label).x;
            fl->AddText(code, font_sz, ImVec2(px + pw - lw - 10.0f, ry + 1.0f), IM_COL32(0x80, 0x80, 0x80, 255), label);
        }
        if (n > rows) {
            float sh = ph * (float)rows / (float)n;
            float sy = py + (ph - sh) * (float)g_comp.top / (float)(n - rows);
            fl->AddRectFilled(ImVec2(px + pw - 4.0f, sy), ImVec2(px + pw - 1.0f, sy + sh), IM_COL32(0x60, 0x60, 0x60, 200), 2.0f);
        }
    }

    // Context menu.
    ImGui::PushFont(g_app->font_ui ? g_app->font_ui : code);
    if (ImGui::BeginPopup("##editor_ctx")) {
        bool has_sel = b.cursor != b.sel_anchor;
        if (b.is_nexa && ImGui::MenuItem("Go to Definition", "F12")) editor_exec(EdCmd::GotoDefinition);
        if (b.is_nexa) ImGui::Separator();
        if (ImGui::MenuItem("Cut", "Ctrl+X")) editor_exec(EdCmd::Cut);
        if (ImGui::MenuItem("Copy", "Ctrl+C")) editor_exec(EdCmd::Copy);
        if (ImGui::MenuItem("Paste", "Ctrl+V")) editor_exec(EdCmd::Paste);
        ImGui::Separator();
        if (ImGui::MenuItem("Select All", "Ctrl+A")) editor_exec(EdCmd::SelectAll);
        if (lang_line_comment(b.lang, b.path) && ImGui::MenuItem("Toggle Line Comment", "Ctrl+/"))
            editor_exec(EdCmd::ToggleComment);
        if (ImGui::MenuItem("Find", "Ctrl+F")) g_app->find_open = true;
        if (ImGui::MenuItem("Replace", "Ctrl+H")) g_app->find_open = g_app->replace_open = true;
        if (has_sel) {
            ImGui::Separator();
            if (ImGui::MenuItem("Transform to Uppercase")) editor_exec(EdCmd::UpperCase);
            if (ImGui::MenuItem("Transform to Lowercase")) editor_exec(EdCmd::LowerCase);
        }
        ImGui::EndPopup();
    }
    ImGui::PopFont();

    b.scroll_x = ImGui::GetScrollX();
    b.scroll_y = ImGui::GetScrollY();
    const float view_w = ed_win->InnerRect.GetWidth();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopFont();

    draw_find_widget(b, view_min, view_w - minimap_w);
    draw_goto_widget(b, view_min, view_w);
}
