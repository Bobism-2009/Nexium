// Win32 pieces Nexa's standard library does not cover yet: the window's real
// client size, a keyboard and mouse event queue with key repeat, cursors,
// Unicode clipboard, folder pickers, and child processes with piped output.
// Everything here takes and returns ints and std::strings so Nexa can call it.
#pragma once
#include <windows.h>
#include <shobjidl.h>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>

// ---- text ---------------------------------------------------------------------

static std::wstring nx__wide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

static std::string nx__utf8(const wchar_t* w, int len = -1) {
    if (!w) return std::string();
    if (len < 0) len = (int)wcslen(w);
    if (len == 0) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, len, &s[0], n, nullptr, nullptr);
    return s;
}

// ---- window ---------------------------------------------------------------------

struct NxEvent {
    int type;  // 1 key down, 2 char, 3 mouse down, 4 mouse up, 5 wheel
    int a;     // key: virtual key; char: code point; mouse: button; wheel: delta
    int x, y;
    int mods;  // 1 ctrl, 2 shift, 4 alt
    int clicks;
};

static HWND nx__wnd = nullptr;
static WNDPROC nx__orig = nullptr;
static std::deque<NxEvent> nx__events;
static NxEvent nx__cur = {0, 0, 0, 0, 0, 0};
static int nx__cursor = 0;
static int nx__close_req = 0;
static int nx__focused = 1;
static DWORD nx__last_down_t = 0;
static int nx__last_down_x = -100, nx__last_down_y = -100, nx__last_down_b = -1, nx__clicks = 0;
static int nx__min_w = 640, nx__min_h = 400;
static int nx__drop_char = 0;
static int nx__test = GetEnvironmentVariableA("NEXIUM_OFFSCREEN", nullptr, 0) > 0 ? 1 : 0;

static BOOL CALLBACK nx__find(HWND h, LPARAM) {
    char cls[32];
    if (GetClassNameA(h, cls, sizeof cls) && lstrcmpA(cls, "NexaGfx") == 0) {
        nx__wnd = h;
        return FALSE;
    }
    return TRUE;
}

static int nx__mods() {
    int m = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) m |= 1;
    if (GetKeyState(VK_SHIFT) & 0x8000) m |= 2;
    if (GetKeyState(VK_MENU) & 0x8000) m |= 4;
    return m;
}

static void nx__push(int type, int a, int x, int y, int clicks = 1) {
    if (nx__events.size() > 4096) return;
    NxEvent e = {type, a, x, y, nx__mods(), clicks};
    nx__events.push_back(e);
}

