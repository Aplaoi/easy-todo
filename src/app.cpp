#include "app.h"

namespace hf {

namespace {

constexpr wchar_t kBallClass[] = L"HomeworkFloat.Ball";
constexpr wchar_t kPanelClass[] = L"HomeworkFloat.Panel";
constexpr wchar_t kTrayClass[] = L"HomeworkFloat.Tray";

constexpr UINT kTrayCallback = WM_APP + 1;
constexpr UINT kShowExisting = WM_APP + 10;
constexpr UINT kTrayId = 1;

enum TrayCommand {
    TRAY_OPEN = 1001,
    TRAY_TOGGLE = 1002,
    TRAY_AUTOSTART = 1003,
    TRAY_SETTINGS = 1004,
    TRAY_EXIT = 1005,
};

constexpr UINT kMenuEdit = 2001;
constexpr UINT kMenuComplete = 2002;
constexpr UINT kMenuCopy = 2003;
constexpr UINT kMenuDelete = 2004;

std::wstring ExePath() {
    wchar_t buf[MAX_PATH * 2]{};
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n == 0) return L"";
    return std::wstring(buf, n);
}

struct MonitorSearch {
    const wchar_t* name = nullptr;
    MONITORINFOEXW info{};
    bool found = false;
};

BOOL CALLBACK EnumMonitorProc(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto* search = reinterpret_cast<MonitorSearch*>(data);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        if (_wcsicmp(info.szDevice, search->name) == 0) {
            search->info = info;
            search->found = true;
            return FALSE;
        }
    }
    return TRUE;
}

HICON CreateAppIcon(int size) {
    using namespace Gdiplus;
    Bitmap bitmap(size, size, PixelFormat32bppPARGB);
    Graphics g(&bitmap);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));
    float inset = size * 0.06f;
    RectF rect(inset, inset, size - inset * 2, size - inset * 2);
    LinearGradientBrush brush(PointF(rect.X, rect.Y), PointF(rect.X + rect.Width, rect.Y + rect.Height),
                              Color(255, 92, 126, 255), Color(255, 107, 88, 248));
    g.FillEllipse(&brush, rect);
    Pen pen(Color(255, 255, 255, 255), std::max(1.6f, size * 0.085f));
    pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    PointF points[3] = {PointF(size * 0.29f, size * 0.52f), PointF(size * 0.44f, size * 0.67f),
                        PointF(size * 0.72f, size * 0.35f)};
    g.DrawLines(&pen, points, 3);
    HICON icon = nullptr;
    bitmap.GetHICON(&icon);
    return icon;
}

}  // namespace

App::App() = default;

App::~App() {
    if (trayIcon_) DestroyIcon(trayIcon_);
    trayIcon_ = nullptr;
}

int App::Run(HINSTANCE hinst, bool autostart, int nCmdShow) {
    (void)nCmdShow;
    hinst_ = hinst;
    autostartRun_ = autostart;
    if (!InitApp()) return 1;

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    Shutdown();
    return (int)msg.wParam;
}

bool App::InitApp() {
    std::wstring error;
    if (!store_.Init(error)) {
        MessageBoxW(nullptr, error.c_str(), L"作业悬浮球无法启动", MB_OK | MB_ICONERROR);
        return false;
    }
    load_ = store_.Load();
    tasks_ = load_.tasks;
    settings_ = load_.settings;
    dataNotice_ = load_.notice;
    lastBackup_ = load_.lastBackup;
    if (!PathExists(store_.tasksPath())) {
        std::wstring createError;
        store_.SaveTasks(tasks_, createError);
    }

    dpi_ = GetDpiForSystem();
    s_ = dpi_ / 96.0f;
    shadowPx_ = std::max(8, Px(10));
    ballMargin_ = std::max(10, Px(15));
    ballPx_ = Px((float)settings_.ball.size);
    reduceMotion_ = ReduceMotionEnabled();
    lastDark_ = CanUseDarkMode();

    RegisterClasses();
    CreateWindows();
    RecomputeScale();
    ResizeBallWindow();
    PlaceBallFromSettings();
    ApplyAlwaysOnTop();
    AddTrayIcon();

    if (settings_.app.autoStart) ApplyAutoStart();
    ScheduleMidnight();

    if (autostartRun_) {
        if (settings_.app.showOnBoot) SetTimer(tray_, TIMER_BOOT, 3000, nullptr);
    } else {
        ShowBall(false);
    }

    if (showMsg_ == 0) showMsg_ = kShowExisting;

    if (!load_.notice.empty()) ShowRecoveryNotice();

    if (!settings_.app.seenIntro) {
        settings_.app.seenIntro = true;
        SaveSettingsQuiet();
        if (trayAdded_) {
            NOTIFYICONDATAW nid{};
            nid.cbSize = sizeof(nid);
            nid.hWnd = tray_;
            nid.uID = kTrayId;
            nid.uFlags = NIF_INFO;
            nid.dwInfoFlags = NIIF_INFO;
            wcscpy_s(nid.szInfoTitle, L"作业悬浮球已启动");
            wcscpy_s(nid.szInfo,
                     L"鼠标移到小球上展开待办；右键托盘图标可打开设置。");
            Shell_NotifyIconW(NIM_MODIFY, &nid);
        }
    }
    return true;
}

