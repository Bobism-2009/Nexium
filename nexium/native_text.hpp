// Crisp text for the editor: glyphs rendered once by GDI with ClearType,
// cached, and blended per colour channel straight into the gfx framebuffer.
// Included from native.hpp; reaches the framebuffer through __nexa_g, which
// the std/gfx runtime defines before this header in the generated C++.
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>

struct NxGlyph {
    int w = 0, h = 0;      // bitmap size
    int ox = 0;            // bitmap left relative to the pen
    int adv = 0;           // advance
    std::vector<unsigned char> cov;  // w*h*3 coverage, R G B
};

struct NxFont {
    HFONT font = nullptr;
    int height = 0;        // line height in pixels
    int ascent = 0;
    int cell = 0;          // advance of '0'
    int pad = 0;
    std::unordered_map<unsigned, NxGlyph> glyphs;
};

static std::vector<NxFont> nx__fonts;
static HDC nx__tdc = nullptr;
static HBITMAP nx__tbmp = nullptr;
static unsigned char* nx__tbits = nullptr;
static int nx__tbw = 0, nx__tbh = 0;
static int nx__cx0 = 0, nx__cy0 = 0, nx__cx1 = 1 << 30, nx__cy1 = 1 << 30;

static std::wstring nx__wide_t(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

static bool nx__font_exists(const std::wstring& face) {
    HDC dc = GetDC(nullptr);
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, face.c_str(), LF_FACESIZE - 1);
    bool found = false;
    EnumFontFamiliesExW(dc, &lf, [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM p) -> int {
        *(bool*)p = true;
        return 0;
    }, (LPARAM)&found, 0);
    ReleaseDC(nullptr, dc);
    return found;
}

// A font by face name (the first installed one of a ';'-separated list) at px
// pixels; returns its id for the other nx_text calls.
static int nx_font(const std::string& faces, int px, int bold) {
    std::wstring chosen = L"Consolas";
    std::wstring all = nx__wide_t(faces);
    size_t start = 0;
    while (start <= all.size()) {
        size_t e = all.find(L';', start);
        if (e == std::wstring::npos) e = all.size();
        std::wstring f = all.substr(start, e - start);
        while (!f.empty() && f[0] == L' ') f.erase(0, 1);
        while (!f.empty() && f.back() == L' ') f.pop_back();
        if (!f.empty() && nx__font_exists(f)) {
            chosen = f;
            break;
        }
        start = e + 1;
    }
    NxFont nf;
    nf.font = CreateFontW(-px, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
                          chosen.c_str());
    if (!nx__tdc) nx__tdc = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(nx__tdc, nf.font);
    TEXTMETRICW tm;
    GetTextMetricsW(nx__tdc, &tm);
    nf.height = tm.tmHeight;
    nf.ascent = tm.tmAscent;
    SIZE sz;
    GetTextExtentPoint32W(nx__tdc, L"0", 1, &sz);
    nf.cell = sz.cx;
    nf.pad = px / 3 + 2;
    SelectObject(nx__tdc, old);
    nx__fonts.push_back(std::move(nf));
    return (int)nx__fonts.size() - 1;
}

static void nx_font_free_all() {
    for (auto& f : nx__fonts)
        if (f.font) DeleteObject(f.font);
    nx__fonts.clear();
}

static int nx_font_h(int id) { return id >= 0 && id < (int)nx__fonts.size() ? nx__fonts[(size_t)id].height : 0; }
static int nx_font_cw(int id) { return id >= 0 && id < (int)nx__fonts.size() ? nx__fonts[(size_t)id].cell : 0; }

static void nx__ensure_scratch(int w, int h) {
    if (w <= nx__tbw && h <= nx__tbh && nx__tbmp) return;
    if (nx__tbmp) DeleteObject(nx__tbmp);
    nx__tbw = w > nx__tbw ? w : nx__tbw;
    nx__tbh = h > nx__tbh ? h : nx__tbh;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = nx__tbw;
    bi.bmiHeader.biHeight = -nx__tbh;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    nx__tbmp = CreateDIBSection(nx__tdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    nx__tbits = (unsigned char*)bits;
    SelectObject(nx__tdc, nx__tbmp);
}

static const NxGlyph& nx__glyph(NxFont& f, unsigned cp) {
    auto it = f.glyphs.find(cp);
    if (it != f.glyphs.end()) return it->second;
    NxGlyph g;
    wchar_t w[2];
    int wn = 1;
    if (cp >= 0x10000) {
        unsigned v = cp - 0x10000;
        w[0] = (wchar_t)(0xD800 + (v >> 10));
        w[1] = (wchar_t)(0xDC00 + (v & 0x3FF));
        wn = 2;
    } else {
        w[0] = (wchar_t)cp;
    }
    HGDIOBJ old = SelectObject(nx__tdc, f.font);
    SIZE sz;
    GetTextExtentPoint32W(nx__tdc, w, wn, &sz);
    g.adv = sz.cx;
    int bw = sz.cx + f.pad * 2;
    int bh = f.height;
    nx__ensure_scratch(bw, bh);
    SelectObject(nx__tdc, f.font);
    // white on black: each channel of the result is that subpixel's coverage
    memset(nx__tbits, 0, (size_t)nx__tbw * nx__tbh * 4);
    SetBkMode(nx__tdc, TRANSPARENT);
    SetTextColor(nx__tdc, RGB(255, 255, 255));
    TextOutW(nx__tdc, f.pad, 0, w, wn);
    GdiFlush();
    // trim empty columns
    int x0 = bw, x1 = -1;
    for (int y = 0; y < bh; y++) {
        const unsigned char* row = nx__tbits + (size_t)y * nx__tbw * 4;
        for (int x = 0; x < bw; x++) {
            const unsigned char* p = row + x * 4;
            if (p[0] | p[1] | p[2]) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
            }
        }
    }
    if (x1 >= x0) {
        g.w = x1 - x0 + 1;
        g.h = bh;
        g.ox = x0 - f.pad;
        g.cov.resize((size_t)g.w * g.h * 3);
        for (int y = 0; y < bh; y++) {
            const unsigned char* row = nx__tbits + (size_t)y * nx__tbw * 4;
            for (int x = 0; x < g.w; x++) {
                const unsigned char* p = row + (x0 + x) * 4;
                unsigned char* c = &g.cov[((size_t)y * g.w + x) * 3];
                c[0] = p[2];
                c[1] = p[1];
                c[2] = p[0];
            }
        }
    }
    SelectObject(nx__tdc, old);
    return f.glyphs.emplace(cp, std::move(g)).first->second;
}

