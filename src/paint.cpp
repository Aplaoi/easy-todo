#include "paint.h"

namespace hf {

using namespace Gdiplus;

Palette LightPalette() {
    Palette p;
    p.card = Color(246, 250, 251, 253);
    p.cardSolid = Color(255, 250, 251, 253);
    p.border = Color(255, 226, 230, 238);
    p.text = Color(255, 29, 34, 45);
    p.subtext = Color(255, 112, 121, 137);
    p.faint = Color(255, 153, 160, 174);
    p.accent = Color(255, 66, 111, 246);
    p.accentSoft = Color(255, 232, 238, 255);
    p.hover = Color(255, 238, 242, 249);
    p.field = Color(255, 241, 244, 249);
    p.divider = Color(255, 233, 236, 242);
    p.overdue = Color(255, 220, 64, 74);
    p.soon = Color(255, 231, 132, 44);
    p.normal = Color(255, 88, 101, 124);
    p.done = Color(255, 46, 166, 112);
    p.white = Color(255, 255, 255, 255);
    p.toast = Color(245, 38, 43, 55);
    p.banner = Color(255, 255, 243, 229);
    p.bannerText = Color(255, 166, 92, 18);
    return p;
}

Palette DarkPalette() {
    Palette p;
    p.card = Color(246, 29, 32, 40);
    p.cardSolid = Color(255, 29, 32, 40);
    p.border = Color(255, 52, 57, 69);
    p.text = Color(255, 237, 240, 246);
    p.subtext = Color(255, 154, 162, 176);
    p.faint = Color(255, 112, 120, 134);
    p.accent = Color(255, 116, 148, 255);
    p.accentSoft = Color(255, 44, 55, 86);
    p.hover = Color(255, 39, 44, 55);
    p.field = Color(255, 36, 40, 50);
    p.divider = Color(255, 47, 52, 64);
    p.overdue = Color(255, 255, 107, 110);
    p.soon = Color(255, 245, 165, 74);
    p.normal = Color(255, 168, 177, 192);
    p.done = Color(255, 72, 199, 142);
    p.white = Color(255, 255, 255, 255);
    p.toast = Color(245, 12, 14, 20);
    p.banner = Color(255, 63, 47, 26);
    p.bannerText = Color(255, 255, 190, 120);
    return p;
}

void Fonts::Clear() { cache_.clear(); }

Font* Fonts::Get(float px, bool bold, bool icon) {
    int key = (int)std::lround(px * 10.0f) + (bold ? 1000 : 0) + (icon ? 100000 : 0);
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second.get();
    FontFamily* family = icon ? &icon_ : &ui_;
    int style = bold ? FontStyleBold : FontStyleRegular;
    auto font = std::make_unique<Font>(family, px, style, UnitPixel);
    Font* raw = font.get();
    cache_[key] = std::move(font);
    return raw;
}

Canvas::Canvas(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0) return;
    screenDc_ = GetDC(nullptr);
    if (!screenDc_) return;
    memDc_ = CreateCompatibleDC(screenDc_);
    if (!memDc_) return;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    bitmap_ = CreateDIBSection(screenDc_, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap_ || !bits) return;
    bits_ = bits;
    oldBitmap_ = (HBITMAP)SelectObject(memDc_, bitmap_);
    if (!oldBitmap_) return;

    gdiBitmap_ = std::make_unique<Bitmap>(width, height, width * 4, PixelFormat32bppPARGB,
                                          static_cast<BYTE*>(bits_));
    if (gdiBitmap_->GetLastStatus() != Ok) {
        gdiBitmap_.reset();
        return;
    }
    graphics_ = std::make_unique<Graphics>(gdiBitmap_.get());
    if (graphics_->GetLastStatus() != Ok) {
        graphics_.reset();
        return;
    }
    graphics_->SetSmoothingMode(SmoothingModeAntiAlias);
    graphics_->SetPixelOffsetMode(PixelOffsetModeHalf);
    graphics_->SetTextRenderingHint(TextRenderingHintAntiAlias);
    graphics_->SetInterpolationMode(InterpolationModeHighQualityBicubic);
    graphics_->Clear(Color(0, 0, 0, 0));
    ok_ = true;
}

Canvas::~Canvas() {
    graphics_.reset();
    gdiBitmap_.reset();
    if (memDc_ && oldBitmap_) SelectObject(memDc_, oldBitmap_);
    if (bitmap_) DeleteObject(bitmap_);
    if (memDc_) DeleteDC(memDc_);
    if (screenDc_) ReleaseDC(nullptr, screenDc_);
}

