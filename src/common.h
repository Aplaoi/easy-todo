// Homework Float - common Win32 / GDI+ helpers
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <imm.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <map>
#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "rpcrt4.lib")

namespace hf {

inline std::wstring Format(const wchar_t* fmt, ...) {
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    return std::wstring(buf);
}

inline std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), n, nullptr, nullptr);
    return out;
}

inline std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), n);
    return out;
}

inline std::wstring Trim(const std::wstring& s) {
    size_t b = 0, e = s.size();
    while (b < e && iswspace(s[b])) ++b;
    while (e > b && iswspace(s[e - 1])) --e;
    return s.substr(b, e - b);
}

inline bool PathExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES;
}

inline bool DirectoryExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

inline std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}

inline bool ReadWholeFile(const std::wstring& path, std::string& out, std::wstring* err = nullptr) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        if (err) *err = Format(L"无法打开文件（错误 %lu）", GetLastError());
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > 32 * 1024 * 1024) {
        CloseHandle(h);
        if (err) *err = L"文件大小异常";
        return false;
    }
    out.resize((size_t)size.QuadPart);
    DWORD total = 0;
    while (total < out.size()) {
        DWORD got = 0;
        if (!ReadFile(h, out.data() + total, (DWORD)(out.size() - total), &got, nullptr) || got == 0) {
            CloseHandle(h);
            if (err) *err = Format(L"读取文件失败（错误 %lu）", GetLastError());
            return false;
        }
        total += got;
    }
    CloseHandle(h);
    return true;
}

inline std::wstring KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &p)) && p) {
        std::wstring result(p);
        CoTaskMemFree(p);
        return result;
    }
    if (p) CoTaskMemFree(p);
    return {};
}

inline uint64_t NowMs() {
    return GetTickCount64();
}

inline int ScaleInt(float v) {
    return (int)std::lround(v);
}

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;

    Rect() = default;
    Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}

    bool Contains(float px, float py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    bool Contains(const POINT& p) const { return Contains((float)p.x, (float)p.y); }
    Rect Inflate(float d) const { return Rect(x - d, y - d, w + 2 * d, h + 2 * d); }
    Rect Offset(float dx, float dy) const { return Rect(x + dx, y + dy, w, h); }
    float centerX() const { return x + w / 2; }
    float centerY() const { return y + h / 2; }
    Gdiplus::RectF ToGdi() const { return Gdiplus::RectF(x, y, w, h); }
};

inline bool IsDarkSystemTheme() {
    HKEY key = nullptr;
    DWORD value = 1, size = sizeof(value), type = REG_DWORD;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0,
                      KEY_READ, &key) == ERROR_SUCCESS) {
        RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type, (LPBYTE)&value, &size);
        RegCloseKey(key);
    }
    return value == 0;
}

inline bool SystemAnimationsEnabled() {
    BOOL enabled = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0);
    return enabled != FALSE;
}

}  // namespace hf