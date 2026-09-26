#include "nexium.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <functional>

#include "imgui_internal.h"

// ---------------------------------------------------------------------------
// VS Code "Dark Modern" palette
// ---------------------------------------------------------------------------
namespace vs {
    static const ImU32 editor_bg       = IM_COL32(0x1F, 0x1F, 0x1F, 255);
    static const ImU32 sidebar_bg      = IM_COL32(0x18, 0x18, 0x18, 255);
    static const ImU32 panel_bg        = IM_COL32(0x18, 0x18, 0x18, 255);
    static const ImU32 activity_bg     = IM_COL32(0x18, 0x18, 0x18, 255);
    static const ImU32 activity_active = IM_COL32(0xD7, 0xD7, 0xD7, 255);
    static const ImU32 activity_dim    = IM_COL32(0x86, 0x86, 0x86, 255);
    static const ImU32 activity_hover  = IM_COL32(0x2A, 0x2D, 0x2E, 255);
    static const ImU32 activity_bar    = IM_COL32(0x00, 0x78, 0xD4, 255);
    static const ImU32 accent          = IM_COL32(0x00, 0x78, 0xD4, 255);
    static const ImU32 accent_hover    = IM_COL32(0x02, 0x6E, 0xC1, 255);
    static const ImU32 status_bg       = IM_COL32(0x18, 0x18, 0x18, 255);
    static const ImU32 status_text     = IM_COL32(0xCC, 0xCC, 0xCC, 255);
    static const ImU32 status_hover    = IM_COL32(0x2A, 0x2A, 0x2A, 255);
    static const ImU32 tab_active_bg   = IM_COL32(0x1F, 0x1F, 0x1F, 255);
    static const ImU32 tab_inactive_bg = IM_COL32(0x18, 0x18, 0x18, 255);
    static const ImU32 tab_hover_bg    = IM_COL32(0x1F, 0x1F, 0x1F, 255);
    static const ImU32 tab_active_line = IM_COL32(0x00, 0x78, 0xD4, 255);
    static const ImU32 tab_text_active = IM_COL32(0xFF, 0xFF, 0xFF, 255);
    static const ImU32 tab_text_dim    = IM_COL32(0x9D, 0x9D, 0x9D, 255);
    static const ImU32 splitter_hover  = IM_COL32(0x00, 0x78, 0xD4, 255);
    static const ImU32 border          = IM_COL32(0x2B, 0x2B, 0x2B, 255);
    static const ImU32 tabstrip_bg     = IM_COL32(0x18, 0x18, 0x18, 255);
    static const ImU32 heading         = IM_COL32(0xCC, 0xCC, 0xCC, 255);
    static const ImU32 heading_action  = IM_COL32(0xB0, 0xB0, 0xB0, 255);
    static const ImU32 subtle          = IM_COL32(0x9D, 0x9D, 0x9D, 255);
    static const ImU32 text_bright     = IM_COL32(0xE0, 0xE0, 0xE0, 255);
    static const ImU32 text_normal     = IM_COL32(0xCC, 0xCC, 0xCC, 255);
    static const ImU32 list_hover      = IM_COL32(0x2A, 0x2D, 0x2E, 255);
    static const ImU32 nexa_icon       = IM_COL32(0x7E, 0xC4, 0xE0, 255);
    static const ImU32 file_icon       = IM_COL32(0x9A, 0x9A, 0x9A, 255);
    static const ImU32 error           = IM_COL32(0xF1, 0x4C, 0x4C, 255);
    static const ImU32 warn            = IM_COL32(0xCC, 0xA7, 0x00, 255);
}

static ImVec4 rgb(int r, int g, int b, float a = 1.0f) {
    return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, a);
}

void apply_vscode_theme() {
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding      = 0;
    st.ChildRounding       = 0;
    st.FrameRounding       = 4;
    st.PopupRounding       = 5;
    st.ScrollbarRounding   = 0;
    st.GrabRounding        = 3;
    st.TabRounding         = 0;
    st.WindowBorderSize    = 0;
    st.ChildBorderSize     = 0;
    st.FrameBorderSize     = 0;
    st.PopupBorderSize     = 1;
    st.WindowPadding       = ImVec2(0, 0);
    st.FramePadding        = ImVec2(8, 4);
    st.ItemSpacing         = ImVec2(8, 6);
    st.ItemInnerSpacing    = ImVec2(6, 4);
    st.IndentSpacing       = 12;
    st.ScrollbarSize       = 12;

    ImVec4* c = st.Colors;
    c[ImGuiCol_Text]                  = rgb(0xCC, 0xCC, 0xCC);
    c[ImGuiCol_TextDisabled]          = rgb(0x6E, 0x6E, 0x6E);
    c[ImGuiCol_TextSelectedBg]        = rgb(0x26, 0x4F, 0x78);
    c[ImGuiCol_WindowBg]              = rgb(0x18, 0x18, 0x18);
    c[ImGuiCol_ChildBg]               = rgb(0x18, 0x18, 0x18);
    c[ImGuiCol_PopupBg]               = rgb(0x1F, 0x1F, 0x1F);
    c[ImGuiCol_Border]                = rgb(0x45, 0x45, 0x45);
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]               = rgb(0x31, 0x31, 0x31);
    c[ImGuiCol_FrameBgHovered]        = rgb(0x37, 0x37, 0x37);
    c[ImGuiCol_FrameBgActive]         = rgb(0x3C, 0x3C, 0x3C);
    c[ImGuiCol_TitleBg]               = rgb(0x18, 0x18, 0x18);
    c[ImGuiCol_TitleBgActive]         = rgb(0x18, 0x18, 0x18);
    c[ImGuiCol_MenuBarBg]             = rgb(0x18, 0x18, 0x18);
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]         = rgb(0x79, 0x79, 0x79, 0.40f);
    c[ImGuiCol_ScrollbarGrabHovered]  = rgb(0x64, 0x64, 0x64, 0.70f);
    c[ImGuiCol_ScrollbarGrabActive]   = rgb(0xBF, 0xBF, 0xBF, 0.40f);
    c[ImGuiCol_CheckMark]             = rgb(0xFF, 0xFF, 0xFF);
    c[ImGuiCol_SliderGrab]            = rgb(0x00, 0x78, 0xD4);
    c[ImGuiCol_SliderGrabActive]      = rgb(0x02, 0x6E, 0xC1);
    c[ImGuiCol_Button]                = rgb(0x31, 0x31, 0x31);
    c[ImGuiCol_ButtonHovered]         = rgb(0x3C, 0x3C, 0x3C);
    c[ImGuiCol_ButtonActive]          = rgb(0x45, 0x45, 0x45);
    c[ImGuiCol_Header]                = rgb(0x04, 0x39, 0x5E);
    c[ImGuiCol_HeaderHovered]         = rgb(0x2A, 0x2D, 0x2E);
    c[ImGuiCol_HeaderActive]          = rgb(0x04, 0x39, 0x5E);
    c[ImGuiCol_Separator]             = rgb(0x2B, 0x2B, 0x2B);
    c[ImGuiCol_SeparatorHovered]      = rgb(0x00, 0x78, 0xD4);
    c[ImGuiCol_SeparatorActive]       = rgb(0x00, 0x78, 0xD4);
    c[ImGuiCol_ResizeGrip]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_Tab]                   = rgb(0x18, 0x18, 0x18);
    c[ImGuiCol_TabHovered]            = rgb(0x1F, 0x1F, 0x1F);
    c[ImGuiCol_TabSelected]           = rgb(0x1F, 0x1F, 0x1F);
    c[ImGuiCol_TabDimmed]             = rgb(0x18, 0x18, 0x18);
    c[ImGuiCol_TabDimmedSelected]     = rgb(0x1F, 0x1F, 0x1F);
    c[ImGuiCol_TableHeaderBg]         = rgb(0x1F, 0x1F, 0x1F);
    c[ImGuiCol_TableBorderStrong]     = rgb(0x2B, 0x2B, 0x2B);
    c[ImGuiCol_TableBorderLight]      = rgb(0x2B, 0x2B, 0x2B);
    c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1, 1, 1, 0.02f);
    c[ImGuiCol_NavHighlight]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0, 0, 0, 0.45f);
}

// ---------------------------------------------------------------------------
// Shared chrome (matches the agent panel)
// ---------------------------------------------------------------------------
void ui_empty_state(const char* icon, const char* title, const char* hint) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::Dummy(ImVec2(1, 28.0f));
    if (g_app->font_icon) ImGui::PushFont(g_app->font_icon);
    ImVec2 ic = ImGui::CalcTextSize(icon);
    float cx = ImGui::GetCursorScreenPos().x + (ImGui::GetContentRegionAvail().x - ic.x) * 0.5f;
    dl->AddText(ImVec2(cx, ImGui::GetCursorScreenPos().y), IM_COL32(0x6C, 0xB6, 0xFF, 180), icon);
    if (g_app->font_icon) ImGui::PopFont();
    ImGui::Dummy(ImVec2(1, ic.y + 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.82f, 1));
    ImGui::TextWrapped("%s", title);
    ImGui::PopStyleColor();
    if (hint && hint[0]) {
        ImGui::Dummy(ImVec2(1, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1));
        ImGui::TextWrapped("%s", hint);
        ImGui::PopStyleColor();
    }
}

bool ui_accent_button(const char* label, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        rgb(0x00, 0x78, 0xD4));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(0x02, 0x6E, 0xC1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  rgb(0x00, 0x5F, 0xA8));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1, 1, 1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    bool hit = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return hit;
}

void ui_push_field() {
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        rgb(0x31, 0x31, 0x31));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, rgb(0x31, 0x31, 0x31));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  rgb(0x31, 0x31, 0x31));
    ImGui::PushStyleColor(ImGuiCol_Border,         rgb(0x3C, 0x3C, 0x3C));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 5.0f));
}

void ui_pop_field() {
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);
}

void ui_begin_card(const char* id) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, rgb(0x1F, 0x1F, 0x1F));
    ImGui::PushStyleColor(ImGuiCol_Border,  rgb(0x2B, 0x2B, 0x2B));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::BeginChild(id, ImVec2(-FLT_MIN, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders |
                      ImGuiChildFlags_AlwaysUseWindowPadding);
}

void ui_end_card() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

bool ui_chip_row(const char* id, const char* icon, ImU32 icon_col,
                 const char* title, const char* detail) {
    ImGui::PushID(id);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float wrap = ImGui::GetContentRegionAvail().x;
    if (wrap < 40.0f) wrap = 40.0f;
    const float row_h = dp(22.0f);
    ImGui::InvisibleButton("##chip", ImVec2(wrap, row_h));
    bool hov = ImGui::IsItemHovered();
    bool clk = ImGui::IsItemClicked();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hov) dl->AddRectFilled(origin, ImVec2(origin.x + wrap, origin.y + row_h), vs::list_hover);
    float x = origin.x + 8.0f;
    float mid = origin.y + row_h * 0.5f;
    if (icon && icon[0]) {
        if (g_app->font_icon) ImGui::PushFont(g_app->font_icon);
        ImVec2 isz = ImGui::CalcTextSize(icon);
        dl->AddText(ImVec2(x, mid - isz.y * 0.5f), icon_col, icon);
        x += isz.x + 8.0f;
        if (g_app->font_icon) ImGui::PopFont();
    }
    ImVec2 ts = ImGui::CalcTextSize(title);
    float right = origin.x + wrap - 8.0f;
    if (detail && detail[0]) {
        ImVec2 ds = ImGui::CalcTextSize(detail);
        right -= ds.x + 6.0f;
        dl->AddText(ImVec2(origin.x + wrap - 8.0f - ds.x, mid - ds.y * 0.5f),
                    vs::subtle, detail);
    }
    ImGui::RenderTextEllipsis(dl, ImVec2(x, mid - ts.y * 0.5f),
                              ImVec2(right, origin.y + row_h),
                              right, right, title, nullptr, nullptr);
    ImGui::PopID();
    return clk;
}

// ---------------------------------------------------------------------------
// Splitters
// ---------------------------------------------------------------------------
// dir: +1 = drag right grows *size (left-anchored panel)
//      -1 = drag left grows *size  (right-anchored panel, e.g. agent)
static const float kSplitter = 4.0f;

static void splitter_v(const char* id, float* size, float min_v, float max_v, float height, int dir = +1) {
    ImGui::PushID(id);
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("spl", ImVec2(kSplitter, height));
    bool held = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();
    if (hovered || held) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (held) *size = std::clamp(*size + (float)dir * ImGui::GetIO().MouseDelta.x, min_v, max_v);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(cursor, ImVec2(cursor.x + kSplitter, cursor.y + height), vs::sidebar_bg);
    float x = (float)(int)(cursor.x + kSplitter * 0.5f) + 0.5f;
    if (held || hovered) {
        dl->AddRectFilled(ImVec2(cursor.x, cursor.y), ImVec2(cursor.x + kSplitter, cursor.y + height), vs::splitter_hover);
    } else {
        dl->AddLine(ImVec2(x, cursor.y), ImVec2(x, cursor.y + height), vs::border, 1.0f);
    }
    ImGui::PopID();
}

static void splitter_h(const char* id, float* size, float min_v, float max_v, float width) {
    ImGui::PushID(id);
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("spl", ImVec2(width, kSplitter));
    bool held = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();
    if (hovered || held) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if (held) *size = std::clamp(*size - ImGui::GetIO().MouseDelta.y, min_v, max_v);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + kSplitter), vs::panel_bg);
    float y = (float)(int)(cursor.y + kSplitter * 0.5f) + 0.5f;
    if (held || hovered) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + kSplitter), vs::splitter_hover);
    } else {
        dl->AddLine(ImVec2(cursor.x, y), ImVec2(cursor.x + width, y), vs::border, 1.0f);
    }
    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// Activity bar
// ---------------------------------------------------------------------------
struct ActivityItem {
    ActivityView view;
    const char* icon;   // MDL2 Assets private-use codepoint
    const char* tip;
};

static const ActivityItem kActivityItems[] = {
    {ActivityView::Explorer, IC_EXPLORER, "Explorer  (Ctrl+Shift+E)"},
    {ActivityView::Search,   IC_SEARCH,   "Search  (Ctrl+Shift+F)"},
    {ActivityView::Outline,  IC_OUTLINE,  "Outline"},
    {ActivityView::Run,      IC_RUN,      "Run and Debug"},
    {ActivityView::Settings, IC_SETTINGS, "Settings"},
};

