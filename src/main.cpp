#include "nexium.hpp"
#include "resource.h"

#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

#include <shellapi.h>
#include <tchar.h>
#include <windowsx.h>
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int title_bar_h() {
    return g_app ? (int)(32.0f * g_app->dpi + 0.5f) : 32;
}

static bool is_window_maximized(HWND h) {
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (!GetWindowPlacement(h, &wp)) return false;
    return wp.showCmd == SW_SHOWMAXIMIZED;
}

static void enable_dark_titlebar(HWND hwnd) {
    BOOL dark = TRUE;
    // Try the modern attribute first, then the pre-20H1 attribute (19).
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    if (FAILED(hr)) {
        const DWORD legacy = 19;
        DwmSetWindowAttribute(hwnd, legacy, &dark, sizeof(dark));
    }
    // Match VS Code caption color (matches menu bar / activity area).
    COLORREF caption = RGB(0x0E, 0x0E, 0x0E);
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    COLORREF text = RGB(0xCC, 0xCC, 0xCC);
    DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &text, sizeof(text));
    COLORREF border = RGB(0x0A, 0x0A, 0x0A);
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
}

static void create_rtv(App& app) {
    ID3D11Texture2D* bb = nullptr;
    app.swap->GetBuffer(0, IID_PPV_ARGS(&bb));
    app.device->CreateRenderTargetView(bb, nullptr, &app.rtv);
    bb->Release();
}

static void destroy_rtv(App& app) {
    if (app.rtv) {
        app.rtv->Release();
        app.rtv = nullptr;
    }
}

static bool create_d3d(App& app, HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL levels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               levels, 2, D3D11_SDK_VERSION, &sd, &app.swap,
                                               &app.device, &fl, &app.context);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                           levels, 2, D3D11_SDK_VERSION, &sd, &app.swap,
                                           &app.device, &fl, &app.context);
    }
    if (hr != S_OK) return false;
    create_rtv(app);
    return true;
}

static void destroy_d3d(App& app) {
    destroy_rtv(app);
    if (app.swap) {
        app.swap->Release();
        app.swap = nullptr;
    }
    if (app.context) {
        app.context->Release();
        app.context = nullptr;
    }
    if (app.device) {
        app.device->Release();
        app.device = nullptr;
    }
}

