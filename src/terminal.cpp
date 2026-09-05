#include "nexium.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#include "imgui_internal.h"

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE ((DWORD)0x00020016)
#endif

struct PtyApi {
    HRESULT (WINAPI* Create)(COORD, HANDLE, HANDLE, DWORD, void**) = nullptr;
    void (WINAPI* Close)(void*) = nullptr;
    HRESULT (WINAPI* Resize)(void*, COORD) = nullptr;
    bool ok = false;
};

static PtyApi& pty_api() {
    static PtyApi a;
    static bool init = false;
    if (!init) {
        init = true;
        HMODULE k = GetModuleHandleW(L"kernel32.dll");
        if (k) {
            a.Create = (decltype(a.Create))GetProcAddress(k, "CreatePseudoConsole");
            a.Close = (decltype(a.Close))GetProcAddress(k, "ClosePseudoConsole");
            a.Resize = (decltype(a.Resize))GetProcAddress(k, "ResizePseudoConsole");
            a.ok = a.Create && a.Close && a.Resize;
        }
    }
    return a;
}

static const ImU32 kDefFg = IM_COL32(0xCC, 0xCC, 0xCC, 255);
static const ImU32 kAnsi[16] = {
    IM_COL32(0x0C, 0x0C, 0x0C, 255), IM_COL32(0xC5, 0x0F, 0x1F, 255),
    IM_COL32(0x13, 0xA1, 0x0E, 255), IM_COL32(0xC1, 0x9C, 0x00, 255),
    IM_COL32(0x00, 0x37, 0xDA, 255), IM_COL32(0x88, 0x17, 0x98, 255),
    IM_COL32(0x3A, 0x96, 0xDD, 255), IM_COL32(0xCC, 0xCC, 0xCC, 255),
    IM_COL32(0x76, 0x76, 0x76, 255), IM_COL32(0xE7, 0x48, 0x56, 255),
    IM_COL32(0x16, 0xC6, 0x0C, 255), IM_COL32(0xF9, 0xF1, 0xA5, 255),
    IM_COL32(0x3B, 0x78, 0xFF, 255), IM_COL32(0xB4, 0x00, 0x9E, 255),
    IM_COL32(0x61, 0xD6, 0xD6, 255), IM_COL32(0xF2, 0xF2, 0xF2, 255),
};

static void utf8_push(std::string& o, uint32_t ch) {
    if (ch < 0x80) o.push_back((char)ch);
    else if (ch < 0x800) {
        o.push_back((char)(0xC0 | (ch >> 6)));
        o.push_back((char)(0x80 | (ch & 0x3F)));
    } else if (ch < 0x10000) {
        o.push_back((char)(0xE0 | (ch >> 12)));
        o.push_back((char)(0x80 | ((ch >> 6) & 0x3F)));
        o.push_back((char)(0x80 | (ch & 0x3F)));
    } else {
        o.push_back((char)(0xF0 | (ch >> 18)));
        o.push_back((char)(0x80 | ((ch >> 12) & 0x3F)));
        o.push_back((char)(0x80 | ((ch >> 6) & 0x3F)));
        o.push_back((char)(0x80 | (ch & 0x3F)));
    }
}

static void screen_ensure(TermScreen& s) {
    size_t n = (size_t)s.cols * (size_t)s.rows;
    if (s.cells.size() != n) s.cells.assign(n, TermCell{});
    if (s.cx < 0) s.cx = 0;
    if (s.cy < 0) s.cy = 0;
    if (s.cx >= s.cols) s.cx = s.cols - 1;
    if (s.cy >= s.rows) s.cy = s.rows - 1;
}

static TermCell& cell_at(TermScreen& s, int x, int y) {
    return s.cells[(size_t)y * (size_t)s.cols + (size_t)x];
}

static void pad_row(std::vector<TermCell>& row, int cols) {
    if ((int)row.size() < cols) row.resize((size_t)cols, TermCell{});
    else if ((int)row.size() > cols) row.resize((size_t)cols);
}

static std::vector<TermCell> row_copy(const TermScreen& s, int y) {
    auto b = s.cells.begin() + (size_t)y * (size_t)s.cols;
    return std::vector<TermCell>(b, b + s.cols);
}

static void scroll_up(TermScreen& s) {
    auto r = row_copy(s, 0);
    pad_row(r, s.cols);
    s.scrollback.push_back(std::move(r));
    if ((int)s.scrollback.size() > 4000)
        s.scrollback.erase(s.scrollback.begin(), s.scrollback.begin() + ((int)s.scrollback.size() - 3000));
    if (s.rows > 1)
        std::memmove(s.cells.data(), s.cells.data() + s.cols,
                     sizeof(TermCell) * (size_t)(s.rows - 1) * (size_t)s.cols);
    for (int x = 0; x < s.cols; x++) cell_at(s, x, s.rows - 1) = TermCell{};
    s.gen++;
}

static void line_feed(TermScreen& s) {
    s.cx = 0;
    if (s.cy + 1 < s.rows) s.cy++;
    else scroll_up(s);
    s.gen++;
}

