#include "app.h"

namespace hf {

using namespace Gdiplus;

namespace {

constexpr float kPanelWidth = 336.0f;
constexpr float kHeaderHeight = 56.0f;
constexpr float kFooterHeight = 40.0f;
constexpr float kRowHeight = 58.0f;
constexpr float kArchiveRowHeight = 62.0f;
constexpr float kSettingRowHeight = 46.0f;
constexpr int kSettingCount = 15;
constexpr UINT kTrayIdLocal = 1;
constexpr UINT kMenuEdit = 2001;
constexpr UINT kMenuComplete = 2002;
constexpr UINT kMenuCopy = 2003;
constexpr UINT kMenuDelete = 2004;

const wchar_t* SettingLabel(int i) {
    switch (i) {
        case 0: return L"开机自动启动";
        case 1: return L"开机后显示小球";
        case 2: return L"展开触发方式";
        case 3: return L"面板置顶";
        case 4: return L"主题";
        case 5: return L"小球显示内容";
        case 6: return L"小球尺寸";
        case 7: return L"贴边半隐藏";
        case 8: return L"减少动画";
        case 9: return L"小球透明度";
        case 10: return L"面板透明度";
        case 11: return L"重置小球位置";
        case 12: return L"查看归档";
        case 13: return L"打开数据目录";
        case 14: return L"最近备份";
        default: return L"";
    }
}

std::wstring SettingValue(const Settings& s, const std::wstring& lastBackup, int archiveCount, int i) {
    switch (i) {
        case 0: return s.app.autoStart ? L"开" : L"关";
        case 1: return s.app.showOnBoot ? L"开" : L"关";
        case 2: return s.app.trigger == L"hover" ? L"悬停" : L"仅点击";
        case 3: return s.panel.alwaysOnTop ? L"开" : L"关";
        case 4:
            if (s.app.theme == L"light") return L"浅色";
            if (s.app.theme == L"dark") return L"深色";
            return L"跟随系统";
        case 5: return s.ball.display == L"days" ? L"剩余天数" : L"纯图标";
        case 6: return Format(L"%d px", s.ball.size);
        case 7: return s.ball.halfHidden ? L"开" : L"关";
        case 8:
            if (s.app.reduceMotion == L"on") return L"强制减少";
            if (s.app.reduceMotion == L"off") return L"不减少";
            return L"跟随系统";
        case 9: return Format(L"%d%%", s.ball.opacity);
        case 10: return Format(L"%d%%", s.panel.opacity);
        case 11: return L"执行";
        case 12: return archiveCount > 0 ? Format(L"%d 项", archiveCount) : L"暂无";
        case 13: return L"打开";
        case 14: return lastBackup.empty() ? L"暂无" : lastBackup;
        default: return L"";
    }
}

bool IsToggleSetting(int i) {
    return i == 0 || i == 1 || i == 3 || i == 7;
}

bool IsDropdownSetting(int i) {
    return i == 2 || i == 4 || i == 5 || i == 8;
}

bool IsSliderSetting(int i) {
    return i == 6 || i == 9 || i == 10;
}

std::vector<std::wstring> SettingOptions(int i) {
    switch (i) {
        case 2: return {L"悬停", L"仅点击"};
        case 4: return {L"跟随系统", L"浅色", L"深色"};
        case 5: return {L"剩余天数", L"纯图标"};
        case 8: return {L"跟随系统", L"强制减少", L"不减少"};
        default: return {};
    }
}

int SettingOptionIndex(const Settings& s, int i) {
    switch (i) {
        case 2: return s.app.trigger == L"click" ? 1 : 0;
        case 4:
            if (s.app.theme == L"light") return 1;
            if (s.app.theme == L"dark") return 2;
            return 0;
        case 5: return s.ball.display == L"icon" ? 1 : 0;
        case 8:
            if (s.app.reduceMotion == L"on") return 1;
            if (s.app.reduceMotion == L"off") return 2;
            return 0;
        default: return 0;
    }
}

float SliderMin(int i) {
    switch (i) {
        case 6: return 48.0f;
        case 9: return 70.0f;
        case 10: return 80.0f;
        default: return 0.0f;
    }
}

float SliderMax(int i) {
    switch (i) {
        case 6: return 64.0f;
        case 9: return 100.0f;
        case 10: return 100.0f;
        default: return 100.0f;
    }
}

float SliderStep(int i) {
    return i == 6 ? 2.0f : 1.0f;
}

float SliderValue(const Settings& s, int i) {
    if (i == 6) return (float)s.ball.size;
    if (i == 9) return (float)s.ball.opacity;
    if (i == 10) return (float)s.panel.opacity;
    return 0.0f;
}

std::wstring CompletedLabel(const std::wstring& iso) {
    if (iso.size() >= 10) {
        int month = (iso[5] - L'0') * 10 + (iso[6] - L'0');
        int day = (iso[8] - L'0') * 10 + (iso[9] - L'0');
        if (month >= 1 && month <= 12 && day >= 1 && day <= 31)
            return Format(L"完成于 %d/%d", month, day);
    }
    return L"已完成";
}

std::wstring ExecutablePath() {
    wchar_t buf[MAX_PATH * 2]{};
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n == 0) return L"";
    return std::wstring(buf, n);
}

void IconButton(Canvas& c, Fonts& fonts, const Rect& r, const wchar_t* glyph, bool hovered,
                Color fg, Color hoverBg, float iconPx) {
    if (hovered) FillRound(c, r, r.h * 0.32f, hoverBg);
    DrawText(c, fonts, glyph, r, fg, iconPx, false, TextAlign::Center, false, true);
}

void Button(Canvas& c, Fonts& fonts, const Rect& r, const std::wstring& text, bool primary,
            bool hovered, const Palette& p, float radius, float fontPx) {
    Color bg = primary ? p.accent : p.field;
    Color fg = primary ? p.white : p.text;
    if (hovered) {
        if (primary)
            bg = Color(255, (BYTE)(p.accent.GetR() * 0.9f), (BYTE)(p.accent.GetG() * 0.9f),
                       (BYTE)(p.accent.GetB() * 0.9f));
        else
            bg = p.hover;
    }
    FillRound(c, r, radius, bg);
    if (!primary) StrokeRound(c, r, radius, p.border, std::max(1.0f, radius * 0.1f));
    DrawText(c, fonts, text, r, fg, fontPx, true, TextAlign::Center);
}

}  // namespace

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void App::BuildLayout() {
    layout_ = PanelLayout{};
    float sh = (float)shadowPx_;
    Rect card(sh, sh, (float)panelW_ - 2 * sh, (float)panelH_ - 2 * sh);
    layout_.card = card;
    layout_.header = Rect(card.x, card.y, card.w, S(kHeaderHeight));

    float bs = S(28);
    float gap = S(4);
    float right = card.x + card.w - S(14);
    layout_.btnClose = Rect(right - bs, card.y + S(12), bs, bs);
    right -= bs + gap;
    layout_.btnSettings = Rect(right - bs, card.y + S(12), bs, bs);
    right -= bs + gap;
    layout_.btnPin = Rect(right - bs, card.y + S(12), bs, bs);
    right -= bs + gap;
    layout_.btnAdd = Rect(right - bs, card.y + S(12), bs, bs);
    layout_.btnBack = Rect(card.x + S(12), card.y + S(12), bs, bs);

    if (view_ == View::List) {
        bool hasBanner = !saveError_.empty() || !dataNotice_.empty();
        float bannerH = hasBanner ? S(38) : 0.0f;
        if (hasBanner)
            layout_.banner = Rect(card.x + S(12), card.y + S(54), card.w - S(24), S(34));
        float contentTop = card.y + S(kHeaderHeight) + bannerH;
        float contentBottom = card.y + card.h - S(kFooterHeight);
        layout_.content =
            Rect(card.x + S(8), contentTop, card.w - S(16), contentBottom - contentTop);
        layout_.footer = Rect(card.x, card.y + card.h - S(kFooterHeight), card.w, S(kFooterHeight));
        layout_.archiveLink =
            Rect(card.x + S(18), layout_.footer.y, S(170), layout_.footer.h);
    } else {
        layout_.content = Rect(card.x + S(8), card.y + S(kHeaderHeight), card.w - S(16),
                               card.h - S(kHeaderHeight) - S(12));
    }

    ClampScroll();

    if (view_ == View::List) {
        auto todo = SortedTodo();
        float y = layout_.content.y + S(4) - listScroll_;
        for (const Task* t : todo) {
            RowHit hit;
            hit.id = t->id;
            hit.row = Rect(layout_.content.x, y, layout_.content.w, S(kRowHeight));
            hit.checkbox = Rect(hit.row.x + S(9), hit.row.y + S(19), S(20), S(20));
            hit.edit = Rect(hit.row.x + hit.row.w - S(44), hit.row.y + S(15), S(28), S(28));
            layout_.rows.push_back(hit);
            y += S(kRowHeight);
        }
        layout_.emptyAdd = Rect(card.centerX() - S(75), layout_.content.y + layout_.content.h * 0.64f,
                                S(150), S(36));
    } else if (view_ == View::Form) {
        float x = card.x + S(18);
        float w = card.w - S(36);
        layout_.titleField = Rect(x, card.y + S(86), w, S(42));
        layout_.dateValue = Rect(x, card.y + S(162), w - S(76), S(42));
        layout_.dateMinus = Rect(x + w - S(76), card.y + S(162), S(38), S(42));
        layout_.datePlus = Rect(x + w - S(38), card.y + S(162), S(38), S(42));
        layout_.chipToday = Rect(x, card.y + S(216), S(78), S(28));
        layout_.chipTomorrow = Rect(x + S(86), card.y + S(216), S(78), S(28));
        layout_.chipWeek = Rect(x + S(172), card.y + S(216), S(78), S(28));
        layout_.repeatField = Rect(x + S(44), card.y + S(252), S(150), S(30));
        layout_.btnCancel = Rect(card.x + card.w - S(18) - S(84), card.y + card.h - S(56), S(84),
                                 S(38));
        layout_.btnSave =
            Rect(layout_.btnCancel.x - S(12) - S(132), layout_.btnCancel.y, S(132), S(38));
    } else if (view_ == View::Archive) {
        layout_.btnClear = Rect(card.x + card.w - S(14) - S(52), card.y + S(12), S(52), S(28));
        auto archive = SortedArchive();
        float y = layout_.content.y + S(2) - archiveScroll_;
        for (const Task* t : archive) {
            RowHit hit;
            hit.id = t->id;
            hit.row = Rect(layout_.content.x, y, layout_.content.w, S(kArchiveRowHeight));
            hit.action1 = Rect(hit.row.x + hit.row.w - S(66), hit.row.y + S(17), S(28), S(28));
            hit.action2 = Rect(hit.row.x + hit.row.w - S(34), hit.row.y + S(17), S(28), S(28));
            layout_.rows.push_back(hit);
            y += S(kArchiveRowHeight);
        }
    } else if (view_ == View::DatePicker) {
        layout_.calPrev = Rect(card.x + S(18), card.y + S(64), S(32), S(32));
        layout_.calNext =
            Rect(card.x + card.w - S(18) - S(32), card.y + S(64), S(32), S(32));
        layout_.calTitle =
            Rect(card.x + S(56), card.y + S(64), card.w - S(112), S(32));
        layout_.calToday = Rect(card.x + S(18), card.y + card.h - S(56), S(120), S(36));
        layout_.calCancel =
            Rect(card.x + card.w - S(18) - S(90), card.y + card.h - S(56), S(90), S(36));

        int64_t first = DaysFromCivil(calYear_, calMonth_, 1);
        int weekday = (int)(((first + 3) % 7 + 7) % 7);  // Monday = 0
        int64_t start = first - weekday;
        float gridX = card.x + S(28);
        float gridY = card.y + S(128);
        float cellW = (card.w - S(56)) / 7.0f;
        float cellH = S(36);
        layout_.calendarDays.reserve(42);
        for (int i = 0; i < 42; ++i) {
            int row = i / 7;
            int col = i % 7;
            layout_.calendarDays.emplace_back(
                start + i, Rect(gridX + col * cellW, gridY + row * cellH, cellW, cellH));
        }
    } else {
        float y = layout_.content.y + S(4) - settingsScroll_;
        for (int i = 0; i < kSettingCount; ++i) {
            Rect row(layout_.content.x + S(4), y, layout_.content.w - S(8), S(kSettingRowHeight));
            layout_.settingRows.push_back(std::make_pair(i, row));
            float rowRight = row.x + row.w - S(16);
            if (IsToggleSetting(i)) {
                layout_.settingValues.push_back(std::make_pair(
                    i, Rect(rowRight - S(44), row.y + S(11), S(44), S(24))));
            } else if (IsDropdownSetting(i)) {
                layout_.settingValues.push_back(std::make_pair(
                    i, Rect(rowRight - S(132), row.y + S(8), S(132), S(30))));
            } else if (IsSliderSetting(i)) {
                layout_.settingValues.push_back(std::make_pair(
                    i, Rect(rowRight - S(164), row.y + S(17), S(100), S(12))));
            }
            y += S(kSettingRowHeight);
        }

        if (settingsDropdown_ >= 0) {
            const Rect* anchor = nullptr;
            for (const auto& entry : layout_.settingValues) {
                if (entry.first == settingsDropdown_) {
                    anchor = &entry.second;
                    break;
                }
            }
            auto options = SettingOptions(settingsDropdown_);
            if (anchor && !options.empty()) {
                float itemH = S(34);
                float width = S(152);
                float total = itemH * (float)options.size() + S(8);
                float x = card.x + card.w - S(20) - width;
                float yTop = anchor->y + anchor->h + S(6);
                if (yTop + total > card.y + card.h - S(10))
                    yTop = anchor->y - total - S(6);
                yTop = std::max(card.y + S(56), yTop);
                for (size_t k = 0; k < options.size(); ++k) {
                    layout_.dropdownOptions.push_back(std::make_pair(
                        (int)k, Rect(x + S(4), yTop + S(4) + itemH * (float)k, width - S(8),
                                     itemH)));
                }
            }
        }
    }

    float maxScroll = MaxScrollForView();
    if (maxScroll > 0.5f) {
        layout_.scrollbar =
            Rect(card.x + card.w - S(6), layout_.content.y, S(3), layout_.content.h);
    }
}

