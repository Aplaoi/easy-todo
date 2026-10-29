#pragma once

#include "common.h"

namespace hf {

struct Palette {
    Gdiplus::Color card;
    Gdiplus::Color cardSolid;
    Gdiplus::Color border;
    Gdiplus::Color text;
    Gdiplus::Color subtext;
    Gdiplus::Color faint;
    Gdiplus::Color accent;
    Gdiplus::Color accentSoft;
    Gdiplus::Color blue;
    Gdiplus::Color cyanSoft;
    Gdiplus::Color track;
    Gdiplus::Color closeHover;
    Gdiplus::Color hover;
    Gdiplus::Color field;
    Gdiplus::Color divider;
    Gdiplus::Color overdue;
    Gdiplus::Color soon;
    Gdiplus::Color exam;
    Gdiplus::Color examSoft;
    Gdiplus::Color normal;
    Gdiplus::Color done;
    Gdiplus::Color white;
    Gdiplus::Color toast;
    Gdiplus::Color banner;
    Gdiplus::Color bannerText;
};

Palette LightPalette();
Palette DarkPalette();

class Fonts {
public:
    void Clear();
    Gdiplus::Font* Get(float px, bool bold = false, bool icon = false);

private:
    std::map<int, std::unique_ptr<Gdiplus::Font>> cache_;
    Gdiplus::FontFamily ui_{L"Segoe UI Variable Text"};
    Gdiplus::FontFamily uiFallback_{L"Microsoft YaHei UI"};
    Gdiplus::FontFamily icon_{L"Segoe MDL2 Assets"};
};

class Canvas {
public:
    Canvas(int width, int height);
    ~Canvas();
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    bool ok() const { return ok_; }
    Gdiplus::Graphics& g() { return *graphics_; }
    int width() const { return width_; }
    int height() const { return height_; }

    void Fade(float alpha);
    bool Present(HWND hwnd, int x, int y);
    void Clear();
    void SavePixels(std::vector<BYTE>& out) const;
    void CopyPixels(const std::vector<BYTE>& src, int dx, int dy);

private:
    int width_ = 0;
    int height_ = 0;
    bool ok_ = false;
    HDC screenDc_ = nullptr;
    HDC memDc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HBITMAP oldBitmap_ = nullptr;
    void* bits_ = nullptr;
    std::unique_ptr<Gdiplus::Bitmap> gdiBitmap_;
    std::unique_ptr<Gdiplus::Graphics> graphics_;
};

enum class TextAlign { Left, Center, Right };

void FillRound(Canvas& c, const Rect& r, float radius, Gdiplus::Color color);
void StrokeRound(Canvas& c, const Rect& r, float radius, Gdiplus::Color color, float width);
void FillCircle(Canvas& c, float cx, float cy, float radius, Gdiplus::Color color);
void DrawLine(Canvas& c, float x1, float y1, float x2, float y2, Gdiplus::Color color, float width);
void DrawText(Canvas& c, Fonts& fonts, const std::wstring& text, const Rect& r,
              Gdiplus::Color color, float px, bool bold = false, TextAlign align = TextAlign::Left,
              bool ellipsis = true, bool icon = false);
float MeasureText(Fonts& fonts, const std::wstring& text, float px, bool bold = false,
                  bool icon = false);
void DrawShadow(Canvas& c, const Rect& card, float radius);

}  // namespace hf