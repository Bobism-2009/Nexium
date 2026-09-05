#include "nexium.hpp"

#include <cstring>
#include <vector>

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

static void term_append(const char* data, size_t n) {
    ProcJob& p = g_app->proc;
    std::lock_guard<std::mutex> lock(p.mu);
    p.output.reserve(p.output.size() + n);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)data[i];
        if (p.vt == 1) {
            if (c == '[') p.vt = 2;
            else if (c == ']') p.vt = 3;
            else p.vt = 0;
            continue;
        }
        if (p.vt == 2) {
            if (c >= 0x40 && c <= 0x7E) p.vt = 0;
            continue;
        }
        if (p.vt == 3) {
            if (c == 0x07 || c == '\\') p.vt = 0;
            continue;
        }
        if (c == 0x1B) {
            p.vt = 1;
            continue;
        }
        if (c == '\r') {
            p.cr = true;
            continue;
        }
        if (c == '\n') {
            p.cr = false;
            p.output.push_back('\n');
            continue;
        }
        if (c == '\b') {
            p.cr = false;
            if (!p.output.empty() && p.output.back() != '\n') p.output.pop_back();
            continue;
        }
        if (c == 0x07) continue;
        if (p.cr) {
            while (!p.output.empty() && p.output.back() != '\n') p.output.pop_back();
            p.cr = false;
        }
        p.output.push_back((char)c);
    }
    const size_t cap = 512 * 1024;
    if (p.output.size() > cap) p.output.erase(0, p.output.size() - (cap * 3 / 4));
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
    p.vt = 0;
    p.cr = false;
}

void term_write(const void* data, size_t n) {
    if (!data || n == 0 || !g_app->proc.stdin_wr) return;
    DWORD wrote = 0;
    WriteFile(g_app->proc.stdin_wr, data, (DWORD)n, &wrote, nullptr);
}

void term_resize(int cols, int rows) {
    if (cols < 20) cols = 20;
    if (rows < 4) rows = 4;
    g_app->proc.cols = cols;
    g_app->proc.rows = rows;
    if (g_app->proc.pty && pty_api().Resize) {
        COORD c;
        c.X = (SHORT)cols;
        c.Y = (SHORT)rows;
        pty_api().Resize(g_app->proc.pty, c);
    }
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
    wchar_t cmd[] = L"cmd.exe";
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
    wchar_t cmd[] = L"cmd.exe /k";
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
    if (!launch_pty(dir) && !launch_pipes(dir)) {
        std::lock_guard<std::mutex> lock(g_app->proc.mu);
        g_app->proc.output += "Failed to start a shell.\n";
    }
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
    {
        std::lock_guard<std::mutex> lock(g_app->proc.mu);
        g_app->proc.output += "\n[process exited " + std::to_string((int)g_app->proc.exit_code) + "]\n";
    }
    if (was_shell && !g_app->proc.shutdown) term_ensure_shell(cwd);
}

void proc_start(const std::string& cmdline, const std::string& cwd) {
    term_ensure_shell(cwd);
    g_app->bottom = BottomTab::Terminal;
    g_app->settings.show_panel = true;
    if (!g_app->proc.use_pty) {
        std::lock_guard<std::mutex> lock(g_app->proc.mu);
        if (!g_app->proc.output.empty() && g_app->proc.output.back() != '\n') g_app->proc.output += "\n";
        g_app->proc.output += "> " + cmdline + "\n";
    }
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

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        const char c = 3;
        term_write(&c, 1);
        return;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        const char* clip = ImGui::GetClipboardText();
        if (clip && clip[0]) term_write(clip, std::strlen(clip));
        return;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_L, false)) {
        const char c = 12;
        term_write(&c, 1);
        return;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
        const char c = 4;
        term_write(&c, 1);
        return;
    }

    auto send = [](const char* s) { term_write(s, std::strlen(s)); };

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
        if (g_app->proc.use_pty) send("\r");
        else {
            std::string line = g_app->proc.typed + "\r\n";
            {
                std::lock_guard<std::mutex> lock(g_app->proc.mu);
                g_app->proc.output += g_app->proc.typed + "\n";
            }
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
    }

    if (ctrl) return;
    for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
        unsigned int ch = (unsigned int)io.InputQueueCharacters[i];
        if (ch < 32 && ch != 9) continue;
        if (g_app->proc.use_pty) {
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
            term_write(buf, (size_t)n);
        } else if (ch >= 32 && ch < 0x80) {
            g_app->proc.typed.push_back((char)ch);
        }
    }
    io.InputQueueCharacters.resize(0);
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
        std::lock_guard<std::mutex> lock(g_app->proc.mu);
        g_app->proc.output += "Open a folder (or a .nxa file) so Nexa can find the project.\n";
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