float App::ScrollForView() const {
    switch (view_) {
        case View::Archive: return archiveScroll_;
        case View::Settings: return settingsScroll_;
        case View::Form:
        case View::DatePicker: return 0;
        default: return listScroll_;
    }
}

void App::SetScrollForView(float value) {
    switch (view_) {
        case View::Archive: archiveScroll_ = value; break;
        case View::Settings: settingsScroll_ = value; break;
        case View::Form:
        case View::DatePicker: break;
        default: listScroll_ = value; break;
    }
}

float App::MaxScrollForView() const {
    if (view_ == View::Form || view_ == View::DatePicker) return 0;
    float total = 0;
    if (view_ == View::List)
        total = (float)SortedTodo().size() * S(kRowHeight) + S(8);
    else if (view_ == View::Archive)
        total = (float)SortedArchive().size() * S(kArchiveRowHeight) + S(8);
    else if (view_ == View::Settings)
        total = (float)kSettingCount * S(kSettingRowHeight) + S(8);
    return std::max(0.0f, total - layout_.content.h);
}

void App::ClampScroll() {
    if (view_ == View::Form || view_ == View::DatePicker) return;
    float maxScroll = MaxScrollForView();
    if (maxScroll <= 0) {
        SetScrollForView(0);
        return;
    }
    SetScrollForView(std::clamp(ScrollForView(), 0.0f, maxScroll));
}