// Limits nx_text drawing to a rectangle (x1/y1 exclusive); nx_clip_off lifts it.
static void nx_clip(int x0, int y0, int x1, int y1) {
    nx__cx0 = x0;
    nx__cy0 = y0;
    nx__cx1 = x1;
    nx__cy1 = y1;
}
static void nx_clip_off() {
    nx__cx0 = nx__cy0 = 0;
    nx__cx1 = nx__cy1 = 1 << 30;
}

static void nx__blit_glyph(const NxGlyph& g, int px, int py, int r, int gr, int b) {
    if (g.w == 0 || !__nexa_g.fb) return;
    int W = __nexa_g.w, H = __nexa_g.h;
    int cx0 = nx__cx0 > 0 ? nx__cx0 : 0, cy0 = nx__cy0 > 0 ? nx__cy0 : 0;
    int cx1 = nx__cx1 < W ? nx__cx1 : W, cy1 = nx__cy1 < H ? nx__cy1 : H;
    for (int y = 0; y < g.h; y++) {
        int dy = py + y;
        if (dy < cy0 || dy >= cy1) continue;
        unsigned char* row = __nexa_g.fb + (size_t)dy * W * 4;
        const unsigned char* c = &g.cov[(size_t)y * g.w * 3];
        for (int x = 0; x < g.w; x++, c += 3) {
            int dx = px + x;
            if (dx < cx0 || dx >= cx1) continue;
            if (!(c[0] | c[1] | c[2])) continue;
            unsigned char* d = row + dx * 4;
            d[2] = (unsigned char)((r * c[0] + d[2] * (255 - c[0]) + 127) / 255);
            d[1] = (unsigned char)((gr * c[1] + d[1] * (255 - c[1]) + 127) / 255);
            d[0] = (unsigned char)((b * c[2] + d[0] * (255 - c[2]) + 127) / 255);
            d[3] = 255;
        }
    }
}

static unsigned nx__next_cp(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i];
    unsigned cp = c;
    int extra = 0;
    if (c >= 0xF0) { cp = c & 7; extra = 3; }
    else if (c >= 0xE0) { cp = c & 15; extra = 2; }
    else if (c >= 0xC0) { cp = c & 31; extra = 1; }
    i++;
    while (extra-- > 0 && i < s.size() && ((unsigned char)s[i] & 0xC0) == 0x80) {
        cp = (cp << 6) | ((unsigned char)s[i] & 0x3F);
        i++;
    }
    return cp;
}

// Draws UTF-8 text with its top-left at (x, y) in 0xRRGGBB; returns the width.
static int nx_text(int id, int x, int y, const std::string& s, int color) {
    if (id < 0 || id >= (int)nx__fonts.size()) return 0;
    NxFont& f = nx__fonts[(size_t)id];
    int r = (color >> 16) & 255, g = (color >> 8) & 255, b = color & 255;
    int pen = x;
    size_t i = 0;
    while (i < s.size()) {
        unsigned cp = nx__next_cp(s, i);
        if (cp == '\t') cp = ' ';
        const NxGlyph& gl = nx__glyph(f, cp);
        if (cp != ' ') nx__blit_glyph(gl, pen + gl.ox, y, r, g, b);
        pen += gl.adv;
    }
    return pen - x;
}

// One character (the UTF-8 sequence s) centred in a cell cw wide: the code
// editor's column grid.
static void nx_text_cell(int id, int x, int y, int cw, const std::string& s, int color) {
    if (id < 0 || id >= (int)nx__fonts.size() || s.empty()) return;
    NxFont& f = nx__fonts[(size_t)id];
    size_t i = 0;
    unsigned cp = nx__next_cp(s, i);
    const NxGlyph& gl = nx__glyph(f, cp);
    nx__blit_glyph(gl, x + (cw - gl.adv) / 2 + gl.ox, y, (color >> 16) & 255, (color >> 8) & 255, color & 255);
}

static int nx_text_w(int id, const std::string& s) {
    if (id < 0 || id >= (int)nx__fonts.size()) return 0;
    NxFont& f = nx__fonts[(size_t)id];
    int w = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned cp = nx__next_cp(s, i);
        w += nx__glyph(f, cp).adv;
    }
    return w;
}