static void draw_activity() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), vs::activity_bg);
    dl->AddLine(ImVec2(pos.x + size.x - 0.5f, pos.y),
                ImVec2(pos.x + size.x - 0.5f, pos.y + size.y), vs::border);

    ImFont* f = g_app->font_icon ? g_app->font_icon : ImGui::GetFont();
    const float item_h = dp(48.0f);
    const int n_items = (int)(sizeof(kActivityItems) / sizeof(kActivityItems[0]));
    for (int i = 0; i < n_items; i++) {
        const ActivityItem& it = kActivityItems[i];
        bool active = g_app->activity == it.view && g_app->settings.show_sidebar;
        if (it.view == ActivityView::Settings) {
            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y - item_h - dp(4.0f)));
        }
        ImVec2 c = ImGui::GetCursorScreenPos();
        ImVec2 rmin = c;
        ImVec2 rmax = ImVec2(c.x + size.x, c.y + item_h);
        ImGui::PushID(i);
        ImGui::InvisibleButton("act", ImVec2(size.x, item_h));
        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked();
        if (hovered) ImGui::SetTooltip("%s", it.tip);
        if (clicked) {
            if (g_app->activity == it.view) {
                g_app->settings.show_sidebar = !g_app->settings.show_sidebar;
            } else {
                g_app->activity = it.view;
                g_app->settings.show_sidebar = true;
            }
        }
        if (active) {
            dl->AddRectFilled(ImVec2(rmin.x, rmin.y), ImVec2(rmin.x + 2.0f, rmax.y), vs::activity_bar);
        }
        ImU32 col = (active || hovered) ? vs::activity_active : vs::activity_dim;
        const float isz = f->FontSize * 1.3f;
        ImVec2 ts = f->CalcTextSizeA(isz, FLT_MAX, 0.0f, it.icon);
        dl->AddText(f, isz,
                    ImVec2((float)(int)(rmin.x + (size.x - ts.x) * 0.5f),
                           (float)(int)(rmin.y + (item_h - isz) * 0.5f)),
                    col, it.icon);
        ImGui::PopID();
    }
}

// ---------------------------------------------------------------------------
// Sidebar
// ---------------------------------------------------------------------------
struct HeaderAction { const char* icon; const char* tip; std::function<void()> run; };

static void section_header(const char* icon, const char* text,
                           const std::vector<HeaderAction>& actions = {}) {
    (void)icon;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 c = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    const float h = dp(35.0f);
    dl->AddRectFilled(c, ImVec2(c.x + w, c.y + h), vs::sidebar_bg);

    std::string title = text;
    for (char& ch : title) ch = (char)std::toupper((unsigned char)ch);
    ImFont* small = g_app->font_ui_small ? g_app->font_ui_small : ImGui::GetFont();
    dl->AddText(small, small->FontSize, ImVec2(c.x + dp(20.0f), c.y + (h - small->FontSize) * 0.5f),
                vs::heading, title.c_str());

    ImGui::PushID(text);
    const float bw = dp(22.0f);
    float bx = c.x + w - dp(8.0f);
    for (int i = (int)actions.size() - 1; i >= 0; i--) {
        bx -= bw + 2.0f;
        ImGui::SetCursorScreenPos(ImVec2(bx, c.y + (h - bw) * 0.5f));
        ImGui::PushID(i);
        ImGui::InvisibleButton("act", ImVec2(bw, bw));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        if (hov) {
            dl->AddRectFilled(ImVec2(bx, c.y + (h - bw) * 0.5f),
                              ImVec2(bx + bw, c.y + (h + bw) * 0.5f),
                              IM_COL32(0x5A, 0x5D, 0x5E, 0x50), 4.0f);
            ImGui::SetTooltip("%s", actions[(size_t)i].tip);
        }
        if (g_app->font_icon) ImGui::PushFont(g_app->font_icon);
        ImVec2 ts = ImGui::CalcTextSize(actions[(size_t)i].icon);
        dl->AddText(ImVec2(bx + (bw - ts.x) * 0.5f, c.y + (h - ts.y) * 0.5f),
                    hov ? vs::text_bright : vs::heading_action,
                    actions[(size_t)i].icon);
        if (g_app->font_icon) ImGui::PopFont();
        if (clk) actions[(size_t)i].run();
        ImGui::PopID();
    }
    ImGui::PopID();

    ImGui::SetCursorScreenPos(ImVec2(c.x, c.y + h));
    ImGui::Dummy(ImVec2(w, 0));
}

static void draw_sidebar() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), vs::sidebar_bg);

    switch (g_app->activity) {
    case ActivityView::Explorer: {
        std::vector<HeaderAction> acts;
        if (!g_app->settings.folder.empty()) {
            acts.push_back({IC_ADD, "New File", [] {
                g_app->new_file_modal = true;
                g_app->modal_parent = g_app->settings.folder;
                std::strcpy(g_app->new_name, "untitled.nxa");
            }});
            acts.push_back({IC_ADD_FOLDER, "New Folder", [] {
                g_app->new_folder_modal = true;
                g_app->modal_parent = g_app->settings.folder;
                std::strcpy(g_app->new_name, "new-folder");
            }});
            acts.push_back({IC_REFRESH, "Refresh", [] {
                scan_node(g_app->root, true);
            }});
        }
        section_header(IC_EXPLORER, "Explorer", acts);
        draw_explorer();
        break;
    }
    case ActivityView::Search:
        section_header(IC_SEARCH, "Search");
        draw_search();
        break;
    case ActivityView::Outline:
        section_header(IC_OUTLINE, "Outline");
        draw_outline();
        break;
    case ActivityView::Run:
        section_header(IC_RUN, "Run and Debug");
        draw_run_view();
        break;
    case ActivityView::Settings:
        section_header(IC_SETTINGS, "Settings");
        draw_settings_panel();
        break;
    default: {
        ActivityView unk = g_app->activity;
        (void)unk;
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Editor tab strip (custom drawn, VS Code style)
// ---------------------------------------------------------------------------
static ImU32 lang_icon_color(Lang lang) {
    switch (lang) {
    case Lang::Nexa: return vs::nexa_icon;
    case Lang::Json:
    case Lang::Ini: return IM_COL32(0xE2, 0xC0, 0x8D, 255);
    case Lang::Cpp: return IM_COL32(0xC5, 0x93, 0xE7, 255);
    case Lang::Markdown: return IM_COL32(0x9C, 0xDC, 0xFE, 255);
    case Lang::Shell: return IM_COL32(0x89, 0xD1, 0x85, 255);
    default: return vs::file_icon;
    }
}

void draw_editor_tabs(float strip_h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float avail_w = ImGui::GetContentRegionAvail().x;
    dl->AddRectFilled(origin, ImVec2(origin.x + avail_w, origin.y + strip_h), vs::tabstrip_bg);
    // The strip's bottom border; the active tab paints over it to join the editor.
    dl->AddLine(ImVec2(origin.x, origin.y + strip_h - 0.5f),
                ImVec2(origin.x + avail_w, origin.y + strip_h - 0.5f), vs::border);

    int close_i = -1;
    const float pad_x   = dp(10.0f);
    const float icon_w  = dp(16.0f);
    const float gap     = dp(6.0f);
    const float close_w = dp(20.0f);
    const float min_w   = dp(80.0f);
    const float text_y  = origin.y + (strip_h - ImGui::GetFontSize()) * 0.5f;

    // Tabs that do not fit scroll sideways (mouse wheel), keeping the active one in view.
    static float tab_scroll = 0.0f;
    static int tab_scroll_active = -1;
    std::vector<float> widths;
    float total_w = 0.0f;
    for (const auto& tb : g_app->buffers) {
        float tw = pad_x + icon_w + gap + ImGui::CalcTextSize(tb.name.c_str()).x + gap + close_w + dp(6.0f);
        tw = std::max(tw, min_w);
        widths.push_back(tw);
        total_w += tw;
    }
    float max_scroll = std::max(0.0f, total_w - avail_w);
    if (ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + avail_w, origin.y + strip_h))) {
        float wheel = ImGui::GetIO().MouseWheel + ImGui::GetIO().MouseWheelH;
        tab_scroll -= wheel * 60.0f;
    }
    if (tab_scroll_active != g_app->active && g_app->active >= 0 && g_app->active < (int)widths.size()) {
        float ax = 0.0f;
        for (int i = 0; i < g_app->active; i++) ax += widths[(size_t)i];
        float aw = widths[(size_t)g_app->active];
        if (ax < tab_scroll) tab_scroll = ax;
        if (ax + aw > tab_scroll + avail_w) tab_scroll = ax + aw - avail_w;
        tab_scroll_active = g_app->active;
    }
    tab_scroll = std::clamp(tab_scroll, 0.0f, max_scroll);
    dl->PushClipRect(origin, ImVec2(origin.x + avail_w, origin.y + strip_h), true);
    float x = origin.x - tab_scroll;

    for (int i = 0; i < (int)g_app->buffers.size(); i++) {
        TextBuffer& b = g_app->buffers[(size_t)i];
        bool active = i == g_app->active;
        float tab_w = widths[(size_t)i];
        ImVec2 rmin(x, origin.y);
        ImVec2 rmax(x + tab_w, origin.y + strip_h);

        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(rmin);
        ImGui::InvisibleButton("tab", ImVec2(tab_w, strip_h));
        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        bool middle = ImGui::IsItemClicked(ImGuiMouseButton_Middle);
        if (ImGui::BeginPopupContextItem("tabctx")) {
            if (ImGui::MenuItem("Close", "Ctrl+W")) close_i = i;
            // Tabs with unsaved changes stay open rather than losing their edits.
            if (ImGui::MenuItem("Close Others")) {
                for (int j = (int)g_app->buffers.size() - 1; j >= 0; j--) {
                    if (j != i && !g_app->buffers[(size_t)j].dirty) close_buffer(j, true);
                }
            }
            if (ImGui::MenuItem("Close All")) {
                for (int j = (int)g_app->buffers.size() - 1; j >= 0; j--) {
                    if (!g_app->buffers[(size_t)j].dirty) close_buffer(j, true);
                }
            }
            ImGui::Separator();
            if (!b.untitled && ImGui::MenuItem("Copy Path")) ImGui::SetClipboardText(b.path.c_str());
            if (!b.untitled && ImGui::MenuItem("Reveal in File Explorer")) reveal_in_explorer(b.path);
            ImGui::EndPopup();
        }
        if (clicked) {
            g_app->active = i;
            if (g_app->preview_tab == i && ImGui::IsMouseDoubleClicked(0)) g_app->preview_tab = -1;
            g_app->editor_focused = true;
            g_app->terminal_focused = false;
        }
        if (middle) close_i = i;

        ImU32 bg = active ? vs::tab_active_bg : (hovered ? vs::tab_hover_bg : vs::tab_inactive_bg);
        dl->AddRectFilled(rmin, rmax, bg);
        dl->AddLine(ImVec2(rmax.x - 0.5f, rmin.y), ImVec2(rmax.x - 0.5f, rmax.y), vs::border);
        if (active) {
            dl->AddRectFilled(rmin, ImVec2(rmax.x - 1.0f, rmin.y + 1.0f), vs::tab_active_line);
        } else {
            dl->AddLine(ImVec2(rmin.x, rmax.y - 0.5f), ImVec2(rmax.x, rmax.y - 0.5f), vs::border);
        }

        ImU32 tcol = active ? vs::tab_text_active : vs::tab_text_dim;
        ImU32 ficol = lang_icon_color(b.lang);
        float ix = rmin.x + pad_x;
        dl->AddText(ImVec2(ix, text_y), ficol, IC_FILE);
        float tx = ix + icon_w + gap;
        // A preview tab (single-click open) is shown dimmer until it is kept.
        if (g_app->preview_tab == i) tcol = active ? IM_COL32(0xD0, 0xD0, 0xD0, 255) : IM_COL32(0x80, 0x80, 0x80, 255);
        dl->AddText(ImVec2(tx, text_y), tcol, b.name.c_str());

        float cx = rmax.x - close_w * 0.5f - dp(6.0f);
        float cy = rmin.y + strip_h * 0.5f;
        ImVec2 close_min(cx - dp(9.0f), cy - dp(9.0f));
        ImVec2 close_max(cx + dp(9.0f), cy + dp(9.0f));
        bool over_close = ImGui::IsMouseHoveringRect(close_min, close_max);
        if (over_close) dl->AddRectFilled(close_min, close_max, IM_COL32(0x5A, 0x5D, 0x5E, 0x60), 4.0f);
        if (over_close && ImGui::IsMouseClicked(0)) close_i = i;

        const float xs = dp(4.0f);
        if (b.dirty && !over_close) {
            dl->AddCircleFilled(ImVec2(cx, cy), dp(4.0f), active ? vs::tab_text_active : vs::tab_text_dim, 16);
        } else if (hovered || active || over_close) {
            ImU32 xcol = over_close ? vs::text_bright : (active ? IM_COL32(0xCC, 0xCC, 0xCC, 255) : vs::tab_text_dim);
            dl->AddLine(ImVec2(cx - xs, cy - xs), ImVec2(cx + xs, cy + xs), xcol, 1.2f);
            dl->AddLine(ImVec2(cx - xs, cy + xs), ImVec2(cx + xs, cy - xs), xcol, 1.2f);
        }
        ImGui::PopID();
        x += tab_w;
    }
    dl->PopClipRect();
    if (close_i >= 0) close_buffer(close_i);

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + strip_h));
}

// ---------------------------------------------------------------------------
// Markdown (agent chat)
// ---------------------------------------------------------------------------
struct MdRun {
    const char* p = nullptr;
    int n = 0;
    ImFont* font = nullptr;
    ImU32 col = 0;
    bool code = false;
};

static float md_line_height() {
    float h = ImGui::GetTextLineHeight();
    if (g_app->font_agent) h = (std::max)(h, g_app->font_agent->FontSize);
    if (g_app->font_agent_bold) h = (std::max)(h, g_app->font_agent_bold->FontSize);
    if (g_app->font_agent_code) h = (std::max)(h, g_app->font_agent_code->FontSize);
    return h + 4.0f;
}

