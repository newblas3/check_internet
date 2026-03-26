#include "tray_app.h"
#include "resource.h"

namespace {

constexpr UINT ID_TRAY_RUN_NOW = 2001;
constexpr UINT ID_TRAY_OPEN_CONFIG = 2002;
constexpr UINT ID_TRAY_RELOAD_CONFIG = 2003;
constexpr UINT ID_TRAY_INSTALL_STARTUP = 2004;
constexpr UINT ID_TRAY_UNINSTALL_STARTUP = 2005;
constexpr UINT ID_TRAY_EXIT = 2006;
constexpr wchar_t kWindowClassName[] = L"EdgeNetworkGuardTrayWindow";
constexpr wchar_t kTaskName[] = L"EdgeNetworkGuard";

HICON LoadAppIcon(HINSTANCE instance) {
    HICON icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!icon) {
        icon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    return icon;
}

} // namespace

TrayApp::TrayApp(HINSTANCE instance,
                 std::filesystem::path baseDir,
                 ConfigManager& configManager,
                 AppConfig& config,
                 Logger& logger)
    : instance_(instance),
      baseDir_(std::move(baseDir)),
      configManager_(configManager),
      config_(config),
      logger_(logger),
      controller_(std::make_unique<EdgeController>(config_, logger_)),
      installer_(std::make_unique<TaskSchedulerInstaller>(kTaskName, logger_)) {
}

TrayApp::~TrayApp() {
    RemoveTrayIcon();
}

int TrayApp::Run() {
    if (!CreateHiddenWindow() || !AddTrayIcon()) {
        return 1;
    }

    SetTimer(hwnd_, kTimerId, config_.checkIntervalSeconds * 1000, nullptr);
    ExecuteCheckAsync();

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

bool TrayApp::RunOnce() {
    return controller_->ExecuteCheck() != CheckResult::Failed;
}

bool TrayApp::InstallOrUpdateStartup() {
    const std::wstring exe = ExecutablePath();
    if (!installer_->IsRunningAsAdmin()) {
        const bool launched = installer_->EnsureElevatedAndInstall(exe);
        if (!launched) {
            logger_.Error(L"Elevation failed or was cancelled.");
            return false;
        }
        logger_.Info(L"Launched elevated installer instance.");
        return true;
    }

    if (!installer_->Install(exe, config_, _wcsicmp(config_.startupScope.c_str(), L"boot") == 0)) {
        logger_.Error(L"Failed to install startup task.");
        return false;
    }
    return true;
}

bool TrayApp::UninstallStartup() {
    const std::wstring exe = ExecutablePath();
    if (!installer_->IsRunningAsAdmin()) {
        const bool launched = installer_->EnsureElevatedAndUninstall(exe);
        if (!launched) {
            logger_.Error(L"Elevation failed or was cancelled.");
            return false;
        }
        logger_.Info(L"Launched elevated uninstall instance.");
        return true;
    }
    return installer_->Uninstall();
}

LRESULT CALLBACK TrayApp::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        const auto createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<TrayApp*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(hwnd, msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT TrayApp::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_RUN_NOW:
            ExecuteCheckAsync();
            return 0;
        case ID_TRAY_OPEN_CONFIG:
            OpenConfigFile();
            return 0;
        case ID_TRAY_RELOAD_CONFIG:
            ReloadConfig();
            return 0;
        case ID_TRAY_INSTALL_STARTUP:
            if (InstallOrUpdateStartup()) {
                ShowNotification(L"Edge Network Guard", L"Startup task installed or update launched.");
            } else {
                ShowNotification(L"Edge Network Guard", L"Failed to install startup task.");
            }
            return 0;
        case ID_TRAY_UNINSTALL_STARTUP:
            if (UninstallStartup()) {
                ShowNotification(L"Edge Network Guard", L"Startup task removed or uninstall launched.");
            } else {
                ShowNotification(L"Edge Network Guard", L"Failed to remove startup task.");
            }
            return 0;
        case ID_TRAY_EXIT:
            DestroyWindow(hwnd_);
            return 0;
        default:
            break;
        }
        break;
    case WM_TIMER:
        if (wParam == kTimerId) {
            ExecuteCheckAsync();
            return 0;
        }
        break;
    case kTrayMessage:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            ShowContextMenu();
            return 0;
        }
        if (lParam == WM_LBUTTONDBLCLK) {
            ExecuteCheckAsync(true);
            return 0;
        }
        break;
    case WM_DESTROY:
        KillTimer(hwnd_, kTimerId);
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool TrayApp::CreateHiddenWindow() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TrayApp::WndProc;
    wc.hInstance = instance_;
    wc.lpszClassName = kWindowClassName;
    wc.hIcon = LoadAppIcon(instance_);
    wc.hIconSm = LoadAppIcon(instance_);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        logger_.Error(L"Failed to register window class.");
        return false;
    }

    hwnd_ = CreateWindowExW(0, kWindowClassName, L"Edge Network Guard", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                            nullptr, nullptr, instance_, this);
    if (!hwnd_) {
        logger_.Error(L"Failed to create hidden window.");
        return false;
    }
    ShowWindow(hwnd_, SW_HIDE);
    return true;
}