App::Control App::HitTest(POINT pt, std::wstring* taskId) const {
    if (taskId) taskId->clear();
    if (!layout_.card.Contains((float)pt.x, (float)pt.y)) return C_NONE;

    if (view_ == View::Form) {
        if (layout_.btnBack.Contains(pt)) return C_BACK;
        if (layout_.btnSave.Contains(pt)) return C_SAVE;
        if (layout_.btnCancel.Contains(pt)) return C_CANCEL;
        if (layout_.titleField.Contains(pt)) return C_TITLE_FIELD;
        if (layout_.dateMinus.Contains(pt)) return C_DATE_MINUS;
        if (layout_.datePlus.Contains(pt)) return C_DATE_PLUS;
        if (layout_.dateValue.Contains(pt)) return C_DATE_FIELD;
        if (layout_.chipToday.Contains(pt)) return C_CHIP_TODAY;
        if (layout_.chipTomorrow.Contains(pt)) return C_CHIP_TOMORROW;
        if (layout_.chipWeek.Contains(pt)) return C_CHIP_WEEK;
        if (layout_.repeatField.Contains(pt)) return C_REPEAT;
        return C_NONE;
    }

    if (view_ == View::DatePicker) {
        if (layout_.btnBack.Contains(pt)) return C_BACK;
        if (layout_.calPrev.Contains(pt)) return C_CAL_PREV;
        if (layout_.calNext.Contains(pt)) return C_CAL_NEXT;
        if (layout_.calToday.Contains(pt)) return C_CAL_TODAY;
        if (layout_.calCancel.Contains(pt)) return C_CAL_CANCEL;
        for (const auto& cell : layout_.calendarDays) {
            if (cell.second.Contains(pt)) {
                if (taskId) *taskId = std::to_wstring(cell.first);
                return C_CAL_DAY;
            }
        }
        return C_NONE;
    }

    if (view_ == View::Archive) {
        if (layout_.btnBack.Contains(pt)) return C_BACK;
        if (!SortedArchive().empty() && layout_.btnClear.Contains(pt)) return C_CLEAR_ARCHIVE;
        if (!layout_.content.Contains(pt)) return C_NONE;
        for (const auto& row : layout_.rows) {
            if (row.action1.Contains(pt)) {
                if (taskId) *taskId = row.id;
                return C_RESTORE;
            }
            if (row.action2.Contains(pt)) {
                if (taskId) *taskId = row.id;
                return C_DELETE;
            }
            if (row.row.Contains(pt)) {
                if (taskId) *taskId = row.id;
                return C_ROW;
            }
        }
        return C_NONE;
    }

    if (view_ == View::Settings) {
        if (layout_.btnBack.Contains(pt)) return C_BACK;
        for (const auto& entry : layout_.dropdownOptions) {
            if (entry.second.Contains(pt)) {
                if (taskId) *taskId = Format(L"dd:%d", entry.first);
                return C_SETTING_OPTION;
            }
        }
        if (!layout_.content.Contains(pt)) return C_NONE;
        for (const auto& entry : layout_.settingRows) {
            if (entry.first == 14) continue;
            if (entry.second.Contains(pt)) {
                if (taskId) *taskId = Format(L"setting:%d", entry.first);
                if (IsSliderSetting(entry.first)) {
                    for (const auto& value : layout_.settingValues) {
                        if (value.first != entry.first) continue;
                        Rect hit(value.second.x - S(12), entry.second.y,
                                 entry.second.x + entry.second.w - value.second.x + S(12),
                                 entry.second.h);
                        if (hit.Contains(pt)) return C_SLIDER;
                    }
                }
                return C_SETTING;
            }
        }
        return C_NONE;
    }

    // List view
    if (!saveError_.empty() || !dataNotice_.empty()) {
        if (layout_.banner.Contains(pt)) return C_BANNER;
    }
    if (layout_.btnAdd.Contains(pt)) return C_ADD;
    if (layout_.btnPin.Contains(pt)) return C_PIN;
    if (layout_.btnSettings.Contains(pt)) return C_SETTINGS;
    if (layout_.btnClose.Contains(pt)) return C_CLOSE;
    if (layout_.archiveLink.Contains(pt)) return C_ARCHIVE_LINK;
    if (SortedTodo().empty()) {
        if (layout_.emptyAdd.Contains(pt)) return C_EMPTY_ADD;
        return C_NONE;
    }
    if (!layout_.content.Contains(pt)) return C_NONE;
    for (const auto& row : layout_.rows) {
        if (row.edit.Contains(pt)) {
            if (taskId) *taskId = row.id;
            return C_EDIT;
        }
        if (row.checkbox.Contains(pt)) {
            if (taskId) *taskId = row.id;
            return C_CHECKBOX;
        }
        if (row.row.Contains(pt)) {
            if (taskId) *taskId = row.id;
            return C_ROW;
        }
    }
    return C_NONE;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

Palette App::CurrentPalette() const { return lastDark_ ? DarkPalette() : LightPalette(); }

void App::PaintPanel() {
    if (!panelVisible_ || !panel_) return;
    if (panelW_ <= 0 || panelH_ <= 0) return;

    RECT wr{};
    GetWindowRect(panel_, &wr);
    Canvas c(panelW_, panelH_);
    if (!c.ok()) return;

    Palette p = CurrentPalette();
    float slideX = 0.0f, slideY = 0.0f;
    if (panelAnimating_ && animT_ < 1.0f) {
        float k = 1.0f - animT_;
        if (settings_.ball.edge == L"right") slideX = S(10) * k;
        else if (settings_.ball.edge == L"left") slideX = -S(10) * k;
        else if (settings_.ball.edge == L"top") slideY = -S(10) * k;
        else slideY = S(10) * k;
    }
    c.g().TranslateTransform(slideX, slideY);

    DrawShadow(c, layout_.card, S(14));
    FillRound(c, layout_.card, S(14), p.card);
    StrokeRound(c, layout_.card, S(14), p.border, 1.0f);

    PaintPanelView(c, p);
    DrawToast(c, p);
    c.g().ResetTransform();

    float alpha = std::clamp(settings_.panel.opacity / 100.0f * animT_, 0.0f, 1.0f);
    c.Fade(alpha);
    c.Present(panel_, wr.left, wr.top);
}

void App::PaintPanelView(Canvas& c, const Palette& p) {
    switch (view_) {
        case View::List: DrawPanelList(c, p); break;
        case View::Form: DrawPanelForm(c, p); break;
        case View::DatePicker: DrawPanelDatePicker(c, p); break;
        case View::Archive: DrawPanelArchive(c, p); break;
        case View::Settings: DrawPanelSettings(c, p); break;
    }
}

void App::DrawHeaderButtons(Canvas& c, const Palette& p) {
    float iconPx = S(14);
    IconButton(c, fonts_, layout_.btnAdd, L"\xE710", hoverControl_ == C_ADD, p.subtext, p.hover,
               iconPx);
    IconButton(c, fonts_, layout_.btnPin, panelPinned_ ? L"\xE77A" : L"\xE718",
               hoverControl_ == C_PIN, panelPinned_ ? p.accent : p.subtext, p.hover, iconPx);
    IconButton(c, fonts_, layout_.btnSettings, L"\xE713", hoverControl_ == C_SETTINGS, p.subtext,
               p.hover, iconPx);
    IconButton(c, fonts_, layout_.btnClose, L"\xE711", hoverControl_ == C_CLOSE, p.subtext, p.hover,
               iconPx);
}

void App::DrawFooter(Canvas& c, const Palette& p) {
    Rect footer = layout_.footer;
    if (footer.h <= 0) return;
    DrawLine(c, layout_.card.x + S(18), footer.y, layout_.card.x + layout_.card.w - S(18), footer.y,
             p.divider, 1.0f);
    auto archive = SortedArchive();
    std::wstring text = archive.empty() ? L"查看归档" : Format(L"查看归档 · %d", (int)archive.size());
    Color color = hoverControl_ == C_ARCHIVE_LINK ? p.accent : p.subtext;
    DrawText(c, fonts_, text, Rect(footer.x + S(18), footer.y, S(180), footer.h), color, S(12),
             false, TextAlign::Left);
    DrawText(c, fonts_, L"\xE76C", Rect(footer.x + footer.w - S(36), footer.y, S(16), footer.h),
             p.faint, S(11), false, TextAlign::Center, false, true);
}

void App::DrawPanelList(Canvas& c, const Palette& p) {
    Rect card = layout_.card;
    DrawText(c, fonts_, L"待完成作业", Rect(card.x + S(18), card.y + S(12), S(160), S(30)), p.text,
             S(15), true);
    DrawHeaderButtons(c, p);
    DrawLine(c, card.x + S(18), card.y + S(52), card.x + card.w - S(18), card.y + S(52), p.divider,
             1.0f);

    if (!saveError_.empty() || !dataNotice_.empty()) {
        bool isError = !saveError_.empty();
        Color bg = isError ? Color(40, p.overdue.GetR(), p.overdue.GetG(), p.overdue.GetB())
                           : p.banner;
        FillRound(c, layout_.banner, S(9), bg);
        std::wstring text = isError ? saveError_ : dataNotice_;
        Color fg = isError ? p.overdue : p.bannerText;
        DrawText(c, fonts_, text,
                 Rect(layout_.banner.x + S(10), layout_.banner.y, layout_.banner.w - S(20),
                      layout_.banner.h),
                 fg, S(10.5f), false, TextAlign::Left);
    }

    auto todo = SortedTodo();
    if (todo.empty()) {
        Rect content = layout_.content;
        float cx = content.centerX();
        float cy = content.y + content.h * 0.34f;
        FillCircle(c, cx, cy, S(26), p.accentSoft);
        DrawText(c, fonts_, L"\xE73E", Rect(cx - S(18), cy - S(18), S(36), S(36)), p.accent, S(23),
                 false, TextAlign::Center, false, true);
        DrawText(c, fonts_, L"暂无待完成作业", Rect(content.x, cy + S(38), content.w, S(26)), p.text,
                 S(13), true, TextAlign::Center);
        DrawText(c, fonts_, L"添加一条，让今天更有条理", Rect(content.x, cy + S(62), content.w, S(22)),
                 p.subtext, S(11), false, TextAlign::Center);
        Button(c, fonts_, layout_.emptyAdd, L"添加作业", true, hoverControl_ == C_EMPTY_ADD, p, S(10), S(13));
        DrawFooter(c, p);
        return;
    }

    c.g().SetClip(RectF(layout_.content.x, layout_.content.y, layout_.content.w,
                        layout_.content.h));
    int64_t today = TodayLocalDay();
    for (size_t i = 0; i < todo.size(); ++i) {
        const Task* t = todo[i];
        const RowHit& hit = layout_.rows[i];
        if (hit.row.y + hit.row.h < layout_.content.y || hit.row.y > layout_.content.y + layout_.content.h)
            continue;
        bool rowHover = hoverTaskId_ == t->id &&
                        (hoverControl_ == C_ROW || hoverControl_ == C_CHECKBOX ||
                         hoverControl_ == C_EDIT);
        if (rowHover) FillRound(c, hit.row.Inflate(-S(2)), S(10), p.hover);

        float completeT = 0.0f;
        if (completing_ && t->id == completingId_)
            completeT = std::clamp((NowMs() - completeStart_) / 320.0f, 0.0f, 1.0f);

        Color checkBorder = (rowHover || completeT > 0) ? p.accent : p.faint;
        FillCircle(c, hit.checkbox.centerX(), hit.checkbox.centerY(), S(9), Color(0, 0, 0, 0));
        StrokeRound(c, hit.checkbox, S(10), checkBorder, 1.5f);
        if (rowHover || completeT > 0)
            DrawText(c, fonts_, L"\xE73E",
                     Rect(hit.checkbox.x + S(3), hit.checkbox.y + S(3), S(14), S(14)), p.accent,
                     S(10), false, TextAlign::Center, false, true);

        float textX = hit.checkbox.x + S(30);
        float textRight = hit.edit.x - S(8);
        float textW = textRight - textX;
        int titleAlpha = (int)(255.0f * (1.0f - 0.35f * completeT));
        Color titleColor((BYTE)titleAlpha, p.text.GetR(), p.text.GetG(), p.text.GetB());
        DrawText(c, fonts_, t->title, Rect(textX, hit.row.y + S(7), textW, S(24)), titleColor,
                 S(13.5f), false, TextAlign::Left);
        IconButton(c, fonts_, hit.edit, L"\xE70F",
                   hoverTaskId_ == t->id && hoverControl_ == C_EDIT,
                   hoverTaskId_ == t->id && hoverControl_ == C_EDIT ? p.accent : p.faint, p.hover,
                   S(12));
        DueTone tone;
        std::wstring label = DueLabel(t->dueDay, today, tone);
        std::wstring dateText = L"截止 " + FormatMMDD(t->dueDay);
        float dateW = MeasureText(fonts_, dateText, S(10.5f));
        Color toneColor = tone == DueTone::Overdue ? p.overdue
                          : tone == DueTone::Soon  ? p.soon
                                                   : p.accent;
        DrawText(c, fonts_, dateText,
                 Rect(textX, hit.row.y + S(31), dateW + S(4), S(18)), p.subtext, S(10.5f));
        float pillX = textX + dateW + S(8);
        float pillW = MeasureText(fonts_, label, S(10.5f)) + S(14);
        pillW = std::min(pillW, textRight - pillX);
        if (pillW > S(18)) {
            Rect pill(pillX, hit.row.y + S(27), pillW, S(22));
            Color pillBg = tone == DueTone::Overdue
                               ? Color(34, p.overdue.GetR(), p.overdue.GetG(), p.overdue.GetB())
                               : tone == DueTone::Soon
                                     ? Color(38, p.soon.GetR(), p.soon.GetG(), p.soon.GetB())
                                     : p.accentSoft;
            FillRound(c, pill, S(11), pillBg);
            DrawText(c, fonts_, label,
                     Rect(pill.x + S(7), pill.y, pill.w - S(14), pill.h), toneColor, S(10.5f),
                     true);
            if (!t->repeat.empty()) {
                float rx = pill.x + pill.w + S(8);
                if (rx + S(38) <= textRight) {
                    DrawText(c, fonts_, L"\xE8EE", Rect(rx, pill.y, S(14), pill.h), p.accent,
                             S(10), false, TextAlign::Center, false, true);
                    DrawText(c, fonts_, RepeatShort(t->repeat),
                             Rect(rx + S(15), pill.y, S(32), pill.h), p.accent, S(9.5f), true);
                }
            }
        }
        if (completeT > 0) {
            float titleW = std::min(MeasureText(fonts_, t->title, S(13.5f)), textW);
            DrawLine(c, textX, hit.row.y + S(19), textX + titleW * completeT,
                     hit.row.y + S(19), p.subtext, S(1.4f));
        }
        if (i + 1 < todo.size())
            DrawLine(c, textX, hit.row.y + hit.row.h, hit.row.x + hit.row.w - S(14),
                     hit.row.y + hit.row.h, p.divider, 1.0f);
    }
    c.g().ResetClip();
    DrawFooter(c, p);
    if (layout_.scrollbar.w > 0) {
        float maxScroll = MaxScrollForView();
        float total = layout_.content.h + maxScroll;
        float thumbH = std::max(S(24), layout_.content.h * layout_.content.h / total);
        float thumbT = maxScroll > 0 ? listScroll_ / maxScroll : 0;
        float thumbY = layout_.content.y + (layout_.content.h - thumbH) * thumbT;
        FillRound(c, Rect(layout_.scrollbar.x, thumbY, layout_.scrollbar.w, thumbH), S(2),
                  Color(90, p.faint.GetR(), p.faint.GetG(), p.faint.GetB()));
    }
}

void App::DrawPanelForm(Canvas& c, const Palette& p) {
    Rect card = layout_.card;
    IconButton(c, fonts_, layout_.btnBack, L"\xE72B", hoverControl_ == C_BACK, p.subtext, p.hover,
               S(14));
    DrawText(c, fonts_, formEditing_ ? L"编辑作业" : L"新增作业",
             Rect(card.x + S(52), card.y + S(12), card.w - S(70), S(30)), p.text, S(15), true);
    DrawLine(c, card.x + S(18), card.y + S(52), card.x + card.w - S(18), card.y + S(52), p.divider,
             1.0f);

    DrawText(c, fonts_, L"作业标题", Rect(card.x + S(18), card.y + S(64), card.w - S(36), S(18)),
             p.subtext, S(11));

    Rect field = layout_.titleField;
    FillRound(c, field, S(10), p.field);
    StrokeRound(c, field, S(10), p.border, 1.0f);
    if (GetFocus() == panel_)
        StrokeRound(c, field, S(10),
                    Color(170, p.accent.GetR(), p.accent.GetG(), p.accent.GetB()), 1.4f);

    float fontPx = S(14);
    float innerW = field.w - S(28);
    std::wstring prefix = formTitle_.substr(0, (size_t)std::min<int>(caret_, (int)formTitle_.size()));
    float prefixW = MeasureText(fonts_, prefix, fontPx);
    float compW = MeasureText(fonts_, imeComp_, fontPx);
    float fullW = MeasureText(fonts_, formTitle_, fontPx);
    float maxScroll = std::max(0.0f, fullW - innerW);
    float textScroll = std::clamp(prefixW + compW - innerW + S(2), 0.0f, maxScroll);
    float textX = field.x + S(14) - textScroll;

    c.g().SetClip(RectF(field.x + S(10), field.y + S(2), field.w - S(20), field.h - S(4)));
    if (formTitle_.empty() && imeComp_.empty()) {
        DrawText(c, fonts_, L"例如：数学卷子",
                 Rect(textX + S(1), field.y, field.w - S(24), field.h), p.faint, fontPx);
    } else {
        std::wstring display =
            formTitle_.substr(0, (size_t)caret_) + imeComp_ + formTitle_.substr((size_t)caret_);
        DrawText(c, fonts_, display, Rect(textX, field.y, fullW + compW + S(4), field.h), p.text,
                 fontPx);
        if (!imeComp_.empty()) {
            DrawLine(c, textX + prefixW, field.y + field.h - S(10), textX + prefixW + compW,
                     field.y + field.h - S(10), p.accent, std::max(1.0f, S(1)));
        }
    }
    if (caretOn_ && imeComp_.empty()) {
        float cx = textX + prefixW;
        DrawLine(c, cx, field.y + S(11), cx, field.y + field.h - S(11), p.accent,
                 std::max(1.0f, S(1.5f)));
    }
    c.g().ResetClip();

    DrawText(c, fonts_, L"截止日期", Rect(card.x + S(18), card.y + S(140), card.w - S(36), S(18)),
             p.subtext, S(11));

    Rect dateRow = Rect(layout_.dateValue.x, layout_.dateValue.y, layout_.dateValue.w + S(76),
                        layout_.dateValue.h);
    FillRound(c, dateRow, S(10), p.field);
    StrokeRound(c, dateRow, S(10), p.border, 1.0f);

    std::wstring dateText = FormatFullDate(formDue_) + L" · " + WeekdayCN(formDue_);
    if (formDue_ == TodayLocalDay()) dateText += L"（今天）";
    if (hoverControl_ == C_DATE_FIELD)
        FillRound(c, layout_.dateValue, S(9), Color(24, p.accent.GetR(), p.accent.GetG(),
                                                    p.accent.GetB()));
    DrawText(c, fonts_, dateText,
             Rect(layout_.dateValue.x + S(14), layout_.dateValue.y, layout_.dateValue.w - S(48),
                  layout_.dateValue.h),
             p.text, S(12.5f));
    DrawText(c, fonts_, L"\xE787",
             Rect(layout_.dateValue.x + layout_.dateValue.w - S(32), layout_.dateValue.y, S(24),
                  layout_.dateValue.h),
             hoverControl_ == C_DATE_FIELD ? p.accent : p.faint, S(14), false, TextAlign::Center,
             false, true);

    if (hoverControl_ == C_DATE_MINUS)
        FillRound(c, layout_.dateMinus, S(9), p.hover);
    if (hoverControl_ == C_DATE_PLUS) FillRound(c, layout_.datePlus, S(9), p.hover);
    DrawText(c, fonts_, L"−",
             Rect(layout_.dateMinus.x, layout_.dateMinus.y, layout_.dateMinus.w,
                  layout_.dateMinus.h),
             p.subtext, S(17), false, TextAlign::Center);
    DrawText(c, fonts_, L"+",
             Rect(layout_.datePlus.x, layout_.datePlus.y, layout_.datePlus.w, layout_.datePlus.h),
             p.subtext, S(16), false, TextAlign::Center);
    DrawLine(c, layout_.dateMinus.x, layout_.dateMinus.y + S(10), layout_.dateMinus.x,
             layout_.dateMinus.y + layout_.dateMinus.h - S(10), p.divider, 1.0f);

    int64_t today = TodayLocalDay();
    auto drawChip = [&](const Rect& r, const std::wstring& text, bool active, int control) {
        Color bg = active ? p.accentSoft : p.field;
        Color fg = active ? p.accent : p.subtext;
        if (hoverControl_ == control) bg = active ? p.accentSoft : p.hover;
        FillRound(c, r, S(9), bg);
        DrawText(c, fonts_, text, r, fg, S(11), active, TextAlign::Center);
    };
    drawChip(layout_.chipToday, L"今天", formDue_ == today, C_CHIP_TODAY);
    drawChip(layout_.chipTomorrow, L"明天", formDue_ == today + 1, C_CHIP_TOMORROW);
    drawChip(layout_.chipWeek, L"一周后", formDue_ == today + 7, C_CHIP_WEEK);

    Rect repeat = layout_.repeatField;
    DrawText(c, fonts_, L"重复", Rect(card.x + S(18), repeat.y, S(38), repeat.h), p.subtext,
             S(10.5f));
    FillRound(c, repeat, S(9), hoverControl_ == C_REPEAT ? p.hover : p.field);
    StrokeRound(c, repeat, S(9), p.border, 1.0f);
    std::wstring repeatText = RepeatShort(formRepeat_);
    if (formRepeat_ == L"weekly") repeatText = L"每" + WeekdayCN(formDue_);
    DrawText(c, fonts_, repeatText,
             Rect(repeat.x + S(12), repeat.y, repeat.w - S(34), repeat.h), p.text, S(11));
    DrawText(c, fonts_, L"\xE8EE",
             Rect(repeat.x + repeat.w - S(24), repeat.y, S(16), repeat.h), p.faint, S(10), false,
             TextAlign::Center, false, true);

    if (!formError_.empty()) {
        DrawText(c, fonts_, formError_,
                 Rect(card.x + S(18), card.y + S(292), card.w - S(36), S(20)), p.overdue, S(10.5f));
    } else {
        DrawText(c, fonts_,
                 formRepeat_.empty() ? L"仅按自然日计算，不设置具体时间"
                                     : L"完成后自动生成下一次",
                 Rect(card.x + S(18), card.y + S(292), card.w - S(36), S(20)), p.faint, S(10.5f));
    }

    Button(c, fonts_, layout_.btnSave, L"保存", true, hoverControl_ == C_SAVE, p, S(10), S(13));
    Button(c, fonts_, layout_.btnCancel, L"取消", false, hoverControl_ == C_CANCEL, p, S(10), S(13));
}

void App::DrawPanelDatePicker(Canvas& c, const Palette& p) {
    Rect card = layout_.card;
    IconButton(c, fonts_, layout_.btnBack, L"\xE72B", hoverControl_ == C_BACK, p.subtext, p.hover,
               S(14));
    DrawText(c, fonts_, L"选择截止日期",
             Rect(card.x + S(52), card.y + S(12), card.w - S(70), S(30)), p.text, S(15), true);
    DrawLine(c, card.x + S(18), card.y + S(52), card.x + card.w - S(18), card.y + S(52), p.divider,
             1.0f);

    if (hoverControl_ == C_CAL_PREV) FillRound(c, layout_.calPrev, S(8), p.hover);
    if (hoverControl_ == C_CAL_NEXT) FillRound(c, layout_.calNext, S(8), p.hover);
    DrawText(c, fonts_, L"\xE76B", layout_.calPrev, p.subtext, S(12), false, TextAlign::Center,
             false, true);
    DrawText(c, fonts_, L"\xE76C", layout_.calNext, p.subtext, S(12), false, TextAlign::Center,
             false, true);
    DrawText(c, fonts_, Format(L"%d 年 %d 月", calYear_, calMonth_), layout_.calTitle, p.text,
             S(14), true, TextAlign::Center);

    const wchar_t* weekdays[] = {L"一", L"二", L"三", L"四", L"五", L"六", L"日"};
    float gridX = layout_.calendarDays.empty() ? card.x + S(28) : layout_.calendarDays[0].second.x;
    float gridY = layout_.calendarDays.empty() ? card.y + S(128) : layout_.calendarDays[0].second.y;
    float cellW = layout_.calendarDays.empty() ? S(40) : layout_.calendarDays[0].second.w;
    for (int i = 0; i < 7; ++i) {
        DrawText(c, fonts_, weekdays[i], Rect(gridX + i * cellW, gridY - S(24), cellW, S(20)),
                 p.faint, S(10.5f), false, TextAlign::Center);
    }

    int64_t today = TodayLocalDay();
    for (const auto& cell : layout_.calendarDays) {
        int y = 0, m = 0, d = 0;
        CivilFromDay(cell.first, y, m, d);
        bool inMonth = (y == calYear_ && m == calMonth_);
        bool selected = (cell.first == formDue_);
        bool hovered = hoverControl_ == C_CAL_DAY &&
                       hoverTaskId_ == std::to_wstring(cell.first);
        float cx = cell.second.centerX();
        float cy = cell.second.centerY();
        float radius = S(15);
        if (hovered && !selected) FillCircle(c, cx, cy, radius, p.hover);
        if (selected) {
            FillCircle(c, cx, cy, radius, p.accent);
        } else if (cell.first == today) {
            StrokeRound(c, Rect(cx - radius, cy - radius, radius * 2, radius * 2), radius, p.accent,
                        1.2f);
        }
        Color fg = selected ? p.white : (inMonth ? p.text : p.faint);
        DrawText(c, fonts_, std::to_wstring(d),
                 Rect(cell.second.x, cell.second.y, cell.second.w, cell.second.h), fg, S(11.5f),
                 selected, TextAlign::Center);
    }

    Button(c, fonts_, layout_.calToday, L"今天", true, hoverControl_ == C_CAL_TODAY, p, S(9),
           S(12.5f));
    Button(c, fonts_, layout_.calCancel, L"取消", false, hoverControl_ == C_CAL_CANCEL, p, S(9),
           S(12.5f));
}

void App::DrawPanelArchive(Canvas& c, const Palette& p) {
    Rect card = layout_.card;
    IconButton(c, fonts_, layout_.btnBack, L"\xE72B", hoverControl_ == C_BACK, p.subtext, p.hover,
               S(14));
    auto archive = SortedArchive();
    std::wstring title = archive.empty() ? L"归档" : Format(L"归档 · %d", (int)archive.size());
    DrawText(c, fonts_, title, Rect(card.x + S(52), card.y + S(12), card.w - S(150), S(30)), p.text,
             S(15), true);
    if (!archive.empty()) {
        if (hoverControl_ == C_CLEAR_ARCHIVE) {
            FillRound(c, layout_.btnClear, S(8), Color(30, p.overdue.GetR(), p.overdue.GetG(),
                                                       p.overdue.GetB()));
        }
        DrawText(c, fonts_, L"清空", layout_.btnClear, p.overdue, S(11.5f), false, TextAlign::Center);
    }
    DrawLine(c, card.x + S(18), card.y + S(52), card.x + card.w - S(18), card.y + S(52), p.divider,
             1.0f);

    if (archive.empty()) {
        Rect content = layout_.content;
        float cx = content.centerX();
        float cy = content.y + content.h * 0.40f;
        FillCircle(c, cx, cy, S(26), p.hover);
        DrawText(c, fonts_, L"\xE7B8", Rect(cx - S(18), cy - S(18), S(36), S(36)), p.faint, S(22),
                 false, TextAlign::Center, false, true);
        DrawText(c, fonts_, L"暂无归档作业", Rect(content.x, cy + S(38), content.w, S(26)), p.text,
                 S(13), true, TextAlign::Center);
        return;
    }

    c.g().SetClip(RectF(layout_.content.x, layout_.content.y, layout_.content.w,
                        layout_.content.h));
    for (size_t i = 0; i < archive.size(); ++i) {
        const Task* t = archive[i];
        const RowHit& hit = layout_.rows[i];
        if (hit.row.y + hit.row.h < layout_.content.y ||
            hit.row.y > layout_.content.y + layout_.content.h)
            continue;
        bool hover = hoverTaskId_ == t->id;
        if (hover) FillRound(c, hit.row.Inflate(-S(2)), S(10), p.hover);
        float textX = hit.row.x + S(14);
        DrawText(c, fonts_, t->title, Rect(textX, hit.row.y + S(9), hit.row.w - S(90), S(24)),
                 p.text, S(13.5f));
        DrawText(c, fonts_, CompletedLabel(t->completedAt),
                 Rect(textX, hit.row.y + S(33), hit.row.w - S(90), S(18)), p.subtext, S(11));
        if (hoverControl_ == C_RESTORE && hoverTaskId_ == t->id)
            FillRound(c, hit.action1, S(8), p.accentSoft);
        if (hoverControl_ == C_DELETE && hoverTaskId_ == t->id)
            FillRound(c, hit.action2, S(8),
                      Color(30, p.overdue.GetR(), p.overdue.GetG(), p.overdue.GetB()));
        DrawText(c, fonts_, L"\xE7A7",
                 Rect(hit.action1.x + S(4), hit.action1.y + S(4), S(20), S(20)), p.subtext, S(13),
                 false, TextAlign::Center, false, true);
        DrawText(c, fonts_, L"\xE74D",
                 Rect(hit.action2.x + S(4), hit.action2.y + S(4), S(20), S(20)), p.overdue, S(13),
                 false, TextAlign::Center, false, true);
        if (i + 1 < archive.size())
            DrawLine(c, textX, hit.row.y + hit.row.h, hit.row.x + hit.row.w - S(14),
                     hit.row.y + hit.row.h, p.divider, 1.0f);
    }
    c.g().ResetClip();
    if (layout_.scrollbar.w > 0) {
        float maxScroll = MaxScrollForView();
        float total = layout_.content.h + maxScroll;
        float thumbH = std::max(S(24), layout_.content.h * layout_.content.h / total);
        float t = maxScroll > 0 ? archiveScroll_ / maxScroll : 0;
        float thumbY = layout_.content.y + (layout_.content.h - thumbH) * t;
        FillRound(c, Rect(layout_.scrollbar.x, thumbY, layout_.scrollbar.w, thumbH), S(2),
                  Color(90, p.faint.GetR(), p.faint.GetG(), p.faint.GetB()));
    }
}

void App::DrawPanelSettings(Canvas& c, const Palette& p) {
    Rect card = layout_.card;
    IconButton(c, fonts_, layout_.btnBack, L"\xE72B", hoverControl_ == C_BACK, p.subtext, p.hover,
               S(14));
    DrawText(c, fonts_, L"设置", Rect(card.x + S(52), card.y + S(12), card.w - S(70), S(30)), p.text,
             S(15), true);
    DrawLine(c, card.x + S(18), card.y + S(52), card.x + card.w - S(18), card.y + S(52), p.divider,
             1.0f);

    int archiveCount = (int)SortedArchive().size();
    auto valueRect = [&](int row) -> Rect {
        for (const auto& entry : layout_.settingValues)
            if (entry.first == row) return entry.second;
        return Rect();
    };

    c.g().SetClip(RectF(layout_.content.x, layout_.content.y, layout_.content.w,
                        layout_.content.h));
    for (const auto& entry : layout_.settingRows) {
        int index = entry.first;
        const Rect& r = entry.second;
        if (r.y + r.h < layout_.content.y || r.y > layout_.content.y + layout_.content.h) continue;
        std::wstring settingId = Format(L"setting:%d", index);
        bool hover = hoverTaskId_ == settingId &&
                     (hoverControl_ == C_SETTING || hoverControl_ == C_SLIDER);
        if (hover) FillRound(c, Rect(r.x + S(2), r.y, r.w - S(4), r.h), S(8), p.hover);
        DrawText(c, fonts_, SettingLabel(index),
                 Rect(r.x + S(12), r.y, r.w - S(170), r.h), p.text, S(12.5f));

        std::wstring value = SettingValue(settings_, lastBackup_, archiveCount, index);
        if (IsToggleSetting(index)) {
            Rect sw = valueRect(index);
            bool on = (index == 0 && settings_.app.autoStart) ||
                      (index == 1 && settings_.app.showOnBoot) ||
                      (index == 3 && settings_.panel.alwaysOnTop) ||
                      (index == 7 && settings_.ball.halfHidden);
            FillRound(c, sw, sw.h / 2, on ? p.accent : p.field);
            StrokeRound(c, sw, sw.h / 2, on ? p.accent : p.border, 1.0f);
            float knob = sw.h - S(6);
            float cx = on ? sw.x + sw.w - S(3) - knob / 2 : sw.x + S(3) + knob / 2;
            FillCircle(c, cx, sw.centerY(), knob / 2, p.white);
        } else if (IsDropdownSetting(index)) {
            Rect ctrl = valueRect(index);
            bool open = settingsDropdown_ == index;
            FillRound(c, ctrl, S(8), p.field);
            StrokeRound(c, ctrl, S(8), open || (hover && hoverControl_ == C_SETTING) ? p.accent
                                                                                    : p.border,
                        open ? 1.5f : 1.0f);
            DrawText(c, fonts_, value, Rect(ctrl.x + S(10), ctrl.y, ctrl.w - S(32), ctrl.h),
                     p.text, S(11.5f));
            DrawText(c, fonts_, open ? L"\xE70E" : L"\xE70D",
                     Rect(ctrl.x + ctrl.w - S(26), ctrl.y, S(18), ctrl.h), p.subtext, S(9), false,
                     TextAlign::Center, false, true);
        } else if (IsSliderSetting(index)) {
            Rect track = valueRect(index);
            float minValue = SliderMin(index);
            float maxValue = SliderMax(index);
            float valueNow = SliderValue(settings_, index);
            float t = maxValue > minValue ? (valueNow - minValue) / (maxValue - minValue) : 0.0f;
            t = std::clamp(t, 0.0f, 1.0f);
            Rect line(track.x, track.centerY() - S(2), track.w, S(4));
            FillRound(c, line, S(2), p.field);
            if (t > 0.001f)
                FillRound(c, Rect(line.x, line.y, line.w * t, line.h), S(2), p.accent);
            float thumbX = track.x + track.w * t;
            FillCircle(c, thumbX, track.centerY(), S(7), p.accent);
            FillCircle(c, thumbX, track.centerY(), S(3), p.white);
            DrawText(c, fonts_, value,
                     Rect(r.x + r.w - S(60), r.y, S(46), r.h), p.subtext, S(11), false,
                     TextAlign::Right);
        } else {
            bool action = index >= 11;
            Color valueColor = action ? p.accent : p.subtext;
            DrawText(c, fonts_, value,
                     Rect(r.x + r.w - S(150), r.y, S(122), r.h), valueColor, S(11.5f), false,
                     TextAlign::Right);
            if (index >= 12 && index <= 13)
                DrawText(c, fonts_, L"\xE76C",
                         Rect(r.x + r.w - S(24), r.y + S(13), S(14), S(20)), p.faint, S(9), false,
                         TextAlign::Center, false, true);
        }
        if (index + 1 < kSettingCount)
            DrawLine(c, r.x + S(12), r.y + r.h, r.x + r.w - S(12), r.y + r.h, p.divider, 1.0f);
    }
    c.g().ResetClip();

    if (settingsDropdown_ >= 0 && !layout_.dropdownOptions.empty()) {
        const Rect& first = layout_.dropdownOptions.front().second;
        const Rect& last = layout_.dropdownOptions.back().second;
        Rect box(first.x - S(4), first.y - S(4), first.w + S(8), last.y + last.h - first.y + S(8));
        FillRound(c, Rect(box.x + S(1), box.y + S(4), box.w, box.h), S(10), Color(38, 8, 12, 24));
        FillRound(c, box, S(10), p.card);
        StrokeRound(c, box, S(10), p.border, 1.0f);
        auto options = SettingOptions(settingsDropdown_);
        int current = SettingOptionIndex(settings_, settingsDropdown_);
        for (const auto& entry : layout_.dropdownOptions) {
            int k = entry.first;
            const Rect& opt = entry.second;
            bool optHover = hoverControl_ == C_SETTING_OPTION &&
                            hoverTaskId_ == Format(L"dd:%d", k);
            if (optHover) FillRound(c, opt, S(7), p.hover);
            if (k == current)
                DrawText(c, fonts_, L"\xE73E",
                         Rect(opt.x + S(7), opt.y, S(16), opt.h), p.accent, S(10), false,
                         TextAlign::Center, false, true);
            if (k >= 0 && k < (int)options.size())
                DrawText(c, fonts_, options[(size_t)k],
                         Rect(opt.x + S(28), opt.y, opt.w - S(34), opt.h), p.text, S(11.5f));
        }
    }

    if (layout_.scrollbar.w > 0) {
        float maxScroll = MaxScrollForView();
        float total = layout_.content.h + maxScroll;
        float thumbH = std::max(S(24), layout_.content.h * layout_.content.h / total);
        float t = maxScroll > 0 ? settingsScroll_ / maxScroll : 0;
        float thumbY = layout_.content.y + (layout_.content.h - thumbH) * t;
        FillRound(c, Rect(layout_.scrollbar.x, thumbY, layout_.scrollbar.w, thumbH), S(2),
                  Color(90, p.faint.GetR(), p.faint.GetG(), p.faint.GetB()));
    }
}

void App::DrawToast(Canvas& c, const Palette& p) {
    if (toast_.empty() || NowMs() > toastUntil_) return;
    float width = MeasureText(fonts_, toast_, S(11)) + S(28);
    width = std::min(width, layout_.card.w - S(40));
    Rect r(layout_.card.centerX() - width / 2, layout_.card.y + layout_.card.h - S(92), width,
           S(32));
    FillRound(c, r, S(16), p.toast);
    DrawText(c, fonts_, toast_, r, p.white, S(11), false, TextAlign::Center);
}

void App::PaintBall() {
    if (!ball_ || ballW_ <= 0) return;
    RECT wr{};
    GetWindowRect(ball_, &wr);
    Canvas c(ballW_, ballH_);
    if (!c.ok()) return;

    float cx = ballW_ / 2.0f;
    float cy = ballH_ / 2.0f;
    float radius = ballPx_ / 2.0f;
    auto todo = SortedTodo();
    int pendingCount = (int)todo.size();
    int64_t today = TodayLocalDay();
    int64_t minDue = todo.empty() ? today : todo.front()->dueDay;
    bool overdue = !todo.empty() && minDue < today;

    FillCircle(c, cx, cy + S(2), radius * 1.04f, Color(50, 20, 24, 40));
    FillCircle(c, cx, cy + S(1), radius * 1.02f, Color(30, 20, 24, 40));

    if (ballGlow_ > 0.01f) {
        Color glowColor = overdue ? Color(255, 244, 92, 96) : Color(255, 91, 124, 255);
        float extent = std::max(S(2), (float)ballMargin_ - S(2));
        for (int i = 12; i >= 1; --i) {
            float k = i / 12.0f;
            float rr = radius + extent * k;
            int a = (int)(7.0f * ballGlow_);
            FillCircle(c, cx, cy, rr,
                       Color((BYTE)a, glowColor.GetR(), glowColor.GetG(), glowColor.GetB()));
        }
        Pen halo(Color((BYTE)(130 * ballGlow_), glowColor.GetR(), glowColor.GetG(),
                       glowColor.GetB()),
                 std::max(1.2f, S(1.4f)));
        c.g().DrawEllipse(&halo, cx - radius - S(2), cy - radius - S(2), (radius + S(2)) * 2,
                          (radius + S(2)) * 2);
    }

    Color c1 = overdue ? Color(255, 244, 92, 96) : Color(255, 91, 124, 255);
    Color c2 = overdue ? Color(255, 209, 55, 69) : Color(255, 124, 95, 248);
    LinearGradientBrush brush(PointF(cx - radius, cy - radius), PointF(cx + radius, cy + radius), c1,
                              c2);
    RectF circle(cx - radius, cy - radius, radius * 2, radius * 2);
    c.g().FillEllipse(&brush, circle);
    Pen rim(overdue ? Color(255, 255, 255, 110) : Color(80, 255, 255, 255), 1.0f);
    c.g().DrawEllipse(&rim, circle);

    if (todo.empty()) {
        DrawText(c, fonts_, L"\xE73E",
                 Rect(cx - radius * 0.62f, cy - radius * 0.62f, radius * 1.24f, radius * 1.24f),
                 Color(230, 255, 255, 255), radius * 0.78f, false, TextAlign::Center, false, true);
    } else if (settings_.ball.display == L"icon") {
        DrawText(c, fonts_, L"\xE787",
                 Rect(cx - radius * 0.58f, cy - radius * 0.58f, radius * 1.16f, radius * 1.16f),
                 Color(240, 255, 255, 255), radius * 0.72f, false, TextAlign::Center, false, true);
    } else {
        if (overdue) {
            DrawText(c, fonts_, L"!",
                     Rect(cx - radius, cy - radius * 0.48f, radius * 2, radius * 0.95f),
                     Color(255, 255, 255, 255), radius * 0.95f, true, TextAlign::Center);
            DrawText(c, fonts_, L"逾期",
                     Rect(cx - radius, cy + radius * 0.36f, radius * 2, radius * 0.30f),
                     Color(225, 255, 255, 255), radius * 0.28f, true, TextAlign::Center);
        } else {
            int64_t diff = minDue - today;
            if (diff == 0) {
                DrawText(c, fonts_, L"今天",
                         Rect(cx - radius, cy - radius * 0.44f, radius * 2, radius * 0.88f),
                         Color(255, 255, 255, 255), radius * 0.46f, true, TextAlign::Center);
            } else {
                std::wstring text = diff > 99 ? L"99" : Format(L"%lld", (long long)diff);
                DrawText(c, fonts_, text,
                         Rect(cx - radius, cy - radius * 0.40f, radius * 2, radius * 0.92f),
                         Color(255, 255, 255, 255), radius * 0.68f, true, TextAlign::Center);
                DrawText(c, fonts_, L"天",
                         Rect(cx - radius, cy + radius * 0.34f, radius * 2, radius * 0.30f),
                         Color(225, 255, 255, 255), radius * 0.28f, true, TextAlign::Center);
            }
        }
    }

    if (pendingCount > 0) {
        float br = std::max(S(8), radius * 0.36f);
        float bx = cx + radius * 0.68f;
        float by = cy - radius * 0.68f;
        FillCircle(c, bx, by, br, Color(255, 220, 64, 74));
        Pen border(Color(235, 255, 255, 255), std::max(1.2f, S(1.6f)));
        c.g().DrawEllipse(&border, bx - br, by - br, br * 2, br * 2);
        std::wstring countText =
            pendingCount > 99 ? L"99+" : Format(L"%d", pendingCount);
        DrawText(c, fonts_, countText, Rect(bx - br * 1.4f, by - br, br * 2.8f, br * 2),
                 Color(255, 255, 255, 255), std::max(S(8), br * 0.95f), true, TextAlign::Center);
    }

    c.Fade(settings_.ball.opacity / 100.0f);
    c.Present(ball_, wr.left, wr.top);
}

// ---------------------------------------------------------------------------
// Panel state / animation
// ---------------------------------------------------------------------------

bool App::ReduceMotionEnabled() const {
    if (settings_.app.reduceMotion == L"on") return true;
    if (settings_.app.reduceMotion == L"off") return false;
    return !SystemAnimationsEnabled();
}

bool App::CanUseDarkMode() const {
    if (settings_.app.theme == L"dark") return true;
    if (settings_.app.theme == L"light") return false;
    return IsDarkSystemTheme();
}

void App::RefreshTheme() {
    bool dark = CanUseDarkMode();
    if (dark == lastDark_) return;
    lastDark_ = dark;
    PaintBall();
    if (panelVisible_) PaintPanel();
}

void App::ShowPanel(bool pinned, bool animate) {
    if (!panel_) return;
    panelPinned_ = pinned;
    if (pinned) KillTimer(panel_, TIMER_COLLAPSE);
    if (!panelVisible_) {
        LayoutPanelWindow(true);
        ShowWindow(panel_, SW_SHOWNOACTIVATE);
        panelVisible_ = true;
        if (animate && !ReduceMotionEnabled()) {
            StartPanelAnimation(true);
        } else {
            animT_ = 1.0f;
            PaintPanel();
        }
    } else {
        LayoutPanelWindow(true);
        PaintPanel();
    }
    if (view_ == View::Form) {
        SetForegroundWindow(panel_);
        SetFocus(panel_);
        ResetCaretBlink();
        SetImePosition();
    }
}

void App::HidePanel(bool animate, bool resetView) {
    (void)resetView;
    if (!panelVisible_) {
        panelPinned_ = false;
        return;
    }
    KillTimer(panel_, TIMER_COLLAPSE);
    KillTimer(panel_, TIMER_CARET);
    caretTimerOn_ = false;
    if (animate && !ReduceMotionEnabled()) {
        StartPanelAnimation(false);
        return;
    }
    CompleteHidePanel();
}

void App::CompleteHidePanel() {
    KillTimer(panel_, TIMER_PANEL_ANIM);
    KillTimer(panel_, TIMER_CARET);
    KillTimer(panel_, TIMER_TOAST);
    panelAnimating_ = false;
    panelVisible_ = false;
    panelPinned_ = false;
    settingsDropdown_ = -1;
    sliderDragging_ = false;
    settingsSlider_ = -1;
    animT_ = 1.0f;
    if (panel_) ShowWindow(panel_, SW_HIDE);
    view_ = View::List;
    formEditing_ = false;
    formTitle_.clear();
    formError_.clear();
    imeComp_.clear();
    caret_ = 0;
    caretTimerOn_ = false;
    toast_.clear();
}

void App::StartPanelAnimation(bool show) {
    panelAnimating_ = true;
    animTargetVisible_ = show;
    animStart_ = NowMs();
    animDuration_ = show ? 190 : 150;
    animT_ = show ? 0.0f : 1.0f;
    SetTimer(panel_, TIMER_PANEL_ANIM, 15, nullptr);
    PaintPanel();
}

void App::OnPanelAnimTimer() {
    if (!panelAnimating_) {
        KillTimer(panel_, TIMER_PANEL_ANIM);
        return;
    }
    float elapsed = (float)(NowMs() - animStart_);
    float t = std::clamp(elapsed / (float)animDuration_, 0.0f, 1.0f);
    float e = 1.0f - std::pow(1.0f - t, 3.0f);
    animT_ = animTargetVisible_ ? e : 1.0f - e;
    PaintPanel();
    if (t >= 1.0f) {
        KillTimer(panel_, TIMER_PANEL_ANIM);
        panelAnimating_ = false;
        if (!animTargetVisible_) {
            CompleteHidePanel();
        } else {
            animT_ = 1.0f;
            PaintPanel();
        }
    }
}

// ---------------------------------------------------------------------------
// Mouse / keyboard
// ---------------------------------------------------------------------------

void App::SetCursorForControl(Control control) {
    if (control == C_TITLE_FIELD) {
        SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
    } else if (control == C_NONE) {
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    } else {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
    }
}

void App::OnPanelMouseMove(POINT pt) {
    std::wstring id;
    Control c = HitTest(pt, &id);
    if (c != hoverControl_ || id != hoverTaskId_) {
        hoverControl_ = c;
        hoverTaskId_ = id;
        PaintPanel();
    }
    SetCursorForControl(c);
}

void App::OnPanelClick(POINT pt) {
    if (completing_) return;
    std::wstring id;
    Control c = HitTest(pt, &id);

    if (view_ == View::List) {
        switch (c) {
            case C_BANNER:
                if (!saveError_.empty())
                    RetrySave();
                else {
                    dataNotice_.clear();
                    LayoutPanelWindow(true);
                    PaintPanel();
                }
                return;
            case C_ADD:
            case C_EMPTY_ADD:
                OpenForm(nullptr);
                return;
            case C_PIN:
                panelPinned_ = !panelPinned_;
                if (panelPinned_)
                    KillTimer(panel_, TIMER_COLLAPSE);
                else if (!mouseInPanel_ && !mouseInBall_)
                    SetTimer(panel_, TIMER_COLLAPSE, 350, nullptr);
                PaintPanel();
                return;
            case C_SETTINGS:
                OpenSettings();
                return;
            case C_CLOSE:
                HidePanel(true);
                return;
            case C_ARCHIVE_LINK:
                OpenArchive();
                return;
            case C_CHECKBOX:
            case C_ROW:
                CompleteTask(id);
                return;
            case C_EDIT:
                if (const Task* t = FindTask(id)) OpenForm(t);
                return;
            default:
                return;
        }
    }

    if (view_ == View::Form) {
        switch (c) {
            case C_BACK:
            case C_CANCEL:
                CancelForm();
                return;
            case C_SAVE:
                SaveForm();
                return;
            case C_DATE_MINUS:
                formDue_ -= 1;
                formError_.clear();
                PaintPanel();
                return;
            case C_DATE_PLUS:
                formDue_ += 1;
                formError_.clear();
                PaintPanel();
                return;
            case C_DATE_FIELD:
                OpenDatePicker();
                return;
            case C_CHIP_TODAY:
                formDue_ = TodayLocalDay();
                PaintPanel();
                return;
            case C_CHIP_TOMORROW:
                formDue_ = TodayLocalDay() + 1;
                PaintPanel();
                return;
            case C_CHIP_WEEK:
                formDue_ = TodayLocalDay() + 7;
                PaintPanel();
                return;
            case C_REPEAT:
                if (formRepeat_.empty())
                    formRepeat_ = L"daily";
                else if (formRepeat_ == L"daily")
                    formRepeat_ = L"weekly";
                else if (formRepeat_ == L"weekly")
                    formRepeat_ = L"monthly";
                else
                    formRepeat_.clear();
                PaintPanel();
                return;
            case C_TITLE_FIELD:
                SetFocus(panel_);
                SetCaretFromX((float)pt.x);
                ResetCaretBlink();
                SetImePosition();
                return;
            default:
                return;
        }
    }

    if (view_ == View::Archive) {
        switch (c) {
            case C_BACK:
                OpenList();
                return;
            case C_CLEAR_ARCHIVE:
                ClearArchive();
                return;
            case C_RESTORE:
                RestoreTask(id);
                return;
            case C_DELETE:
                DeleteArchiveTask(id);
                return;
            default:
                return;
        }
    }

    if (view_ == View::DatePicker) {
        switch (c) {
            case C_BACK:
            case C_CAL_CANCEL:
                CloseDatePicker();
                return;
            case C_CAL_PREV:
                if (--calMonth_ < 1) {
                    calMonth_ = 12;
                    --calYear_;
                }
                LayoutPanelWindow(true);
                PaintPanel();
                return;
            case C_CAL_NEXT:
                if (++calMonth_ > 12) {
                    calMonth_ = 1;
                    ++calYear_;
                }
                LayoutPanelWindow(true);
                PaintPanel();
                return;
            case C_CAL_TODAY:
                formDue_ = TodayLocalDay();
                CloseDatePicker();
                return;
            case C_CAL_DAY:
                if (!id.empty()) {
                    formDue_ = _wtoi64(id.c_str());
                    CloseDatePicker();
                }
                return;
            default:
                return;
        }
    }

    if (view_ == View::Settings) {
        if (c == C_BACK) {
            OpenList();
            return;
        }
        if (settingsDropdown_ >= 0) {
            if (c == C_SETTING_OPTION && id.rfind(L"dd:", 0) == 0) {
                SelectSettingOption(settingsDropdown_, _wtoi(id.c_str() + 3));
            } else {
                settingsDropdown_ = -1;
                LayoutPanelWindow(true);
                PaintPanel();
            }
            return;
        }
        if (c == C_SETTING && id.rfind(L"setting:", 0) == 0) {
            int index = _wtoi(id.c_str() + 8);
            if (IsDropdownSetting(index)) {
                OpenSettingDropdown(index);
            } else if (IsSliderSetting(index)) {
                return;
            } else {
                CycleSetting(index);
            }
            return;
        }
        if (c == C_SLIDER && id.rfind(L"setting:", 0) == 0) {
            SetSliderFromX(_wtoi(id.c_str() + 8), pt.x);
            return;
        }
    }
}

void App::OnPanelMouseWheel(int delta) {
    if (settingsDropdown_ >= 0) {
        settingsDropdown_ = -1;
        BuildLayout();
        PaintPanel();
        return;
    }
    float maxScroll = MaxScrollForView();
    if (maxScroll <= 0) return;
    float step = S(56) * (delta > 0 ? -1.0f : 1.0f);
    SetScrollForView(std::clamp(ScrollForView() + step, 0.0f, maxScroll));
    BuildLayout();
    PaintPanel();
}

void App::OnPanelRightClick(POINT pt) {
    if (view_ != View::List) return;
    std::wstring id;
    Control c = HitTest(pt, &id);
    if (c != C_ROW && c != C_CHECKBOX && c != C_EDIT) return;
    const Task* t = FindTask(id);
    if (!t) return;

    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, kMenuEdit, L"编辑");
    AppendMenuW(menu, MF_STRING, kMenuComplete, L"标记完成");
    AppendMenuW(menu, MF_STRING, kMenuCopy, L"复制标题");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuDelete, L"删除…");

    POINT screen = pt;
    ClientToScreen(panel_, &screen);
    menuOpen_ = true;
    SetForegroundWindow(panel_);
    int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, screen.x,
                                  screen.y, 0, panel_, nullptr);
    PostMessageW(panel_, WM_NULL, 0, 0);
    menuOpen_ = false;
    DestroyMenu(menu);

    switch (cmd) {
        case kMenuEdit:
            if (const Task* task = FindTask(id)) OpenForm(task);
            break;
        case kMenuComplete:
            CompleteTask(id);
            break;
        case kMenuCopy:
            CopyTaskTitle(id);
            break;
        case kMenuDelete:
            DeleteTask(id);
            break;
        default:
            break;
    }
    if (!panelPinned_) SetTimer(panel_, TIMER_COLLAPSE, 350, nullptr);
}