static void put_ch(TermScreen& s, uint32_t ch) {
    screen_ensure(s);
    if (ch == 0) return;
    if (s.cx >= s.cols) {
        line_feed(s);
    }
    TermCell& c = cell_at(s, s.cx, s.cy);
    c.ch = ch;
    c.fg = s.fg;
    s.cx++;
    s.gen++;
}

static void erase_line(TermScreen& s, int mode) {
    screen_ensure(s);
    int a = 0, b = s.cols;
    if (mode == 0) a = s.cx;
    else if (mode == 1) b = s.cx + 1;
    for (int x = a; x < b && x < s.cols; x++) cell_at(s, x, s.cy) = TermCell{};
    s.gen++;
}

static void erase_disp(TermScreen& s, int mode) {
    screen_ensure(s);
    if (mode >= 2) {
        for (auto& c : s.cells) c = TermCell{};
        if (mode >= 3) s.scrollback.clear();
        s.cx = 0;
        s.cy = 0;
    } else if (mode == 0) {
        erase_line(s, 0);
        for (int y = s.cy + 1; y < s.rows; y++)
            for (int x = 0; x < s.cols; x++) cell_at(s, x, y) = TermCell{};
    } else {
        erase_line(s, 1);
        for (int y = 0; y < s.cy; y++)
            for (int x = 0; x < s.cols; x++) cell_at(s, x, y) = TermCell{};
    }
    s.gen++;
}

static int csi_num(const std::vector<int>& p, int i, int defv) {
    if (i >= (int)p.size() || p[(size_t)i] <= 0) return defv;
    return p[(size_t)i];
}

static std::vector<int> parse_params(const std::string& seq) {
    std::vector<int> p;
    int n = 0;
    bool any = false;
    for (char ch : seq) {
        if (ch == '?') continue;
        if (ch >= '0' && ch <= '9') {
            n = n * 10 + (ch - '0');
            any = true;
        } else if (ch == ';') {
            p.push_back(any ? n : 0);
            n = 0;
            any = false;
        }
    }
    p.push_back(any ? n : 0);
    return p;
}

static void apply_sgr(TermScreen& s, const std::vector<int>& p) {
    if (p.empty()) {
        s.fg = kDefFg;
        return;
    }
    for (size_t i = 0; i < p.size(); i++) {
        int v = p[i];
        if (v == 0 || v == 39) s.fg = kDefFg;
        else if (v >= 30 && v <= 37) s.fg = kAnsi[v - 30];
        else if (v >= 90 && v <= 97) s.fg = kAnsi[v - 90 + 8];
        else if (v == 38 && i + 1 < p.size()) {
            if (p[i + 1] == 5 && i + 2 < p.size()) {
                int idx = p[i + 2];
                if (idx >= 0 && idx < 16) s.fg = kAnsi[idx];
                else s.fg = kDefFg;
                i += 2;
            } else if (p[i + 1] == 2 && i + 4 < p.size()) {
                s.fg = IM_COL32((int)p[i + 2] & 255, (int)p[i + 3] & 255, (int)p[i + 4] & 255, 255);
                i += 4;
            }
        }
    }
}

static void apply_csi(TermScreen& s, const std::string& seq, char fin) {
    screen_ensure(s);
    auto p = parse_params(seq);
    int n = csi_num(p, 0, 1);
    if (s.csi_q) {
        if ((fin == 'h' || fin == 'l') && !p.empty() && p[0] == 25)
            s.cursor_vis = (fin == 'h');
        return;
    }
    switch (fin) {
    case 'A': s.cy = std::max(0, s.cy - n); break;
    case 'B': s.cy = std::min(s.rows - 1, s.cy + n); break;
    case 'C': s.cx = std::min(s.cols - 1, s.cx + n); break;
    case 'D': s.cx = std::max(0, s.cx - n); break;
    case 'G': s.cx = std::min(s.cols, std::max(1, csi_num(p, 0, 1))) - 1; break;
    case 'd': s.cy = std::min(s.rows, std::max(1, csi_num(p, 0, 1))) - 1; break;
    case 'H':
    case 'f': {
        int r = csi_num(p, 0, 1);
        int c = p.size() > 1 ? csi_num(p, 1, 1) : 1;
        s.cy = std::min(s.rows, std::max(1, r)) - 1;
        s.cx = std::min(s.cols, std::max(1, c)) - 1;
        break;
    }
    case 'J': erase_disp(s, p.empty() ? 0 : p[0]); break;
    case 'K': erase_line(s, p.empty() ? 0 : p[0]); break;
    case 'm': apply_sgr(s, p); break;
    case 's': s.saved_cx = s.cx; s.saved_cy = s.cy; break;
    case 'u': s.cx = s.saved_cx; s.cy = s.saved_cy; break;
    default: break;
    }
    s.gen++;
}

static void screen_reset_parser(TermScreen& s) {
    s.parse = 0;
    s.csi_q = false;
    s.seq.clear();
    s.utf = 0;
    s.utf_need = 0;
}

