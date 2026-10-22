#include "model.h"

#include <rpc.h>

namespace hf {

void NormalizeSettings(Settings& s) {
    if (s.ball.edge != L"left" && s.ball.edge != L"right" && s.ball.edge != L"top" &&
        s.ball.edge != L"bottom")
        s.ball.edge = L"right";
    if (s.ball.offset < 0.0) s.ball.offset = 0.0;
    if (s.ball.offset > 1.0) s.ball.offset = 1.0;
    if (s.ball.display != L"days" && s.ball.display != L"icon") s.ball.display = L"days";
    if (s.ball.size < 48) s.ball.size = 48;
    if (s.ball.size > 64) s.ball.size = 64;
    if (s.ball.opacity < 70) s.ball.opacity = 70;
    if (s.ball.opacity > 100) s.ball.opacity = 100;
    if (s.panel.opacity < 80) s.panel.opacity = 80;
    if (s.panel.opacity > 100) s.panel.opacity = 100;
    if (s.app.trigger != L"hover" && s.app.trigger != L"click") s.app.trigger = L"hover";
    if (s.app.theme != L"system" && s.app.theme != L"light" && s.app.theme != L"dark")
        s.app.theme = L"system";
    if (s.app.reduceMotion != L"system" && s.app.reduceMotion != L"on" &&
        s.app.reduceMotion != L"off")
        s.app.reduceMotion = L"system";
}

int64_t DaysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy =
        static_cast<unsigned>((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

void CivilFromDay(int64_t z, int& y, int& m, int& d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int year = static_cast<int>(yoe) + static_cast<int>(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp + (mp < 10 ? 3 : -9));
    y = year + (m <= 2);
}

int64_t TodayLocalDay() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return DaysFromCivil(st.wYear, st.wMonth, st.wDay);
}

std::wstring FormatISODate(int64_t day) {
    int y = 0, m = 0, d = 0;
    CivilFromDay(day, y, m, d);
    return Format(L"%04d-%02d-%02d", y, m, d);
}

bool ParseISODate(const std::wstring& text, int64_t& out) {
    if (text.size() != 10) return false;
    if (text[4] != L'-' || text[7] != L'-') return false;
    for (int i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) continue;
        if (text[i] < L'0' || text[i] > L'9') return false;
    }
    int y = (text[0] - L'0') * 1000 + (text[1] - L'0') * 100 + (text[2] - L'0') * 10 +
            (text[3] - L'0');
    int m = (text[5] - L'0') * 10 + (text[6] - L'0');
    int d = (text[8] - L'0') * 10 + (text[9] - L'0');
    if (y < 1900 || y > 2200 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    int64_t day = DaysFromCivil(y, m, d);
    int y2 = 0, m2 = 0, d2 = 0;
    CivilFromDay(day, y2, m2, d2);
    if (y2 != y || m2 != m || d2 != d) return false;
    out = day;
    return true;
}

std::wstring FormatMMDD(int64_t day) {
    int y = 0, m = 0, d = 0;
    CivilFromDay(day, y, m, d);
    return Format(L"%d/%d", m, d);
}

std::wstring FormatFullDate(int64_t day) {
    int y = 0, m = 0, d = 0;
    CivilFromDay(day, y, m, d);
    return Format(L"%04d/%d/%d", y, m, d);
}

std::wstring WeekdayCN(int64_t day) {
    static const wchar_t* names[] = {L"周日", L"周一", L"周二", L"周三", L"周四", L"周五", L"周六"};
    int64_t idx = ((day + 4) % 7 + 7) % 7;  // 1970-01-01 was a Thursday
    return names[idx];
}

std::wstring LocalNowISO() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return Format(L"%04d-%02d-%02dT%02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour,
                  st.wMinute, st.wSecond);
}

std::wstring DueLabel(int64_t dueDay, int64_t today, DueTone& tone) {
    int64_t diff = dueDay - today;
    if (diff < 0) {
        tone = DueTone::Overdue;
        return Format(L"已逾期 %lld 天", (long long)(-diff));
    }
    if (diff == 0) {
        tone = DueTone::Soon;
        return L"今天截止";
    }
    if (diff == 1) {
        tone = DueTone::Soon;
        return L"剩 1 天";
    }
    tone = DueTone::Normal;
    return Format(L"剩 %lld 天", (long long)diff);
}

std::wstring MakeTaskId() {
    UUID uuid{};
    if (UuidCreate(&uuid) != RPC_S_OK) return Format(L"task-%llu", (unsigned long long)NowMs());
    RPC_WSTR str = nullptr;
    if (UuidToStringW(&uuid, &str) != RPC_S_OK || !str) return Format(L"task-%llu", (unsigned long long)NowMs());
    std::wstring result(reinterpret_cast<wchar_t*>(str));
    RpcStringFreeW(&str);
    return result;
}

bool SortTodo(const Task* a, const Task* b) {
    if (a->dueDay != b->dueDay) return a->dueDay < b->dueDay;
    if (a->createdAt != b->createdAt) return a->createdAt < b->createdAt;
    return a->id < b->id;
}

bool SortArchive(const Task* a, const Task* b) {
    if (a->completedAt != b->completedAt) return a->completedAt > b->completedAt;
    return a->id < b->id;
}

}  // namespace hf