void App::OnPanelKey(WPARAM key) {
    if (settingsDropdown_ >= 0) {
        settingsDropdown_ = -1;
        BuildLayout();
        PaintPanel();
        if (key == VK_ESCAPE) return;
    }
    if (view_ == View::DatePicker) {
        if (key == VK_ESCAPE) {
            CloseDatePicker();
            return;
        }
        if (key == VK_LEFT || key == VK_RIGHT) {
            int delta = key == VK_LEFT ? -1 : 1;
            if (delta < 0 && --calMonth_ < 1) {
                calMonth_ = 12;
                --calYear_;
            } else if (delta > 0 && ++calMonth_ > 12) {
                calMonth_ = 1;
                ++calYear_;
            }
            LayoutPanelWindow(true);
            PaintPanel();
        }
        return;
    }

    if (view_ == View::Form) {
        switch (key) {
            case VK_ESCAPE:
                CancelForm();
                return;
            case VK_RETURN:
                SaveForm();
                return;
            case VK_LEFT:
                if (caret_ > 0) --caret_;
                ResetCaretBlink();
                PaintPanel();
                return;
            case VK_RIGHT:
                if (caret_ < (int)formTitle_.size()) ++caret_;
                ResetCaretBlink();
                PaintPanel();
                return;
            case VK_HOME:
                caret_ = 0;
                ResetCaretBlink();
                PaintPanel();
                return;
            case VK_END:
                caret_ = (int)formTitle_.size();
                ResetCaretBlink();
                PaintPanel();
                return;
            case VK_DELETE:
                if (caret_ < (int)formTitle_.size()) {
                    formTitle_.erase((size_t)caret_, 1);
                    formError_.clear();
                    ResetCaretBlink();
                    PaintPanel();
                }
                return;
            case VK_TAB:
                return;
            default:
                return;
        }
    }

    if (key == VK_ESCAPE) {
        if (view_ == View::List) {
            HidePanel(true);
        } else {
            OpenList();
        }
        return;
    }
    if (key == VK_PRIOR || key == VK_NEXT) {
        float maxScroll = MaxScrollForView();
        if (maxScroll <= 0) return;
        float step = S(120) * (key == VK_PRIOR ? -1.0f : 1.0f);
        SetScrollForView(std::clamp(ScrollForView() + step, 0.0f, maxScroll));
        BuildLayout();
        PaintPanel();
    }
}

