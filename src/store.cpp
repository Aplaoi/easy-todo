#include "store.h"

#include "json.h"

namespace hf {

namespace {

bool EnsureDirectory(const std::wstring& path) {
    if (path.empty()) return false;
    if (DirectoryExists(path)) return true;
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return CreateDirectoryW(path.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

std::wstring TimestampForFile() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return Format(L"%04d%02d%02d-%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                  st.wSecond);
}

std::string StripBom(const std::string& s) {
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB &&
        (unsigned char)s[2] == 0xBF)
        return s.substr(3);
    return s;
}

}  // namespace

bool Store::Init(std::wstring& error) {
    std::wstring appData = KnownFolder(FOLDERID_RoamingAppData);
    if (appData.empty()) {
        error = L"无法定位 %AppData% 目录";
        return false;
    }
    dir_ = JoinPath(appData, L"HomeworkFloat");
    wchar_t overrideDir[32768]{};
    DWORD overrideLen = GetEnvironmentVariableW(L"HF_DATA_DIR", overrideDir, 32768);
    if (overrideLen > 0 && overrideLen < 32768) dir_ = overrideDir;
    tasksPath_ = JoinPath(dir_, L"tasks.json");
    settingsPath_ = JoinPath(dir_, L"settings.json");
    backupsDir_ = JoinPath(dir_, L"backups");
    logsDir_ = JoinPath(dir_, L"logs");
    logPath_ = JoinPath(logsDir_, L"app.log");

    if (!EnsureDirectory(dir_) || !EnsureDirectory(backupsDir_) || !EnsureDirectory(logsDir_)) {
        error = Format(L"无法创建数据目录：%s", dir_.c_str());
        return false;
    }
    RotateLogs();
    Log(L"--- Homework Float 启动 ---");
    return true;
}

void Store::RotateLogs() {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(logPath_.c_str(), GetFileExInfoStandard, &data)) return;
    ULARGE_INTEGER size{};
    size.LowPart = data.nFileSizeLow;
    size.HighPart = data.nFileSizeHigh;
    if (size.QuadPart < 256 * 1024) return;