static void md_emit_runs(const std::vector<MdRun>& runs, float wrap) {
    if (wrap < 24.0f) wrap = 24.0f;
    const float line_h = md_line_height();
    if (runs.empty()) {
        ImGui::Dummy(ImVec2(wrap, 4.0f));
        return;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImFont* space_font = ImGui::GetFont();
    const float space_w = space_font->CalcTextSizeA(space_font->FontSize, FLT_MAX, 0.0f, " ").x;
    float x = 0.0f;
    float y = 0.0f;
    for (const MdRun& r : runs) {
        if (r.n <= 0 || !r.p) continue;
        ImFont* f = r.font ? r.font : ImGui::GetFont();
        const float sz = f->FontSize;
        const char* p = r.p;
        const char* end = r.p + r.n;
        while (p < end) {
            if (*p == '\n' || *p == '\r') {
                if (*p == '\r' && p + 1 < end && p[1] == '\n') p++;
                x = 0.0f;
                y += line_h;
                p++;
                continue;
            }
            const char* q = p;
            while (q < end && *q != ' ' && *q != '\n' && *q != '\r') q++;
            if (q == p) { p++; continue; }
            ImVec2 ts = f->CalcTextSizeA(sz, FLT_MAX, 0.0f, p, q);
            if (x > 0.0f && x + space_w + ts.x > wrap) {
                x = 0.0f;
                y += line_h;
            }
            if (x > 0.0f) x += space_w;
            const char* slice = p;
            while (slice < q) {
                float avail = wrap - x;
                if (avail < 8.0f) {
                    x = 0.0f;
                    y += line_h;
                    avail = wrap;
                }
                const char* cut = f->CalcWordWrapPositionA(1.0f, slice, q, avail);
                if (cut <= slice) {
                    cut = slice + 1;
                    while (cut < q && ((unsigned char)*cut & 0xC0) == 0x80) cut++;
                }
                ImVec2 part = f->CalcTextSizeA(sz, FLT_MAX, 0.0f, slice, cut);
                ImVec2 tp(origin.x + x, origin.y + y + (line_h - sz) * 0.5f);
                if (r.code) {
                    dl->AddRectFilled(ImVec2(tp.x - 2.0f, tp.y + 1.0f),
                                      ImVec2(tp.x + part.x + 2.0f, tp.y + sz + 3.0f),
                                      IM_COL32(0x2A, 0x2A, 0x2A, 255), 2.0f);
                }
                dl->AddText(f, sz, tp, r.col, slice, cut);
                x += part.x;
                slice = cut;
                if (slice < q) {
                    x = 0.0f;
                    y += line_h;
                }
            }
            p = q;
            while (p < end && *p == ' ') p++;
        }
    }
    ImGui::Dummy(ImVec2(wrap, y + line_h));
}

static void md_parse_inlines(const char* s, int n, ImU32 base, std::vector<MdRun>& out) {
    ImFont* regular = g_app->font_agent ? g_app->font_agent : ImGui::GetFont();
    ImFont* bold = g_app->font_agent_bold ? g_app->font_agent_bold : regular;
    ImFont* code = g_app->font_agent_code ? g_app->font_agent_code : regular;
    const ImU32 code_col = IM_COL32(0xCE, 0x91, 0x78, 255);
    int i = 0;
    while (i < n) {
        if (s[i] == '`' ) {
            int j = i + 1;
            while (j < n && s[j] != '`') j++;
            if (j < n) {
                out.push_back({s + i + 1, j - i - 1, code, code_col, true});
                i = j + 1;
                continue;
            }
        }
        if (i + 1 < n && s[i] == '*' && s[i + 1] == '*') {
            int j = i + 2;
            while (j + 1 < n && !(s[j] == '*' && s[j + 1] == '*')) j++;
            if (j + 1 < n) {
                out.push_back({s + i + 2, j - i - 2, bold, base, false});
                i = j + 2;
                continue;
            }
        }
        if (s[i] == '*' || s[i] == '_') {
            char m = s[i];
            int j = i + 1;
            while (j < n && s[j] != m) j++;
            if (j < n && j > i + 1) {
                out.push_back({s + i + 1, j - i - 1, regular, IM_COL32(0xC8, 0xC8, 0xD8, 255), false});
                i = j + 1;
                continue;
            }
        }
        if (s[i] == '[') {
            int rb = i + 1;
            while (rb < n && s[rb] != ']') rb++;
            if (rb + 1 < n && s[rb] == ']' && s[rb + 1] == '(') {
                int rp = rb + 2;
                while (rp < n && s[rp] != ')') rp++;
                if (rp < n) {
                    out.push_back({s + i + 1, rb - i - 1, regular, IM_COL32(0x6C, 0xB6, 0xFF, 255), false});
                    i = rp + 1;
                    continue;
                }
            }
        }
        int j = i + 1;
        while (j < n && s[j] != '`' && s[j] != '*' && s[j] != '_' && s[j] != '[') j++;
        out.push_back({s + i, j - i, regular, base, false});
        i = j;
    }
}

static void draw_markdown(const std::string& md, ImU32 base) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    float wrap = ImGui::GetContentRegionAvail().x;
    if (wrap < 40.0f) wrap = 40.0f;
    const char* s = md.c_str();
    int n = (int)md.size();
    int i = 0;
    while (i < n) {
        if (i + 2 < n && s[i] == '`' && s[i + 1] == '`' && s[i + 2] == '`') {
            int j = i + 3;
            while (j < n && s[j] != '\n') j++;
            if (j < n) j++;
            int k = j;
            while (k + 2 < n && !(s[k] == '`' && s[k + 1] == '`' && s[k + 2] == '`')) k++;
            int end = k;
            if (k + 2 < n) k += 3;
            ImFont* cf = g_app->font_agent_code ? g_app->font_agent_code : ImGui::GetFont();
            const float pad = 8.0f;
            ImVec2 c0 = ImGui::GetCursorScreenPos();
            ImGui::PushFont(cf);
            ImVec2 inner = cf->CalcTextSizeA(cf->FontSize, wrap - pad * 2.0f, wrap - pad * 2.0f, s + j, s + end);
            float box_h = inner.y + pad * 2.0f;
            if (box_h < cf->FontSize + pad * 2.0f) box_h = cf->FontSize + pad * 2.0f;
            ImGui::GetWindowDrawList()->AddRectFilled(
                c0, ImVec2(c0.x + wrap, c0.y + box_h), IM_COL32(0x18, 0x18, 0x18, 255), 4.0f);
            ImGui::SetCursorScreenPos(ImVec2(c0.x + pad, c0.y + pad));
            ImGui::PushTextWrapPos(c0.x + wrap - pad);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.78f, 1));
            ImGui::TextUnformatted(s + j, s + end);
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
            float text_bottom = ImGui::GetItemRectMax().y;
            float box_bottom = (std::max)(c0.y + box_h, text_bottom + pad);
            ImGui::SetCursorScreenPos(ImVec2(c0.x, box_bottom + 8.0f));
            ImGui::Dummy(ImVec2(wrap, 0.0f));
            ImGui::PopFont();
            i = k;
            if (i < n && s[i] == '\n') i++;
            continue;
        }

        int line_end = i;
        while (line_end < n && s[line_end] != '\n') line_end++;
        const char* ls = s + i;
        int ln = line_end - i;

        int hashes = 0;
        while (hashes < ln && hashes < 3 && ls[hashes] == '#') hashes++;
        if (hashes > 0 && hashes < ln && ls[hashes] == ' ') {
            ImFont* hf = g_app->font_agent_bold ? g_app->font_agent_bold : ImGui::GetFont();
            std::vector<MdRun> runs;
            runs.push_back({ls + hashes + 1, ln - hashes - 1, hf, IM_COL32(0xEE, 0xEE, 0xEE, 255), false});
            md_emit_runs(runs, wrap);
            i = line_end + (line_end < n ? 1 : 0);
            continue;
        }

        bool bullet = (ln >= 2 && (ls[0] == '-' || ls[0] == '*') && ls[1] == ' ');
        int num_end = 0;
        while (num_end < ln && ls[num_end] >= '0' && ls[num_end] <= '9') num_end++;
        bool numbered = num_end > 0 && num_end + 1 < ln && ls[num_end] == '.' && ls[num_end + 1] == ' ';
        if (bullet || numbered) {
            int skip = bullet ? 2 : num_end + 2;
            ImGui::Indent(16.0f);
            std::vector<MdRun> mark;
            if (bullet) mark.push_back({"• ", 4, ImGui::GetFont(), base, false});
            else mark.push_back({ls, skip, ImGui::GetFont(), base, false});
            md_parse_inlines(ls + skip, ln - skip, base, mark);
            md_emit_runs(mark, wrap - 16.0f);
            ImGui::Unindent(16.0f);
            i = line_end + (line_end < n ? 1 : 0);
            continue;
        }

        if (ln == 0) {
            ImGui::Dummy(ImVec2(1, 8));
            i = line_end + 1;
            continue;
        }

        std::vector<MdRun> runs;
        md_parse_inlines(ls, ln, base, runs);
        md_emit_runs(runs, wrap);
        i = line_end + (line_end < n ? 1 : 0);
    }
    ImGui::PopStyleVar();
}

// ---------------------------------------------------------------------------
// Agent tool chips (Cursor-style collapsible)
// ---------------------------------------------------------------------------
static bool agent_tool_is_open(const std::string& id) {
    const auto& v = g_app->agent.tool_open;
    return std::find(v.begin(), v.end(), id) != v.end();
}

static void agent_tool_toggle(const std::string& id) {
    auto& v = g_app->agent.tool_open;
    auto it = std::find(v.begin(), v.end(), id);
    if (it == v.end()) v.push_back(id);
    else v.erase(it);
}

static const char* agent_tool_icon(const std::string& name) {
    if (name == "run_command" || name == "run_nexa") return IC_TERMINAL;
    if (name == "list_dir" || name == "glob_files") return IC_FOLDER;
    if (name == "search_workspace") return IC_SEARCH;
    if (name == "delete_file") return IC_DELETE;
    if (name == "rename_file") return IC_EDIT;
    if (name == "workspace_info") return IC_INFO;
    return IC_FILE;
}

static const char* agent_tool_title(const std::string& name, bool done) {
    if (name == "run_command") return done ? "Ran command" : "Running command";
    if (name == "read_file") return done ? "Read file" : "Reading file";
    if (name == "write_file") return done ? "Wrote file" : "Writing file";
    if (name == "edit_file") return done ? "Edited file" : "Editing file";
    if (name == "list_dir") return done ? "Listed folder" : "Listing folder";
    if (name == "search_workspace") return done ? "Searched" : "Searching";
    if (name == "glob_files") return done ? "Found files" : "Finding files";
    if (name == "delete_file") return done ? "Deleted" : "Deleting";
    if (name == "rename_file") return done ? "Renamed" : "Renaming";
    if (name == "run_nexa") return done ? "Ran Nexa" : "Running Nexa";
    if (name == "workspace_info") return "Workspace";
    return name.c_str();
}

static std::string agent_tool_detail(const std::string& name, const std::string& args) {
    Json j;
    if (!args.empty()) json_parse(args, j);
    std::string s;
    if (name == "run_command") s = j.str("command");
    else if (name == "glob_files") s = j.str("pattern");
    else if (name == "search_workspace") s = j.str("query");
    else if (name == "rename_file") {
        s = path_filename(j.str("from"));
        if (!j.str("to").empty()) s += " → " + path_filename(j.str("to"));
    } else if (name == "run_nexa") {
        s = j.boolean("run", true) ? "NexaC --run" : "NexaC build";
    } else {
        s = j.str("path");
        if (!s.empty()) s = path_filename(s);
    }
    return s;
}

static int agent_tool_exit(const std::string& result) {
    if (result.rfind("exit ", 0) != 0) return -1;
    return std::atoi(result.c_str() + 5);
}

static void draw_agent_tool(const ChatMessage& call, const ChatMessage* result, size_t idx) {
    const std::string& name = call.tool_name.empty() && result ? result->tool_name : call.tool_name;
    const std::string& args = call.tool_args.empty() && result ? result->tool_args : call.tool_args;
    std::string id = call.tool_id.empty() ? ("tool" + std::to_string(idx)) : call.tool_id;
    const bool done = result != nullptr;
    const std::string& out = result ? result->content : call.content;
    const bool failed = done && (out.rfind("ERROR:", 0) == 0 || agent_tool_exit(out) > 0);
    const bool open = agent_tool_is_open(id);
    std::string detail = agent_tool_detail(name, args);
    const char* title = agent_tool_title(name, done);

    ImGui::PushID(id.c_str());

    const float row_h = dp(22.0f);
    const float chev_w = dp(16.0f);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float wrap = ImGui::GetContentRegionAvail().x;
    if (wrap < 40.0f) wrap = 40.0f;

    ImGui::InvisibleButton("##chip", ImVec2(wrap, row_h));
    bool hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) agent_tool_toggle(id);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg = hov ? IM_COL32(0x22, 0x22, 0x22, 255) : IM_COL32(0x18, 0x18, 0x18, 255);
    dl->AddRectFilled(origin, ImVec2(origin.x + wrap, origin.y + row_h), bg, 4.0f);

    float x = origin.x + 4.0f;
    float mid = origin.y + row_h * 0.5f;
    const char* chev = open ? IC_CHEV_D : IC_CHEV_R;
    if (g_app->font_icon) {
        ImGui::PushFont(g_app->font_icon);
        ImVec2 cs = ImGui::CalcTextSize(chev);
        dl->AddText(ImVec2(x + (chev_w - cs.x) * 0.5f, mid - cs.y * 0.5f), vs::subtle, chev);
        ImGui::PopFont();
    } else {
        dl->AddText(ImVec2(x, mid - ImGui::GetFontSize() * 0.5f), vs::subtle, open ? "v" : ">");
    }
    x += chev_w;

    const char* ic = agent_tool_icon(name);
    ImU32 ic_col = failed ? vs::error : (done ? IM_COL32(0x7A, 0xC8, 0x8A, 255) : vs::subtle);
    if (g_app->font_icon) {
        ImGui::PushFont(g_app->font_icon);
        ImVec2 isz = ImGui::CalcTextSize(ic);
        dl->AddText(ImVec2(x, mid - isz.y * 0.5f), ic_col, ic);
        ImGui::PopFont();
        x += isz.x + 6.0f;
    } else {
        x += 6.0f;
    }

    ImVec2 ts = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(x, mid - ts.y * 0.5f), vs::text_normal, title);
    x += ts.x + 8.0f;

    int exit_code = result ? agent_tool_exit(out) : -1;
    float right = origin.x + wrap - 8.0f;
    if (exit_code >= 0) {
        char ex[16];
        std::snprintf(ex, sizeof(ex), "%d", exit_code);
        ImVec2 es = ImGui::CalcTextSize(ex);
        right -= es.x;
        dl->AddText(ImVec2(right, mid - es.y * 0.5f),
                    exit_code == 0 ? IM_COL32(0x7A, 0xC8, 0x8A, 255) : vs::error, ex);
        right -= 8.0f;
    }

    if (!detail.empty() && x < right - 8.0f) {
        ImVec2 clip(right, origin.y + row_h);
        ImGui::RenderTextEllipsis(dl, ImVec2(x, mid - ts.y * 0.5f), clip, right, right,
                                  detail.c_str(), nullptr, nullptr);
    }

    if (open) {
        ImGui::Dummy(ImVec2(1, 4.0f));
        float box_w = wrap;
        std::string cmd = agent_tool_detail(name, args);
        std::string body;
        if (!cmd.empty() && (name == "run_command" || name == "run_nexa"))
            body += "$ " + cmd + "\n";
        else if (!args.empty() && name != "run_command")
            body += args + "\n";
        if (done && !out.empty()) {
            if (!body.empty()) body += "\n";
            body += out;
        } else if (!done) {
            body += "Running…";
        }
        ImFont* code = g_app->font_code ? g_app->font_code : ImGui::GetFont();
        ImGui::PushFont(code);
        float text_h = code->CalcTextSizeA(code->FontSize, box_w - 16.0f, box_w - 16.0f,
                                           body.c_str(), body.c_str() + body.size()).y;
        float box_h = text_h + 12.0f;
        if (box_h > 200.0f) box_h = 200.0f;
        if (box_h < 36.0f) box_h = 36.0f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.07f, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
        ImGui::BeginChild("##out", ImVec2(box_w, box_h), ImGuiChildFlags_Borders);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.70f, 0.68f, 1));
        ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + box_w - 20.0f);
        ImGui::TextUnformatted(body.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    ImGui::Dummy(ImVec2(1, 4.0f));
    ImGui::PopID();
}