void App::OnPanelChar(wchar_t ch) {
    if (view_ != View::Form) return;
    if (ch == L'\b') {
        if (caret_ > 0) {
            formTitle_.erase((size_t)caret_ - 1, 1);
            --caret_;
            formError_.clear();
        }
    } else if (ch == L'\r' || ch == L'\n' || ch == L'\t') {
        return;
    } else if (ch >= 32 && ch != 127) {
        if (formTitle_.size() < 100) {
            formTitle_.insert((size_t)caret_, 1, ch);
            ++caret_;
        }
        formError_.clear();
    } else {
        return;
    }
    ResetCaretBlink();
    SetImePosition();
    PaintPanel();
}

void App::OnImeComposition(LPARAM lParam) {
    if (view_ != View::Form) return;
    HIMC himc = ImmGetContext(panel_);
    if (!himc) return;
    bool changed = false;
    if (lParam & GCS_RESULTSTR) {
        LONG bytes = ImmGetCompositionStringW(himc, GCS_RESULTSTR, nullptr, 0);
        if (bytes > 0) {
            std::wstring text((size_t)bytes / sizeof(wchar_t), L'\0');
            ImmGetCompositionStringW(himc, GCS_RESULTSTR, text.data(), bytes);
            if (formTitle_.size() + text.size() <= 100) {
                formTitle_.insert((size_t)caret_, text);
                caret_ += (int)text.size();
            }
            imeComp_.clear();
            formError_.clear();
            changed = true;
        }
    }
    if (lParam & GCS_COMPSTR) {
        LONG bytes = ImmGetCompositionStringW(himc, GCS_COMPSTR, nullptr, 0);
        if (bytes > 0) {
            std::wstring text((size_t)bytes / sizeof(wchar_t), L'\0');
            ImmGetCompositionStringW(himc, GCS_COMPSTR, text.data(), bytes);
            imeComp_ = text;
        } else {
            imeComp_.clear();
        }
        changed = true;
    }
    ImmReleaseContext(panel_, himc);
    if (changed) {
        ResetCaretBlink();
        SetImePosition();
        PaintPanel();
    }
}