    std::wstring log1 = JoinPath(logsDir_, L"app.1.log");
    std::wstring log2 = JoinPath(logsDir_, L"app.2.log");
    DeleteFileW(log2.c_str());
    MoveFileExW(log1.c_str(), log2.c_str(), MOVEFILE_REPLACE_EXISTING);
    MoveFileExW(logPath_.c_str(), log1.c_str(), MOVEFILE_REPLACE_EXISTING);
}

void Store::Log(const std::wstring& text) {
    if (logPath_.empty()) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::wstring line = Format(L"[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n", st.wYear, st.wMonth, st.wDay,
                               st.wHour, st.wMinute, st.wSecond, text.c_str());
    std::string utf8 = WideToUtf8(line);
    HANDLE h = CreateFileW(logPath_.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    CloseHandle(h);
}

bool Store::AtomicWrite(const std::wstring& path, const std::string& utf8, std::wstring& error) {
    std::wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        error = Format(L"无法写入临时文件（错误 %lu）", GetLastError());
        return false;
    }
    const char* p = utf8.data();
    size_t remaining = utf8.size();
    bool ok = true;
    while (remaining > 0) {
        DWORD chunk = (DWORD)std::min<size_t>(remaining, 1u << 20);
        DWORD written = 0;
        if (!WriteFile(h, p, chunk, &written, nullptr) || written == 0) {
            error = Format(L"写入失败（错误 %lu）", GetLastError());
            ok = false;
            break;
        }
        p += written;
        remaining -= written;
    }
    if (ok && !FlushFileBuffers(h)) {
        error = Format(L"刷新磁盘失败（错误 %lu）", GetLastError());
        ok = false;
    }
    CloseHandle(h);
    if (!ok) {
        DeleteFileW(tmp.c_str());
        return false;
    }

    bool replaced = false;
    if (PathExists(path)) {
        replaced = ReplaceFileW(path.c_str(), tmp.c_str(), nullptr,
                                REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_WRITE_THROUGH, nullptr,
                                nullptr) != 0;
    }
    if (!replaced) {
        replaced = MoveFileExW(tmp.c_str(), path.c_str(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }
    if (!replaced) {
        error = Format(L"替换文件失败（错误 %lu）", GetLastError());
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

bool Store::ParseTasksText(const std::string& utf8, std::vector<Task>& tasks, std::wstring& error,
                           int& skipped) {
    tasks.clear();
    skipped = 0;
    JsonValue root;
    if (!ParseJson(Utf8ToWide(StripBom(utf8)), root, error)) return false;
    if (!root.IsObject()) {
        error = L"顶层不是 JSON 对象";
        return false;
    }
    const JsonValue* schema = root.Find(L"schemaVersion");
    if (!schema || schema->type != JsonValue::Type::Number) {
        error = L"缺少 schemaVersion";
        return false;
    }
    if (schema->number != 1) {
        error = Format(L"不支持的 schemaVersion：%.0f", schema->number);
        return false;
    }
    const JsonValue* arr = root.Find(L"tasks");
    if (!arr || !arr->IsArray()) {
        error = L"tasks 不是数组";
        return false;
    }
    for (const auto& item : arr->array) {
        if (!item.IsObject()) {
            ++skipped;
            continue;
        }
        Task t;
        t.id = item.GetString(L"id");
        t.title = Trim(item.GetString(L"title"));
        std::wstring due = item.GetString(L"dueDate");
        std::wstring status = item.GetString(L"status", L"todo");
        t.createdAt = item.GetString(L"createdAt");
        t.completedAt = item.GetString(L"completedAt");
        t.repeat = item.GetString(L"repeat");
        if (t.repeat != L"daily" && t.repeat != L"weekly" && t.repeat != L"monthly")
            t.repeat.clear();
        t.kind = item.GetString(L"kind", L"homework");
        if (t.kind != L"exam") t.kind = L"homework";
        if (t.id.empty() || t.title.empty() || !ParseISODate(due, t.dueDay)) {
            ++skipped;
            continue;
        }
        if (t.title.size() > 100) t.title.resize(100);
        t.done = status == L"done";
        if (t.done && t.completedAt.empty()) t.completedAt = t.createdAt;
        tasks.push_back(std::move(t));
    }
    return true;
}

bool Store::ParseSettingsText(const std::string& utf8, Settings& settings, std::wstring& error) {
    JsonValue root;
    if (!ParseJson(Utf8ToWide(StripBom(utf8)), root, error)) return false;
    if (!root.IsObject()) {
        error = L"顶层不是 JSON 对象";
        return false;
    }
    Settings s;  // defaults
    if (const JsonValue* ball = root.Find(L"ball")) {
        if (ball->IsObject()) {
            s.ball.edge = ball->GetString(L"edge", s.ball.edge);
            s.ball.offset = ball->GetNumber(L"offset", s.ball.offset);
            s.ball.display = ball->GetString(L"display", s.ball.display);
            s.ball.size = (int)ball->GetNumber(L"size", s.ball.size);
            s.ball.halfHidden = ball->GetBool(L"halfHidden", s.ball.halfHidden);
            s.ball.opacity = (int)ball->GetNumber(L"opacity", s.ball.opacity);
            s.ball.monitor = ball->GetString(L"monitor", s.ball.monitor);
        }
    }
    if (const JsonValue* panel = root.Find(L"panel")) {
        if (panel->IsObject()) {
            s.panel.alwaysOnTop = panel->GetBool(L"alwaysOnTop", s.panel.alwaysOnTop);
            s.panel.opacity = (int)panel->GetNumber(L"opacity", s.panel.opacity);
        }
    }
    if (const JsonValue* app = root.Find(L"app")) {
        if (app->IsObject()) {
            s.app.autoStart = app->GetBool(L"autoStart", s.app.autoStart);
            s.app.showOnBoot = app->GetBool(L"showOnBoot", s.app.showOnBoot);
            s.app.trigger = app->GetString(L"trigger", s.app.trigger);
            s.app.theme = app->GetString(L"theme", s.app.theme);
            s.app.reduceMotion = app->GetString(L"reduceMotion", s.app.reduceMotion);
            s.app.seenIntro = app->GetBool(L"seenIntro", s.app.seenIntro);
        }
    }
    NormalizeSettings(s);
    settings = s;
    return true;
}

bool Store::LoadTasksFile(const std::wstring& path, std::vector<Task>& tasks, std::wstring& error,
                          int& skipped) {
    std::string utf8;
    if (!ReadWholeFile(path, utf8, &error)) return false;
    return ParseTasksText(utf8, tasks, error, skipped);
}

bool Store::LoadSettingsFile(const std::wstring& path, Settings& settings, std::wstring& error) {
    std::string utf8;
    if (!ReadWholeFile(path, utf8, &error)) return false;
    return ParseSettingsText(utf8, settings, error);
}

std::vector<std::wstring> Store::ListBackups() const {
    std::vector<std::wstring> result;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(JoinPath(backupsDir_, L"tasks-*.json").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return result;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            result.push_back(JoinPath(backupsDir_, fd.cFileName));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(result.begin(), result.end(), [](const std::wstring& a, const std::wstring& b) {
        return a > b;  // newest filename first
    });
    return result;
}

std::wstring Store::NewestBackupDate() const {
    auto backups = ListBackups();
    if (backups.empty()) return L"";
    const std::wstring& name = backups.front();
    size_t slash = name.find_last_of(L"\\/");
    std::wstring base = slash == std::wstring::npos ? name : name.substr(slash + 1);
    // tasks-YYYYMMDD.json
    if (base.size() >= 18) return base.substr(6, 4) + L"-" + base.substr(10, 2) + L"-" + base.substr(12, 2);
    return L"";
}

bool Store::TryRestoreBackup(std::vector<Task>& tasks, std::wstring& usedBackup, int& skipped) {
    for (const auto& path : ListBackups()) {
        std::wstring error;
        int skip = 0;
        std::vector<Task> restored;
        if (LoadTasksFile(path, restored, error, skip)) {
            tasks = std::move(restored);
            usedBackup = path;
            skipped = skip;
            return true;
        }
    }
    return false;
}

void Store::BackupToday() {
    if (!PathExists(tasksPath_)) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::wstring dest =
        JoinPath(backupsDir_, Format(L"tasks-%04d%02d%02d.json", st.wYear, st.wMonth, st.wDay));
    if (PathExists(dest)) return;
    if (CopyFileW(tasksPath_.c_str(), dest.c_str(), FALSE)) {
        Log(Format(L"每日备份完成：%s", dest.c_str()));
        PruneBackups();
    } else {
        Log(Format(L"每日备份失败（错误 %lu）", GetLastError()));
    }
}

void Store::PruneBackups() {
    auto backups = ListBackups();
    for (size_t i = 7; i < backups.size(); ++i) {
        DeleteFileW(backups[i].c_str());
    }
}

LoadResult Store::Load() {
    LoadResult result;
    result.firstRun = !PathExists(tasksPath_) && !PathExists(settingsPath_);

    if (PathExists(settingsPath_)) {
        std::wstring error;
        if (!LoadSettingsFile(settingsPath_, result.settings, error)) {
            Log(L"设置读取失败：" + error + L"，已使用默认设置");
            result.settings = Settings{};
            NormalizeSettings(result.settings);
        }
    } else {
        result.settings = Settings{};
        NormalizeSettings(result.settings);
    }

    if (PathExists(tasksPath_)) {
        std::wstring error;
        int skipped = 0;
        if (!LoadTasksFile(tasksPath_, result.tasks, error, skipped)) {
            Log(L"任务文件损坏：" + error);
            std::wstring corrupt =
                JoinPath(dir_, L"tasks.corrupt-" + TimestampForFile() + L".json");
            for (int attempt = 1; PathExists(corrupt) && attempt < 100; ++attempt)
                corrupt = JoinPath(dir_, Format(L"tasks.corrupt-%s-%d.json", TimestampForFile().c_str(), attempt));
            if (!MoveFileExW(tasksPath_.c_str(), corrupt.c_str(), MOVEFILE_REPLACE_EXISTING))
                Log(Format(L"无法重命名损坏文件（错误 %lu）", GetLastError()));

            std::wstring usedBackup;
            int backupSkipped = 0;
            if (TryRestoreBackup(result.tasks, usedBackup, backupSkipped)) {
                result.tasksRecovered = true;
                result.notice = L"任务数据损坏，已从备份恢复：" + usedBackup;
                std::wstring saveError;
                // Re-establish tasks.json from the recovered backup.
                std::vector<Task> copy = result.tasks;
                if (!SaveTasks(copy, saveError))
                    Log(L"恢复后写回 tasks.json 失败：" + saveError);
                Log(result.notice);
            } else {
                result.tasks.clear();
                result.tasksRecovered = true;
                result.notice = L"任务数据损坏且没有可用备份，已创建空列表。损坏文件已保留：" + corrupt;
                Log(result.notice);
            }
        } else {
            if (skipped > 0) {
                Log(Format(L"已跳过 %d 条无效任务记录", skipped));
                result.notice = Format(L"已跳过 %d 条损坏的作业记录", skipped);
            }
        }
    }

    result.lastBackup = NewestBackupDate();
    return result;
}

bool Store::SaveTasks(const std::vector<Task>& tasks, std::wstring& error) {
    BackupToday();

    JsonValue root = JsonObject();
    root.Set(L"schemaVersion", JsonNum(1));
    JsonValue arr = JsonArray();
    for (const auto& t : tasks) {
        JsonValue item = JsonObject();
        item.Set(L"id", JsonStr(t.id));
        item.Set(L"title", JsonStr(t.title));
        item.Set(L"dueDate", JsonStr(FormatISODate(t.dueDay)));
        item.Set(L"status", JsonStr(t.done ? L"done" : L"todo"));
        item.Set(L"note", JsonStr(L""));
        item.Set(L"priority", JsonStr(L"normal"));
        item.Set(L"createdAt", JsonStr(t.createdAt.empty() ? LocalNowISO() : t.createdAt));
        if (!t.repeat.empty()) item.Set(L"repeat", JsonStr(t.repeat));
        if (t.kind == L"exam") item.Set(L"kind", JsonStr(L"exam"));
        if (t.completedAt.empty())
            item.object[L"completedAt"] = JsonValue{};
        else
            item.Set(L"completedAt", JsonStr(t.completedAt));
        arr.array.push_back(std::move(item));
    }
    root.Set(L"tasks", std::move(arr));

    std::string utf8 = WideToUtf8(WriteJson(root));
    if (!AtomicWrite(tasksPath_, utf8, error)) {
        lastWriteError_ = error;
        Log(L"任务保存失败：" + error);
        return false;
    }
    lastWriteError_.clear();
    return true;
}

bool Store::SaveSettings(const Settings& settings, std::wstring& error) {
    JsonValue root = JsonObject();
    root.Set(L"schemaVersion", JsonNum(1));

    JsonValue ball = JsonObject();
    ball.Set(L"edge", JsonStr(settings.ball.edge));
    ball.Set(L"offset", JsonNum(settings.ball.offset));
    ball.Set(L"display", JsonStr(settings.ball.display));
    ball.Set(L"size", JsonNum(settings.ball.size));
    ball.Set(L"halfHidden", JsonBool(settings.ball.halfHidden));
    ball.Set(L"opacity", JsonNum(settings.ball.opacity));
    ball.Set(L"monitor", JsonStr(settings.ball.monitor));
    root.Set(L"ball", std::move(ball));

    JsonValue panel = JsonObject();
    panel.Set(L"alwaysOnTop", JsonBool(settings.panel.alwaysOnTop));
    panel.Set(L"opacity", JsonNum(settings.panel.opacity));
    root.Set(L"panel", std::move(panel));

    JsonValue app = JsonObject();
    app.Set(L"autoStart", JsonBool(settings.app.autoStart));
    app.Set(L"showOnBoot", JsonBool(settings.app.showOnBoot));
    app.Set(L"trigger", JsonStr(settings.app.trigger));
    app.Set(L"theme", JsonStr(settings.app.theme));
    app.Set(L"reduceMotion", JsonStr(settings.app.reduceMotion));
    app.Set(L"seenIntro", JsonBool(settings.app.seenIntro));
    root.Set(L"app", std::move(app));

    std::string utf8 = WideToUtf8(WriteJson(root));
    if (!AtomicWrite(settingsPath_, utf8, error)) {
        lastWriteError_ = error;
        Log(L"设置保存失败：" + error);
        return false;
    }
    lastWriteError_.clear();
    return true;
}

}  // namespace hf