#include "app_config.h"
#include "common.h"
#include "logger.h"
#include "tray_app.h"

namespace {

std::filesystem::path GetExecutableDirectory() {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return std::filesystem::path(path).parent_path();
}

bool HasArg(const std::wstring& needle) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return false;
    }
    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], needle.c_str()) == 0) {
            found = true;
            break;
        }
    }
    LocalFree(argv);
    return found;
}

int RunApplication(HINSTANCE instance) {
    const auto baseDir = GetExecutableDirectory();

    ConfigManager configManager(baseDir);
    AppConfig config;
    std::wstring configError;
    configManager.Load(config, configError);

    const std::filesystem::path logPath = baseDir / config.logFile;
    Logger logger(logPath);

    if (!configError.empty()) {
        logger.Error(configError);
    }

    TrayApp app(instance, baseDir, configManager, config, logger);

    if (HasArg(L"--install")) {
        return app.InstallOrUpdateStartup() ? 0 : 1;
    }
    if (HasArg(L"--uninstall")) {
        return app.UninstallStartup() ? 0 : 1;
    }
    if (HasArg(L"--run-once")) {
        return app.RunOnce() ? 0 : 1;
    }

    return app.Run();
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    return RunApplication(instance);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR, int show) {
    return wWinMain(instance, previous, GetCommandLineW(), show);
}