void App::ResetCaretBlink() {
    caretOn_ = true;
    if (panel_ && view_ == View::Form && panelVisible_) {
        SetTimer(panel_, TIMER_CARET, 530, nullptr);
        caretTimerOn_ = true;
    }
}

void App::SetImePosition() {
    if (!panel_ || view_ != View::Form) return;
    HIMC himc = ImmGetContext(panel_);
    if (!himc) return;
    const Rect& f = layout_.titleField;
    POINT pos{(LONG)(f.x + S(14)), (LONG)(f.y + f.h / 2)};
    COMPOSITIONFORM cf{};
    cf.dwStyle = CFS_POINT;
    cf.ptCurrentPos = pos;
    ImmSetCompositionWindow(himc, &cf);

    CANDIDATEFORM cand{};
    cand.dwIndex = 0;
    cand.dwStyle = CFS_CANDIDATEPOS;
    cand.ptCurrentPos = pos;
    ImmSetCandidateWindow(himc, &cand);

    LOGFONTW lf{};
    lf.lfHeight = -(LONG)std::lround(S(14));
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    ImmSetCompositionFontW(himc, &lf);
    ImmReleaseContext(panel_, himc);
}

void App::SetCaretFromX(float x) {
    const Rect& f = layout_.titleField;
    float innerW = f.w - S(28);
    float prefixW = MeasureText(fonts_, formTitle_.substr(0, (size_t)caret_), S(14));
    float fullW = MeasureText(fonts_, formTitle_, S(14));
    float maxScroll = std::max(0.0f, fullW - innerW);
    float textScroll = std::clamp(prefixW - innerW + S(2), 0.0f, maxScroll);
    float target = x - (f.x + S(14)) + textScroll;
    int best = 0;
    float bestDist = std::abs(target);
    for (int i = 1; i <= (int)formTitle_.size(); ++i) {
        float w = MeasureText(fonts_, formTitle_.substr(0, (size_t)i), S(14));
        float dist = std::abs(target - w);
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    caret_ = best;
    PaintPanel();
}

// ---------------------------------------------------------------------------
// Views
// ---------------------------------------------------------------------------

void App::OpenList() {
    view_ = View::List;
    panelPinned_ = false;
    settingsDropdown_ = -1;
    sliderDragging_ = false;
    settingsSlider_ = -1;
    formEditing_ = false;
    formError_.clear();
    imeComp_.clear();
    KillTimer(panel_, TIMER_CARET);
    if (panelVisible_) {
        LayoutPanelWindow(true);
        PaintPanel();
    } else {
        ShowPanel(false, true);
    }
}

void App::OpenForm(const Task* task) {
    view_ = View::Form;
    panelPinned_ = true;
    if (task) {
        formEditing_ = true;
        formId_ = task->id;
        formTitle_ = task->title;
        formDue_ = task->dueDay;
        formRepeat_ = task->repeat;
    } else {
        formEditing_ = false;
        formId_.clear();
        formTitle_.clear();
        formDue_ = TodayLocalDay();
        formRepeat_.clear();
    }
    formError_.clear();
    imeComp_.clear();
    caret_ = (int)formTitle_.size();
    KillTimer(panel_, TIMER_COLLAPSE);
    if (!panelVisible_) {
        ShowPanel(true, true);
    } else {
        LayoutPanelWindow(true);
        PaintPanel();
    }
    SetForegroundWindow(panel_);
    SetFocus(panel_);
    ResetCaretBlink();
    SetImePosition();
}

void App::OpenDatePicker() {
    if (view_ != View::Form) return;
    int y = 0, m = 0, d = 0;
    CivilFromDay(formDue_, y, m, d);
    calYear_ = y;
    calMonth_ = m;
    view_ = View::DatePicker;
    panelPinned_ = true;
    imeComp_.clear();
    toast_.clear();
    KillTimer(panel_, TIMER_TOAST);
    KillTimer(panel_, TIMER_CARET);
    caretTimerOn_ = false;
    LayoutPanelWindow(true);
    PaintPanel();
    SetForegroundWindow(panel_);
    SetFocus(panel_);
}

void App::CloseDatePicker() {
    if (view_ != View::DatePicker) return;
    view_ = View::Form;
    LayoutPanelWindow(true);
    PaintPanel();
    SetForegroundWindow(panel_);
    SetFocus(panel_);
    ResetCaretBlink();
    SetImePosition();
}

void App::CancelForm() {
    view_ = View::List;
    panelPinned_ = false;
    formEditing_ = false;
    formId_.clear();
    formTitle_.clear();
    formError_.clear();
    formRepeat_.clear();
    imeComp_.clear();
    KillTimer(panel_, TIMER_CARET);
    caretTimerOn_ = false;
    LayoutPanelWindow(true);
    PaintPanel();
}

void App::SaveForm() {
    std::wstring title = Trim(formTitle_);
    if (title.empty()) {
        formError_ = L"请输入作业标题";
        PaintPanel();
        return;
    }
    if (formEditing_) {
        if (Task* t = FindTask(formId_)) {
            t->title = title;
            t->dueDay = formDue_;
            t->repeat = formRepeat_;
        }
    } else {
        Task t;
        t.id = MakeTaskId();
        t.title = title;
        t.dueDay = formDue_;
        t.repeat = formRepeat_;
        t.done = false;
        t.createdAt = LocalNowISO();
        tasks_.push_back(std::move(t));
    }

    view_ = View::List;
    panelPinned_ = false;
    formEditing_ = false;
    formId_.clear();
    formTitle_.clear();
    formError_.clear();
    formRepeat_.clear();
    imeComp_.clear();
    KillTimer(panel_, TIMER_CARET);
    caretTimerOn_ = false;

    SaveTasksQuiet();
    LayoutPanelWindow(true);
    PaintPanel();
    PaintBall();
    UpdateTrayTip();
    ShowToast(L"已保存");
}

void App::OpenArchive() {
    view_ = View::Archive;
    panelPinned_ = true;
    archiveScroll_ = 0;
    if (!panelVisible_) {
        ShowPanel(true, true);
    } else {
        LayoutPanelWindow(true);
        PaintPanel();
    }
}

void App::OpenSettings() {
    view_ = View::Settings;
    panelPinned_ = true;
    settingsScroll_ = 0;
    settingsDropdown_ = -1;
    sliderDragging_ = false;
    settingsSlider_ = -1;
    if (!panelVisible_) {
        ShowPanel(true, true);
    } else {
        LayoutPanelWindow(true);
        PaintPanel();
    }
}

void App::OpenDataFolder() {
    ShellExecuteW(nullptr, L"open", store_.dataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void App::ResetBallPosition() {
    settings_.ball.edge = L"right";
    settings_.ball.offset = 0.42;
    MonitorArea primary = PrimaryMonitor();
    settings_.ball.monitor = primary.device;
    PlaceBallFromSettings();
    SaveSettingsQuiet();
    if (panelVisible_) {
        LayoutPanelWindow(true);
        PaintPanel();
    }
    ShowToast(L"小球位置已重置");
}

void App::CycleSetting(int row) {
    switch (row) {
        case 0:
            settings_.app.autoStart = !settings_.app.autoStart;
            if (settings_.app.autoStart)
                ApplyAutoStart();
            else {
                std::wstring error;
                SetAutoStartRegistry(false, error);
            }
            break;
        case 1:
            settings_.app.showOnBoot = !settings_.app.showOnBoot;
            break;
        case 2:
            settings_.app.trigger = settings_.app.trigger == L"hover" ? L"click" : L"hover";
            break;
        case 3:
            settings_.panel.alwaysOnTop = !settings_.panel.alwaysOnTop;
            break;
        case 4:
            if (settings_.app.theme == L"system")
                settings_.app.theme = L"light";
            else if (settings_.app.theme == L"light")
                settings_.app.theme = L"dark";
            else
                settings_.app.theme = L"system";
            break;
        case 5:
            settings_.ball.display = settings_.ball.display == L"days" ? L"icon" : L"days";
            break;
        case 6: {
            static const int sizes[] = {48, 52, 56, 60, 64};
            int next = 48;
            for (int size : sizes) {
                if (size > settings_.ball.size) {
                    next = size;
                    break;
                }
            }
            settings_.ball.size = next;
            break;
        }
        case 7:
            settings_.ball.halfHidden = !settings_.ball.halfHidden;
            break;
        case 8:
            if (settings_.app.reduceMotion == L"system")
                settings_.app.reduceMotion = L"on";
            else if (settings_.app.reduceMotion == L"on")
                settings_.app.reduceMotion = L"off";
            else
                settings_.app.reduceMotion = L"system";
            break;
        case 9: {
            static const int values[] = {100, 95, 90, 85, 80, 75, 70};
            int next = 100;
            for (int value : values) {
                if (value < settings_.ball.opacity) {
                    next = value;
                    break;
                }
            }
            settings_.ball.opacity = next;
            break;
        }
        case 10: {
            static const int values[] = {100, 95, 90, 85, 80};
            int next = 100;
            for (int value : values) {
                if (value < settings_.panel.opacity) {
                    next = value;
                    break;
                }
            }
            settings_.panel.opacity = next;
            break;
        }
        case 11:
            ResetBallPosition();
            return;
        case 12:
            OpenArchive();
            return;
        case 13:
            OpenDataFolder();
            return;
        default:
            return;
    }
    OnSettingsChanged();
}

void App::OnSettingsChanged() {
    NormalizeSettings(settings_);
    reduceMotion_ = ReduceMotionEnabled();
    ResizeBallWindow();
    PlaceBallFromSettings();
    ApplyAlwaysOnTop();
    lastDark_ = CanUseDarkMode();
    SaveSettingsQuiet();
    if (panelVisible_) {
        LayoutPanelWindow(true);
        PaintPanel();
    }
    PaintBall();
    UpdateTrayTip();
}

void App::OpenSettingDropdown(int row) {
    if (!IsDropdownSetting(row)) return;
    settingsDropdown_ = settingsDropdown_ == row ? -1 : row;
    BuildLayout();
    PaintPanel();
}

void App::SelectSettingOption(int row, int option) {
    auto options = SettingOptions(row);
    if (option < 0 || option >= (int)options.size()) return;
    switch (row) {
        case 2: settings_.app.trigger = option == 1 ? L"click" : L"hover"; break;
        case 4:
            settings_.app.theme = option == 1 ? L"light" : (option == 2 ? L"dark" : L"system");
            break;
        case 5: settings_.ball.display = option == 1 ? L"icon" : L"days"; break;
        case 8:
            settings_.app.reduceMotion =
                option == 1 ? L"on" : (option == 2 ? L"off" : L"system");
            break;
        default: return;
    }
    settingsDropdown_ = -1;
    OnSettingsChanged();
}

void App::SetSliderFromX(int row, int x) {
    if (!IsSliderSetting(row)) return;
    Rect track;
    bool found = false;
    for (const auto& entry : layout_.settingValues) {
        if (entry.first == row) {
            track = entry.second;
            found = true;
            break;
        }
    }
    if (!found || track.w <= 0) return;
    float t = std::clamp(((float)x - track.x) / track.w, 0.0f, 1.0f);
    float minValue = SliderMin(row);
    float maxValue = SliderMax(row);
    float step = SliderStep(row);
    float raw = minValue + t * (maxValue - minValue);
    int value = (int)std::lround((raw - minValue) / step) * (int)step + (int)minValue;
    value = std::clamp(value, (int)minValue, (int)maxValue);

    bool changed = false;
    if (row == 6 && settings_.ball.size != value) {
        settings_.ball.size = value;
        changed = true;
    } else if (row == 9 && settings_.ball.opacity != value) {
        settings_.ball.opacity = value;
        changed = true;
    } else if (row == 10 && settings_.panel.opacity != value) {
        settings_.panel.opacity = value;
        changed = true;
    }
    if (changed) ApplySettingsLive();
}

void App::ApplySettingsLive() {
    reduceMotion_ = ReduceMotionEnabled();
    ResizeBallWindow();
    PlaceBallFromSettings();
    PaintBall();
    if (panelVisible_) PaintPanel();
}

void App::ApplyAlwaysOnTop() {
    if (!panel_) return;
    SetWindowPos(panel_, settings_.panel.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void App::ApplyAutoStart() {
    if (settings_.app.autoStart) {
        std::wstring error;
        if (!SetAutoStartRegistry(true, error)) {
            settings_.app.autoStart = false;
            ShowToast(L"开机自启设置失败");
            store_.Log(L"开机自启设置失败：" + error);
            SaveSettingsQuiet();
        }
    } else {
        std::wstring error;
        SetAutoStartRegistry(false, error);
    }
}

bool App::SetAutoStartRegistry(bool enable, std::wstring& error) {
    HKEY key = nullptr;
    LONG result = RegCreateKeyExW(HKEY_CURRENT_USER,
                                  L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0,
                                  KEY_SET_VALUE, nullptr, &key, nullptr);
    if (result != ERROR_SUCCESS) {
        error = Format(L"无法打开启动项注册表（错误 %ld）", result);
        return false;
    }
    if (enable) {
        std::wstring command = L"\"" + ExecutablePath() + L"\" --autostart";
        result = RegSetValueExW(key, L"HomeworkFloat", 0, REG_SZ,
                                reinterpret_cast<const BYTE*>(command.c_str()),
                                (DWORD)((command.size() + 1) * sizeof(wchar_t)));
        if (result != ERROR_SUCCESS) {
            error = Format(L"写入启动项失败（错误 %ld）", result);
            RegCloseKey(key);
            return false;
        }
    } else {
        result = RegDeleteValueW(key, L"HomeworkFloat");
        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
            error = Format(L"删除启动项失败（错误 %ld）", result);
            RegCloseKey(key);
            return false;
        }
    }
    RegCloseKey(key);
    return true;
}

bool App::IsAutoStartRegistryEnabled() const {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                      KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    wchar_t buf[1024]{};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    LONG result = RegQueryValueExW(key, L"HomeworkFloat", nullptr, &type, reinterpret_cast<LPBYTE>(buf),
                                   &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ;
}

// ---------------------------------------------------------------------------
// Tasks
// ---------------------------------------------------------------------------

Task* App::FindTask(const std::wstring& id) {
    for (auto& t : tasks_)
        if (t.id == id) return &t;
    return nullptr;
}

const Task* App::FindTask(const std::wstring& id) const {
    for (const auto& t : tasks_)
        if (t.id == id) return &t;
    return nullptr;
}

std::vector<const Task*> App::SortedTodo() const {
    std::vector<const Task*> result;
    for (const auto& t : tasks_)
        if (!t.done) result.push_back(&t);
    std::sort(result.begin(), result.end(), SortTodo);
    return result;
}

std::vector<const Task*> App::SortedArchive() const {
    std::vector<const Task*> result;
    for (const auto& t : tasks_)
        if (t.done) result.push_back(&t);
    std::sort(result.begin(), result.end(), SortArchive);
    return result;
}

void App::SaveTasksQuiet() {
    std::wstring error;
    if (!store_.SaveTasks(tasks_, error)) {
        saveError_ = L"保存失败：" + error + L"（点击重试）";
        store_.Log(L"保存任务失败：" + error);
    } else {
        saveError_.clear();
    }
    UpdateTrayTip();
}

void App::SaveSettingsQuiet() {
    std::wstring error;
    if (!store_.SaveSettings(settings_, error)) {
        store_.Log(L"保存设置失败：" + error);
        if (panelVisible_) ShowToast(L"设置保存失败");
    }
}

void App::RetrySave() {
    std::wstring error;
    if (store_.SaveTasks(tasks_, error)) {
        saveError_.clear();
        ShowToast(L"已保存");
    } else {
        saveError_ = L"保存失败：" + error + L"（点击重试）";
    }
    if (panelVisible_) {
        LayoutPanelWindow(true);
        PaintPanel();
    }
}

void App::AfterTasksChanged(bool rebuildShape) {
    SaveTasksQuiet();
    PaintBall();
    UpdateTrayTip();
    if (rebuildShape) {
        ClampScroll();
        LayoutPanelWindow(true);
    }
    if (panelVisible_) PaintPanel();
}

void App::DeleteTask(const std::wstring& id) {
    const Task* t = FindTask(id);
    if (!t) return;
    std::wstring msg = L"确定删除「" + t->title + L"」吗？\n删除后无法恢复。";
    KillTimer(panel_, TIMER_COLLAPSE);
    if (MessageBoxW(panel_, msg.c_str(), L"删除作业", MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST) !=
        IDOK)
        return;
    tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                                [&](const Task& task) { return task.id == id; }),
                 tasks_.end());
    AfterTasksChanged();
}

void App::CompleteTask(const std::wstring& id) {
    if (completing_) return;
    Task* t = FindTask(id);
    if (!t || t->done) return;
    completing_ = true;
    completingId_ = id;
    completeStart_ = NowMs();
    if (ReduceMotionEnabled()) {
        FinishComplete();
        return;
    }
    SetTimer(panel_, TIMER_COMPLETE, 15, nullptr);
    PaintPanel();
}

void App::FinishComplete() {
    KillTimer(panel_, TIMER_COMPLETE);
    completing_ = false;
    std::wstring id = completingId_;
    completingId_.clear();
    std::wstring toast = L"已完成，已移入归档";
    if (Task* t = FindTask(id)) {
        t->done = true;
        t->completedAt = LocalNowISO();
        if (!t->repeat.empty()) {
            int64_t nextDue = NextRepeatDay(t->dueDay, t->repeat);
            int64_t today = TodayLocalDay();
            for (int guard = 0; nextDue <= today && guard < 500; ++guard)
                nextDue = NextRepeatDay(nextDue, t->repeat);
            Task next;
            next.id = MakeTaskId();
            next.title = t->title;
            next.dueDay = nextDue;
            next.repeat = t->repeat;
            next.createdAt = LocalNowISO();
            tasks_.push_back(std::move(next));
            toast = L"已完成，下次 " + FormatMMDD(nextDue);
        }
    }
    AfterTasksChanged();
    ShowToast(toast);
}

void App::RestoreTask(const std::wstring& id) {
    if (Task* t = FindTask(id)) {
        t->done = false;
        t->completedAt.clear();
        AfterTasksChanged();
        ShowToast(L"已恢复到待办");
    }
}

void App::DeleteArchiveTask(const std::wstring& id) {
    const Task* t = FindTask(id);
    if (!t) return;
    std::wstring msg = L"彻底删除「" + t->title + L"」吗？";
    if (MessageBoxW(panel_, msg.c_str(), L"删除归档", MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST) !=
        IDOK)
        return;
    tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                                [&](const Task& task) { return task.id == id; }),
                 tasks_.end());
    AfterTasksChanged();
}

void App::ClearArchive() {
    if (SortedArchive().empty()) return;
    if (MessageBoxW(panel_, L"确定清空全部归档作业吗？此操作无法撤销。", L"清空归档",
                    MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST) != IDOK)
        return;
    tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                                [](const Task& task) { return task.done; }),
                 tasks_.end());
    AfterTasksChanged();
}