void App::RegisterClasses() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = &App::BallProcStatic;
    wc.hInstance = hinst_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kBallClass;
    RegisterClassExW(&wc);

    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = &App::PanelProcStatic;
    wc.lpszClassName = kPanelClass;
    RegisterClassExW(&wc);

    wc.style = 0;
    wc.lpfnWndProc = &App::TrayProcStatic;
    wc.lpszClassName = kTrayClass;
    RegisterClassExW(&wc);
}

void App::CreateWindows() {
    DWORD ballEx = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE;
    ball_ = CreateWindowExW(ballEx, kBallClass, L"", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr,
                            hinst_, this);

    DWORD panelEx = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    if (settings_.panel.alwaysOnTop) panelEx |= WS_EX_TOPMOST;
    panel_ = CreateWindowExW(panelEx, kPanelClass, L"", WS_POPUP, 0, 0, 100, 100, nullptr,
                             nullptr, hinst_, this);

    tray_ = CreateWindowExW(0, kTrayClass, L"", WS_POPUP, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                            hinst_, this);
    taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");
}

void App::Shutdown() {
    SaveOnShutdown();
    RemoveTrayIcon();
    if (panel_ && IsWindow(panel_)) DestroyWindow(panel_);
    if (ball_ && IsWindow(ball_)) DestroyWindow(ball_);
    if (tray_ && IsWindow(tray_)) DestroyWindow(tray_);
    panel_ = ball_ = tray_ = nullptr;
    fonts_.Clear();
    store_.Log(L"--- Homework Float 退出 ---");
}

void App::SaveOnShutdown() {
    std::wstring error;
    if (!store_.SaveTasks(tasks_, error)) store_.Log(L"退出时保存任务失败：" + error);
    if (!store_.SaveSettings(settings_, error)) store_.Log(L"退出时保存设置失败：" + error);
}

void App::AddTrayIcon() {
    if (!tray_ || trayAdded_) return;
    trayIcon_ = CreateAppIcon(GetSystemMetrics(SM_CXSMICON) > 0 ? GetSystemMetrics(SM_CXSMICON) : 16);
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = tray_;
    nid.uID = kTrayId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kTrayCallback;
    nid.hIcon = trayIcon_ ? trayIcon_ : LoadIconW(nullptr, IDI_APPLICATION);
    int todoCount = 0;
    for (const auto& t : tasks_)
        if (!t.done) ++todoCount;
    std::wstring tip = Format(L"作业悬浮球 · %d 项待办", todoCount);
    wcsncpy_s(nid.szTip, tip.c_str(), _TRUNCATE);
    if (Shell_NotifyIconW(NIM_ADD, &nid)) {
        trayAdded_ = true;
    } else {
        store_.Log(L"托盘图标创建失败");
    }
}

void App::RemoveTrayIcon() {
    if (!trayAdded_ || !tray_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = tray_;
    nid.uID = kTrayId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    trayAdded_ = false;
}

void App::UpdateTrayIcon() {
    if (!trayAdded_ || !tray_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = tray_;
    nid.uID = kTrayId;
    nid.uFlags = NIF_ICON;
    nid.hIcon = trayIcon_ ? trayIcon_ : LoadIconW(nullptr, IDI_APPLICATION);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void App::UpdateTrayTip() {
    if (!trayAdded_ || !tray_) return;
    int todoCount = 0;
    for (const auto& t : tasks_)
        if (!t.done) ++todoCount;
    std::wstring tip = Format(L"作业悬浮球 · %d 项待办", todoCount);
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = tray_;
    nid.uID = kTrayId;
    nid.uFlags = NIF_TIP;
    wcsncpy_s(nid.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void App::ShowTrayMenu(POINT pt) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, TRAY_OPEN, L"展开待办");
    AppendMenuW(menu, MF_STRING, TRAY_TOGGLE, ballVisible_ ? L"隐藏小球" : L"显示小球");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (settings_.app.autoStart ? MF_CHECKED : 0), TRAY_AUTOSTART,
                L"开机自动启动");
    AppendMenuW(menu, MF_STRING, TRAY_SETTINGS, L"设置…");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, TRAY_EXIT, L"退出");

    menuOpen_ = true;
    SetForegroundWindow(panelVisible_ ? panel_ : ball_);
    int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y,
                                  0, tray_, nullptr);
    PostMessageW(panelVisible_ ? panel_ : ball_, WM_NULL, 0, 0);
    menuOpen_ = false;
    DestroyMenu(menu);
    if (cmd) OnTrayCommand(cmd);
}