static void screen_feed(TermScreen& s, const char* data, size_t n) {
    screen_ensure(s);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)data[i];
        if (s.utf_need > 0 && s.parse == 0) {
            if ((c & 0xC0) == 0x80) {
                s.utf = (s.utf << 6) | (c & 0x3F);
                s.utf_need--;
                if (s.utf_need == 0) {
                    if (s.utf >= 32 && s.utf <= 0x10FFFF && !(s.utf >= 0xD800 && s.utf <= 0xDFFF))
                        put_ch(s, s.utf);
                }
                continue;
            }
            s.utf_need = 0;
            s.utf = 0;
        }

        // OSC / DCS / APC / PM — end on BEL or ST (ESC \), never on a lone '\'
        if (s.parse == 3) {
            if (c == 0x07) {
                s.parse = 0;
                s.seq.clear();
            } else if (c == 0x1B) {
                s.parse = 4;
            } else if (c == 0x9C) {
                s.parse = 0;
                s.seq.clear();
            }
            continue;
        }
        if (s.parse == 4) {
            if (c == '\\') {
                s.parse = 0;
                s.seq.clear();
                continue;
            }
            s.parse = 1;
        }

        if (s.parse == 1) {
            if (c == '[') {
                s.parse = 2;
                s.csi_q = false;
                s.seq.clear();
            } else if (c == ']' || c == 'P' || c == '_' || c == '^' || c == 'X') {
                s.parse = 3;
                s.seq.clear();
            } else if (c == '7') {
                s.saved_cx = s.cx;
                s.saved_cy = s.cy;
                s.parse = 0;
            } else if (c == '8') {
                s.cx = s.saved_cx;
                s.cy = s.saved_cy;
                s.parse = 0;
            } else if (c == 'M') {
                if (s.cy > 0) s.cy--;
                s.parse = 0;
            } else if (c == 'c') {
                erase_disp(s, 2);
                s.fg = kDefFg;
                s.parse = 0;
            } else if (c == 'E') {
                line_feed(s);
                s.parse = 0;
            } else if (c == 'D') {
                if (s.cy + 1 < s.rows) s.cy++;
                else scroll_up(s);
                s.parse = 0;
            } else {
                s.parse = 0;
            }
            continue;
        }
        if (s.parse == 2) {
            if (c == '?') {
                s.csi_q = true;
                continue;
            }
            if (c >= 0x40 && c <= 0x7E) {
                apply_csi(s, s.seq, (char)c);
                s.parse = 0;
                s.seq.clear();
            } else if (c >= 0x20 && c <= 0x3F) {
                s.seq.push_back((char)c);
                if (s.seq.size() > 256) {
                    s.parse = 0;
                    s.seq.clear();
                }
            } else {
                s.parse = 0;
                s.seq.clear();
            }
            continue;
        }
        if (c == 0x1B) {
            s.parse = 1;
            continue;
        }
        if (c == 0x9B) {
            s.parse = 2;
            s.csi_q = false;
            s.seq.clear();
            continue;
        }
        if (c == 0x9D || c == 0x90 || c == 0x9E || c == 0x9F) {
            s.parse = 3;
            s.seq.clear();
            continue;
        }
        if (c == '\r') {
            s.cx = 0;
            s.gen++;
            continue;
        }
        if (c == '\n') {
            line_feed(s);
            continue;
        }
        if (c == '\b') {
            if (s.cx > 0) s.cx--;
            s.gen++;
            continue;
        }
        if (c == '\t') {
            int next = (s.cx + 8) & ~7;
            if (next >= s.cols) next = s.cols - 1;
            if (next < 0) next = 0;
            s.cx = next;
            continue;
        }
        if (c == 0x07 || c == 0) continue;
        if (c == 0x0C) {
            erase_disp(s, 2);
            continue;
        }
        if (c < 32) continue;
        if (c < 0x80) {
            put_ch(s, c);
        } else if ((c & 0xE0) == 0xC0) {
            s.utf = c & 0x1F;
            s.utf_need = 1;
        } else if ((c & 0xF0) == 0xE0) {
            s.utf = c & 0x0F;
            s.utf_need = 2;
        } else if ((c & 0xF8) == 0xF0) {
            s.utf = c & 0x07;
            s.utf_need = 3;
        } else {
            put_ch(s, c);
        }
    }
}

static void log_plain(ProcJob& p, const char* data, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == 0x1B) {
            size_t j = i + 1;
            if (j < n && data[j] == '[') {
                j++;
                while (j < n && (unsigned char)data[j] < 0x40) j++;
                i = j;
                continue;
            }
            continue;
        }
        if (c == '\r') continue;
        if (c >= 32 || c == '\n' || c == '\t') p.output.push_back((char)c);
    }
    const size_t cap = 512 * 1024;
    if (p.output.size() > cap) p.output.erase(0, p.output.size() - (cap * 3 / 4));
}

static void term_append(const char* data, size_t n) {
    ProcJob& p = g_app->proc;
    std::lock_guard<std::mutex> lock(p.mu);
    screen_feed(p.screen, data, n);
    log_plain(p, data, n);
}

