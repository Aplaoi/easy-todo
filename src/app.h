#pragma once

#include "common.h"
#include "model.h"
#include "paint.h"
#include "store.h"

namespace hf {

enum class View { List, Form, DatePicker, Archive, Settings };

struct RowHit {
    std::wstring id;
    Rect row;
    Rect checkbox;
    Rect action1;
    Rect action2;
    Rect edit;
};

struct PanelLayout {
    Rect card;
    Rect header;
    Rect content;
    Rect footer;
    Rect btnAdd, btnPin, btnSettings, btnClose;
    Rect btnBack;
    Rect archiveLink;
    Rect emptyAdd;
    Rect banner;
    Rect titleField;
    Rect dateMinus, datePlus, dateValue;
    Rect chipToday, chipTomorrow, chipWeek;
    Rect repeatField;
    Rect btnSave, btnCancel;
    Rect btnClear;
    Rect scrollbar;
    Rect calPrev, calNext, calTitle, calToday, calCancel;
    std::vector<RowHit> rows;
    std::vector<std::pair<int, Rect>> settingRows;
    std::vector<std::pair<int, Rect>> settingValues;
    std::vector<std::pair<int, Rect>> dropdownOptions;
    std::vector<std::pair<int64_t, Rect>> calendarDays;
};

struct MonitorArea {
    RECT work{};
    std::wstring device;
    HMONITOR handle = nullptr;
};

class App {
public:
    App();
    ~App();

    int Run(HINSTANCE hinst, bool autostart, int nCmdShow);

private:
    enum TimerId {
        TIMER_HOVER = 1,
        TIMER_COLLAPSE = 2,
        TIMER_PANEL_ANIM = 3,
        TIMER_CARET = 4,
        TIMER_TOAST = 5,
        TIMER_MIDNIGHT = 6,
        TIMER_BOOT = 7,
        TIMER_SNAP = 8,
        TIMER_BALL_GLOW = 9,
        TIMER_COMPLETE = 10,
    };

    enum Control {
        C_NONE = 0,
        C_ADD,
        C_PIN,
        C_SETTINGS,
        C_CLOSE,
        C_BACK,
        C_ARCHIVE_LINK,
        C_EMPTY_ADD,
        C_SAVE,
        C_CANCEL,
        C_DATE_MINUS,
        C_DATE_PLUS,
        C_DATE_FIELD,
        C_CHIP_TODAY,
        C_CHIP_TOMORROW,
        C_CHIP_WEEK,
        C_REPEAT,
        C_CLEAR_ARCHIVE,
        C_TITLE_FIELD,
        C_ROW,
        C_EDIT,
        C_CHECKBOX,
        C_RESTORE,
        C_DELETE,
        C_SETTING,
        C_BANNER,
        C_CAL_PREV,
        C_CAL_NEXT,
        C_CAL_DAY,
        C_CAL_TODAY,
        C_CAL_CANCEL,
        C_SETTING_OPTION,
        C_SLIDER,
    };