static void settings_label(const char* text) {
    ImGui::Dummy(ImVec2(1, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.72f, 1));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(1, 2.0f));
}

void draw_agent_settings() {
    if (g_app->font_agent) ImGui::PushFont(g_app->font_agent);
    AgentSettings& a = g_app->settings.agent;
    ProviderSlot& slot = a.current();
    bool dirty = false;

    settings_label("Provider");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##sprov", provider_name(a.provider))) {
        AgentProvider all[] = {
            AgentProvider::OpenAI, AgentProvider::Anthropic, AgentProvider::OpenRouter,
            AgentProvider::Groq, AgentProvider::Ollama, AgentProvider::Custom
        };
        for (AgentProvider p : all) {
            bool sel = a.provider == p;
            if (ImGui::Selectable(provider_name(p), sel)) {
                a.provider = p;
                apply_provider_defaults(a);
                g_app->extra_models.clear();
                g_app->models_listed = false;
                agent_refresh_models();
                dirty = true;
            }
        }
        ImGui::EndCombo();
    }

    settings_label("Model");
    std::vector<std::pair<std::string, std::string>> models;
    provider_models(a.provider, models);
    if (!g_app->models_listed && !slot.api_key.empty()) agent_refresh_models();
    std::string model_label = slot.custom_model.empty() ? slot.model : slot.custom_model;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##smodel", model_label.c_str())) {
        for (const auto& m : models) {
            bool sel = slot.model == m.first && slot.custom_model.empty();
            if (ImGui::Selectable(m.second.c_str(), sel)) {
                slot.model = m.first;
                slot.custom_model.clear();
                dirty = true;
            }
        }
        if (!g_app->extra_models.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("From provider");
            for (const auto& id : g_app->extra_models) {
                bool listed = false;
                for (const auto& m : models) {
                    if (m.first == id) { listed = true; break; }
                }
                if (listed) continue;
                bool sel = slot.model == id && slot.custom_model.empty();
                if (ImGui::Selectable(id.c_str(), sel)) {
                    slot.model = id;
                    slot.custom_model.clear();
                    dirty = true;
                }
            }
        }
        ImGui::Separator();
        ImGui::TextDisabled("Custom model id");
        char custom[128];
        std::snprintf(custom, sizeof(custom), "%s", slot.custom_model.c_str());
        if (ImGui::InputText("##scmodel", custom, sizeof(custom))) {
            slot.custom_model = custom;
            dirty = true;
        }
        ImGui::EndCombo();
    }

    settings_label("API key");
    char key[256];
    std::snprintf(key, sizeof(key), "%s", slot.api_key.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##skey", "Key for this provider only",
                                 key, sizeof(key), ImGuiInputTextFlags_Password)) {
        slot.api_key = key;
        g_app->models_listed = false;
        dirty = true;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1));
    ImGui::TextWrapped("Stored per provider. Switching to Groq, OpenAI, etc. keeps each key separate. Empty falls back to that provider's env var.");
    ImGui::PopStyleColor();

    settings_label("Base URL");
    char url[256];
    std::snprintf(url, sizeof(url), "%s", slot.base_url.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##surl", "https://api.example.com/v1", url, sizeof(url))) {
        slot.base_url = url;
        dirty = true;
    }

    settings_label("Default mode");
    ImGui::SetNextItemWidth(-FLT_MIN);
    const char* mode = a.mode == AgentMode::Agent ? "Agent  —  can edit files and run commands" : "Chat  —  answers only";
    if (ImGui::BeginCombo("##smode", mode)) {
        if (ImGui::Selectable("Agent  —  can edit files and run commands", a.mode == AgentMode::Agent)) {
            a.mode = AgentMode::Agent;
            dirty = true;
        }
        if (ImGui::Selectable("Chat  —  answers only", a.mode == AgentMode::Chat)) {
            a.mode = AgentMode::Chat;
            dirty = true;
        }
        ImGui::EndCombo();
    }

    settings_label("Temperature");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat("##stemp", &a.temperature, 0.0f, 1.0f, "%.2f")) dirty = true;

    if (dirty) save_settings(g_app->settings);
    if (g_app->font_agent) ImGui::PopFont();
}

static bool agent_icon_hit(const char* id, const char* icon, const char* tip, ImVec2 c, float bw, ImDrawList* dl) {
    ImGui::SetCursorScreenPos(c);
    ImGui::InvisibleButton(id, ImVec2(bw, bw));
    bool hov = ImGui::IsItemHovered();
    bool clk = ImGui::IsItemClicked();
    if (hov) {
        dl->AddRectFilled(ImVec2(c.x + 1, c.y + 1), ImVec2(c.x + bw - 1, c.y + bw - 1),
                          vs::activity_hover, 4.0f);
        ImGui::SetTooltip("%s", tip);
    }
    if (g_app->font_icon) ImGui::PushFont(g_app->font_icon);
    ImVec2 ts = ImGui::CalcTextSize(icon);
    dl->AddText(ImVec2(c.x + (bw - ts.x) * 0.5f, c.y + (bw - ts.y) * 0.5f),
                hov ? vs::text_bright : vs::heading_action, icon);
    if (g_app->font_icon) ImGui::PopFont();
    return clk;
}

static void draw_agent_bubble(bool user, const std::string& text, int id) {
    float wrap = ImGui::GetContentRegionAvail().x;
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, user
        ? ImVec4(0.078f, 0.157f, 0.227f, 1)
        : ImVec4(0.086f, 0.086f, 0.086f, 1));
    ImGui::PushStyleColor(ImGuiCol_Border, user
        ? ImVec4(0.118f, 0.235f, 0.337f, 1)
        : ImVec4(0.157f, 0.157f, 0.157f, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::BeginChild("bub", ImVec2(wrap, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders |
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImU32 col = user ? IM_COL32(0xC8, 0xD8, 0xE8, 255) : IM_COL32(0xE0, 0xDC, 0xD0, 255);
    if (g_app->font_agent) ImGui::PushFont(g_app->font_agent);
    draw_markdown(text, col);
    if (g_app->font_agent) ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PopID();
}

static void draw_agent_panel() {
    if (g_app->font_agent) ImGui::PushFont(g_app->font_agent);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(dp(10.0f), dp(7.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(dp(8.0f), dp(6.0f)));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), vs::sidebar_bg);
    dl->AddLine(ImVec2(pos.x + 0.5f, pos.y), ImVec2(pos.x + 0.5f, pos.y + size.y), vs::border);

    AgentSettings& a = g_app->settings.agent;
    // Same 35px uppercase header as the sidebar sections.
    const float hdr_h = dp(35.0f);
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + hdr_h), vs::sidebar_bg);
    {
        ImFont* small = g_app->font_ui_small ? g_app->font_ui_small : ImGui::GetFont();
        dl->AddText(small, small->FontSize, ImVec2(pos.x + dp(20.0f), pos.y + (hdr_h - small->FontSize) * 0.5f),
                    vs::heading, "AGENT");
    }

    const float bw = dp(22.0f);
    float bx = pos.x + size.x - 8.0f - bw;
    if (agent_icon_hit("agnew", IC_ADD, "New chat", ImVec2(bx, pos.y + (hdr_h - bw) * 0.5f), bw, dl))
        agent_new_chat();
    bx -= bw + 2.0f;
    if (agent_icon_hit("agset", IC_SETTINGS, "Agent settings", ImVec2(bx, pos.y + (hdr_h - bw) * 0.5f), bw, dl)) {
        g_app->activity = ActivityView::Settings;
        g_app->settings.show_sidebar = true;
    }

    // Mode pill
    {
        const char* modes[2] = {"Agent", "Chat"};
        float pill_w = dp(52.0f);
        float pill_h = dp(22.0f);
        float px0 = bx - 8.0f - pill_w * 2.0f;
        float py0 = pos.y + (hdr_h - pill_h) * 0.5f;
        for (int i = 0; i < 2; i++) {
            bool on = (i == 0) ? (a.mode == AgentMode::Agent) : (a.mode == AgentMode::Chat);
            ImVec2 r0(px0 + pill_w * (float)i, py0);
            ImVec2 r1(r0.x + pill_w, r0.y + pill_h);
            ImGui::SetCursorScreenPos(r0);
            ImGui::InvisibleButton(i == 0 ? "##modeA" : "##modeC", ImVec2(pill_w, pill_h));
            if (ImGui::IsItemClicked()) {
                a.mode = (i == 0) ? AgentMode::Agent : AgentMode::Chat;
                save_settings(g_app->settings);
            }
            bool hov = ImGui::IsItemHovered();
            ImU32 bg = on ? vs::accent
                          : (hov ? IM_COL32(0x2A, 0x2A, 0x2A, 255) : IM_COL32(0x1F, 0x1F, 0x1F, 255));
            float rnd = (i == 0) ? 4.0f : 4.0f;
            dl->AddRectFilled(r0, r1, bg, rnd);
            ImVec2 ts = ImGui::CalcTextSize(modes[i]);
            dl->AddText(ImVec2(r0.x + (pill_w - ts.x) * 0.5f, r0.y + (pill_h - ts.y) * 0.5f),
                        on ? IM_COL32(0xF0, 0xF0, 0xF0, 255) : vs::subtle, modes[i]);
        }
        dl->AddRect(ImVec2(px0, py0), ImVec2(px0 + pill_w * 2.0f, py0 + pill_h),
                    IM_COL32(0x3C, 0x3C, 0x3C, 255), 4.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x + 12, pos.y + hdr_h + 8));

    std::string model = a.current().custom_model.empty() ? a.current().model : a.current().custom_model;
    {
        ImVec2 c = ImGui::GetCursorScreenPos();
        float chip_h = dp(22.0f);
        std::string chip = std::string(provider_name(a.provider)) + "  ·  " + model;
        ImVec2 ts = ImGui::CalcTextSize(chip.c_str());
        float chip_w = ts.x + 16.0f;
        if (chip_w > size.x - 24.0f) chip_w = size.x - 24.0f;
        ImGui::InvisibleButton("##modelchip", ImVec2(chip_w, chip_h));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open agent settings");
        if (ImGui::IsItemClicked()) {
            g_app->activity = ActivityView::Settings;
            g_app->settings.show_sidebar = true;
        }
        dl->AddRectFilled(c, ImVec2(c.x + chip_w, c.y + chip_h), IM_COL32(0x18, 0x18, 0x18, 255), 11.0f);
        dl->AddRect(c, ImVec2(c.x + chip_w, c.y + chip_h), IM_COL32(0x2A, 0x2A, 0x2A, 255), 11.0f);
        ImGui::RenderTextEllipsis(dl, ImVec2(c.x + 8, c.y + (chip_h - ts.y) * 0.5f),
                                  ImVec2(c.x + chip_w - 6, c.y + chip_h),
                                  c.x + chip_w - 6, c.x + chip_w - 6, chip.c_str(), nullptr, nullptr);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 12, c.y + chip_h + 8));
    }

    const float composer_h = dp(118.0f);
    ImGui::BeginChild("agent_msgs", ImVec2(0, -composer_h), ImGuiChildFlags_None);
    std::vector<ChatMessage> snap;
    std::string status, error;
    {
        std::lock_guard<std::mutex> lock(g_app->agent.mu);
        snap = g_app->agent.messages;
        status = g_app->agent.status;
        error = g_app->agent.error;
    }
    if (snap.empty()) {
        ImGui::Dummy(ImVec2(1, 28.0f));
        if (g_app->font_icon) ImGui::PushFont(g_app->font_icon);
        ImVec2 ic = ImGui::CalcTextSize(IC_SPARKLE);
        float cx = ImGui::GetCursorScreenPos().x + (ImGui::GetContentRegionAvail().x - ic.x) * 0.5f;
        dl->AddText(ImVec2(cx, ImGui::GetCursorScreenPos().y), IM_COL32(0x6C, 0xB6, 0xFF, 180), IC_SPARKLE);
        if (g_app->font_icon) ImGui::PopFont();
        ImGui::Dummy(ImVec2(1, ic.y + 10.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.82f, 1));
        ImGui::TextWrapped(a.mode == AgentMode::Agent
            ? "Ask Nexium to write, edit, search, or run commands in this folder."
            : "Ask Nexium about Nexa. Switch to Agent mode to let it change files and run commands.");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(1, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1));
        if (a.current().api_key.empty())
            ImGui::TextWrapped("No API key yet — open Settings (gear) to add one.");
        else
            ImGui::TextWrapped("Provider and keys live in Settings. This panel is just the conversation.");
        ImGui::PopStyleColor();
    }
    for (size_t i = 0; i < snap.size(); i++) {
        const ChatMessage& m = snap[i];
        if (m.role == "tool") continue;
        if (!m.tool_name.empty()) {
            if (!m.content.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.62f, 0.62f, 1));
                if (g_app->font_ui_small) ImGui::PushFont(g_app->font_ui_small);
                ImGui::TextUnformatted("Nexium");
                if (g_app->font_ui_small) ImGui::PopFont();
                ImGui::PopStyleColor();
                ImGui::Dummy(ImVec2(1, 3.0f));
                draw_agent_bubble(false, m.content, (int)i);
                ImGui::Dummy(ImVec2(1, 6.0f));
            }
            const ChatMessage* result = nullptr;
            if (i + 1 < snap.size() && snap[i + 1].role == "tool" &&
                (snap[i + 1].tool_id == m.tool_id || m.tool_id.empty())) {
                result = &snap[i + 1];
                i++;
            }
            draw_agent_tool(m, result, i);
            continue;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1));
        if (g_app->font_ui_small) ImGui::PushFont(g_app->font_ui_small);
        ImGui::TextUnformatted(m.role == "user" ? "You" : "Nexium");
        if (g_app->font_ui_small) ImGui::PopFont();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(1, 3.0f));
        draw_agent_bubble(m.role == "user", m.content, (int)i);
        ImGui::Dummy(ImVec2(1, 10.0f));
    }
    if (!status.empty()) {
        ImGui::Dummy(ImVec2(1, 4.0f));
        ImGui::TextDisabled("%s", status.c_str());
    }
    if (!error.empty()) {
        ImGui::Dummy(ImVec2(1, 4.0f));
        draw_agent_bubble(false, error, -2);
    }
    if (g_app->agent.busy) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(1, 4.0f));
    bool busy = g_app->agent.busy;
    ImVec2 box0 = ImGui::GetCursorScreenPos();
    float box_w = ImGui::GetContentRegionAvail().x;
    float box_h = composer_h - 12.0f;
    dl->AddRectFilled(box0, ImVec2(box0.x + box_w, box0.y + box_h), IM_COL32(0x14, 0x14, 0x14, 255), 8.0f);
    dl->AddRect(box0, ImVec2(box0.x + box_w, box0.y + box_h), IM_COL32(0x2E, 0x2E, 0x2E, 255), 8.0f);

    ImGui::SetCursorScreenPos(ImVec2(box0.x + 8, box0.y + 6));
    ImGui::BeginDisabled(busy);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CtrlEnterForNewLine | ImGuiInputTextFlags_EnterReturnsTrue;
    bool submitted = ImGui::InputTextMultiline("##agent_in", g_app->agent_input, sizeof(g_app->agent_input),
                                               ImVec2(box_w - 16.0f, box_h - dp(40.0f)), flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemActive()) {
        g_app->editor_focused = false;
        g_app->terminal_focused = false;
    }
    if (submitted && !ImGui::GetIO().KeyShift) agent_send();
    ImGui::EndDisabled();

    float send_h = dp(28.0f);
    ImGui::SetCursorScreenPos(ImVec2(box0.x + 8, box0.y + box_h - send_h - 6));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1));
    if (g_app->font_ui_small) ImGui::PushFont(g_app->font_ui_small);
    ImGui::TextUnformatted(a.mode == AgentMode::Agent ? "Enter to send  ·  Shift+Enter for a new line" : "Chat mode  ·  Enter to send");
    if (g_app->font_ui_small) ImGui::PopFont();
    ImGui::PopStyleColor();

    float send_w = dp(86.0f);
    ImGui::SetCursorScreenPos(ImVec2(box0.x + box_w - send_w - 8, box0.y + box_h - send_h - 6));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.00f, 0.31f, 0.53f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.00f, 0.38f, 0.64f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.00f, 0.26f, 0.45f, 1));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1, 1, 1, 1));
    if (busy) {
        if (ImGui::Button(IC_STOP "  Stop", ImVec2(send_w, send_h))) agent_stop();
    } else {
        if (ImGui::Button(IC_SEND "  Send", ImVec2(send_w, send_h))) agent_send();
    }
    ImGui::PopStyleColor(4);

    ImGui::SetCursorScreenPos(ImVec2(box0.x, box0.y + box_h));
    ImGui::Dummy(ImVec2(box_w, 4.0f));
    ImGui::PopStyleVar(2);
    if (g_app->font_agent) ImGui::PopFont();
}