static void screen_resize(TermScreen& s, int cols, int rows) {
    if (cols < 20) cols = 20;
    if (rows < 4) rows = 4;
    if (cols == s.cols && rows == s.rows && !s.cells.empty()) return;
    std::vector<TermCell> next((size_t)cols * (size_t)rows);
    int copy_c = std::min(s.cols, cols);
    if (!s.cells.empty() && s.rows > 0) {
        if (rows < s.rows) {
            int drop = s.rows - rows;
            for (int y = 0; y < drop; y++) {
                auto r = row_copy(s, y);
                pad_row(r, cols);
                s.scrollback.push_back(std::move(r));
            }
            for (int y = 0; y < rows; y++)
                for (int x = 0; x < copy_c; x++)
                    next[(size_t)y * (size_t)cols + (size_t)x] =
                        s.cells[(size_t)(y + drop) * (size_t)s.cols + (size_t)x];
            s.cy = std::max(0, s.cy - drop);
        } else {
            int copy_r = std::min(s.rows, rows);
            for (int y = 0; y < copy_r; y++)
                for (int x = 0; x < copy_c; x++)
                    next[(size_t)y * (size_t)cols + (size_t)x] =
                        s.cells[(size_t)y * (size_t)s.cols + (size_t)x];
        }
    }
    for (auto& row : s.scrollback) pad_row(row, cols);
    s.cells.swap(next);
    s.cols = cols;
    s.rows = rows;
    if (s.cx >= cols) s.cx = cols - 1;
    if (s.cy >= rows) s.cy = rows - 1;
    if (s.cx < 0) s.cx = 0;
    if (s.cy < 0) s.cy = 0;
}

static DWORD WINAPI proc_reader(LPVOID) {
    char buf[4096];
    for (;;) {
        DWORD n = 0;
        BOOL ok = ReadFile(g_app->proc.stdout_rd, buf, sizeof(buf), &n, nullptr);
        if (!ok || n == 0) break;
        term_append(buf, n);
    }
    DWORD code = 0;
    if (g_app->proc.process) {
        WaitForSingleObject(g_app->proc.process, 50);
        GetExitCodeProcess(g_app->proc.process, &code);
    }
    g_app->proc.exit_code = code;
    g_app->proc.running = false;
    return 0;
}

static void close_handles() {
    ProcJob& p = g_app->proc;
    if (p.pty && pty_api().Close) {
        pty_api().Close(p.pty);
        p.pty = nullptr;
    }
    if (p.stdin_wr) {
        CloseHandle(p.stdin_wr);
        p.stdin_wr = nullptr;
    }
    if (p.stdout_rd) {
        CloseHandle(p.stdout_rd);
        p.stdout_rd = nullptr;
    }
    if (p.process) {
        if (WaitForSingleObject(p.process, 400) == WAIT_TIMEOUT)
            TerminateProcess(p.process, 1);
        CloseHandle(p.process);
        p.process = nullptr;
    }
    if (p.thread) {
        WaitForSingleObject(p.thread, 400);
        CloseHandle(p.thread);
        p.thread = nullptr;
    }
    p.running = false;
    p.is_shell = false;
    p.use_pty = false;
    {
        std::lock_guard<std::mutex> lock(p.mu);
        screen_reset_parser(p.screen);
    }
}

void term_write(const void* data, size_t n) {
    if (!data || n == 0 || !g_app->proc.stdin_wr) return;
    DWORD wrote = 0;
    WriteFile(g_app->proc.stdin_wr, data, (DWORD)n, &wrote, nullptr);
}

void term_write_local(const std::string& s) {
    if (s.empty()) return;
    std::lock_guard<std::mutex> lock(g_app->proc.mu);
    screen_feed(g_app->proc.screen, s.data(), s.size());
    g_app->proc.output += s;
}

void term_clear() {
    std::lock_guard<std::mutex> lock(g_app->proc.mu);
    g_app->proc.output.clear();
    g_app->proc.typed.clear();
    TermScreen& s = g_app->proc.screen;
    s.scrollback.clear();
    s.cells.assign((size_t)s.cols * (size_t)s.rows, TermCell{});
    s.cx = 0;
    s.cy = 0;
    s.fg = kDefFg;
    screen_reset_parser(s);
    s.gen++;
}

void term_resize(int cols, int rows) {
    if (cols < 20) cols = 20;
    if (rows < 4) rows = 4;
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(g_app->proc.mu);
        if (cols != g_app->proc.cols || rows != g_app->proc.rows || g_app->proc.screen.cells.empty()) {
            screen_resize(g_app->proc.screen, cols, rows);
            g_app->proc.cols = cols;
            g_app->proc.rows = rows;
            changed = true;
        }
    }
    if (changed && g_app->proc.pty && pty_api().Resize) {
        COORD c;
        c.X = (SHORT)cols;
        c.Y = (SHORT)rows;
        pty_api().Resize(g_app->proc.pty, c);
    }
}