void Canvas::Fade(float alpha) {
    if (!bits_ || alpha >= 0.999f) return;
    if (alpha <= 0.0f) {
        memset(bits_, 0, (size_t)width_ * height_ * 4);
        return;
    }
    int k = (int)std::lround(alpha * 255.0f);
    BYTE* p = static_cast<BYTE*>(bits_);
    size_t count = (size_t)width_ * height_;
    for (size_t i = 0; i < count; ++i, p += 4) {
        p[0] = (BYTE)((p[0] * k + 127) / 255);
        p[1] = (BYTE)((p[1] * k + 127) / 255);
        p[2] = (BYTE)((p[2] * k + 127) / 255);
        p[3] = (BYTE)((p[3] * k + 127) / 255);
    }
}

bool Canvas::Present(HWND hwnd, int x, int y) {
    if (!ok_ || !memDc_) return false;
    HDC screen = GetDC(nullptr);
    if (!screen) return false;
    POINT src{0, 0};
    POINT dst{x, y};
    SIZE size{width_, height_};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    BOOL ok = UpdateLayeredWindow(hwnd, screen, &dst, &size, memDc_, &src, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
    return ok != FALSE;
}

namespace {

void AddRoundRect(GraphicsPath& path, const Rect& r, float radius) {
    float rad = std::min(radius, std::min(r.w, r.h) / 2.0f);
    if (rad <= 0.5f) {
        path.AddRectangle(RectF(r.x, r.y, r.w, r.h));
        return;
    }
    float d = rad * 2;
    path.AddArc(r.x, r.y, d, d, 180, 90);
    path.AddArc(r.x + r.w - d, r.y, d, d, 270, 90);
    path.AddArc(r.x + r.w - d, r.y + r.h - d, d, d, 0, 90);
    path.AddArc(r.x, r.y + r.h - d, d, d, 90, 90);
    path.CloseFigure();
}

}  // namespace

void FillRound(Canvas& c, const Rect& r, float radius, Color color) {
    if (r.w <= 0 || r.h <= 0 || color.GetA() == 0) return;
    GraphicsPath path;
    AddRoundRect(path, r, radius);
    SolidBrush brush(color);
    c.g().FillPath(&brush, &path);
}

void StrokeRound(Canvas& c, const Rect& r, float radius, Color color, float width) {
    if (r.w <= 0 || r.h <= 0 || color.GetA() == 0) return;
    GraphicsPath path;
    AddRoundRect(path, r, radius);
    Pen pen(color, width);
    pen.SetLineJoin(LineJoinRound);
    c.g().DrawPath(&pen, &path);
}

void FillCircle(Canvas& c, float cx, float cy, float radius, Color color) {
    if (radius <= 0 || color.GetA() == 0) return;
    SolidBrush brush(color);
    c.g().FillEllipse(&brush, cx - radius, cy - radius, radius * 2, radius * 2);
}

void DrawLine(Canvas& c, float x1, float y1, float x2, float y2, Color color, float width) {
    Pen pen(color, width);
    c.g().DrawLine(&pen, x1, y1, x2, y2);
}

void DrawText(Canvas& c, Fonts& fonts, const std::wstring& text, const Rect& r, Color color,
              float px, bool bold, TextAlign align, bool ellipsis, bool icon) {
    if (text.empty() || r.w <= 0) return;
    Font* font = fonts.Get(px, bold, icon);
    if (!font || font->GetLastStatus() != Ok) return;
    StringFormat fmt;
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetAlignment(align == TextAlign::Left
                         ? StringAlignmentNear
                         : (align == TextAlign::Center ? StringAlignmentCenter : StringAlignmentFar));
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    fmt.SetTrimming(ellipsis ? StringTrimmingEllipsisCharacter : StringTrimmingNone);
    SolidBrush brush(color);
    RectF rect(r.x, r.y, r.w, r.h);
    c.g().DrawString(text.c_str(), (INT)text.size(), font, rect, &fmt, &brush);
}

float MeasureText(Fonts& fonts, const std::wstring& text, float px, bool bold, bool icon) {
    if (text.empty()) return 0;
    Font* font = fonts.Get(px, bold, icon);
    if (!font) return 0;
    StringFormat fmt;
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsMeasureTrailingSpaces);
    RectF bounds;
    static Bitmap measureBitmap(1, 1, PixelFormat32bppPARGB);
    static Graphics measureGraphics(&measureBitmap);
    measureGraphics.MeasureString(text.c_str(), (INT)text.size(), font, PointF(0, 0), &fmt, &bounds);
    return bounds.Width;
}

void DrawShadow(Canvas& c, const Rect& card, float radius) {
    for (int i = 9; i >= 1; --i) {
        Rect r = card.Inflate((float)i * 0.9f);
        r.y += i * 0.5f;
        FillRound(c, r, radius + i * 0.7f, Color(5, 0, 0, 0));
    }
    Rect contact = card;
    contact.y += 2.0f;
    FillRound(c, contact, radius, Color(22, 0, 0, 0));
}

}  // namespace hf