void App::OnTrayCommand(int id) {
    switch (id) {
        case TRAY_OPEN:
            ShowBall(true);
            OpenList();
            ShowPanel(true, true);
            break;
        case TRAY_TOGGLE:
            ToggleBallVisible();
            break;
        case TRAY_AUTOSTART: {
            settings_.app.autoStart = !settings_.app.autoStart;
            if (settings_.app.autoStart) {
                ApplyAutoStart();
                if (!settings_.app.autoStart) break;
            } else {
                std::wstring error;
                SetAutoStartRegistry(false, error);
            }
            SaveSettingsQuiet();
            if (panelVisible_) PaintPanel();
            break;
        }
        case TRAY_SETTINGS:
            ShowBall(true);
            OpenSettings();
            ShowPanel(true, true);
            break;
        case TRAY_EXIT:
            ExitApp();
            break;
        default:
            break;
    }
}

void App::ToggleBallVisible() {
    if (ballVisible_)
        HideBall();
    else
        ShowBall(true);
}

void App::ShowBall(bool activate) {
    if (!ball_) return;
    PlaceBallFromSettings();
    ShowWindow(ball_, activate ? SW_SHOW : SW_SHOWNOACTIVATE);
    UINT actualDpi = GetDpiForWindow(ball_);
    if (actualDpi != 0 && actualDpi != dpi_) ApplyDpi(actualDpi);
    ballVisible_ = true;
    PaintBall();
    UpdateTrayTip();
}

void App::HideBall() {
    if (!ball_) return;
    HidePanel(false);
    KillTimer(ball_, TIMER_BALL_GLOW);
    ballGlow_ = 0.0f;
    ballGlowAnimating_ = false;
    ShowWindow(ball_, SW_HIDE);
    ballVisible_ = false;
    UpdateTrayTip();
}