std::string term_plain_text() {
    std::lock_guard<std::mutex> lock(g_app->proc.mu);
    const TermScreen& s = g_app->proc.screen;
    std::string o;
    auto emit_row = [&](const std::vector<TermCell>& row) {
        int last = (int)row.size();
        while (last > 0 && row[(size_t)last - 1].ch == 32) last--;
        for (int i = 0; i < last; i++) utf8_push(o, row[(size_t)i].ch);
        o.push_back('\n');
    };
    for (const auto& row : s.scrollback) emit_row(row);
    if (!s.cells.empty()) {
        for (int y = 0; y < s.rows; y++) {
            std::vector<TermCell> row(s.cells.begin() + (size_t)y * (size_t)s.cols,
                                      s.cells.begin() + (size_t)(y + 1) * (size_t)s.cols);
            emit_row(row);
        }
    }
    return o;
}

static std::string default_cwd() {
    if (!g_app->settings.folder.empty() && dir_exists(g_app->settings.folder))
        return g_app->settings.folder;
    if (g_app->active >= 0) {
        const TextBuffer& b = g_app->buffers[(size_t)g_app->active];
        if (!b.untitled && !b.path.empty()) return path_parent(b.path);
    }
    return {};
}

static bool launch_pty(const std::string& cwd) {
    PtyApi& api = pty_api();
    if (!api.ok) return false;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE pty_in = nullptr, our_in = nullptr;
    HANDLE our_out = nullptr, pty_out = nullptr;
    if (!CreatePipe(&pty_in, &our_in, &sa, 0)) return false;
    if (!CreatePipe(&our_out, &pty_out, &sa, 0)) {
        CloseHandle(pty_in);
        CloseHandle(our_in);
        return false;
    }
    SetHandleInformation(our_in, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(our_out, HANDLE_FLAG_INHERIT, 0);
    COORD size;
    size.X = (SHORT)g_app->proc.cols;
    size.Y = (SHORT)g_app->proc.rows;
    void* pty = nullptr;
    if (FAILED(api.Create(size, pty_in, pty_out, 0, &pty))) {
        CloseHandle(pty_in);
        CloseHandle(our_in);
        CloseHandle(our_out);
        CloseHandle(pty_out);
        return false;
    }
    CloseHandle(pty_in);
    CloseHandle(pty_out);

    SIZE_T attr = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attr);
    auto* list = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attr);
    if (!list) {
        api.Close(pty);
        CloseHandle(our_in);
        CloseHandle(our_out);
        return false;
    }
    InitializeProcThreadAttributeList(list, 1, 0, &attr);
    UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pty, sizeof(pty), nullptr, nullptr);

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.lpAttributeList = list;
    PROCESS_INFORMATION pi{};
    wchar_t cmd[] = L"cmd.exe /d /k chcp 65001>nul";
    std::wstring wcwd = cwd.empty() ? std::wstring() : utf8_to_wide(cwd);
    BOOL ok = CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE,
                             EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
                             nullptr, wcwd.empty() ? nullptr : wcwd.c_str(),
                             &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList(list);
    HeapFree(GetProcessHeap(), 0, list);
    if (!ok) {
        api.Close(pty);
        CloseHandle(our_in);
        CloseHandle(our_out);
        return false;
    }
    CloseHandle(pi.hThread);
    g_app->proc.process = pi.hProcess;
    g_app->proc.stdin_wr = our_in;
    g_app->proc.stdout_rd = our_out;
    g_app->proc.pty = pty;
    g_app->proc.use_pty = true;
    g_app->proc.is_shell = true;
    g_app->proc.running = true;
    g_app->proc.cwd = cwd;
    g_app->proc.thread = CreateThread(nullptr, 0, proc_reader, nullptr, 0, nullptr);
    return true;
}

static bool launch_pipes(const std::string& cwd) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE out_rd = nullptr, out_wr = nullptr;
    HANDLE in_rd = nullptr, in_wr = nullptr;
    if (!CreatePipe(&out_rd, &out_wr, &sa, 0)) return false;
    if (!CreatePipe(&in_rd, &in_wr, &sa, 0)) {
        CloseHandle(out_rd);
        CloseHandle(out_wr);
        return false;
    }
    SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(in_wr, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = out_wr;
    si.hStdError = out_wr;
    si.hStdInput = in_rd;
    PROCESS_INFORMATION pi{};
    wchar_t cmd[] = L"cmd.exe /d /k chcp 65001>nul";
    std::wstring wcwd = cwd.empty() ? std::wstring() : utf8_to_wide(cwd);
    BOOL ok = CreateProcessW(nullptr, cmd, nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr,
                             wcwd.empty() ? nullptr : wcwd.c_str(),
                             &si, &pi);
    CloseHandle(out_wr);
    CloseHandle(in_rd);
    if (!ok) {
        CloseHandle(out_rd);
        CloseHandle(in_wr);
        return false;
    }
    CloseHandle(pi.hThread);
    g_app->proc.process = pi.hProcess;
    g_app->proc.stdin_wr = in_wr;
    g_app->proc.stdout_rd = out_rd;
    g_app->proc.use_pty = false;
    g_app->proc.is_shell = true;
    g_app->proc.running = true;
    g_app->proc.cwd = cwd;
    g_app->proc.thread = CreateThread(nullptr, 0, proc_reader, nullptr, 0, nullptr);
    return true;
}