    // --- window plumbing ---
    static LRESULT CALLBACK BallProcStatic(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK PanelProcStatic(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK TrayProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT BallProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT PanelProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT TrayProc(HWND, UINT, WPARAM, LPARAM);

    bool InitApp();
    void RegisterClasses();
    void CreateWindows();
    void Shutdown();
    void AddTrayIcon();
    void RemoveTrayIcon();
    void UpdateTrayIcon();
    void UpdateTrayTip();
    void ShowTrayMenu(POINT pt);
    void OnTrayCommand(int id);
    void ToggleBallVisible();
    void ShowBall(bool activate);
    void HideBall();
    void ExitApp();

    // --- geometry ---
    MonitorArea MonitorAt(POINT pt) const;
    MonitorArea MonitorByName(const std::wstring& name) const;
    MonitorArea PrimaryMonitor() const;
    void RecomputeScale();
    void ResizeBallWindow();
    POINT ComputeBallWindowPos(const std::wstring& edge, double offset,
                               const std::wstring& monitor, int sizePx) const;
    void PlaceBallFromSettings();
    RECT BallCircleRect() const;
    void SnapBall();
    void StartSnapAnimation(POINT target);
    void FinishSnapAnimation();
    int DesiredCardHeight() const;
    void LayoutPanelWindow(bool force);
    void ComputePanelWindowPos(int w, int h, int& x, int& y) const;
    void BuildLayout();

    // --- painting ---
    void PaintBall();
    void PaintPanel();
    void PaintPanelView(Canvas& c, const Palette& p);
    void DrawPanelList(Canvas& c, const Palette& p);
    void DrawPanelForm(Canvas& c, const Palette& p);
    void DrawPanelArchive(Canvas& c, const Palette& p);
    void DrawPanelSettings(Canvas& c, const Palette& p);
    void DrawPanelDatePicker(Canvas& c, const Palette& p);
    void DrawHeaderButtons(Canvas& c, const Palette& p);
    void DrawFooter(Canvas& c, const Palette& p);
    void DrawToast(Canvas& c, const Palette& p);
    void RefreshTheme();
    bool ReduceMotionEnabled() const;
    Palette CurrentPalette() const;

    // --- panel state ---
    void ShowPanel(bool pinned, bool animate);
    void HidePanel(bool animate, bool resetView = true);
    void CompleteHidePanel();
    void StartPanelAnimation(bool show);
    void OnPanelAnimTimer();
    void OnPanelClick(POINT pt);
    void OnPanelMouseMove(POINT pt);
    void OnPanelMouseWheel(int delta);
    void OnPanelRightClick(POINT pt);
    void OnPanelKey(WPARAM key);
    void OnPanelChar(wchar_t ch);
    void OnImeComposition(LPARAM lParam);
    void SetCaretFromX(float x);
    void ResetCaretBlink();
    void SetImePosition();

    // --- views ---
    void OpenList();
    void OpenForm(const Task* task);
    void OpenDatePicker();
    void CloseDatePicker();
    void CancelForm();
    void SaveForm();
    void OpenArchive();
    void OpenSettings();
    void OpenDataFolder();
    void ResetBallPosition();
    void CycleSetting(int row);
    void OpenSettingDropdown(int row);
    void SelectSettingOption(int row, int option);
    void SetSliderFromX(int row, int x);
    void ApplySettingsLive();

    // --- tasks ---
    Task* FindTask(const std::wstring& id);
    const Task* FindTask(const std::wstring& id) const;
    std::vector<const Task*> SortedTodo() const;
    std::vector<const Task*> SortedArchive() const;
    void SaveTasksQuiet();
    void SaveSettingsQuiet();
    void RetrySave();
    void AfterTasksChanged(bool rebuildShape = true);
    void DeleteTask(const std::wstring& id);
    void CompleteTask(const std::wstring& id);
    void FinishComplete();
    void RestoreTask(const std::wstring& id);
    void DeleteArchiveTask(const std::wstring& id);
    void ClearArchive();
    void CopyTaskTitle(const std::wstring& id);

    // --- settings / system ---
    void ApplyAlwaysOnTop();
    void ApplyAutoStart();
    bool SetAutoStartRegistry(bool enable, std::wstring& error);
    bool IsAutoStartRegistryEnabled() const;
    bool CanUseDarkMode() const;
    void ScheduleMidnight();
    void OnMidnightTimer();
    void OnSettingsChanged();
    void ShowToast(const std::wstring& text);
    void SetBallGlowTarget(bool on);
    void OnBallGlowTimer();

    // --- helpers ---
    float S(float v) const { return v * s_; }
    int Px(float v) const { return (int)std::lround(v * s_); }
    float ScrollForView() const;
    void SetScrollForView(float value);
    float MaxScrollForView() const;
    void ClampScroll();
    Control HitTest(POINT pt, std::wstring* taskId = nullptr) const;
    void UpdateHover(POINT pt);
    void SetCursorForControl(Control control);
    void TrackMouseLeave(HWND hwnd, bool& flag);
    void ApplyDpi(UINT dpi);
    void SaveOnShutdown();
    void ShowRecoveryNotice();
    void UpdateTrayMenuState();

    HINSTANCE hinst_ = nullptr;
    HWND ball_ = nullptr;
    HWND panel_ = nullptr;
    HWND tray_ = nullptr;
    HICON trayIcon_ = nullptr;
    bool trayAdded_ = false;
    UINT taskbarCreatedMsg_ = 0;
    UINT showMsg_ = 0;

    Store store_;
    Settings settings_;
    std::vector<Task> tasks_;
    LoadResult load_;
    bool exiting_ = false;
    bool autostartRun_ = false;
    bool ballVisible_ = false;

    bool panelVisible_ = false;
    bool panelPinned_ = false;
    bool panelAnimating_ = false;
    bool animTargetVisible_ = false;
    float animT_ = 1.0f;
    ULONGLONG animStart_ = 0;
    int animDuration_ = 180;
    bool reduceMotion_ = false;

    bool mouseInBall_ = false;
    bool mouseInPanel_ = false;
    bool trackBallLeave_ = false;
    bool trackPanelLeave_ = false;
    bool hoverSuppressed_ = false;
    bool menuOpen_ = false;
    int hoverControl_ = C_NONE;
    std::wstring hoverTaskId_;

    bool dragging_ = false;
    bool dragMoved_ = false;
    POINT dragStart_{};
    RECT dragWindowStart_{};
    bool snapAnimating_ = false;
    POINT snapFrom_{};
    POINT snapTo_{};
    ULONGLONG snapStart_ = 0;

    bool ballGlowOn_ = false;
    bool ballGlowAnimating_ = false;
    float ballGlow_ = 0.0f;
    float ballGlowFrom_ = 0.0f;
    float ballGlowTarget_ = 0.0f;
    ULONGLONG ballGlowStart_ = 0;

    bool completing_ = false;
    std::wstring completingId_;
    ULONGLONG completeStart_ = 0;

    int calYear_ = 0;
    int calMonth_ = 0;

    UINT dpi_ = 96;
    float s_ = 1.0f;
    int ballPx_ = 52;
    int ballMargin_ = 8;
    int ballW_ = 0;
    int ballH_ = 0;
    int panelW_ = 0;
    int panelH_ = 0;
    int cardH_ = 0;
    int shadowPx_ = 10;
    Fonts fonts_;
    PanelLayout layout_;

    View view_ = View::List;
    float listScroll_ = 0;
    float settingsScroll_ = 0;
    float archiveScroll_ = 0;
    int settingsDropdown_ = -1;
    int settingsSlider_ = -1;
    bool sliderDragging_ = false;
    bool lastDark_ = false;

    std::wstring toast_;
    ULONGLONG toastUntil_ = 0;
    std::wstring saveError_;
    std::wstring dataNotice_;

    bool formEditing_ = false;
    std::wstring formId_;
    std::wstring formTitle_;
    int64_t formDue_ = 0;
    std::wstring formRepeat_;
    std::wstring formError_;
    std::wstring imeComp_;
    int caret_ = 0;
    bool caretOn_ = true;
    bool caretTimerOn_ = false;

    std::wstring menuTaskId_;
    std::wstring lastBackup_;
};

}  // namespace hf