static LRESULT CALLBACK nx__proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE:
        nx__close_req = 1;
        return 0;
    case WM_SYSCOMMAND:
        // gfx turns maximise into borderless fullscreen; an IDE wants a normal
        // maximised window with the taskbar still there.
        if ((wp & 0xFFF0) == SC_MAXIMIZE || ((wp & 0xFFF0) == SC_RESTORE && IsZoomed(h)))
            return DefWindowProcA(h, msg, wp, lp);
        // F10 and Alt would otherwise open the system menu and swallow the next key.
        if ((wp & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = nx__min_w;
        mm->ptMinTrackSize.y = nx__min_h;
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            LPCSTR id = IDC_ARROW;
            switch (nx__cursor) {
            case 1: id = IDC_IBEAM; break;
            case 2: id = IDC_SIZEWE; break;
            case 3: id = IDC_SIZENS; break;
            case 4: id = IDC_HAND; break;
            default: break;
            }
            SetCursor(LoadCursorA(nullptr, id));
            return TRUE;
        }
        break;
    case WM_SETFOCUS: nx__focused = 1; break;
    case WM_KILLFOCUS: nx__focused = 0; break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        nx__push(1, (int)wp, 0, 0);
        // Automated tests post keys with their modifiers in lParam's reserved
        // bits 25-27, since posting cannot change the real keyboard state.
        if (nx__test && ((lp >> 25) & 7)) {
            nx__events.back().mods |= (int)((lp >> 25) & 7);
            // the character TranslateMessage makes from it would not exist with Ctrl really held
            if ((lp >> 25) & 1) nx__drop_char = 1;
        }
        if (msg == WM_SYSKEYDOWN && wp != VK_F4) {
            CallWindowProcA(nx__orig, h, msg, wp, lp);
            return 0;
        }
        break;
    case WM_CHAR: {
        if (nx__drop_char) {
            nx__drop_char = 0;
            break;
        }
        // The gfx window class is ANSI, so characters arrive in the process
        // code page; widen them back to Unicode.
        unsigned char b = (unsigned char)wp;
        int cp = b;
        if (b >= 0x80) {
            wchar_t w = 0;
            char c = (char)b;
            if (MultiByteToWideChar(CP_ACP, 0, &c, 1, &w, 1) == 1) cp = (int)w;
        }
        if (cp >= 32 && cp != 127) nx__push(2, cp, 0, 0);
        break;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        int btn = msg == WM_LBUTTONDOWN ? 0 : (msg == WM_RBUTTONDOWN ? 1 : 2);
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        DWORD t = GetMessageTime();
        if (btn == nx__last_down_b && t - nx__last_down_t <= GetDoubleClickTime() &&
            abs(x - nx__last_down_x) <= 4 && abs(y - nx__last_down_y) <= 4) {
            nx__clicks = nx__clicks >= 3 ? 1 : nx__clicks + 1;
        } else {
            nx__clicks = 1;
        }
        nx__last_down_t = t;
        nx__last_down_x = x;
        nx__last_down_y = y;
        nx__last_down_b = btn;
        nx__push(3, btn, x, y, nx__clicks);
        SetCapture(h);
        break;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        int btn = msg == WM_LBUTTONUP ? 0 : (msg == WM_RBUTTONUP ? 1 : 2);
        nx__push(4, btn, (short)LOWORD(lp), (short)HIWORD(lp));
        if (!(GetKeyState(VK_LBUTTON) & 0x8000) && !(GetKeyState(VK_RBUTTON) & 0x8000)) ReleaseCapture();
        break;
    }
    case WM_MOUSEWHEEL: {
        POINT p = {(short)LOWORD(lp), (short)HIWORD(lp)};
        ScreenToClient(h, &p);
        nx__push(5, GET_WHEEL_DELTA_WPARAM(wp), p.x, p.y);
        break;
    }
    case WM_MOUSEHWHEEL: {
        POINT p = {(short)LOWORD(lp), (short)HIWORD(lp)};
        ScreenToClient(h, &p);
        nx__push(6, GET_WHEEL_DELTA_WPARAM(wp), p.x, p.y);
        break;
    }
    default: break;
    }
    return CallWindowProcA(nx__orig, h, msg, wp, lp);
}

// Makes the process DPI-aware; call before the window opens. Returns the
// scale of the primary monitor in percent (100 at 96 DPI).
static int nx_dpi_init() {
    HMODULE u = GetModuleHandleA("user32.dll");
    typedef BOOL(WINAPI * SetCtx)(HANDLE);
    SetCtx set = u ? (SetCtx)(void*)GetProcAddress(u, "SetProcessDpiAwarenessContext") : nullptr;
    if (!set || !set((HANDLE)-4)) SetProcessDPIAware();
    HDC dc = GetDC(nullptr);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return dpi * 100 / 96;
}

