#include "paint.h"

namespace hf {

using namespace Gdiplus;

Palette LightPalette() {
    Palette p;
    p.card = Color(250, 255, 255, 255);
    p.cardSolid = Color(255, 255, 255, 255);
    p.border = Color(255, 218, 228, 238);
    p.text = Color(255, 15, 23, 34);
    p.subtext = Color(255, 92, 107, 126);
    p.faint = Color(255, 138, 153, 170);
    p.accent = Color(255, 14, 165, 198);
    p.accentSoft = Color(255, 230, 248, 252);
    p.blue = Color(255, 59, 111, 245);
    p.cyanSoft = Color(255, 221, 247, 252);
    p.track = Color(255, 227, 235, 243);
    p.closeHover = Color(255, 253, 235, 236);
    p.hover = Color(255, 238, 244, 249);
    p.field = Color(255, 242, 246, 250);
    p.divider = Color(255, 228, 235, 242);
    p.overdue = Color(255, 217, 59, 72);
    p.soon = Color(255, 199, 119, 0);
    p.exam = Color(255, 109, 74, 224);
    p.examSoft = Color(255, 240, 236, 255);
    p.normal = Color(255, 92, 107, 126);
    p.done = Color(255, 21, 154, 114);
    p.white = Color(255, 255, 255, 255);
    p.toast = Color(248, 20, 32, 44);
    p.banner = Color(255, 253, 242, 232);
    p.bannerText = Color(255, 138, 75, 22);
    return p;
}

Palette DarkPalette() {
    Palette p;
    p.card = Color(250, 17, 26, 36);
    p.cardSolid = Color(255, 17, 26, 36);
    p.border = Color(255, 38, 56, 74);
    p.text = Color(255, 242, 247, 252);
    p.subtext = Color(255, 155, 168, 183);
    p.faint = Color(255, 100, 115, 135);
    p.accent = Color(255, 79, 215, 255);
    p.accentSoft = Color(255, 18, 49, 61);
    p.blue = Color(255, 92, 141, 255);
    p.cyanSoft = Color(255, 19, 50, 62);
    p.track = Color(255, 26, 38, 52);
    p.closeHover = Color(255, 58, 32, 40);
    p.hover = Color(255, 30, 43, 58);
    p.field = Color(255, 23, 33, 45);
    p.divider = Color(255, 31, 44, 58);
    p.overdue = Color(255, 255, 93, 108);
    p.soon = Color(255, 255, 181, 71);
    p.exam = Color(255, 140, 107, 255);
    p.examSoft = Color(255, 36, 31, 58);
    p.normal = Color(255, 139, 153, 170);
    p.done = Color(255, 68, 215, 168);
    p.white = Color(255, 255, 255, 255);
    p.toast = Color(248, 12, 18, 26);
    p.banner = Color(255, 43, 28, 20);
    p.bannerText = Color(255, 255, 179, 138);
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
    BYTE table[256];
    for (int i = 0; i < 256; ++i) table[i] = (BYTE)((i * k + 127) / 255);
    BYTE* p = static_cast<BYTE*>(bits_);
    size_t count = (size_t)width_ * height_;
    for (size_t i = 0; i < count; ++i, p += 4) {
        p[0] = table[p[0]];
        p[1] = table[p[1]];
        p[2] = table[p[2]];
        p[3] = table[p[3]];
    }
}

void Canvas::Clear() {
    if (!graphics_) return;
    graphics_->ResetTransform();
    graphics_->ResetClip();
    graphics_->Clear(Color(0, 0, 0, 0));
}

void Canvas::SavePixels(std::vector<BYTE>& out) const {
    out.assign((size_t)width_ * height_ * 4, 0);
    if (bits_ && !out.empty()) memcpy(out.data(), bits_, out.size());
}

void Canvas::CopyPixels(const std::vector<BYTE>& src, int dx, int dy) {
    if (!bits_ || src.size() != (size_t)width_ * height_ * 4) return;
    int sx0 = std::max(0, -dx);
    int sy0 = std::max(0, -dy);
    int sx1 = std::min(width_, width_ - dx);
    int sy1 = std::min(height_, height_ - dy);
    if (sx1 <= sx0 || sy1 <= sy0) return;
    size_t rowBytes = (size_t)(sx1 - sx0) * 4;
    for (int sy = sy0; sy < sy1; ++sy) {
        const BYTE* s = src.data() + ((size_t)sy * width_ + sx0) * 4;
        BYTE* d = static_cast<BYTE*>(bits_) +
                  ((size_t)(sy + dy) * width_ + (sx0 + dx)) * 4;
        memcpy(d, s, rowBytes);
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
    for (int i = 12; i >= 1; --i) {
        Rect r = card.Inflate((float)i * 1.1f);
        r.y += i * 0.8f;
        FillRound(c, r, radius + i * 0.8f, Color(4, 0, 0, 0));
    }
    Rect contact = card;
    contact.y += 3.0f;
    FillRound(c, contact, radius, Color(12, 0, 0, 0));
}

}  // namespace hf