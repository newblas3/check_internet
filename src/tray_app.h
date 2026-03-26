#pragma once

#include "app_config.h"
#include "edge_controller.h"
#include "logger.h"
#include "task_scheduler_installer.h"

class TrayApp {
public:
    TrayApp(HINSTANCE instance,
            std::filesystem::path baseDir,
            ConfigManager& configManager,
            AppConfig& config,
            Logger& logger);
    ~TrayApp();

    int Run();
    bool RunOnce();
    bool InstallOrUpdateStartup();
    bool UninstallStartup();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateHiddenWindow();
    bool AddTrayIcon();
    void RemoveTrayIcon();
    void ShowContextMenu();
    void ExecuteCheckAsync(bool notifyIfHealthy = false);
    void ReloadConfig();
    void OpenConfigFile() const;
    void ShowNotification(const std::wstring& title, const std::wstring& message, bool silent = false) const;
    std::wstring ExecutablePath() const;

    HINSTANCE instance_;
    std::filesystem::path baseDir_;
    ConfigManager& configManager_;
    AppConfig& config_;
    Logger& logger_;
    std::unique_ptr<EdgeController> controller_;
    std::unique_ptr<TaskSchedulerInstaller> installer_;
    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW nid_{};
    std::atomic_bool runningCheck_{false};

    static constexpr UINT kTrayIconId = 1001;
    static constexpr UINT kTrayMessage = WM_APP + 1;
    static constexpr UINT_PTR kTimerId = 42;
};