// ---------------------------------------------------------------------------
// Bottom panel
// ---------------------------------------------------------------------------
static void draw_bottom() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), vs::panel_bg);

    const float hdr_h = dp(35.0f);
    struct Tab { const char* label; BottomTab id; };
    const Tab tabs[] = {
        {"PROBLEMS", BottomTab::Problems},
        {"OUTPUT",   BottomTab::Output},
        {"TERMINAL", BottomTab::Terminal},
    };
    ImFont* small = g_app->font_ui_small ? g_app->font_ui_small : ImGui::GetFont();
    float tx = pos.x + dp(10.0f);
    for (const Tab& t : tabs) {
        bool on = g_app->bottom == t.id;
        std::string badge;
        if (t.id == BottomTab::Problems && !g_app->problems.empty()) badge = std::to_string(g_app->problems.size());
        ImVec2 ls = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, t.label);
        float bw = badge.empty() ? 0.0f : small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, badge.c_str()).x + dp(12.0f);
        float w = ls.x + (badge.empty() ? 0.0f : bw + dp(6.0f)) + dp(20.0f);
        ImGui::PushID(t.label);
        ImGui::SetCursorScreenPos(ImVec2(tx, pos.y));
        ImGui::InvisibleButton("##btab", ImVec2(w, hdr_h));
        if (ImGui::IsItemClicked()) g_app->bottom = t.id;
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        float lx = tx + dp(10.0f);
        float ly = pos.y + (hdr_h - small->FontSize) * 0.5f;
        dl->AddText(small, small->FontSize, ImVec2(lx, ly),
                    on ? vs::tab_text_active : (hov ? vs::text_normal : vs::tab_text_dim), t.label);
        if (!badge.empty()) {
            float bx = lx + ls.x + dp(6.0f);
            float bh = small->FontSize + dp(2.0f);
            dl->AddRectFilled(ImVec2(bx, ly - dp(1.0f)), ImVec2(bx + bw, ly - dp(1.0f) + bh),
                              IM_COL32(0x61, 0x61, 0x61, 255), bh * 0.5f);
            dl->AddText(small, small->FontSize, ImVec2(bx + dp(6.0f), ly), IM_COL32(0xFF, 0xFF, 0xFF, 255), badge.c_str());
        }
        if (on) {
            dl->AddRectFilled(ImVec2(lx, pos.y + hdr_h - dp(6.0f)), ImVec2(tx + w - dp(10.0f), pos.y + hdr_h - dp(5.0f)),
                              vs::tab_text_active);
        }
        tx += w;
    }

    const float bw = dp(22.0f);
    float bx = pos.x + size.x - dp(8.0f) - bw;
    auto icon_hit = [&](const char* id, const char* icon, const char* tip) -> bool {
        float by = pos.y + (hdr_h - bw) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(bx, by));
        ImGui::InvisibleButton(id, ImVec2(bw, bw));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        if (hov) {
            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bw), IM_COL32(0x5A, 0x5D, 0x5E, 0x50), 4.0f);
            ImGui::SetTooltip("%s", tip);
        }
        if (g_app->font_icon) ImGui::PushFont(g_app->font_icon);
        ImVec2 ts = ImGui::CalcTextSize(icon);
        dl->AddText(ImVec2(bx + (bw - ts.x) * 0.5f, by + (bw - ts.y) * 0.5f),
                    hov ? vs::text_bright : vs::heading_action, icon);
        if (g_app->font_icon) ImGui::PopFont();
        bx -= bw + dp(4.0f);
        return clk;
    };
    if (icon_hit("##hide", IC_CLOSE_SMALL, "Close Panel (Ctrl+J)")) g_app->settings.show_panel = false;
    if (g_app->proc.running && !g_app->proc.is_shell) {
        if (icon_hit("##kill", IC_STOP, "Stop (Shift+F5)")) proc_stop();
    } else if (g_app->bottom == BottomTab::Terminal) {
        if (icon_hit("##clear", IC_DELETE, "Clear Terminal")) term_clear();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x + dp(12.0f), pos.y + hdr_h));
    ImVec2 body = ImVec2(size.x - dp(16.0f), size.y - hdr_h - dp(4.0f));
    ImGui::BeginChild("bottom_body", body, ImGuiChildFlags_None);
    if (g_app->bottom == BottomTab::Terminal) {
        draw_terminal(body);
    } else if (g_app->bottom == BottomTab::Problems) {
        draw_problems();
    } else {
        // Output: environment facts in the log style of VS Code's output channel.
        static std::string compiler;
        static double checked = -100.0;
        if (ImGui::GetTime() - checked > 5.0) {
            compiler = nexa_compiler();
            checked = ImGui::GetTime();
        }
        if (g_app->font_code) ImGui::PushFont(g_app->font_code);
        ImGui::Dummy(ImVec2(1, 4));
        auto row = [](const char* key, const std::string& value) {
            ImGui::TextColored(ImVec4(0.34f, 0.61f, 0.84f, 1), "[info]");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.61f, 0.61f, 0.61f, 1), "%-10s", key);
            ImGui::SameLine();
            ImGui::TextUnformatted(value.c_str());
        };
        row("compiler", which_exe(compiler).empty() ? compiler + "  (not found on PATH)" : which_exe(compiler));
        row("folder", g_app->settings.folder.empty() ? std::string("(none)") : g_app->settings.folder);
        row("settings", settings_path());
        if (g_app->active >= 0) {
            const TextBuffer& b = g_app->buffers[(size_t)g_app->active];
            row("file", b.untitled ? b.name : b.path);
            row("size", std::to_string((int)b.lines.size()) + " lines, " + std::to_string((int)b.text.size()) + " bytes, " +
                            lang_name(b.lang) + (b.crlf ? ", CRLF" : ", LF"));
        }
        if (g_app->font_code) ImGui::PopFont();
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------
static void draw_status(float y, float w) {
    ImFont* font = g_app->font_ui_small ? g_app->font_ui_small : ImGui::GetFont();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 pos = ImGui::GetMainViewport()->WorkPos;
    const float h = dp(22.0f);
    ImVec2 rmin(pos.x, pos.y + y);
    ImVec2 rmax(pos.x + w, pos.y + y + h);
    dl->AddRectFilled(rmin, rmax, vs::status_bg);
    dl->AddLine(ImVec2(rmin.x, rmin.y + 0.5f), ImVec2(rmax.x, rmin.y + 0.5f), vs::border);

    const char* lang = "Plain Text";
    int line = 1, col = 1;
    int selected = 0;
    int selected_lines = 0;
    bool crlf = false;
    bool has_buffer = g_app->active >= 0 && g_app->active < (int)g_app->buffers.size();
    if (has_buffer) {
        const TextBuffer& b = g_app->buffers[(size_t)g_app->active];
        lang = lang_name(b.lang);
        if (b.is_json && b.name == "nexapkg.json") lang = "Nexa Package";
        line = buffer_line_at(b, b.cursor) + 1;
        col = buffer_vcol_at(b, b.cursor) + 1;
        selected = std::abs(b.cursor - b.sel_anchor);
        if (selected) {
            selected_lines = std::abs(buffer_line_at(b, b.cursor) - buffer_line_at(b, b.sel_anchor)) + 1;
        }
        crlf = b.crlf;
    }
    int n_err = 0, n_warn = 0;
    for (const auto& d : g_app->problems) {
        if (d.error) n_err++;
        else n_warn++;
    }

    const float pad = dp(8.0f);
    const float ty = rmin.y + (h - font->FontSize) * 0.5f;
    ImFont* icons = g_app->font_icon ? g_app->font_icon : font;
    const float isz = font->FontSize;
    // One status item: optional icon + text, hover highlight, returns clicked.
    auto item = [&](float x, const char* icon, const std::string& text, bool right, ImU32 bg) -> float {
        float iw = icon ? icons->CalcTextSizeA(isz, FLT_MAX, 0.0f, icon).x + dp(4.0f) : 0.0f;
        float tw = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text.c_str()).x;
        float wdt = iw + tw + pad * 2.0f;
        float x0 = right ? x - wdt : x;
        ImVec2 a(x0, rmin.y + 1.0f), z(x0 + wdt, rmax.y);
        bool hov = ImGui::IsMouseHoveringRect(a, z, false);
        if (bg) dl->AddRectFilled(a, z, bg);
        else if (hov) dl->AddRectFilled(a, z, vs::status_hover);
        float cx = x0 + pad;
        if (icon) {
            dl->AddText(icons, isz, ImVec2(cx, rmin.y + (h - isz) * 0.5f), bg ? IM_COL32_WHITE : vs::status_text, icon);
            cx += iw;
        }
        dl->AddText(font, font->FontSize, ImVec2(cx, ty), bg ? IM_COL32_WHITE : vs::status_text, text.c_str());
        return right ? x0 : x0 + wdt;
    };

    float x = rmin.x;
    x = item(x, IC_TERMINAL, "Nexium", false, vs::accent);
    if (!g_app->settings.folder.empty()) x = item(x, IC_FOLDER, path_filename(g_app->settings.folder), false, 0);
    x = item(x, IC_ERROR_S, std::to_string(n_err), false, 0);
    x = item(x - pad, IC_WARN_S, std::to_string(n_warn), false, 0);
    if (g_app->proc.running && !g_app->proc.is_shell) x = item(x, IC_RUN, "Running", false, 0);

    float rx = rmax.x - dp(4.0f);
    {
        const AgentSettings& a = g_app->settings.agent;
        std::string model = a.current().custom_model.empty() ? a.current().model : a.current().custom_model;
        rx = item(rx, IC_SPARKLE, model, true, 0);
    }
    rx = item(rx, nullptr, lang, true, 0);
    if (has_buffer) {
        rx = item(rx, nullptr, crlf ? "CRLF" : "LF", true, 0);
        rx = item(rx, nullptr, "UTF-8", true, 0);
        rx = item(rx, nullptr, "Spaces: 4", true, 0);
        char b[128];
        if (selected && selected_lines > 1)
            std::snprintf(b, sizeof(b), "Ln %d, Col %d (%d selected, %d lines)", line, col, selected, selected_lines);
        else if (selected)
            std::snprintf(b, sizeof(b), "Ln %d, Col %d (%d selected)", line, col, selected);
        else
            std::snprintf(b, sizeof(b), "Ln %d, Col %d", line, col);
        rx = item(rx, nullptr, b, true, 0);
    }
}