void term_ensure_shell(const std::string& cwd) {
    if (g_app->proc.shutdown) return;
    std::string dir = cwd.empty() ? default_cwd() : cwd;
    if (g_app->proc.running && g_app->proc.is_shell) {
        if (!dir.empty() && !g_app->proc.cwd.empty() &&
            path_norm(dir) != path_norm(g_app->proc.cwd)) {
            g_app->proc.shutdown = true;
            close_handles();
            g_app->proc.shutdown = false;
        } else {
            return;
        }
    }
    if (g_app->proc.running) return;
    {
        std::lock_guard<std::mutex> lock(g_app->proc.mu);
        screen_ensure(g_app->proc.screen);
        screen_reset_parser(g_app->proc.screen);
    }
    if (!launch_pty(dir) && !launch_pipes(dir))
        term_write_local("Failed to start a shell.\n");
}

void proc_stop() {
    if (g_app->proc.shutdown) {
        if (g_app->proc.process) TerminateProcess(g_app->proc.process, 1);
        close_handles();
        return;
    }
    if (g_app->proc.is_shell && g_app->proc.running) {
        const char c = 3;
        term_write(&c, 1);
        return;
    }
    if (g_app->proc.process) TerminateProcess(g_app->proc.process, 1);
}

void proc_tick() {
    if (g_app->proc.running || !g_app->proc.process) return;
    bool was_shell = g_app->proc.is_shell;
    std::string cwd = g_app->proc.cwd;
    close_handles();
    term_write_local("\n[process exited " + std::to_string((int)g_app->proc.exit_code) + "]\n");
    if (was_shell && !g_app->proc.shutdown) term_ensure_shell(cwd);
}

void proc_start(const std::string& cmdline, const std::string& cwd) {
    term_ensure_shell(cwd);
    g_app->bottom = BottomTab::Terminal;
    g_app->settings.show_panel = true;
    if (!g_app->proc.use_pty) term_write_local("> " + cmdline + "\n");
    std::string line = cmdline + "\r\n";
    term_write(line.data(), line.size());
}