static LRESULT WINAPI wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) return true;
    switch (msg) {
    // Remove standard window frame (title bar + border) so we can draw our own.
    case WM_NCCALCSIZE: {
        if (wparam == TRUE) {
            NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)lparam;
            // Preserve the resize border but strip the caption. When maximized,
            // Windows extends the window past the monitor edge; account for that.
            if (is_window_maximized(hwnd)) {
                RECT work;
                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(mon, &mi)) {
                    p->rgrc[0] = mi.rcWork;
                } else {
                    (void)work;
                }
            } else {
                // Keep a 1-pixel top edge so Windows draws a shadow / border.
                p->rgrc[0].top    += 0;
                p->rgrc[0].left   += 0;
                p->rgrc[0].right  -= 0;
                p->rgrc[0].bottom -= 0;
            }
            return 0;
        }
        break;
    }
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
        RECT wr;
        GetWindowRect(hwnd, &wr);
        int rx = pt.x - wr.left;
        int ry = pt.y - wr.top;
        int ww = wr.right  - wr.left;
        int wh = wr.bottom - wr.top;
        const int border = 6;
        bool max = is_window_maximized(hwnd);
        if (!max) {
            bool top = ry < border;
            bool bot = ry >= wh - border;
            bool left = rx < border;
            bool right = rx >= ww - border;
            if (top && left)  return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bot && left)  return HTBOTTOMLEFT;
            if (bot && right) return HTBOTTOMRIGHT;
            if (top)          return HTTOP;
            if (bot)          return HTBOTTOM;
            if (left)         return HTLEFT;
            if (right)        return HTRIGHT;
        }
        if (ry < title_bar_h()) {
            // Menu-item strip (from just after the app icon to well past the last menu).
            // Anything in this X range is a client hit so ImGui gets the click.
            const float s = g_app ? g_app->dpi : 1.0f;
            const int menu_x0 = (int)(32 * s);
            const int menu_x1 = (int)(380 * s);
            if (rx >= menu_x0 && rx < menu_x1) return HTCLIENT;
            // Window controls (min / max / close): 3 * 46px on the far right.
            const int btns_w = (int)(46 * 3 * s);
            if (rx >= ww - btns_w) return HTCLIENT;
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wparam;
        UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < n; i++) {
            wchar_t w[MAX_PATH];
            if (DragQueryFileW(drop, i, w, MAX_PATH)) {
                std::string p = wide_to_utf8(w);
                if (dir_exists(p)) set_folder(p);
                else open_path(p, false);
            }
        }
        DragFinish(drop);
        return 0;
    }
    case WM_SIZE:
        if (wparam == SIZE_MINIMIZED) return 0;
        if (g_app) {
            g_app->resize_w = LOWORD(lparam);
            g_app->resize_h = HIWORD(lparam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_CLOSE:
        if (g_app) {
            bool dirty = false;
            for (const auto& b : g_app->buffers) {
                if (b.dirty) dirty = true;
            }
            if (dirty) {
                int r = MessageBoxW(hwnd, L"Save all unsaved files before exit?", L"Nexium",
                                    MB_YESNOCANCEL | MB_ICONWARNING);
                if (r == IDCANCEL) return 0;
                if (r == IDYES) save_all();
            }
            g_app->want_quit = true;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ImGui_ImplWin32_EnableDpiAwareness();

    App app;
    g_app = &app;
    load_settings(app.settings);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"NexiumIDE";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_NEXIUM));
    wc.hIconSm = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_NEXIUM),
                                   IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON),
                                   LR_DEFAULTCOLOR);
    RegisterClassExW(&wc);
    // WS_OVERLAPPEDWINDOW without WS_CAPTION — Windows still lets us resize / snap.
    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU);
    style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Nexium", style,
                              80, 40, 1500, 920, nullptr, nullptr, instance, nullptr);
    if (wc.hIcon) SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)wc.hIcon);
    if (wc.hIconSm) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)wc.hIconSm);
    app.dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
    if (app.dpi < 1.0f) app.dpi = 1.0f;
    if (app.dpi > 1.01f) {
        SetWindowPos(hwnd, nullptr, 0, 0,
                     (int)(1500.0f * app.dpi + 0.5f), (int)(920.0f * app.dpi + 0.5f),
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    app.hwnd = hwnd;
    if (!create_d3d(app, hwnd)) {
        destroy_d3d(app);
        UnregisterClassW(wc.lpszClassName, instance);
        return 1;
    }
    DragAcceptFiles(hwnd, TRUE);
    enable_dark_titlebar(hwnd);
    // Force WM_NCCALCSIZE so the caption is removed immediately.
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigNavMoveSetMousePos = false;
    io.IniFilename = nullptr;
    apply_vscode_theme();
    ImGui::GetStyle().ScaleAllSizes(app.dpi);
    {
        ImGuiStyle& st = ImGui::GetStyle();
        auto snap = [](float& v) { v = (float)(int)(v + 0.5f); };
        auto snap2 = [&](ImVec2& p) { snap(p.x); snap(p.y); };
        snap2(st.WindowPadding);
        snap2(st.WindowMinSize);
        snap2(st.FramePadding);
        snap2(st.ItemSpacing);
        snap2(st.ItemInnerSpacing);
        snap2(st.CellPadding);
        snap2(st.TouchExtraPadding);
        snap2(st.DisplaySafeAreaPadding);
        snap(st.IndentSpacing);
        snap(st.ColumnsMinSpacing);
        snap(st.ScrollbarSize);
        snap(st.GrabMinSize);
        snap(st.TabBarOverlineSize);
        snap(st.SeparatorTextPadding.x);
        snap(st.SeparatorTextPadding.y);
        st.AntiAliasedLinesUseTex = false;
    }
    io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines;
    io.FontGlobalScale = 1.0f;

    const float dpi = app.dpi;
    auto px = [dpi](float logical) { return (float)(int)(logical * dpi + 0.5f); };

    ImFontConfig fc;
    fc.OversampleH = 2;
    fc.OversampleV = 1;
    fc.PixelSnapH = true;
    fc.RasterizerMultiply = 1.0f;

    static const ImWchar ui_ranges[] = {
        0x0020, 0x00FF,   // Basic Latin + Latin-1
        0x2010, 0x27BF,   // Punctuation, arrows, geometric shapes, misc symbols, dingbats
        0
    };
    static const ImWchar icon_ranges[] = {
        0xE000, 0xF8FF,   // Segoe MDL2 / Segoe Fluent Icons private-use area
        0
    };

    // Prefer Windows 11 Segoe Fluent Icons; fall back to Windows 10 Segoe MDL2 Assets.
    const char* mdl2 = nullptr;
    if (file_exists("C:\\Windows\\Fonts\\SegoeIcons.ttf"))       mdl2 = "C:\\Windows\\Fonts\\SegoeIcons.ttf";
    else if (file_exists("C:\\Windows\\Fonts\\segmdl2.ttf"))     mdl2 = "C:\\Windows\\Fonts\\segmdl2.ttf";

    auto merge_icons = [&](float logical, float y_offset, float min_adv) {
        if (!mdl2) return;
        ImFontConfig mc;
        mc.MergeMode = true;
        mc.PixelSnapH = true;
        mc.OversampleH = 2;
        mc.OversampleV = 1;
        mc.GlyphMinAdvanceX = px(min_adv);
        mc.GlyphOffset = ImVec2(0.0f, (float)(int)(y_offset * dpi + 0.5f));
        mc.RasterizerMultiply = 1.0f;
        io.Fonts->AddFontFromFileTTF(mdl2, px(logical), &mc, icon_ranges);
    };

    // UI font baked at native DPI so Windows does not stretch a 96-dpi atlas.
    app.font_ui = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", px(16.0f), &fc, ui_ranges);
    merge_icons(14.0f, 1.5f, 16.0f);

    app.font_ui_small = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", px(13.0f), &fc, ui_ranges);
    merge_icons(12.0f, 1.0f, 13.0f);

    if (mdl2) {
        ImFontConfig icf;
        icf.OversampleH = 2;
        icf.OversampleV = 1;
        icf.PixelSnapH = true;
        icf.RasterizerMultiply = 1.0f;
        app.font_icon = io.Fonts->AddFontFromFileTTF(mdl2, px(16.0f), &icf, icon_ranges);
    }

    app.font_title = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", px(26.0f), &fc, ui_ranges);
    merge_icons(22.0f, 2.0f, 24.0f);

    app.font_agent = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", px(18.0f), &fc, ui_ranges);
    merge_icons(16.0f, 1.5f, 18.0f);
    if (file_exists("C:\\Windows\\Fonts\\segoeuib.ttf")) {
        app.font_agent_bold = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", px(18.0f), &fc, ui_ranges);
    }

    ImFontConfig code_fc = fc;
    code_fc.RasterizerMultiply = 1.0f;
    static const ImWchar code_ranges[] = {
        0x0020, 0x00FF,
        0x2190, 0x21FF,
        0x2500, 0x259F,
        0
    };
    const char* code_fonts[] = {
        "C:\\Windows\\Fonts\\CascadiaCode.ttf",
        "C:\\Windows\\Fonts\\cascadiacode.ttf",
        "C:\\Windows\\Fonts\\CascadiaMono.ttf",
        "C:\\Windows\\Fonts\\cascadiamono.ttf",
        "C:\\Windows\\Fonts\\consola.ttf",
    };
    for (const char* f : code_fonts) {
        if (file_exists(f)) {
            app.font_code = io.Fonts->AddFontFromFileTTF(f, px(16.0f), &code_fc, code_ranges);
            if (app.font_code) break;
        }
    }
    for (const char* f : code_fonts) {
        if (file_exists(f)) {
            app.font_agent_code = io.Fonts->AddFontFromFileTTF(f, px(16.0f), &code_fc, code_ranges);
            if (app.font_agent_code) break;
        }
    }
    if (!app.font_ui) app.font_ui = io.Fonts->AddFontDefault();
    if (!app.font_code) app.font_code = app.font_ui;
    if (!app.font_ui_small) app.font_ui_small = app.font_ui;
    if (!app.font_title) app.font_title = app.font_ui;
    if (!app.font_agent) app.font_agent = app.font_ui;
    if (!app.font_agent_bold) app.font_agent_bold = app.font_agent;
    if (!app.font_agent_code) app.font_agent_code = app.font_code;
    if (!app.font_icon) app.font_icon = app.font_ui;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(app.device, app.context);
    load_app_icon();

    if (!app.settings.folder.empty() && dir_exists(app.settings.folder)) {
        set_folder(app.settings.folder);
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool opened_from_cli = false;
    if (argv) {
        for (int i = 1; i < argc; i++) {
            std::string p = wide_to_utf8(argv[i]);
            if (dir_exists(p)) { set_folder(p); opened_from_cli = true; }
            else if (file_exists(p)) { open_path(p, false); opened_from_cli = true; }
        }
        LocalFree(argv);
    }
    (void)opened_from_cli;

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done || app.want_quit) break;
        if (app.occluded && app.swap->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            Sleep(10);
            continue;
        }
        app.occluded = false;
        if (app.resize_w && app.resize_h) {
            destroy_rtv(app);
            app.swap->ResizeBuffers(0, app.resize_w, app.resize_h, DXGI_FORMAT_UNKNOWN, 0);
            app.resize_w = app.resize_h = 0;
            create_rtv(app);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (app.font_ui) ImGui::PushFont(app.font_ui);
        draw_ide();
        if (app.font_ui) ImGui::PopFont();
        ImGui::Render();

        const float clear[4] = {0.071f, 0.071f, 0.071f, 1.0f};
        app.context->OMSetRenderTargets(1, &app.rtv, nullptr);
        app.context->ClearRenderTargetView(app.rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        HRESULT hr = app.swap->Present(1, 0);
        app.occluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    save_settings(app.settings);
    app.proc.shutdown = true;
    proc_stop();
    release_app_icon();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    destroy_d3d(app);
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, instance);
    if (SUCCEEDED(com)) CoUninitialize();
    g_app = nullptr;
    return 0;
}