// ---------------------------------------------------------------------------
// Custom title bar with integrated menu + window controls (VS Code style)
// ---------------------------------------------------------------------------
static void menu_file();
static void menu_edit();
static void menu_selection();
static void menu_go();
static void menu_view();
static void menu_run();
static void menu_agent();

struct TitleMenu {
    const char* label;
    void (*draw)();
};
static const TitleMenu kTitleMenus[] = {
    {"File",  &menu_file},
    {"Edit",  &menu_edit},
    {"Selection", &menu_selection},
    {"View",  &menu_view},
    {"Go",    &menu_go},
    {"Run",   &menu_run},
    {"Agent", &menu_agent},
};

static void menu_file() {
    if (ImGui::MenuItem("New File", "Ctrl+N")) new_untitled();
    if (ImGui::MenuItem("Open File...", "Ctrl+O")) {
        std::string p;
        if (open_file_dialog(p)) open_path(p, false);
    }
    if (ImGui::MenuItem("Open Folder...")) {
        std::string f;
        if (open_folder_dialog(f)) set_folder(f);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Save", "Ctrl+S")) {
        if (g_app->active >= 0) save_buffer(g_app->active);
    }
    if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
        if (g_app->active >= 0) save_buffer(g_app->active, true);
    }
    if (ImGui::MenuItem("Save All")) save_all();
    ImGui::Separator();
    if (ImGui::MenuItem("Close Editor", "Ctrl+W")) {
        if (g_app->active >= 0) close_buffer(g_app->active);
    }
    if (ImGui::MenuItem("Exit")) g_app->want_quit = true;
}

static void menu_edit() {
    const bool has = g_app->active >= 0;
    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, has)) editor_exec(EdCmd::Undo);
    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, has)) editor_exec(EdCmd::Redo);
    ImGui::Separator();
    if (ImGui::MenuItem("Cut", "Ctrl+X", false, has)) editor_exec(EdCmd::Cut);
    if (ImGui::MenuItem("Copy", "Ctrl+C", false, has)) editor_exec(EdCmd::Copy);
    if (ImGui::MenuItem("Paste", "Ctrl+V", false, has)) editor_exec(EdCmd::Paste);
    ImGui::Separator();
    if (ImGui::MenuItem("Find", "Ctrl+F", false, has)) g_app->find_open = g_app->find_focus = true;
    if (ImGui::MenuItem("Replace", "Ctrl+H", false, has)) {
        g_app->find_open = true;
        g_app->replace_open = true;
        g_app->find_focus = true;
    }
    if (ImGui::MenuItem("Find Next", "F3", false, has)) editor_exec(EdCmd::FindNext);
    if (ImGui::MenuItem("Find Previous", "Shift+F3", false, has)) editor_exec(EdCmd::FindPrev);
    ImGui::Separator();
    if (ImGui::MenuItem("Toggle Line Comment", "Ctrl+/", false, has)) editor_exec(EdCmd::ToggleComment);
    if (ImGui::MenuItem("Trim Trailing Whitespace", nullptr, false, has)) editor_exec(EdCmd::TrimTrailingWhitespace);
}

static void menu_selection() {
    const bool has = g_app->active >= 0;
    if (ImGui::MenuItem("Select All", "Ctrl+A", false, has)) editor_exec(EdCmd::SelectAll);
    if (ImGui::MenuItem("Select Line", "Ctrl+L", false, has)) editor_exec(EdCmd::SelectLine);
    ImGui::Separator();
    if (ImGui::MenuItem("Copy Line Up", "Shift+Alt+Up", false, has)) editor_exec(EdCmd::CopyLineUp);
    if (ImGui::MenuItem("Copy Line Down", "Shift+Alt+Down", false, has)) editor_exec(EdCmd::CopyLineDown);
    if (ImGui::MenuItem("Move Line Up", "Alt+Up", false, has)) editor_exec(EdCmd::MoveLineUp);
    if (ImGui::MenuItem("Move Line Down", "Alt+Down", false, has)) editor_exec(EdCmd::MoveLineDown);
    if (ImGui::MenuItem("Duplicate Line", "Ctrl+D", false, has)) editor_exec(EdCmd::DuplicateLine);
    if (ImGui::MenuItem("Delete Line", "Ctrl+Shift+K", false, has)) editor_exec(EdCmd::DeleteLine);
    ImGui::Separator();
    if (ImGui::MenuItem("Indent", "Ctrl+]", false, has)) editor_exec(EdCmd::Indent);
    if (ImGui::MenuItem("Outdent", "Ctrl+[", false, has)) editor_exec(EdCmd::Outdent);
    if (ImGui::MenuItem("Transform to Uppercase", nullptr, false, has)) editor_exec(EdCmd::UpperCase);
    if (ImGui::MenuItem("Transform to Lowercase", nullptr, false, has)) editor_exec(EdCmd::LowerCase);
}

static void open_quick(const char* prefix);

static void menu_go() {
    const bool has = g_app->active >= 0;
    if (ImGui::MenuItem("Go to File...", "Ctrl+P")) open_quick("");
    if (ImGui::MenuItem("Go to Symbol in Editor...", "Ctrl+Shift+O", false, has)) open_quick("@");
    if (ImGui::MenuItem("Go to Line...", "Ctrl+G", false, has)) g_app->goto_open = true;
    ImGui::Separator();
    if (ImGui::MenuItem("Go to Definition", "F12", false, has)) editor_exec(EdCmd::GotoDefinition);
    if (ImGui::MenuItem("Go to Bracket", "Ctrl+Shift+\\", false, has)) editor_exec(EdCmd::JumpToBracket);
    ImGui::Separator();
    int n = (int)g_app->buffers.size();
    if (ImGui::MenuItem("Next Editor", "Ctrl+Tab", false, n > 1)) g_app->active = (g_app->active + 1) % n;
    if (ImGui::MenuItem("Previous Editor", "Ctrl+Shift+Tab", false, n > 1)) g_app->active = (g_app->active - 1 + n) % n;
}

static void menu_view() {
    if (ImGui::MenuItem("Command Palette", "Ctrl+Shift+P")) g_app->palette_open = true;
    if (ImGui::MenuItem("Go to File", "Ctrl+P")) open_quick("");
    ImGui::Separator();
    ImGui::MenuItem("Sidebar", "Ctrl+B", &g_app->settings.show_sidebar);
    ImGui::MenuItem("Panel", "Ctrl+J", &g_app->settings.show_panel);
    ImGui::MenuItem("Agent", "Ctrl+Shift+L", &g_app->settings.show_agent);
}

static void menu_run() {
    if (ImGui::MenuItem("Run", "F5")) run_active(true);
    if (ImGui::MenuItem("Build", "Ctrl+Alt+B")) run_active(false);
    if (ImGui::MenuItem("Stop") && g_app->proc.running) proc_stop();
}

static void menu_agent() {
    if (ImGui::MenuItem("New Chat")) agent_new_chat();
    ImGui::MenuItem("Show Agent Panel", nullptr, &g_app->settings.show_agent);
}

// title-bar action helpers (via SendMessage to WM_SYSCOMMAND)
static void title_minimize() { ShowWindow(g_app->hwnd, SW_MINIMIZE); }
static void title_toggle_max() {
    if (IsZoomed(g_app->hwnd)) ShowWindow(g_app->hwnd, SW_RESTORE);
    else                       ShowWindow(g_app->hwnd, SW_MAXIMIZE);
    g_app->maximized = IsZoomed(g_app->hwnd) != 0;
}
static void title_close() { PostMessageW(g_app->hwnd, WM_CLOSE, 0, 0); }