bool proc_run_capture(const std::string& command, const std::string& cwd,
                      DWORD timeout_ms, ProcCapture& out) {
    out = ProcCapture{};
    if (command.empty()) {
        out.output = "ERROR: empty command";
        out.exit_code = 1;
        return false;
    }
    if (timeout_ms < 1000) timeout_ms = 1000;
    if (timeout_ms > 300000) timeout_ms = 300000;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE out_rd = nullptr, out_wr = nullptr;
    HANDLE in_rd = nullptr, in_wr = nullptr;
    if (!CreatePipe(&out_rd, &out_wr, &sa, 0)) {
        out.output = "ERROR: CreatePipe failed";
        out.exit_code = 1;
        return false;
    }
    if (!CreatePipe(&in_rd, &in_wr, &sa, 0)) {
        CloseHandle(out_rd);
        CloseHandle(out_wr);
        out.output = "ERROR: CreatePipe failed";
        out.exit_code = 1;
        return false;
    }
    SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(in_wr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = in_rd;
    si.hStdOutput = out_wr;
    si.hStdError = out_wr;

    std::wstring cl = L"cmd.exe /S /C \"" + utf8_to_wide(command) + L"\"";
    std::vector<wchar_t> buf(cl.begin(), cl.end());
    buf.push_back(0);
    std::wstring wcwd = utf8_to_wide(cwd);

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
                             nullptr,
                             (wcwd.empty() || !dir_exists(cwd)) ? nullptr : wcwd.c_str(),
                             &si, &pi);
    CloseHandle(out_wr);
    CloseHandle(in_rd);
    CloseHandle(in_wr);
    if (!ok) {
        CloseHandle(out_rd);
        out.output = "ERROR: CreateProcess failed";
        out.exit_code = 1;
        return false;
    }

    const DWORD start = GetTickCount();
    char tmp[4096];
    for (;;) {
        if (g_app && g_app->agent.cancel) {
            TerminateProcess(pi.hProcess, 1);
            out.cancelled = true;
            break;
        }
        DWORD avail = 0;
        if (PeekNamedPipe(out_rd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            if (avail > sizeof(tmp)) avail = sizeof(tmp);
            DWORD n = 0;
            if (ReadFile(out_rd, tmp, avail, &n, nullptr) && n)
                out.output.append(tmp, tmp + n);
            continue;
        }
        DWORD w = WaitForSingleObject(pi.hProcess, 40);
        if (w == WAIT_OBJECT_0) {
            DWORD n = 0;
            while (ReadFile(out_rd, tmp, sizeof(tmp), &n, nullptr) && n)
                out.output.append(tmp, tmp + n);
            break;
        }
        if (GetTickCount() - start >= timeout_ms) {
            TerminateProcess(pi.hProcess, 1);
            out.timed_out = true;
            break;
        }
    }

    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    out.exit_code = (int)code;
    CloseHandle(out_rd);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (out.timed_out) out.output += "\n[timed out]\n";
    if (out.cancelled) out.output += "\n[cancelled]\n";
    return true;
}

void term_poll_input() {
    if (!g_app->terminal_focused) return;
    ImGuiIO& io = ImGui::GetIO();
    io.WantTextInput = true;
    io.WantCaptureKeyboard = true;
    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;

    auto send = [](const char* s) { term_write(s, std::strlen(s)); };
    auto send_ctrl = [](char letter) {
        char c = (char)(letter & 0x1F);
        term_write(&c, 1);
    };

    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        ImGui::SetClipboardText(term_plain_text().c_str());
        return;
    }
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        send_ctrl('C');
        return;
    }
    if ((ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) ||
        (shift && ImGui::IsKeyPressed(ImGuiKey_Insert, false))) {
        const char* clip = ImGui::GetClipboardText();
        if (clip && clip[0]) {
            std::string t = clip;
            for (size_t i = 0; i < t.size(); i++) {
                if (t[i] == '\n' && (i == 0 || t[i - 1] != '\r')) t[i] = '\r';
            }
            term_write(t.data(), t.size());
        }
        return;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_L, false)) { send_ctrl('L'); return; }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) { send_ctrl('D'); return; }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) { send_ctrl('A'); return; }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_E, false)) { send_ctrl('E'); return; }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) { send_ctrl('K'); return; }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_U, false)) { send_ctrl('U'); return; }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_W, false)) { send_ctrl('W'); return; }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { send_ctrl('Z'); return; }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_R, false)) { send_ctrl('R'); return; }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
        if (g_app->proc.use_pty) send("\r");
        else {
            std::string line = g_app->proc.typed + "\r\n";
            term_write_local(g_app->proc.typed + "\n");
            term_write(line.data(), line.size());
            g_app->proc.typed.clear();
        }
    } else if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true)) {
        if (g_app->proc.use_pty) send("\x7f");
        else if (!g_app->proc.typed.empty()) g_app->proc.typed.pop_back();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        send("\t");
    } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
        send("\x1b[A");
    } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
        send("\x1b[B");
    } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) {
        send("\x1b[C");
    } else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) {
        send("\x1b[D");
    } else if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) {
        send("\x1b[H");
    } else if (ImGui::IsKeyPressed(ImGuiKey_End, false)) {
        send("\x1b[F");
    } else if (ImGui::IsKeyPressed(ImGuiKey_Delete, true)) {
        send("\x1b[3~");
    } else if (ImGui::IsKeyPressed(ImGuiKey_PageUp, true)) {
        ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() - ImGui::GetWindowHeight() * 0.9f));
    } else if (ImGui::IsKeyPressed(ImGuiKey_PageDown, true)) {
        ImGui::SetScrollY(ImGui::GetScrollY() + ImGui::GetWindowHeight() * 0.9f);
    }

    if (ctrl) return;
    for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
        unsigned int ch = (unsigned int)io.InputQueueCharacters[i];
        if (ch < 32 && ch != 9) continue;
        char buf[8];
        int n = 0;
        if (ch < 0x80) buf[n++] = (char)ch;
        else if (ch < 0x800) {
            buf[n++] = (char)(0xC0 | (ch >> 6));
            buf[n++] = (char)(0x80 | (ch & 0x3F));
        } else {
            buf[n++] = (char)(0xE0 | (ch >> 12));
            buf[n++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            buf[n++] = (char)(0x80 | (ch & 0x3F));
        }
        if (g_app->proc.use_pty) term_write(buf, (size_t)n);
        else if (ch >= 32 && ch < 0x80) g_app->proc.typed.push_back((char)ch);
    }
    io.InputQueueCharacters.resize(0);
}

