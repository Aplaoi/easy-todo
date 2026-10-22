#include "app.h"

int WINAPI wWinMain(HINSTANCE hinst, HINSTANCE, PWSTR commandLine, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HANDLE instanceMutex =
        CreateMutexW(nullptr, TRUE, L"Local\\HomeworkFloat.SingleInstance");
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = nullptr;
        for (int i = 0; i < 30; ++i) {
            existing = FindWindowW(L"HomeworkFloat.Ball", nullptr);
            if (existing) break;
            Sleep(100);
        }
        if (existing) PostMessageW(existing, WM_APP + 10, 0, 0);
        CloseHandle(instanceMutex);
        return 0;
    }

    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) {
        MessageBoxW(nullptr, L"GDI+ 初始化失败，无法启动。", L"作业悬浮球", MB_OK | MB_ICONERROR);
        if (SUCCEEDED(com)) CoUninitialize();
        if (instanceMutex) CloseHandle(instanceMutex);
        return 1;
    }

    bool autostart = commandLine && wcsstr(commandLine, L"--autostart") != nullptr;
    hf::App app;
    int result = app.Run(hinst, autostart, nCmdShow);

    Gdiplus::GdiplusShutdown(token);
    if (SUCCEEDED(com)) CoUninitialize();
    if (instanceMutex) {
        ReleaseMutex(instanceMutex);
        CloseHandle(instanceMutex);
    }
    return result;
}