void App::ExitApp() {
    if (exiting_) return;
    exiting_ = true;
    SaveOnShutdown();
    RemoveTrayIcon();
    HidePanel(false);
    if (panel_ && IsWindow(panel_)) DestroyWindow(panel_);
    panel_ = nullptr;
    if (ball_ && IsWindow(ball_)) DestroyWindow(ball_);
    ball_ = nullptr;
    if (tray_ && IsWindow(tray_)) DestroyWindow(tray_);
    tray_ = nullptr;
    PostQuitMessage(0);
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

MonitorArea App::MonitorAt(POINT pt) const {
    MonitorArea area;
    HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (monitor && GetMonitorInfoW(monitor, &info)) {
        area.work = info.rcWork;
        area.device = info.szDevice;
        area.handle = monitor;
    }
    return area;
}

MonitorArea App::MonitorByName(const std::wstring& name) const {
    MonitorArea area;
    if (!name.empty()) {
        MonitorSearch search;
        search.name = name.c_str();
        search.info.cbSize = sizeof(search.info);
        EnumDisplayMonitors(nullptr, nullptr, EnumMonitorProc, (LPARAM)&search);
        if (search.found) {
            area.work = search.info.rcWork;
            area.device = search.info.szDevice;
            area.handle = MonitorFromRect(&search.info.rcMonitor, MONITOR_DEFAULTTONEAREST);
            return area;
        }
    }
    return PrimaryMonitor();
}

MonitorArea App::PrimaryMonitor() const {
    MonitorArea area;
    POINT origin{0, 0};
    HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (monitor && GetMonitorInfoW(monitor, &info)) {
        area.work = info.rcWork;
        area.device = info.szDevice;
        area.handle = monitor;
    }
    return area;
}

void App::RecomputeScale() {
    if (ball_) {
        UINT dpi = GetDpiForWindow(ball_);
        if (dpi != 0) dpi_ = dpi;
    } else {
        dpi_ = GetDpiForSystem();
    }
    s_ = dpi_ / 96.0f;
    shadowPx_ = std::max(8, Px(10));
    ballMargin_ = std::max(10, Px(15));
    ballPx_ = Px((float)settings_.ball.size);
    fonts_.Clear();
    reduceMotion_ = ReduceMotionEnabled();
}

void App::ResizeBallWindow() {
    if (!ball_) return;
    int size = Px((float)settings_.ball.size);
    if (size < Px(48)) size = Px(48);
    if (size > Px(64)) size = Px(64);
    ballPx_ = size;
    int w = size + ballMargin_ * 2;
    int h = w;
    if (w != ballW_ || h != ballH_) {
        SetWindowPos(ball_, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        ballW_ = w;
        ballH_ = h;
    }
}

POINT App::ComputeBallWindowPos(const std::wstring& edge, double offset,
                                const std::wstring& monitor, int sizePx) const {
    MonitorArea area = MonitorByName(monitor);
    RECT work = area.work;
    int gap = Px(8);
    int x = work.left;
    int y = work.top;

    bool half = settings_.ball.halfHidden;
    if (edge == L"right") {
        x = half ? work.right - (int)std::lround(sizePx * 0.6) : work.right - gap - sizePx;
        double t = std::clamp(offset, 0.0, 1.0);
        y = work.top + (int)std::lround(t * std::max(0, (int)(work.bottom - work.top - sizePx)));
    } else if (edge == L"left") {
        x = half ? work.left - (int)std::lround(sizePx * 0.4) : work.left + gap;
        double t = std::clamp(offset, 0.0, 1.0);
        y = work.top + (int)std::lround(t * std::max(0, (int)(work.bottom - work.top - sizePx)));
    } else if (edge == L"top") {
        y = half ? work.top - (int)std::lround(sizePx * 0.4) : work.top + gap;
        double t = std::clamp(offset, 0.0, 1.0);
        x = work.left + (int)std::lround(t * std::max(0, (int)(work.right - work.left - sizePx)));
    } else {  // bottom
        y = half ? work.bottom - (int)std::lround(sizePx * 0.6) : work.bottom - gap - sizePx;
        double t = std::clamp(offset, 0.0, 1.0);
        x = work.left + (int)std::lround(t * std::max(0, (int)(work.right - work.left - sizePx)));
    }
    return POINT{x - ballMargin_, y - ballMargin_};
}

void App::PlaceBallFromSettings() {
    if (!ball_) return;
    POINT pos = ComputeBallWindowPos(settings_.ball.edge, settings_.ball.offset,
                                     settings_.ball.monitor, Px((float)settings_.ball.size));
    SetWindowPos(ball_, HWND_TOPMOST, pos.x, pos.y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    if (ballVisible_) PaintBall();
}

RECT App::BallCircleRect() const {
    RECT r{};
    if (!ball_) return r;
    GetWindowRect(ball_, &r);
    r.left += ballMargin_;
    r.top += ballMargin_;
    r.right = r.left + ballPx_;
    r.bottom = r.top + ballPx_;
    return r;
}

void App::SnapBall() {
    POINT cursor{};
    GetCursorPos(&cursor);
    MonitorArea area = MonitorAt(cursor);
    RECT work = area.work;
    RECT circle = BallCircleRect();
    int cx = (circle.left + circle.right) / 2;
    int cy = (circle.top + circle.bottom) / 2;

    int dLeft = cx - work.left;
    int dRight = work.right - cx;
    int dTop = cy - work.top;
    int dBottom = work.bottom - cy;
    int minDist = dLeft;
    std::wstring edge = L"left";
    if (dRight < minDist) {
        minDist = dRight;
        edge = L"right";
    }
    if (dTop < minDist) {
        minDist = dTop;
        edge = L"top";
    }
    if (dBottom < minDist) {
        minDist = dBottom;
        edge = L"bottom";
    }

    double offset = 0.5;
    if (edge == L"left" || edge == L"right") {
        int span = work.bottom - work.top - ballPx_;
        if (span > 0) offset = (double)(cy - work.top - ballPx_ / 2) / span;
    } else {
        int span = work.right - work.left - ballPx_;
        if (span > 0) offset = (double)(cx - work.left - ballPx_ / 2) / span;
    }
    offset = std::clamp(offset, 0.0, 1.0);

    settings_.ball.edge = edge;
    settings_.ball.offset = offset;
    settings_.ball.monitor = area.device;
    SaveSettingsQuiet();

    POINT target = ComputeBallWindowPos(edge, offset, area.device, ballPx_);
    StartSnapAnimation(target);
}

void App::StartSnapAnimation(POINT target) {
    if (!ball_) return;
    RECT current{};
    GetWindowRect(ball_, &current);
    snapFrom_ = POINT{current.left, current.top};
    snapTo_ = target;
    snapStart_ = NowMs();
    snapAnimating_ = true;
    SetTimer(ball_, TIMER_SNAP, 12, nullptr);
}

void App::FinishSnapAnimation() {
    if (!snapAnimating_) return;
    snapAnimating_ = false;
    KillTimer(ball_, TIMER_SNAP);
    SetWindowPos(ball_, HWND_TOPMOST, snapTo_.x, snapTo_.y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    PaintBall();
}

void App::SetBallGlowTarget(bool on) {
    ballGlowTarget_ = on ? 1.0f : 0.0f;
    if (reduceMotion_) {
        ballGlow_ = ballGlowTarget_;
        ballGlowAnimating_ = false;
        PaintBall();
        return;
    }
    if (std::abs(ballGlow_ - ballGlowTarget_) < 0.01f) return;
    ballGlowFrom_ = ballGlow_;
    ballGlowStart_ = NowMs();
    ballGlowAnimating_ = true;
    SetTimer(ball_, TIMER_BALL_GLOW, 15, nullptr);
}

void App::OnBallGlowTimer() {
    if (!ballGlowAnimating_) {
        KillTimer(ball_, TIMER_BALL_GLOW);
        return;
    }
    float t = std::clamp((NowMs() - ballGlowStart_) / 150.0f, 0.0f, 1.0f);
    float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    ballGlow_ = ballGlowFrom_ + (ballGlowTarget_ - ballGlowFrom_) * e;
    PaintBall();
    if (t >= 1.0f) {
        ballGlow_ = ballGlowTarget_;
        ballGlowAnimating_ = false;
        KillTimer(ball_, TIMER_BALL_GLOW);
    }
}

int App::DesiredCardHeight() const {
    float logical = 400.0f;
    switch (view_) {
        case View::List: {
            int rows = (int)SortedTodo().size();
            if (rows < 1) rows = 1;
            logical = 56.0f + 38.0f + rows * 58.0f;
            if (!saveError_.empty() || !dataNotice_.empty()) logical += 38.0f;
            logical = std::clamp(logical, 300.0f, 520.0f);
            break;
        }
        case View::Form:
            logical = 372.0f;
            break;
        case View::DatePicker:
            logical = 432.0f;
            break;
        case View::Archive: {
            int rows = (int)SortedArchive().size();
            if (rows < 1) rows = 1;
            logical = 56.0f + rows * 62.0f + 24.0f;
            logical = std::clamp(logical, 320.0f, 520.0f);
            break;
        }
        case View::Settings:
            logical = 540.0f;
            break;
    }
    int height = Px(logical);
    MonitorArea area = MonitorAt(POINT{(BallCircleRect().left + BallCircleRect().right) / 2,
                                       (BallCircleRect().top + BallCircleRect().bottom) / 2});
    int maxHeight = (area.work.bottom - area.work.top) - Px(24);
    if (maxHeight > Px(260)) height = std::min(height, maxHeight);
    return std::max(height, Px(240));
}

void App::ComputePanelWindowPos(int w, int h, int& x, int& y) const {
    int cardW = w - 2 * shadowPx_;
    int cardH = h - 2 * shadowPx_;
    RECT circle = BallCircleRect();
    int cx = (circle.left + circle.right) / 2;
    int cy = (circle.top + circle.bottom) / 2;
    MonitorArea area = MonitorAt(POINT{cx, cy});
    RECT work = area.work;
    int gap = Px(10);

    int cardX = 0, cardY = 0;
    const std::wstring& edge = settings_.ball.edge;
    if (edge == L"left" || edge == L"right") {
        cardY = cy - cardH / 2;
        if (edge == L"right")
            cardX = circle.left - gap - cardW;
        else
            cardX = circle.right + gap;
    } else {
        cardX = cx - cardW / 2;
        if (edge == L"top")
            cardY = circle.bottom + gap;
        else
            cardY = circle.top - gap - cardH;
    }

    int margin = Px(6);
    if (cardX + cardW > work.right - margin) cardX = work.right - margin - cardW;
    if (cardX < work.left + margin) cardX = work.left + margin;
    if (cardY + cardH > work.bottom - margin) cardY = work.bottom - margin - cardH;
    if (cardY < work.top + margin) cardY = work.top + margin;

    x = cardX - shadowPx_;
    y = cardY - shadowPx_;
}

void App::LayoutPanelWindow(bool force) {
    if (!panel_) return;
    int newW = Px(336.0f) + 2 * shadowPx_;
    int newCardH = DesiredCardHeight();
    int newH = newCardH + 2 * shadowPx_;
    if (!force && newW == panelW_ && newH == panelH_) {
        BuildLayout();
        return;
    }
    int x = 0, y = 0;
    ComputePanelWindowPos(newW, newH, x, y);
    panelW_ = newW;
    panelH_ = newH;
    cardH_ = newCardH;
    SetWindowPos(panel_, settings_.panel.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, x, y, newW,
                 newH, SWP_NOACTIVATE);
    BuildLayout();
    if (panelVisible_) PaintPanel();
}

// ---------------------------------------------------------------------------
// Window procedures
// ---------------------------------------------------------------------------

LRESULT CALLBACK App::BallProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->BallProc(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK App::PanelProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->PanelProc(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK App::TrayProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->TrayProc(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void App::TrackMouseLeave(HWND hwnd, bool& flag) {
    if (flag) return;
    TRACKMOUSEEVENT tme{};
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd;
    if (TrackMouseEvent(&tme)) flag = true;
}

LRESULT App::BallProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            RECT circle{ballMargin_, ballMargin_, ballMargin_ + ballPx_, ballMargin_ + ballPx_};
            if (pt.x >= circle.left && pt.x < circle.right && pt.y >= circle.top &&
                pt.y < circle.bottom)
                return HTCLIENT;
            return HTTRANSPARENT;
        }
        case WM_MOUSEMOVE: {
            TrackMouseLeave(hwnd, trackBallLeave_);
            mouseInBall_ = true;
            if (!ballGlowOn_) {
                ballGlowOn_ = true;
                SetBallGlowTarget(true);
            }
            if (dragging_) {
                POINT cur{};
                GetCursorPos(&cur);
                int dx = cur.x - dragStart_.x;
                int dy = cur.y - dragStart_.y;
                if (!dragMoved_ && (std::abs(dx) > 3 || std::abs(dy) > 3)) {
                    dragMoved_ = true;
                    if (panelVisible_) HidePanel(false);
                }
                if (dragMoved_) {
                    SetWindowPos(hwnd, HWND_TOPMOST, dragWindowStart_.left + dx,
                                 dragWindowStart_.top + dy, 0, 0,
                                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
                }
            } else if (settings_.app.trigger == L"hover" && !panelVisible_ && !hoverSuppressed_ &&
                       ballVisible_) {
                SetTimer(hwnd, TIMER_HOVER, 150, nullptr);
            }
            return 0;
        }
        case WM_MOUSELEAVE: {
            trackBallLeave_ = false;
            mouseInBall_ = false;
            hoverSuppressed_ = false;
            ballGlowOn_ = false;
            SetBallGlowTarget(false);
            KillTimer(hwnd, TIMER_HOVER);
            if (panelVisible_ && !panelPinned_ && view_ == View::List)
                SetTimer(panel_, TIMER_COLLAPSE, 350, nullptr);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetCapture(hwnd);
            dragging_ = true;
            dragMoved_ = false;
            GetCursorPos(&dragStart_);
            GetWindowRect(hwnd, &dragWindowStart_);
            KillTimer(panel_, TIMER_COLLAPSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            if (!dragging_) return 0;
            dragging_ = false;
            ReleaseCapture();
            if (dragMoved_) {
                dragMoved_ = false;
                SnapBall();
            } else {
                if (panelVisible_) {
                    HidePanel(true);
                    hoverSuppressed_ = true;
                } else {
                    ShowPanel(false, true);
                }
            }
            return 0;
        }
        case WM_LBUTTONDBLCLK:
            ShowPanel(false, true);
            return 0;
        case WM_RBUTTONUP: {
            POINT pt{};
            GetCursorPos(&pt);
            ShowTrayMenu(pt);
            return 0;
        }
        case WM_TIMER: {
            if (wp == TIMER_HOVER) {
                KillTimer(hwnd, TIMER_HOVER);
                POINT cur{};
                GetCursorPos(&cur);
                RECT circle = BallCircleRect();
                if (!panelVisible_ && ballVisible_ && !hoverSuppressed_ &&
                    cur.x >= circle.left && cur.x < circle.right && cur.y >= circle.top &&
                    cur.y < circle.bottom) {
                    ShowPanel(false, true);
                }
            } else if (wp == TIMER_SNAP) {
                uint64_t elapsed = NowMs() - snapStart_;
                float t = std::clamp(elapsed / 150.0f, 0.0f, 1.0f);
                float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
                int x = snapFrom_.x + (int)std::lround((snapTo_.x - snapFrom_.x) * e);
                int y = snapFrom_.y + (int)std::lround((snapTo_.y - snapFrom_.y) * e);
                SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0,
                             SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
                if (t >= 1.0f) FinishSnapAnimation();
            } else if (wp == TIMER_BALL_GLOW) {
                OnBallGlowTimer();
            }
            return 0;
        }
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;
        case WM_DPICHANGED: {
            UINT dpi = HIWORD(wp);
            ApplyDpi(dpi);
            return 0;
        }
        case WM_SETTINGCHANGE: {
            bool themeSystem = settings_.app.theme == L"system";
            bool motionSystem = settings_.app.reduceMotion == L"system";
            if (themeSystem || motionSystem) {
                RefreshTheme();
                reduceMotion_ = ReduceMotionEnabled();
                if (panelVisible_) PaintPanel();
            }
            if (lp && (_wcsicmp(reinterpret_cast<const wchar_t*>(lp), L"WindowMetrics") == 0 ||
                       _wcsicmp(reinterpret_cast<const wchar_t*>(lp), L"TraySettings") == 0)) {
                PlaceBallFromSettings();
                if (panelVisible_) LayoutPanelWindow(true);
            }
            return 0;
        }
        case WM_TIMECHANGE:
            ScheduleMidnight();
            PaintBall();
            if (panelVisible_) PaintPanel();
            return 0;
        case kShowExisting:
            ShowBall(true);
            OpenList();
            ShowPanel(true, true);
            return 0;
        case WM_QUERYENDSESSION:
            SaveOnShutdown();
            return TRUE;
        case WM_ENDSESSION:
            if (wp) SaveOnShutdown();
            return 0;
        case WM_DESTROY:
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            if (layout_.card.Contains((float)pt.x, (float)pt.y)) return HTCLIENT;
            return HTTRANSPARENT;
        }
        case WM_MOUSEMOVE: {
            TrackMouseLeave(hwnd, trackPanelLeave_);
            mouseInPanel_ = true;
            KillTimer(hwnd, TIMER_COLLAPSE);
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (sliderDragging_ && settingsSlider_ >= 0) {
                SetSliderFromX(settingsSlider_, pt.x);
                return 0;
            }
            OnPanelMouseMove(pt);
            return 0;
        }
        case WM_MOUSELEAVE: {
            trackPanelLeave_ = false;
            mouseInPanel_ = false;
            if (panelVisible_ && !panelPinned_ && !menuOpen_ && view_ == View::List)
                SetTimer(hwnd, TIMER_COLLAPSE, 350, nullptr);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            KillTimer(hwnd, TIMER_COLLAPSE);
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (view_ == View::Settings) {
                std::wstring id;
                Control c = HitTest(pt, &id);
                if (c == C_SLIDER && id.rfind(L"setting:", 0) == 0) {
                    settingsSlider_ = _wtoi(id.c_str() + 8);
                    sliderDragging_ = true;
                    SetCapture(hwnd);
                    SetSliderFromX(settingsSlider_, pt.x);
                }
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (sliderDragging_) {
                sliderDragging_ = false;
                settingsSlider_ = -1;
                ReleaseCapture();
                OnSettingsChanged();
                return 0;
            }
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            OnPanelClick(pt);
            return 0;
        }
        case WM_CAPTURECHANGED:
            sliderDragging_ = false;
            settingsSlider_ = -1;
            return 0;
        case WM_RBUTTONUP: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            OnPanelRightClick(pt);
            return 0;
        }
        case WM_MOUSEWHEEL:
            OnPanelMouseWheel(GET_WHEEL_DELTA_WPARAM(wp));
            return 0;
        case WM_KEYDOWN:
            OnPanelKey(wp);
            return 0;
        case WM_CHAR:
            OnPanelChar((wchar_t)wp);
            return 0;
        case WM_IME_STARTCOMPOSITION:
            imeComp_.clear();
            if (panelVisible_) PaintPanel();
            return 0;
        case WM_IME_COMPOSITION:
            OnImeComposition(lp);
            return 0;
        case WM_IME_ENDCOMPOSITION:
            imeComp_.clear();
            if (panelVisible_) PaintPanel();
            return 0;
        case WM_TIMER: {
            if (wp == TIMER_PANEL_ANIM) {
                OnPanelAnimTimer();
            } else if (wp == TIMER_CARET) {
                caretOn_ = !caretOn_;
                if (view_ == View::Form && panelVisible_) PaintPanel();
            } else if (wp == TIMER_TOAST) {
                KillTimer(hwnd, TIMER_TOAST);
                toast_.clear();
                if (panelVisible_) PaintPanel();
            } else if (wp == TIMER_COLLAPSE) {
                KillTimer(hwnd, TIMER_COLLAPSE);
                if (menuOpen_ || panelPinned_ || view_ != View::List) return 0;
                POINT cur{};
                GetCursorPos(&cur);
                RECT circle = BallCircleRect();
                RECT panelRect{};
                GetWindowRect(hwnd, &panelRect);
                bool inBall = cur.x >= circle.left && cur.x < circle.right && cur.y >= circle.top &&
                              cur.y < circle.bottom;
                bool inPanel = cur.x >= panelRect.left + shadowPx_ &&
                               cur.x < panelRect.right - shadowPx_ &&
                               cur.y >= panelRect.top + shadowPx_ &&
                               cur.y < panelRect.bottom - shadowPx_;
                if (!inBall && !inPanel) HidePanel(true);
            } else if (wp == TIMER_COMPLETE) {
                float t = std::clamp((NowMs() - completeStart_) / 320.0f, 0.0f, 1.0f);
                if (panelVisible_) PaintPanel();
                if (t >= 1.0f) FinishComplete();
            }
            return 0;
        }
        case WM_KILLFOCUS:
            caretOn_ = false;
            KillTimer(hwnd, TIMER_CARET);
            caretTimerOn_ = false;
            if (view_ == View::Form && panelVisible_) PaintPanel();
            return 0;
        case WM_SETCURSOR: {
            if (LOWORD(lp) == HTCLIENT) {
                SetCursorForControl((Control)hoverControl_);
                return TRUE;
            }
            break;
        }
        case WM_CLOSE:
            HidePanel(true);
            return 0;
        case WM_DPICHANGED:
            return 0;
        case WM_SETTINGCHANGE:
            if (settings_.app.theme == L"system" || settings_.app.reduceMotion == L"system") {
                RefreshTheme();
                reduceMotion_ = ReduceMotionEnabled();
                if (panelVisible_) PaintPanel();
            }
            return 0;
        case WM_QUERYENDSESSION:
            SaveOnShutdown();
            return TRUE;
        case WM_ENDSESSION:
            if (wp) SaveOnShutdown();
            return 0;
        case WM_DESTROY:
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::TrayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == taskbarCreatedMsg_ && taskbarCreatedMsg_ != 0) {
        trayAdded_ = false;
        AddTrayIcon();
        return 0;
    }
    if (msg == kTrayCallback) {
        switch ((UINT)lp) {
            case WM_LBUTTONUP:
                ToggleBallVisible();
                break;
            case WM_LBUTTONDBLCLK:
                ShowBall(true);
                OpenList();
                ShowPanel(true, true);
                break;
            case WM_RBUTTONUP:
            case WM_CONTEXTMENU: {
                POINT pt{};
                GetCursorPos(&pt);
                ShowTrayMenu(pt);
                break;
            }
            case NIN_BALLOONUSERCLICK:
                ShowBall(true);
                OpenList();
                ShowPanel(true, true);
                break;
            default:
                break;
        }
        return 0;
    }
    if (msg == WM_TIMER) {
        if (wp == TIMER_BOOT) {
            KillTimer(hwnd, TIMER_BOOT);
            ShowBall(false);
        } else if (wp == TIMER_MIDNIGHT) {
            OnMidnightTimer();
        }
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void App::ApplyDpi(UINT dpi) {
    if (dpi == 0 || dpi == dpi_) return;
    dpi_ = dpi;
    s_ = dpi_ / 96.0f;
    shadowPx_ = std::max(8, Px(10));
    ballMargin_ = std::max(10, Px(15));
    fonts_.Clear();
    ResizeBallWindow();
    PlaceBallFromSettings();
    if (panelVisible_) LayoutPanelWindow(true);
    PaintBall();
    if (panelVisible_) PaintPanel();
}

void App::ScheduleMidnight() {
    if (!tray_) return;
    KillTimer(tray_, TIMER_MIDNIGHT);
    SYSTEMTIME st{};
    GetLocalTime(&st);
    int seconds = (23 - st.wHour) * 3600 + (59 - st.wMinute) * 60 + (60 - st.wSecond);
    if (seconds < 2) seconds = 2;
    SetTimer(tray_, TIMER_MIDNIGHT, (UINT)seconds * 1000u + 1000u, nullptr);
}

void App::OnMidnightTimer() {
    KillTimer(tray_, TIMER_MIDNIGHT);
    PaintBall();
    if (panelVisible_) {
        LayoutPanelWindow(true);
        PaintPanel();
    }
    UpdateTrayTip();
    ScheduleMidnight();
    store_.Log(L"日期变更，已刷新剩余天数");
}

}  // namespace hf