bool TrayApp::AddTrayIcon() {
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd_;
    nid_.uID = kTrayIconId;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = kTrayMessage;
    nid_.hIcon = LoadAppIcon(instance_);
    wcscpy_s(nid_.szTip, L"Edge Network Guard");
    return Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
}

void TrayApp::RemoveTrayIcon() {
    if (nid_.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        nid_.hWnd = nullptr;
    }
}

void TrayApp::ShowContextMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    AppendMenuW(menu, MF_STRING, ID_TRAY_RUN_NOW, L"Run Now");
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN_CONFIG, L"Open Config");
    AppendMenuW(menu, MF_STRING, ID_TRAY_RELOAD_CONFIG, L"Reload Config");
    AppendMenuW(menu, MF_STRING, ID_TRAY_INSTALL_STARTUP, L"Install Startup");
    AppendMenuW(menu, MF_STRING, ID_TRAY_UNINSTALL_STARTUP, L"Remove Startup");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void TrayApp::ExecuteCheckAsync(bool notifyIfHealthy) {
    bool expected = false;
    if (!runningCheck_.compare_exchange_strong(expected, true)) {
        return;
    }

    std::thread([this, notifyIfHealthy]() {
        const auto result = controller_->ExecuteCheck();
        if (notifyIfHealthy && result == CheckResult::Healthy) {
            ShowNotification(L"Edge Network Guard", L"Already online.", true);
        }
        runningCheck_ = false;
    }).detach();
}

void TrayApp::ReloadConfig() {
    std::wstring error;
    AppConfig updated = config_;
    if (!configManager_.Load(updated, error)) {
        logger_.Error(error);
        ShowNotification(L"Edge Network Guard", L"Config reload failed.");
        return;
    }

    config_ = updated;
    controller_ = std::make_unique<EdgeController>(config_, logger_);
    KillTimer(hwnd_, kTimerId);
    SetTimer(hwnd_, kTimerId, config_.checkIntervalSeconds * 1000, nullptr);
    logger_.Info(L"Configuration reloaded successfully.");
    ShowNotification(L"Edge Network Guard", L"Config reloaded.");
}

void TrayApp::OpenConfigFile() const {
    ShellExecuteW(hwnd_, L"open", configManager_.ConfigPath().wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void TrayApp::ShowNotification(const std::wstring& title, const std::wstring& message, bool silent) const {
    NOTIFYICONDATAW balloon = nid_;
    balloon.uFlags = NIF_INFO;
    wcsncpy_s(balloon.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(balloon.szInfo, message.c_str(), _TRUNCATE);
    balloon.dwInfoFlags = NIIF_INFO;
#ifdef NIIF_NOSOUND
    if (silent) {
        balloon.dwInfoFlags |= NIIF_NOSOUND;
    }
#else
    (void)silent;
#endif
    Shell_NotifyIconW(NIM_MODIFY, &balloon);
}

std::wstring TrayApp::ExecutablePath() const {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return path;
}
