#pragma once

#include "app_config.h"
#include "logger.h"

class TaskSchedulerInstaller {
public:
    TaskSchedulerInstaller(std::wstring taskName, Logger& logger);

    bool Install(const std::wstring& executablePath, const AppConfig& config, bool runAtBoot);
    bool Uninstall();
    bool EnsureElevatedAndInstall(const std::wstring& executablePath);
    bool EnsureElevatedAndUninstall(const std::wstring& executablePath);
    bool IsRunningAsAdmin() const;

private:
    bool RegisterTask(const std::wstring& executablePath, const AppConfig& config, bool runAtBoot);
    bool RelaunchElevated(const std::wstring& executablePath, const std::wstring& arguments) const;

    std::wstring taskName_;
    Logger& logger_;
};