void App::CopyTaskTitle(const std::wstring& id) {
    const Task* t = FindTask(id);
    if (!t) return;
    if (!OpenClipboard(panel_)) return;
    EmptyClipboard();
    size_t bytes = (t->title.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem) {
        void* dest = GlobalLock(mem);
        if (dest) {
            memcpy(dest, t->title.c_str(), bytes);
            GlobalUnlock(mem);
            SetClipboardData(CF_UNICODETEXT, mem);
        } else {
            GlobalFree(mem);
        }
    }
    CloseClipboard();
    ShowToast(L"已复制标题");
}

// ---------------------------------------------------------------------------
// Toast / recovery
// ---------------------------------------------------------------------------

void App::ShowToast(const std::wstring& text) {
    if (!panelVisible_ || !panel_) return;
    toast_ = text;
    toastUntil_ = NowMs() + 2400;
    SetTimer(panel_, TIMER_TOAST, 2400, nullptr);
    PaintPanel();
}

void App::ShowRecoveryNotice() {
    if (load_.notice.empty() || !trayAdded_ || !tray_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = tray_;
    nid.uID = kTrayIdLocal;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_WARNING;
    wcscpy_s(nid.szInfoTitle, L"作业数据恢复");
    wcsncpy_s(nid.szInfo, load_.notice.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void App::UpdateTrayMenuState() {}

}  // namespace hf