// Finds the gfx window, hooks its messages, sets the icon and a dark title bar.
static int nx_init(int min_w, int min_h) {
    nx__min_w = min_w;
    nx__min_h = min_h;
    EnumThreadWindows(GetCurrentThreadId(), nx__find, 0);
    if (!nx__wnd) return 0;
    nx__orig = (WNDPROC)SetWindowLongPtrA(nx__wnd, GWLP_WNDPROC, (LONG_PTR)nx__proc);
    HICON big = (HICON)LoadImageA(GetModuleHandleA(nullptr), MAKEINTRESOURCEA(101), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    HICON small = (HICON)LoadImageA(GetModuleHandleA(nullptr), MAKEINTRESOURCEA(101), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (big) SendMessageA(nx__wnd, WM_SETICON, ICON_BIG, (LPARAM)big);
    if (small) SendMessageA(nx__wnd, WM_SETICON, ICON_SMALL, (LPARAM)small);
    HMODULE dwm = LoadLibraryA("dwmapi.dll");
    if (dwm) {
        typedef HRESULT(WINAPI * SetAttr)(HWND, DWORD, LPCVOID, DWORD);
        SetAttr set = (SetAttr)(void*)GetProcAddress(dwm, "DwmSetWindowAttribute");
        if (set) {
            BOOL dark = TRUE;
            if (FAILED(set(nx__wnd, 20, &dark, sizeof dark))) set(nx__wnd, 19, &dark, sizeof dark);
            COLORREF cap = RGB(0x18, 0x18, 0x18);
            set(nx__wnd, 35, &cap, sizeof cap);
            // repaint the frame so the dark caption shows straight away
            SetWindowPos(nx__wnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }
    return 1;
}

static void nx_place(int w, int h, int maximized) {
    if (!nx__wnd) return;
    RECT wa;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    RECT r = {0, 0, w, h};
    AdjustWindowRect(&r, (DWORD)GetWindowLongA(nx__wnd, GWL_STYLE), FALSE);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    if (ww > wa.right - wa.left) ww = wa.right - wa.left;
    if (wh > wa.bottom - wa.top) wh = wa.bottom - wa.top;
    int x = wa.left + (wa.right - wa.left - ww) / 2, y = wa.top + (wa.bottom - wa.top - wh) / 2;
    // automated tests run the window off-screen, out of the user's way
    if (nx__test) {
        x = -8000;
        maximized = 0;
        // no taskbar button either
        SetWindowLongA(nx__wnd, GWL_EXSTYLE, GetWindowLongA(nx__wnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
    }
    SetWindowPos(nx__wnd, nullptr, x, y, ww, wh, SWP_NOZORDER | SWP_NOACTIVATE);
    if (maximized) ShowWindow(nx__wnd, SW_MAXIMIZE);
}

static int nx_maximized() { return nx__wnd && IsZoomed(nx__wnd) ? 1 : 0; }
static int nx_minimized() { return nx__wnd && IsIconic(nx__wnd) ? 1 : 0; }
static int nx_focused() { return nx__focused; }

static int nx_client_w() {
    RECT r;
    if (!nx__wnd || !GetClientRect(nx__wnd, &r)) return 0;
    return r.right;
}

static int nx_client_h() {
    RECT r;
    if (!nx__wnd || !GetClientRect(nx__wnd, &r)) return 0;
    return r.bottom;
}

// Next queued input event: its type, or 0 when the queue is empty. The
// nx_ev_* calls read the event it took.
static int nx_next() {
    if (nx__events.empty()) return 0;
    nx__cur = nx__events.front();
    nx__events.pop_front();
    return nx__cur.type;
}
static int nx_ev_a() { return nx__cur.a; }
static int nx_ev_x() { return nx__cur.x; }
static int nx_ev_y() { return nx__cur.y; }
static int nx_ev_mods() { return nx__cur.mods; }
static int nx_ev_clicks() { return nx__cur.clicks; }
static int nx_mods() { return nx__mods(); }
static int nx_key_down(int vk) { return (GetKeyState(vk) & 0x8000) ? 1 : 0; }

static int nx_mouse_x() {
    POINT p;
    if (!nx__wnd || !GetCursorPos(&p) || !ScreenToClient(nx__wnd, &p)) return -1;
    return p.x;
}
static int nx_mouse_y() {
    POINT p;
    if (!nx__wnd || !GetCursorPos(&p) || !ScreenToClient(nx__wnd, &p)) return -1;
    return p.y;
}
static int nx_mouse_down(int btn) {
    int vk = btn == 0 ? VK_LBUTTON : (btn == 1 ? VK_RBUTTON : VK_MBUTTON);
    return (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
}

static void nx_cursor(int kind) { nx__cursor = kind; }

// 1 once each time the user asks to close the window.
static int nx_close_requested() {
    int r = nx__close_req;
    nx__close_req = 0;
    return r;
}

static long long nx_ms() { return (long long)GetTickCount64(); }
static void nx_sleep(int ms) { Sleep((DWORD)ms); }

static void nx_title(const std::string& s) {
    // The window class is ANSI; DefWindowProcW stores the title as Unicode anyway.
    if (nx__wnd) DefWindowProcW(nx__wnd, WM_SETTEXT, 0, (LPARAM)nx__wide(s).c_str());
}

// Blocks until an input message arrives or ms pass; keeps an idle IDE cheap.
static void nx_wait(int ms) { MsgWaitForMultipleObjects(0, nullptr, FALSE, (DWORD)ms, QS_ALLINPUT); }

// ---- clipboard ------------------------------------------------------------------

static std::string nx_clip_get() {
    std::string out;
    if (!OpenClipboard(nx__wnd)) return out;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* w = (const wchar_t*)GlobalLock(h);
        if (w) {
            out = nx__utf8(w);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    std::string clean;
    clean.reserve(out.size());
    for (char c : out)
        if (c != '\r') clean += c;
    return clean;
}

static int nx_clip_set(const std::string& s) {
    std::string crlf;
    for (char c : s) {
        if (c == '\n') crlf += '\r';
        crlf += c;
    }
    std::wstring w = nx__wide(crlf);
    if (!OpenClipboard(nx__wnd)) return 0;
    EmptyClipboard();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
    if (g) {
        void* p = GlobalLock(g);
        memcpy(p, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(g);
        SetClipboardData(CF_UNICODETEXT, g);
    }
    CloseClipboard();
    return 1;
}

// ---- dialogs -----------------------------------------------------------------------

// The dialog class and interface ids, spelled out so no import library is needed.
static const GUID nx__clsid_open = {0xDC1C5A9C, 0xE88A, 0x4DDE, {0xA5, 0xA1, 0x60, 0xF8, 0x2A, 0x20, 0xAE, 0xF7}};
static const GUID nx__clsid_save = {0xC0B4E2F3, 0xBA21, 0x4773, {0x8D, 0xBA, 0x33, 0x5E, 0xC9, 0x46, 0xEB, 0x8B}};
static const GUID nx__iid_open = {0xD57C7288, 0xD4AD, 0x4768, {0xBE, 0x02, 0x9D, 0x96, 0x95, 0x32, 0xD9, 0x60}};
static const GUID nx__iid_save = {0x84BCCD23, 0x5FDE, 0x4CDB, {0xAE, 0xA4, 0xAF, 0x64, 0xB8, 0x3D, 0x78, 0xAB}};

static std::string nx__dialog(bool save, bool folder, const std::string& name) {
    static bool com = false;
    if (!com) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        com = true;
    }
    IFileDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(save ? nx__clsid_save : nx__clsid_open, nullptr, CLSCTX_INPROC_SERVER,
                                  save ? nx__iid_save : nx__iid_open, (void**)&dlg);
    if (FAILED(hr) || !dlg) return std::string();
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM;
    if (folder) opts |= FOS_PICKFOLDERS;
    dlg->SetOptions(opts);
    if (!folder) {
        COMDLG_FILTERSPEC spec[] = {{L"Nexa source", L"*.nxa"}, {L"All files", L"*.*"}};
        dlg->SetFileTypes(2, spec);
        if (save) dlg->SetDefaultExtension(L"nxa");
    }
    if (!name.empty()) dlg->SetFileName(nx__wide(name).c_str());
    std::string out;
    if (SUCCEEDED(dlg->Show(nx__wnd))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
                out = nx__utf8(p);
                CoTaskMemFree(p);
            }
            item->Release();
        }
    }
    dlg->Release();
    // The dialog's own clicks must not reach the IDE afterwards.
    nx__events.clear();
    return out;
}

static std::string nx_pick_folder() { return nx__dialog(false, true, ""); }
static std::string nx_pick_file() { return nx__dialog(false, false, ""); }
static std::string nx_save_file(const std::string& name) { return nx__dialog(true, false, name); }

// 0 cancel, 1 yes, 2 no
static int nx_ask_save(const std::string& what) {
    std::wstring msg = L"Do you want to save the changes you made to " + nx__wide(what) + L"?";
    int r = MessageBoxW(nx__wnd, msg.c_str(), L"Nexium", MB_YESNOCANCEL | MB_ICONWARNING);
    nx__events.clear();
    return r == IDYES ? 1 : (r == IDNO ? 2 : 0);
}

static void nx_reveal(const std::string& path) {
    std::wstring args = L"/select,\"" + nx__wide(path) + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
}

// ---- files ---------------------------------------------------------------------------

static long long nx_mtime(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExW(nx__wide(path).c_str(), GetFileExInfoStandard, &d)) return 0;
    return ((long long)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime;
}

// Full path of an executable found on PATH (with .exe/.cmd/.bat), or "".
static std::string nx_which(const std::string& name) {
    wchar_t buf[MAX_PATH];
    const wchar_t* exts[] = {L".exe", L".cmd", L".bat", nullptr};
    for (int i = 0; exts[i]; i++) {
        if (SearchPathW(nullptr, nx__wide(name).c_str(), exts[i], MAX_PATH, buf, nullptr)) return nx__utf8(buf);
    }
    return std::string();
}

// ---- processes -------------------------------------------------------------------------

struct NxProc {
    HANDLE process = nullptr;
    HANDLE in_w = nullptr;
    HANDLE out_r = nullptr;
    std::thread reader;
    std::mutex lock;
    std::string pending;
    std::atomic<bool> done{false};
    DWORD exit_code = 0;
};

static std::vector<std::unique_ptr<NxProc>> nx__procs;

// Starts cmd /c <command> (or an interactive cmd when command is "") in cwd with
// stdin and stdout/stderr piped. Returns a handle for the nx_proc_* calls, or -1.
static int nx_proc_start(const std::string& command, const std::string& cwd) {
    SECURITY_ATTRIBUTES sa = {sizeof sa, nullptr, TRUE};
    HANDLE out_r, out_w, in_r, in_w;
    if (!CreatePipe(&out_r, &out_w, &sa, 0)) return -1;
    if (!CreatePipe(&in_r, &in_w, &sa, 0)) {
        CloseHandle(out_r);
        CloseHandle(out_w);
        return -1;
    }
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = {};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = in_r;
    si.hStdOutput = out_w;
    si.hStdError = out_w;
    PROCESS_INFORMATION pi = {};
    std::wstring cmdline = command.empty() ? std::wstring(L"cmd.exe /Q /K")
                                           : L"cmd.exe /S /C \"" + nx__wide(command) + L"\"";
    std::wstring wcwd = nx__wide(cwd);
    BOOL ok = CreateProcessW(nullptr, &cmdline[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                             wcwd.empty() ? nullptr : wcwd.c_str(), &si, &pi);
    CloseHandle(out_w);
    CloseHandle(in_r);
    if (!ok) {
        CloseHandle(out_r);
        CloseHandle(in_w);
        return -1;
    }
    CloseHandle(pi.hThread);
    std::unique_ptr<NxProc> p(new NxProc());
    p->process = pi.hProcess;
    p->in_w = in_w;
    p->out_r = out_r;
    NxProc* raw = p.get();
    raw->reader = std::thread([raw]() {
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(raw->out_r, buf, sizeof buf, &n, nullptr) && n > 0) {
            std::lock_guard<std::mutex> g(raw->lock);
            raw->pending.append(buf, n);
        }
        WaitForSingleObject(raw->process, INFINITE);
        GetExitCodeProcess(raw->process, &raw->exit_code);
        raw->done = true;
    });
    for (size_t i = 0; i < nx__procs.size(); i++) {
        if (!nx__procs[i]) {
            nx__procs[i] = std::move(p);
            return (int)i;
        }
    }
    nx__procs.push_back(std::move(p));
    return (int)nx__procs.size() - 1;
}

static NxProc* nx__proc_at(int id) {
    if (id < 0 || id >= (int)nx__procs.size()) return nullptr;
    return nx__procs[(size_t)id].get();
}

// Output that arrived since the last call. Console programs write in the OEM
// code page, so bytes that are not valid UTF-8 are converted from it.
static std::string nx_proc_read(int id) {
    NxProc* p = nx__proc_at(id);
    if (!p) return std::string();
    std::string s;
    {
        std::lock_guard<std::mutex> g(p->lock);
        s.swap(p->pending);
    }
    if (s.empty()) return s;
    // Keep an incomplete UTF-8 sequence at the end for next time.
    size_t cut = s.size();
    for (size_t k = 0; k < 3 && k < s.size(); k++) {
        unsigned char c = (unsigned char)s[s.size() - 1 - k];
        if ((c & 0xC0) == 0x80) continue;
        if (c >= 0xC0) {
            size_t need = c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : 2);
            if (k + 1 < need) cut = s.size() - 1 - k;
        }
        break;
    }
    if (cut < s.size() && !p->done) {
        std::lock_guard<std::mutex> g(p->lock);
        p->pending.insert(0, s.substr(cut));
        s.resize(cut);
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0) == 0) {
        int n = MultiByteToWideChar(CP_OEMCP, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w((size_t)n, L'\0');
        MultiByteToWideChar(CP_OEMCP, 0, s.data(), (int)s.size(), &w[0], n);
        s = nx__utf8(w.c_str(), n);
    }
    return s;
}

// 1 while running (or while output is still unread), 0 when finished.
static int nx_proc_running(int id) {
    NxProc* p = nx__proc_at(id);
    if (!p) return 0;
    if (!p->done) return 1;
    std::lock_guard<std::mutex> g(p->lock);
    return p->pending.empty() ? 0 : 1;
}

static int nx_proc_exit_code(int id) {
    NxProc* p = nx__proc_at(id);
    return p && p->done ? (int)p->exit_code : -1;
}

static int nx_proc_write(int id, const std::string& s) {
    NxProc* p = nx__proc_at(id);
    if (!p || p->done || !p->in_w) return 0;
    DWORD n = 0;
    return WriteFile(p->in_w, s.data(), (DWORD)s.size(), &n, nullptr) ? 1 : 0;
}

// Ends the process and everything it started.
static void nx_proc_kill(int id) {
    NxProc* p = nx__proc_at(id);
    if (!p) return;
    if (!p->done) {
        std::wstring cmd = L"taskkill /T /F /PID " + std::to_wstring(GetProcessId(p->process));
        STARTUPINFOW si = {};
        si.cb = sizeof si;
        PROCESS_INFORMATION pi = {};
        if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 3000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        TerminateProcess(p->process, 1);
    }
}

// Frees a finished process's handle so the id can be reused.
static void nx_proc_free(int id) {
    NxProc* p = nx__proc_at(id);
    if (!p) return;
    nx_proc_kill(id);
    if (p->in_w) CloseHandle(p->in_w);
    p->in_w = nullptr;
    if (p->reader.joinable()) p->reader.join();
    CloseHandle(p->out_r);
    CloseHandle(p->process);
    nx__procs[(size_t)id].reset();
}

static void nx_message(const std::string& msg) {
    MessageBoxW(nx__wnd, nx__wide(msg).c_str(), L"Nexium", MB_OK | MB_ICONWARNING);
    nx__events.clear();
}

static int nx_confirm(const std::string& msg) {
    int r = MessageBoxW(nx__wnd, nx__wide(msg).c_str(), L"Nexium", MB_OKCANCEL | MB_ICONQUESTION);
    nx__events.clear();
    return r == IDOK ? 1 : 0;
}

#include "native_text.hpp"
#include "native_http.hpp"