static float draw_title_bar(float w) {
    const float h = dp(32.0f);
    g_app->title_hover_blocks_drag = false;
    g_app->maximized = IsZoomed(g_app->hwnd) != 0;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 rmin = origin;
    ImVec2 rmax = ImVec2(origin.x + w, origin.y + h);

    dl->AddRectFilled(rmin, rmax, vs::sidebar_bg);

    const float ic_pad = 8.0f;
    const float ic_sz  = 16.0f;
    ImVec2 iic(rmin.x + ic_pad, rmin.y + (h - ic_sz) * 0.5f);
    draw_app_icon(dl, iic, ic_sz);

    // ---- Menu items ----
    float mx = rmin.x + ic_pad + ic_sz + 12.0f;
    int hovered_menu = -1;
    int clicked_menu = -1;
    const int n_menus = (int)(sizeof(kTitleMenus) / sizeof(kTitleMenus[0]));
    struct MenuRect { float x; float w; };
    MenuRect rects[8];
    for (int i = 0; i < n_menus; i++) {
        const TitleMenu& m = kTitleMenus[i];
        float mw = ImGui::CalcTextSize(m.label).x + 16.0f;
        rects[i] = {mx, mw};
        ImGui::SetCursorScreenPos(ImVec2(mx, rmin.y));
        ImGui::PushID(i);
        ImGui::InvisibleButton("mi", ImVec2(mw, h));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        if (hov) { hovered_menu = i; g_app->title_hover_blocks_drag = true; }
        if (clk) clicked_menu = i;
        bool is_open = g_app->open_menu == i;
        // If a menu is already open, hovering another opens it (VS Code-like behavior)
        if (g_app->open_menu >= 0 && hov && !is_open) clicked_menu = i;
        if (hov || is_open) {
            dl->AddRectFilled(ImVec2(mx + 1, rmin.y + dp(5.0f)),
                              ImVec2(mx + mw - 1, rmax.y - dp(5.0f)),
                              IM_COL32(0x5A, 0x5D, 0x5E, is_open ? 0x70 : 0x50), 4.0f);
        }
        ImU32 col = IM_COL32(0xCC, 0xCC, 0xCC, 0xFF);
        dl->AddText(ImVec2(mx + 8, rmin.y + (h - ImGui::GetFontSize()) * 0.5f),
                    col, m.label);
        ImGui::PopID();
        mx += mw;
    }
    if (clicked_menu >= 0) g_app->want_open_menu = clicked_menu;

    // ---- Center: command center (opens quick open, like VS Code) ----
    {
        std::string label = "Nexium";
        if (!g_app->settings.folder.empty()) label = path_filename(g_app->settings.folder);
        if (g_app->active >= 0 && g_app->buffers[(size_t)g_app->active].dirty) label = "\xE2\x97\x8F " + label;
        const float cw = std::min(dp(520.0f), w * 0.38f);
        const float ch = h - dp(10.0f);
        ImVec2 c0(rmin.x + (w - cw) * 0.5f, rmin.y + (h - ch) * 0.5f);
        ImVec2 c1(c0.x + cw, c0.y + ch);
        // Only when it clears the menus on the left.
        if (c0.x > mx + dp(12.0f)) {
            ImGui::SetCursorScreenPos(c0);
            ImGui::InvisibleButton("##cmdcenter", ImVec2(cw, ch));
            bool hov = ImGui::IsItemHovered();
            if (hov) {
                g_app->title_hover_blocks_drag = true;
                ImGui::SetTooltip("Search files by name (Ctrl+P)");
            }
            if (ImGui::IsItemClicked()) open_quick("");
            dl->AddRectFilled(c0, c1, hov ? IM_COL32(0x2A, 0x2A, 0x2A, 255) : IM_COL32(0x22, 0x22, 0x22, 255), 6.0f);
            dl->AddRect(c0, c1, IM_COL32(0x3C, 0x3C, 0x3C, 255), 6.0f);
            ImVec2 ls = ImGui::CalcTextSize(label.c_str());
            float lx = c0.x + (cw - ls.x) * 0.5f;
            if (g_app->font_icon) ImGui::PushFont(g_app->font_icon);
            ImVec2 is = ImGui::CalcTextSize(IC_SEARCH);
            dl->AddText(ImVec2(lx - is.x - dp(8.0f), c0.y + (ch - is.y) * 0.5f), IM_COL32(0x9D, 0x9D, 0x9D, 255), IC_SEARCH);
            if (g_app->font_icon) ImGui::PopFont();
            dl->AddText(ImVec2(lx, c0.y + (ch - ls.y) * 0.5f), IM_COL32(0xCC, 0xCC, 0xCC, 255), label.c_str());
        }
    }

    // ---- Window buttons (min / max / close) ----
    const float btn_w = dp(46.0f);
    struct WB { const char* icon; std::function<void()> run; bool close; };
    WB btns[3] = {
        {IC_MIN,                             &title_minimize,   false},
        {g_app->maximized ? IC_RESTORE : IC_MAX, &title_toggle_max, false},
        {IC_XCLOSE,                          &title_close,      true },
    };
    for (int i = 0; i < 3; i++) {
        float bx = rmax.x - btn_w * (3 - i);
        ImGui::SetCursorScreenPos(ImVec2(bx, rmin.y));
        ImGui::PushID(100 + i);
        ImGui::InvisibleButton("wb", ImVec2(btn_w, h));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        if (hov) g_app->title_hover_blocks_drag = true;
        if (hov) {
            ImU32 bg = btns[i].close ? IM_COL32(0xE8, 0x11, 0x23, 0xFF)
                                     : IM_COL32(0xFF, 0xFF, 0xFF, 0x1A);
            dl->AddRectFilled(ImVec2(bx, rmin.y), ImVec2(bx + btn_w, rmax.y), bg);
        }
        ImU32 col = (hov && btns[i].close) ? IM_COL32(0xFF, 0xFF, 0xFF, 0xFF)
                                           : IM_COL32(0xCC, 0xCC, 0xCC, 0xFF);
        ImVec2 ts = ImGui::CalcTextSize(btns[i].icon);
        dl->AddText(ImVec2(bx + (btn_w - ts.x) * 0.5f, rmin.y + (h - ts.y) * 0.5f),
                    col, btns[i].icon);
        if (clk) btns[i].run();
        ImGui::PopID();
    }

    // ---- Popup menu ----
    // Open on click, close when losing focus.
    if (g_app->want_open_menu >= 0) {
        g_app->open_menu = g_app->want_open_menu;
        g_app->want_open_menu = -1;
        ImGui::OpenPopup("titlemenu");
        // remember which menu was opened via the rects[]
    }
    if (g_app->open_menu >= 0) {
        int m = g_app->open_menu;
        ImGui::SetNextWindowPos(ImVec2(rects[m].x, rmax.y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
        if (ImGui::BeginPopup("titlemenu")) {
            kTitleMenus[m].draw();
            ImGui::EndPopup();
        } else {
            g_app->open_menu = -1;
        }
        ImGui::PopStyleVar();
    }

    // ---- Bottom hairline separator ----
    dl->AddLine(ImVec2(rmin.x, rmax.y - 0.5f), ImVec2(rmax.x, rmax.y - 0.5f), vs::border);

    ImGui::SetCursorScreenPos(ImVec2(rmin.x, rmax.y));
    return h;
}

// ---------------------------------------------------------------------------
// Quick open: files (default), ">" commands, "@" symbols, ":" line
// ---------------------------------------------------------------------------
struct QuickItem {
    std::string label;
    std::string detail;
    const char* icon = IC_FILE;
    ImU32 icon_col = IM_COL32(0x9A, 0x9A, 0x9A, 255);
    int score = 0;
    std::function<void()> run;
};

static bool g_quick_just_opened = false;

static void open_quick(const char* prefix) {
    g_app->quick_open = true;
    g_app->palette_open = false;
    std::snprintf(g_app->quick_text, sizeof(g_app->quick_text), "%s", prefix);
    g_app->palette_sel = 0;
    g_quick_just_opened = true;
    if (!prefix[0]) workspace_files(g_app->quick_files);
}

// Subsequence match, case-insensitive. Rewards consecutive runs and matches at
// word starts; -1 means no match.
static int fuzzy_score(const std::string& pat, const std::string& text) {
    if (pat.empty()) return 0;
    int score = 0;
    size_t ti = 0;
    int run = 0;
    for (size_t pi = 0; pi < pat.size(); pi++) {
        char pc = (char)std::tolower((unsigned char)pat[pi]);
        if (pc == ' ') continue;
        bool found = false;
        while (ti < text.size()) {
            char tc = (char)std::tolower((unsigned char)text[ti]);
            if (tc == pc) {
                bool boundary = ti == 0 || text[ti - 1] == '/' || text[ti - 1] == '\\' ||
                                text[ti - 1] == '_' || text[ti - 1] == '.' || text[ti - 1] == ' ' ||
                                text[ti - 1] == ':' ||
                                (std::isupper((unsigned char)text[ti]) && std::islower((unsigned char)text[ti - 1]));
                run++;
                score += 1 + run * 2 + (boundary ? 6 : 0);
                ti++;
                found = true;
                break;
            }
            run = 0;
            ti++;
        }
        if (!found) return -1;
    }
    return score - (int)text.size() / 16;
}

static std::string trim_copy(const std::string& v) {
    size_t a = v.find_first_not_of(" \t");
    if (a == std::string::npos) return {};
    size_t z = v.find_last_not_of(" \t");
    return v.substr(a, z - a + 1);
}

static void quick_commands(std::vector<QuickItem>& out) {
    struct Cmd { const char* label; const char* key; std::function<void()> run; };
    const Cmd cmds[] = {
        {"File: New File", "Ctrl+N", [] { new_untitled(); }},
        {"File: Open File...", "Ctrl+O", [] { std::string p; if (open_file_dialog(p)) open_path(p, false); }},
        {"File: Open Folder...", "", [] { std::string f; if (open_folder_dialog(f)) set_folder(f); }},
        {"File: Save", "Ctrl+S", [] { if (g_app->active >= 0) save_buffer(g_app->active); }},
        {"File: Save As...", "Ctrl+Shift+S", [] { if (g_app->active >= 0) save_buffer(g_app->active, true); }},
        {"File: Save All", "", [] { save_all(); }},
        {"File: Close Editor", "Ctrl+W", [] { if (g_app->active >= 0) close_buffer(g_app->active); }},
        {"File: Reveal Active File in Explorer", "", [] {
            if (g_app->active >= 0 && !g_app->buffers[(size_t)g_app->active].untitled)
                reveal_in_explorer(g_app->buffers[(size_t)g_app->active].path);
        }},
        {"Go to File...", "Ctrl+P", [] { open_quick(""); }},
        {"Go to Symbol in Editor...", "Ctrl+Shift+O", [] { open_quick("@"); }},
        {"Go to Line...", "Ctrl+G", [] { g_app->goto_open = true; }},
        {"Go to Definition", "F12", [] { editor_exec(EdCmd::GotoDefinition); }},
        {"Go to Bracket", "Ctrl+Shift+\\", [] { editor_exec(EdCmd::JumpToBracket); }},
        {"Edit: Undo", "Ctrl+Z", [] { editor_exec(EdCmd::Undo); }},
        {"Edit: Redo", "Ctrl+Y", [] { editor_exec(EdCmd::Redo); }},
        {"Edit: Find", "Ctrl+F", [] { g_app->find_open = g_app->find_focus = true; }},
        {"Edit: Replace", "Ctrl+H", [] { g_app->find_open = g_app->replace_open = g_app->find_focus = true; }},
        {"Edit: Toggle Line Comment", "Ctrl+/", [] { editor_exec(EdCmd::ToggleComment); }},
        {"Edit: Trim Trailing Whitespace", "", [] { editor_exec(EdCmd::TrimTrailingWhitespace); }},
        {"Edit: Trigger Suggest", "Ctrl+Space", [] { editor_exec(EdCmd::TriggerSuggest); }},
        {"Selection: Select Line", "Ctrl+L", [] { editor_exec(EdCmd::SelectLine); }},
        {"Selection: Duplicate Line", "Ctrl+D", [] { editor_exec(EdCmd::DuplicateLine); }},
        {"Selection: Delete Line", "Ctrl+Shift+K", [] { editor_exec(EdCmd::DeleteLine); }},
        {"Selection: Move Line Up", "Alt+Up", [] { editor_exec(EdCmd::MoveLineUp); }},
        {"Selection: Move Line Down", "Alt+Down", [] { editor_exec(EdCmd::MoveLineDown); }},
        {"Selection: Copy Line Up", "Shift+Alt+Up", [] { editor_exec(EdCmd::CopyLineUp); }},
        {"Selection: Copy Line Down", "Shift+Alt+Down", [] { editor_exec(EdCmd::CopyLineDown); }},
        {"Selection: Indent", "Ctrl+]", [] { editor_exec(EdCmd::Indent); }},
        {"Selection: Outdent", "Ctrl+[", [] { editor_exec(EdCmd::Outdent); }},
        {"Selection: Transform to Uppercase", "", [] { editor_exec(EdCmd::UpperCase); }},
        {"Selection: Transform to Lowercase", "", [] { editor_exec(EdCmd::LowerCase); }},
        {"View: Toggle Agent", "Ctrl+Shift+L", [] { g_app->settings.show_agent = !g_app->settings.show_agent; }},
        {"View: Toggle Sidebar", "Ctrl+B", [] { g_app->settings.show_sidebar = !g_app->settings.show_sidebar; }},
        {"View: Toggle Panel", "Ctrl+J", [] { g_app->settings.show_panel = !g_app->settings.show_panel; }},
        {"View: Show Explorer", "Ctrl+Shift+E", [] { g_app->activity = ActivityView::Explorer; g_app->settings.show_sidebar = true; }},
        {"View: Show Search", "Ctrl+Shift+F", [] { g_app->activity = ActivityView::Search; g_app->settings.show_sidebar = true; }},
        {"View: Show Outline", "", [] { g_app->activity = ActivityView::Outline; g_app->settings.show_sidebar = true; }},
        {"View: Show Problems", "Ctrl+Shift+M", [] { g_app->bottom = BottomTab::Problems; g_app->settings.show_panel = true; }},
        {"View: Show Terminal", "Ctrl+`", [] { g_app->bottom = BottomTab::Terminal; g_app->settings.show_panel = true; }},
        {"Run: Run", "F5", [] { run_active(true); }},
        {"Run: Build", "Ctrl+Alt+B", [] { run_active(false); }},
        {"Run: Stop", "Shift+F5", [] { if (g_app->proc.running) proc_stop(); }},
        {"Agent: New Chat", "", [] { agent_new_chat(); }},
    };
    for (const auto& c : cmds) {
        QuickItem it;
        it.label = c.label;
        it.detail = c.key;
        it.icon = IC_SPARKLE;
        it.icon_col = IM_COL32(0x6C, 0xB6, 0xFF, 200);
        it.run = c.run;
        out.push_back(std::move(it));
    }
}

static void quick_build(std::vector<QuickItem>& items) {
    items.clear();
    std::string q = g_app->quick_text;
    if (!q.empty() && q[0] == '>') {
        std::string pat = trim_copy(q.substr(1));
        std::vector<QuickItem> all;
        quick_commands(all);
        for (auto& it : all) {
            it.score = fuzzy_score(pat, it.label);
            if (it.score >= 0) items.push_back(std::move(it));
        }
        if (!pat.empty()) {
            std::stable_sort(items.begin(), items.end(), [](const QuickItem& a, const QuickItem& b) {
                return a.score > b.score;
            });
        }
        return;
    }
    if (!q.empty() && q[0] == ':') {
        if (g_app->active < 0) return;
        const TextBuffer& b = g_app->buffers[(size_t)g_app->active];
        int line = std::atoi(q.c_str() + 1);
        QuickItem it;
        it.icon = IC_OUTLINE;
        if (line > 0) {
            int col = 1;
            size_t c2 = q.find(':', 1);
            if (c2 != std::string::npos) col = std::max(1, std::atoi(q.c_str() + c2 + 1));
            char buf[96];
            std::snprintf(buf, sizeof(buf), "Go to line %d, column %d", line, col);
            it.label = buf;
            it.run = [line, col] {
                if (g_app->active < 0) return;
                TextBuffer& tb = g_app->buffers[(size_t)g_app->active];
                tb.cursor = tb.sel_anchor = buffer_pos_at_vcol(tb, line - 1, col - 1);
                tb.preferred_col = buffer_vcol_at(tb, tb.cursor);
                buffer_reveal(tb, true);
                g_app->editor_focused = true;
            };
        } else {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "Current line: %d. Type a line number between 1 and %d.",
                          buffer_line_at(b, b.cursor) + 1, (int)b.lines.size());
            it.label = buf;
        }
        items.push_back(std::move(it));
        return;
    }
    if (!q.empty() && q[0] == '@') {
        if (g_app->active < 0) return;
        TextBuffer& b = g_app->buffers[(size_t)g_app->active];
        if (b.outline_dirty && b.is_nexa) {
            index_nexa(b.text, b.outline);
            b.outline_dirty = false;
        }
        std::string pat = trim_copy(q.substr(1));
        for (const auto& d : b.outline) {
            QuickItem it;
            it.label = d.name;
            it.score = fuzzy_score(pat, d.name);
            if (it.score < 0) continue;
            switch (d.kind) {
            case DefKind::Function: it.detail = "fn"; it.icon = IC_RUN; it.icon_col = IM_COL32(0xDC, 0xDC, 0xAA, 255); break;
            case DefKind::Struct: it.detail = "struct"; it.icon = IC_FOLDER; it.icon_col = IM_COL32(0x4E, 0xC9, 0xB0, 255); break;
            case DefKind::Enum: it.detail = "enum"; it.icon = IC_OUTLINE; it.icon_col = IM_COL32(0x4E, 0xC9, 0xB0, 255); break;
            default: it.detail = "let"; it.icon = IC_FILE; it.icon_col = IM_COL32(0x9C, 0xDC, 0xFE, 255); break;
            }
            it.detail += "  :" + std::to_string(d.line + 1);
            int s0 = d.name_start, s1 = d.name_end;
            it.run = [s0, s1] {
                if (g_app->active < 0) return;
                TextBuffer& tb = g_app->buffers[(size_t)g_app->active];
                tb.sel_anchor = s0;
                tb.cursor = s1;
                buffer_reveal(tb, true);
                g_app->editor_focused = true;
            };
            items.push_back(std::move(it));
        }
        if (!pat.empty()) {
            std::stable_sort(items.begin(), items.end(), [](const QuickItem& a, const QuickItem& b) {
                return a.score > b.score;
            });
        }
        return;
    }
    // Files: open editors first when nothing is typed, then the workspace.
    std::string pat = trim_copy(q);
    std::string root = g_app->settings.folder;
    auto rel_of = [&](const std::string& path) {
        if (!root.empty() && path.size() > root.size() + 1 && path.compare(0, root.size(), root) == 0)
            return path.substr(root.size() + 1);
        return path;
    };
    std::vector<std::string> seen;
    auto add_file = [&](const std::string& path, bool open_editor) {
        std::string rel = rel_of(path);
        std::string name = path_filename(path);
        int score = fuzzy_score(pat, rel);
        int name_score = fuzzy_score(pat, name);
        if (score < 0) return;
        if (name_score >= 0) score += name_score * 2;
        if (open_editor) score += 4;
        QuickItem it;
        it.label = name;
        std::string dir = path_parent(rel);
        it.detail = dir == rel ? "" : dir;
        if (open_editor) it.detail = it.detail.empty() ? "open" : it.detail + "  -  open";
        it.score = score;
        switch (lang_for_path(path)) {
        case Lang::Nexa: it.icon_col = IM_COL32(0x7E, 0xC4, 0xE0, 255); break;
        case Lang::Json:
        case Lang::Ini: it.icon_col = IM_COL32(0xE2, 0xC0, 0x8D, 255); break;
        case Lang::Cpp: it.icon_col = IM_COL32(0xC5, 0x93, 0xE7, 255); break;
        case Lang::Markdown: it.icon_col = IM_COL32(0x9C, 0xDC, 0xFE, 255); break;
        case Lang::Shell: it.icon_col = IM_COL32(0x89, 0xD1, 0x85, 255); break;
        default: break;
        }
        it.run = [path] {
            open_path(path, false);
            g_app->editor_focused = true;
        };
        items.push_back(std::move(it));
        seen.push_back(path_norm(path));
    };
    for (const auto& b : g_app->buffers) {
        if (!b.untitled) add_file(b.path, true);
    }
    for (const auto& f : g_app->quick_files) {
        if (std::find(seen.begin(), seen.end(), path_norm(f)) != seen.end()) continue;
        add_file(f, false);
        if (items.size() > 4000) break;
    }
    if (!pat.empty()) {
        std::stable_sort(items.begin(), items.end(), [](const QuickItem& a, const QuickItem& b) {
            return a.score > b.score;
        });
    }
    if (items.size() > 200) items.resize(200);
}

static void draw_quick_open() {
    if (g_app->palette_open) open_quick(">");
    if (!g_app->quick_open) return;
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 ds = io.DisplaySize;
    const float w = std::min(dp(640.0f), ds.x - dp(40.0f));
    ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, dp(48.0f)), ImGuiCond_Always, ImVec2(0.5f, 0));
    ImGui::SetNextWindowSize(ImVec2(w, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.098f, 0.098f, 0.098f, 1));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.22f, 0.22f, 1));
    ImGui::Begin("##quick_open", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    bool just_opened = g_quick_just_opened;
    if (just_opened) {
        ImGui::SetWindowFocus();
        ImGui::SetKeyboardFocusHere();
        g_quick_just_opened = false;
    }
    ui_push_field();
    ImGui::SetNextItemWidth(-1);
    std::string before = g_app->quick_text;
    ImGui::InputTextWithHint("##quick", "Search files by name  ( > commands   @ symbols   : line )",
                             g_app->quick_text, sizeof(g_app->quick_text));
    bool input_active = ImGui::IsItemActive();
    ui_pop_field();
    if (before != g_app->quick_text) g_app->palette_sel = 0;

    static std::vector<QuickItem> items;
    quick_build(items);
    int n = (int)items.size();
    bool run_sel = false;
    bool close = false;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && n > 0) g_app->palette_sel = (g_app->palette_sel + 1) % n;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && n > 0) g_app->palette_sel = (g_app->palette_sel - 1 + n) % n;
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown) && n > 0) g_app->palette_sel = std::min(n - 1, g_app->palette_sel + 10);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp) && n > 0) g_app->palette_sel = std::max(0, g_app->palette_sel - 10);
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) run_sel = true;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) close = true;
    g_app->palette_sel = n > 0 ? std::clamp(g_app->palette_sel, 0, n - 1) : 0;

    const float row_h = dp(26.0f);
    const int rows = std::min(n, 12);
    if (n == 0) {
        ImGui::Dummy(ImVec2(1, 4));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1));
        ImGui::TextUnformatted(g_app->quick_text[0] == '>' ? "  No matching commands"
                               : g_app->quick_text[0] == '@' ? "  No matching symbols"
                               : g_app->settings.folder.empty() ? "  Open a folder to search its files"
                                                                : "  No matching files");
        ImGui::PopStyleColor();
    } else {
        ImGui::Dummy(ImVec2(1, 2));
        ImGui::BeginChild("##quick_list", ImVec2(0, row_h * (float)rows + 4.0f), ImGuiChildFlags_None);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        static int last_sel = -1;
        for (int i = 0; i < n; i++) {
            const QuickItem& it = items[(size_t)i];
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            float rw = ImGui::GetContentRegionAvail().x;
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##row", ImVec2(rw, row_h))) {
                g_app->palette_sel = i;
                run_sel = true;
            }
            bool hov = ImGui::IsItemHovered();
            ImGui::PopID();
            bool sel = i == g_app->palette_sel;
            if (sel || hov) {
                dl->AddRectFilled(p0, ImVec2(p0.x + rw, p0.y + row_h),
                                  sel ? IM_COL32(0x04, 0x39, 0x5E, 255) : IM_COL32(0x22, 0x22, 0x22, 255), 4.0f);
            }
            float ty = p0.y + (row_h - ImGui::GetFontSize()) * 0.5f;
            if (g_app->font_icon) ImGui::PushFont(g_app->font_icon);
            dl->AddText(ImVec2(p0.x + 8.0f, ty), it.icon_col, it.icon);
            if (g_app->font_icon) ImGui::PopFont();
            dl->AddText(ImVec2(p0.x + 32.0f, ty), IM_COL32(0xE0, 0xE0, 0xE0, 255), it.label.c_str());
            if (!it.detail.empty()) {
                bool right = g_app->quick_text[0] == '>' || g_app->quick_text[0] == '@';
                float dx = right ? p0.x + rw - ImGui::CalcTextSize(it.detail.c_str()).x - 10.0f
                                 : p0.x + 32.0f + ImGui::CalcTextSize(it.label.c_str()).x + 10.0f;
                dl->AddText(ImVec2(dx, ty), IM_COL32(0x80, 0x80, 0x80, 255), it.detail.c_str());
            }
            if (sel && last_sel != g_app->palette_sel) ImGui::SetScrollHereY(0.5f);
        }
        last_sel = g_app->palette_sel;
        ImGui::EndChild();
    }
    // Clicking anywhere outside closes it, as does losing keyboard focus.
    if (!just_opened && !input_active && !ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) close = true;
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    if (run_sel && n > 0 && items[(size_t)g_app->palette_sel].run) {
        auto fn = items[(size_t)g_app->palette_sel].run;
        g_app->quick_open = false;
        fn();
    } else if (close) {
        g_app->quick_open = false;
    }
}