void draw_terminal(const ImVec2& size) {
    term_ensure_shell();
    ImFont* code = g_app->font_code ? g_app->font_code : ImGui::GetFont();
    float cw = code->GetCharAdvance('M');
    if (cw < 1.0f) cw = 8.0f;
    float ch = code->FontSize;
    float line_h = (float)(int)(ch + 2.0f);
    float sb = ImGui::GetStyle().ScrollbarSize + 4.0f;
    int cols = std::max(20, (int)((size.x - sb) / cw));
    int rows = std::max(4, (int)(size.y / line_h));
    term_resize(cols, rows);

    bool term_hov = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (ImGui::IsMouseClicked(0)) {
        if (term_hov) {
            ImGui::ClearActiveID();
            ImGui::SetWindowFocus();
            g_app->terminal_focused = true;
            g_app->editor_focused = false;
        } else {
            g_app->terminal_focused = false;
        }
    }
    if (term_hov && ImGui::IsMouseClicked(1)) {
        ImGui::ClearActiveID();
        g_app->terminal_focused = true;
        g_app->editor_focused = false;
        const char* clip = ImGui::GetClipboardText();
        if (clip && clip[0]) term_write(clip, std::strlen(clip));
    }
    if (g_app->terminal_focused) term_poll_input();

    TermScreen snap;
    std::string typed;
    bool use_pty = g_app->proc.use_pty;
    {
        std::lock_guard<std::mutex> lock(g_app->proc.mu);
        snap.cols = g_app->proc.screen.cols;
        snap.rows = g_app->proc.screen.rows;
        snap.cx = g_app->proc.screen.cx;
        snap.cy = g_app->proc.screen.cy;
        snap.cursor_vis = g_app->proc.screen.cursor_vis;
        snap.cells = g_app->proc.screen.cells;
        snap.scrollback = g_app->proc.screen.scrollback;
        snap.gen = g_app->proc.screen.gen;
        typed = g_app->proc.typed;
    }

    if (g_app->font_code) ImGui::PushFont(g_app->font_code);
    int hist = (int)snap.scrollback.size();
    int total = hist + snap.rows;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float content_h = (float)total * line_h;
    ImGui::Dummy(ImVec2(std::max(1.0f, size.x - sb), std::max(content_h, size.y)));

    float sy = ImGui::GetScrollY();
    float max_sy = ImGui::GetScrollMaxY();
    static uint64_t last_gen = 0;
    bool at_bottom = (max_sy <= 1.0f) || (sy >= max_sy - line_h);
    if (snap.gen != last_gen && at_bottom) ImGui::SetScrollHereY(1.0f);
    last_gen = snap.gen;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 clip_min = ImGui::GetWindowPos();
    ImVec2 clip_max = ImVec2(clip_min.x + ImGui::GetWindowSize().x, clip_min.y + ImGui::GetWindowSize().y);
    dl->PushClipRect(clip_min, clip_max, true);

    int first = std::max(0, (int)(ImGui::GetScrollY() / line_h) - 1);
    int last = std::min(total, (int)((ImGui::GetScrollY() + size.y) / line_h) + 2);
    bool any = false;
    for (int i = first; i < last; i++) {
        const TermCell* row = nullptr;
        if (i < hist) row = snap.scrollback[(size_t)i].data();
        else if (!snap.cells.empty()) {
            int y = i - hist;
            row = snap.cells.data() + (size_t)y * (size_t)snap.cols;
        }
        int row_n = 0;
        if (i < hist) row_n = (int)snap.scrollback[(size_t)i].size();
        else row_n = snap.cols;
        if (!row || row_n <= 0) continue;
        int limit = std::min(snap.cols, row_n);
        float y = origin.y + (float)i * line_h;
        int typed_at = -1;
        if (!use_pty && !typed.empty() && i == hist + snap.cy) typed_at = snap.cx;
        auto cell_ch = [&](int k) -> uint32_t {
            uint32_t cp = row[k].ch;
            if (typed_at >= 0 && k >= typed_at && k < typed_at + (int)typed.size())
                cp = (unsigned char)typed[(size_t)(k - typed_at)];
            if (cp < 32 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 32;
            return cp;
        };
        int last_x = 0;
        for (int k = 0; k < limit; k++) {
            if (cell_ch(k) != 32) last_x = k + 1;
        }
        if (last_x == 0) continue;
        int x = 0;
        while (x < last_x) {
            ImU32 fg = row[x].fg;
            int x1 = x + 1;
            while (x1 < last_x && row[x1].fg == fg) x1++;
            std::string piece;
            for (int k = x; k < x1; k++) {
                uint32_t cp = cell_ch(k);
                if (cp != 32) any = true;
                utf8_push(piece, cp);
            }
            dl->AddText(code, ch, ImVec2(origin.x + (float)x * cw, y), fg, piece.c_str());
            x = x1;
        }
    }

    if (!any && snap.scrollback.empty()) {
        dl->AddText(ImVec2(origin.x, origin.y), IM_COL32(0x88, 0x88, 0x88, 255),
                    "Click here and type. F5 runs NexaC --run in this shell.");
    }

    if (g_app->terminal_focused && snap.cursor_vis) {
        int cur_line = hist + snap.cy;
        float cx = origin.x + (float)snap.cx * cw;
        if (!use_pty) cx += (float)typed.size() * cw;
        float cy = origin.y + (float)cur_line * line_h;
        ImU32 col = IM_COL32(0xCC, 0xCC, 0xCC, ((int)(ImGui::GetTime() * 2.0) % 2) ? 0xFF : 0x40);
        dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + 2.0f, cy + ch), col);
    }
    dl->PopClipRect();
    if (g_app->font_code) ImGui::PopFont();
}

static std::string quote_cmd(const std::string& s) {
    if (s.find(' ') == std::string::npos) return s;
    return "\"" + s + "\"";
}

static std::string project_dir() {
    return default_cwd();
}

static void save_project_buffers() {
    for (int i = 0; i < (int)g_app->buffers.size(); i++) {
        TextBuffer& b = g_app->buffers[(size_t)i];
        if (b.dirty && !b.untitled && !b.path.empty()) save_buffer(i);
    }
}

void run_active(bool execute) {
    save_project_buffers();
    std::string cwd = project_dir();
    if (cwd.empty()) {
        term_write_local("Open a folder (or a .nxa file) so Nexa can find the project.\n");
        g_app->bottom = BottomTab::Terminal;
        g_app->settings.show_panel = true;
        return;
    }
    std::string nexac = nexa_compiler();
    std::string cmd = quote_cmd(nexac);
    if (execute) {
        cmd += " --run";
        if (g_app->run_args[0]) {
            cmd += " -- ";
            cmd += g_app->run_args;
        }
    } else {
        cmd += " build";
    }
    proc_start(cmd, cwd);
}

void build_folder() {
    run_active(false);
}
