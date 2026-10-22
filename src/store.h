#pragma once

#include "model.h"

namespace hf {

struct LoadResult {
    std::vector<Task> tasks;
    Settings settings;
    bool firstRun = false;
    bool tasksRecovered = false;
    std::wstring notice;       // shown once to the user
    std::wstring lastBackup;   // yyyy-MM-dd of newest backup
};

class Store {
public:
    bool Init(std::wstring& error);

    LoadResult Load();
    bool SaveTasks(const std::vector<Task>& tasks, std::wstring& error);
    bool SaveSettings(const Settings& settings, std::wstring& error);

    void Log(const std::wstring& text);

    const std::wstring& dataDir() const { return dir_; }
    const std::wstring& tasksPath() const { return tasksPath_; }
    const std::wstring& settingsPath() const { return settingsPath_; }
    const std::wstring& backupsDir() const { return backupsDir_; }

private:
    bool AtomicWrite(const std::wstring& path, const std::string& utf8, std::wstring& error);
    bool ParseTasksText(const std::string& utf8, std::vector<Task>& tasks, std::wstring& error,
                        int& skipped);
    bool ParseSettingsText(const std::string& utf8, Settings& settings, std::wstring& error);

    bool LoadTasksFile(const std::wstring& path, std::vector<Task>& tasks, std::wstring& error,
                       int& skipped);
    bool LoadSettingsFile(const std::wstring& path, Settings& settings, std::wstring& error);

    std::vector<std::wstring> ListBackups() const;
    bool TryRestoreBackup(std::vector<Task>& tasks, std::wstring& usedBackup, int& skipped);
    void BackupToday();
    void PruneBackups();
    std::wstring NewestBackupDate() const;
    void RotateLogs();

    std::wstring dir_;
    std::wstring tasksPath_;
    std::wstring settingsPath_;
    std::wstring backupsDir_;
    std::wstring logsDir_;
    std::wstring logPath_;
    std::wstring lastWriteError_;
};

}  // namespace hf