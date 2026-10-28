#pragma once

#include "common.h"

namespace hf {

struct Task {
    std::wstring id;
    std::wstring title;
    int64_t dueDay = 0;  // days since 1970-01-01 (local calendar date)
    bool done = false;
    std::wstring createdAt;    // ISO local datetime
    std::wstring completedAt;  // ISO local datetime, empty when not done
    std::wstring repeat;       // empty / daily / weekly / monthly
    std::wstring kind = L"homework";  // homework / exam
};

struct BallSettings {
    std::wstring edge = L"right";  // left / right / top / bottom
    double offset = 0.42;          // 0..1 along the edge
    std::wstring display = L"days";  // days / icon
    int size = 52;                   // logical px, 48..64
    bool halfHidden = false;
    int opacity = 95;  // 70..100
    std::wstring monitor;  // device name, e.g. \\.\DISPLAY1
};

struct PanelSettings {
    bool alwaysOnTop = true;
    int opacity = 95;  // 80..100
};

struct AppSettings {
    bool autoStart = false;
    bool showOnBoot = true;
    std::wstring trigger = L"hover";       // hover / click
    std::wstring theme = L"system";        // system / light / dark
    std::wstring reduceMotion = L"system"; // system / on / off
    bool seenIntro = false;
};

struct Settings {
    BallSettings ball;
    PanelSettings panel;
    AppSettings app;
};

void NormalizeSettings(Settings& s);

int64_t TodayLocalDay();
int64_t DaysFromCivil(int y, int m, int d);
void CivilFromDay(int64_t z, int& y, int& m, int& d);
std::wstring FormatISODate(int64_t day);
bool ParseISODate(const std::wstring& text, int64_t& out);
std::wstring FormatMMDD(int64_t day);
std::wstring FormatFullDate(int64_t day);
std::wstring WeekdayCN(int64_t day);
std::wstring LocalNowISO();

enum class DueTone { Normal, Soon, Overdue };

std::wstring DueLabel(int64_t dueDay, int64_t today, DueTone& tone);
std::wstring RepeatShort(const std::wstring& repeat);
int64_t NextRepeatDay(int64_t dueDay, const std::wstring& repeat);
std::wstring MakeTaskId();

bool SortTodo(const Task* a, const Task* b);
bool SortArchive(const Task* a, const Task* b);

}  // namespace hf