static void modals() {
    if (g_app->new_file_modal) ImGui::OpenPopup("New File");
    if (g_app->new_folder_modal) ImGui::OpenPopup("New Folder");
    if (g_app->rename_modal) ImGui::OpenPopup("Rename");
    if (g_app->confirm_close) ImGui::OpenPopup("Unsaved");

    if (ImGui::BeginPopupModal("New File", &g_app->new_file_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", g_app->new_name, sizeof(g_app->new_name));
        if (ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            std::string p = path_join(g_app->modal_parent, g_app->new_name);
            if (path_ext(p).empty()) p += ".nxa";
            file_write(p, "#include <std/io>\n\nfn main(): void {\n    \n}\n");
            scan_node(g_app->root, true);
            open_path(p, false);
            g_app->new_file_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            g_app->new_file_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("New Folder", &g_app->new_folder_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", g_app->new_name, sizeof(g_app->new_name));
        if (ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            make_dir(path_join(g_app->modal_parent, g_app->new_name));
            scan_node(g_app->root, true);
            g_app->new_folder_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            g_app->new_folder_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("Rename", &g_app->rename_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", g_app->new_name, sizeof(g_app->new_name));
        if (ImGui::Button("Rename") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            std::string dest = path_join(path_parent(g_app->modal_parent), g_app->new_name);
            rename_path(g_app->modal_parent, dest);
            scan_node(g_app->root, true);
            g_app->rename_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            g_app->rename_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("Unsaved", &g_app->confirm_close, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("This file has unsaved changes.");
        if (ImGui::Button("Save")) {
            if (save_buffer(g_app->close_index)) close_buffer(g_app->close_index, true);
            g_app->confirm_close = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save")) {
            close_buffer(g_app->close_index, true);
            g_app->confirm_close = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            g_app->confirm_close = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    draw_quick_open();
}

// ---------------------------------------------------------------------------
// Shortcuts
// ---------------------------------------------------------------------------
void ide_shortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl  = io.KeyCtrl;
    bool shift = io.KeyShift;
    bool alt   = io.KeyAlt;
    if (g_app->terminal_focused) {
        if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_J))
            g_app->settings.show_panel = !g_app->settings.show_panel;
        if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_B))
            g_app->settings.show_sidebar = !g_app->settings.show_sidebar;
        if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_P)) g_app->palette_open = true;
        if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_P)) open_quick("");
        if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_L))
            g_app->settings.show_agent = !g_app->settings.show_agent;
        if (ImGui::IsKeyPressed(ImGuiKey_F5)) run_active(true);
        return;
    }
    if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_N)) new_untitled();
    if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_O)) {
        std::string p; if (open_file_dialog(p)) open_path(p, false);
    }
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_S) && g_app->active >= 0 &&
        !(g_app->pending_chord > 0.0 && ImGui::GetTime() - g_app->pending_chord < 1.5))
        save_buffer(g_app->active);
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_S) && g_app->active >= 0) save_buffer(g_app->active, true);
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_W) && g_app->active >= 0) close_buffer(g_app->active);
    const bool has_editor = g_app->active >= 0;
    if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_F) && has_editor) {
        g_app->find_open = true;
        g_app->find_focus = true;
    }
    if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_H) && has_editor) {
        g_app->find_open = true;
        g_app->replace_open = true;
        g_app->find_focus = true;
    }
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_G) && has_editor) g_app->goto_open = true;
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_P)) open_quick("");
    if ((ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_P)) || ImGui::IsKeyPressed(ImGuiKey_F1)) g_app->palette_open = true;
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_O) && has_editor) open_quick("@");
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_M)) {
        g_app->bottom = BottomTab::Problems;
        g_app->settings.show_panel = true;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_GraveAccent)) {
        g_app->bottom = BottomTab::Terminal;
        g_app->settings.show_panel = true;
    }
    if (shift && !ctrl && ImGui::IsKeyPressed(ImGuiKey_F5) && g_app->proc.running) proc_stop();
    int nbuf = (int)g_app->buffers.size();
    if (ctrl && nbuf > 1 && (ImGui::IsKeyPressed(ImGuiKey_Tab) || ImGui::IsKeyPressed(ImGuiKey_PageDown) ||
                             ImGui::IsKeyPressed(ImGuiKey_PageUp))) {
        bool back = ImGui::IsKeyPressed(ImGuiKey_PageUp) || (shift && ImGui::IsKeyPressed(ImGuiKey_Tab));
        g_app->active = (g_app->active + (back ? nbuf - 1 : 1)) % nbuf;
    }
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_K, false)) g_app->pending_chord = ImGui::GetTime();
    if (g_app->pending_chord > 0.0 && ImGui::GetTime() - g_app->pending_chord < 1.5 && ctrl &&
        ImGui::IsKeyPressed(ImGuiKey_S, false) && !ImGui::IsKeyPressed(ImGuiKey_K, false)) {
        save_all();
        g_app->pending_chord = 0.0;
    }
    if (ctrl && alt && ImGui::IsKeyPressed(ImGuiKey_B)) run_active(false);
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) run_active(true);
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_B))
        g_app->settings.show_sidebar = !g_app->settings.show_sidebar;
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_J))
        g_app->settings.show_panel = !g_app->settings.show_panel;
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_E)) {
        g_app->activity = ActivityView::Explorer;
        g_app->settings.show_sidebar = true;
    }
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_F)) {
        g_app->activity = ActivityView::Search;
        g_app->settings.show_sidebar = true;
    }
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_L))
        g_app->settings.show_agent = !g_app->settings.show_agent;
}

// ---------------------------------------------------------------------------
// Root
// ---------------------------------------------------------------------------
void draw_ide() {
    ide_shortcuts();
    proc_tick();

    // Switching to another file re-runs diagnostics for it.
    static int diag_active = -2;
    static size_t diag_buffers = 0;
    if (diag_active != g_app->active || diag_buffers != g_app->buffers.size()) {
        diag_active = g_app->active;
        diag_buffers = g_app->buffers.size();
        g_app->diag_dirty = true;
        g_app->last_diag = 0.0;
    }
    if (g_app->diag_dirty && ImGui::GetTime() - g_app->last_diag > 0.45) {
        refresh_problems();
    }
    if (g_app->active >= 0) {
        TextBuffer& b = g_app->buffers[(size_t)g_app->active];
        if (b.spans_dirty && ImGui::GetTime() - b.last_edit > 0.12) buffer_refresh(b);
        if (b.is_nexa && b.outline_dirty && ImGui::GetTime() - b.last_edit > 0.3) {
            index_nexa(b.text, b.outline);
            b.outline_dirty = false;
        }
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 pos = vp->WorkPos;
    ImVec2 sz = vp->WorkSize;
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(sz);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags rflags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                              ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("NexiumRoot", nullptr, rflags);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float title_h = draw_title_bar(avail.x);
    float status_h = dp(22.0f);
    float main_h = avail.y - title_h - status_h;

    // Activity bar
    ImGui::BeginChild("act", ImVec2(dp(48), main_h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    draw_activity();
    ImGui::EndChild();
    if (ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()) &&
        !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
        g_app->editor_focused = false;
    ImGui::SameLine(0, 0);

    if (g_app->settings.show_sidebar) {
        ImGui::BeginChild("side", ImVec2(g_app->settings.sidebar_w, main_h), false,
                          ImGuiWindowFlags_NoScrollbar);
        draw_sidebar();
        ImGui::EndChild();
        if (ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()) &&
            !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
            g_app->editor_focused = false;
        ImGui::SameLine(0, 0);
        splitter_v("s1", &g_app->settings.sidebar_w, 180.0f, 600.0f, main_h);
        ImGui::SameLine(0, 0);
    }

    float used_left = dp(48) + (g_app->settings.show_sidebar ? g_app->settings.sidebar_w + kSplitter : 0);
    float used_right = g_app->settings.show_agent ? g_app->settings.agent_w + kSplitter : 0;
    float mid_w = avail.x - used_left - used_right;
    if (mid_w < 200) mid_w = 200;

    // Middle: editor + bottom panel
    ImGui::BeginChild("mid", ImVec2(mid_w, main_h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (g_app->settings.show_panel) {
        float max_panel = main_h - 80.0f - kSplitter;
        if (max_panel < 80.0f) max_panel = 80.0f;
        if (g_app->settings.panel_h > max_panel) g_app->settings.panel_h = max_panel;
    }
    float panel_total = g_app->settings.show_panel ? g_app->settings.panel_h + kSplitter : 0;
    float editor_h = main_h - panel_total;
    if (editor_h < 80.0f) editor_h = 80.0f;
    ImGui::BeginChild("editor", ImVec2(0, editor_h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    draw_editor(ImGui::GetContentRegionAvail());
    ImGui::EndChild();

    if (g_app->settings.show_panel) {
        splitter_h("s2", &g_app->settings.panel_h, 80.0f, main_h - 100.0f, mid_w);
        float ph = g_app->settings.panel_h;
        if (ph < 80.0f) ph = 80.0f;
        ImGui::BeginChild("panel", ImVec2(0, ph), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        draw_bottom();
        ImGui::EndChild();
    }
    ImGui::EndChild();

    if (g_app->settings.show_agent) {
        ImGui::SameLine(0, 0);
        splitter_v("s3", &g_app->settings.agent_w, 280.0f, 800.0f, main_h, -1);
        ImGui::SameLine(0, 0);
        ImGui::BeginChild("agent", ImVec2(g_app->settings.agent_w, main_h), false,
                          ImGuiWindowFlags_NoScrollbar);
        draw_agent_panel();
        ImGui::EndChild();
    }

    // Status bar drawn on top of foreground draw list
    draw_status(title_h + main_h, avail.x);

    ImGui::End();
    ImGui::PopStyleVar(2);
    modals();
}
