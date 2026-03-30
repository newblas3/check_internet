#include "edge_controller.h"

namespace {

std::optional<std::wstring> ExtractField(const std::wstring& object, const std::wstring& key) {
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = object.find(pattern);
    if (keyPos == std::wstring::npos) {
        return std::nullopt;
    }
    const size_t colon = object.find(L':', keyPos + pattern.size());
    if (colon == std::wstring::npos) {
        return std::nullopt;
    }
    size_t firstQuote = object.find(L'"', colon + 1);
    if (firstQuote == std::wstring::npos) {
        return std::nullopt;
    }
    ++firstQuote;
    std::wstring value;
    bool escape = false;
    for (size_t i = firstQuote; i < object.size(); ++i) {
        const wchar_t ch = object[i];
        if (escape) {
            switch (ch) {
            case L'"': value.push_back(L'"'); break;
            case L'\\': value.push_back(L'\\'); break;
            case L'/': value.push_back(L'/'); break;
            case L'b': value.push_back(L'\b'); break;
            case L'f': value.push_back(L'\f'); break;
            case L'n': value.push_back(L'\n'); break;
            case L'r': value.push_back(L'\r'); break;
            case L't': value.push_back(L'\t'); break;
            default: value.push_back(ch); break;
            }
            escape = false;
            continue;
        }
        if (ch == L'\\') {
            escape = true;
            continue;
        }
        if (ch == L'"') {
            return value;
        }
        value.push_back(ch);
    }
    return std::nullopt;
}

} // namespace

EdgeController::EdgeController(const AppConfig& config, Logger& logger)
    : config_(config), logger_(logger), wifiManager_(logger_) {
}

CheckResult EdgeController::ExecuteCheck() {
    logger_.Info(L"Starting connectivity check.");
    if (IsInternetAvailable()) {
        logger_.Info(L"Internet connectivity check succeeded.");
        return CheckResult::Healthy;
    }

    logger_.Info(L"Internet connectivity check failed.");
    LogProbeDiagnostics();

    if (!config_.wifiSsid.empty()) {
        logger_.Info(L"Attempting Wi-Fi recovery for SSID: " + config_.wifiSsid);
        if (wifiManager_.EnsureConnected(config_.wifiSsid)) {
            Sleep(config_.wifiReconnectWaitSeconds * 1000);
            if (IsInternetAvailable()) {
                logger_.Info(L"Internet connectivity recovered after Wi-Fi reconnect.");
                return CheckResult::RemediationTriggered;
            }
            logger_.Info(L"Wi-Fi reconnect completed, but HTTP connectivity is still unavailable.");
            LogProbeDiagnostics();
        }
    }

    logger_.Info(L"Checking Edge remote debugging endpoint.");
    if (!probe_.IsTcpPortOpen(L"127.0.0.1", config_.debugPort, 1000)) {
        logger_.Info(L"Debug port is closed. Starting Edge with remote debugging.");
        if (!StartEdgeWithDebugPort()) {
            logger_.Error(L"Failed to start Edge with remote debugging.");
            return CheckResult::Failed;
        }
        Sleep(3000);
    }

    const auto tabs = FetchTabs();
    if (tabs.empty()) {
        logger_.Info(L"Could not retrieve tabs. Opening target URL directly.");
        return OpenTargetPage() ? CheckResult::RemediationTriggered : CheckResult::Failed;
    }

    if (ReloadTargetTab(tabs)) {
        logger_.Info(L"Reloaded existing target tab.");
        return CheckResult::RemediationTriggered;
    }

    logger_.Info(L"Target tab not found. Opening target URL.");
    return OpenTargetPage() ? CheckResult::RemediationTriggered : CheckResult::Failed;
}

bool EdgeController::IsInternetAvailable() {
    if (config_.connectivityCheckUrl.empty()) {
        for (const auto& host : config_.testHosts) {
            if (probe_.IsHostReachable(host, 2000)) {
                logger_.Info(L"HTTP connectivity check is disabled. Falling back to probe target: " + host);
                return true;
            }
        }
        return false;
    }

    const auto response = client_.Get(config_.connectivityCheckUrl, 3000);
    if (!response) {
        logger_.Info(L"Connectivity URL did not respond: " + config_.connectivityCheckUrl);
        return false;
    }

    const auto expectedStatus = static_cast<DWORD>(config_.connectivityExpectedStatus);
    if (response->statusCode != expectedStatus) {
        logger_.Info(L"Connectivity URL returned unexpected status " + std::to_wstring(response->statusCode) +
                     L", expected " + std::to_wstring(expectedStatus) + L". URL: " + config_.connectivityCheckUrl);
        return false;
    }

    return true;
}

void EdgeController::LogProbeDiagnostics() {
    for (const auto& host : config_.testHosts) {
        if (probe_.IsHostReachable(host, 2000)) {
            logger_.Info(L"Probe target is reachable, but HTTP connectivity is still unavailable: " + host);
            return;
        }
    }

    logger_.Info(L"All probe targets are unreachable.");
}

bool EdgeController::StartEdgeWithDebugPort() {
    if (!std::filesystem::exists(config_.edgePath)) {
        logger_.Error(L"Edge executable not found: " + config_.edgePath);
        return false;
    }

    std::wstring commandLine = L"\"" + config_.edgePath + L"\" --remote-debugging-port=" + std::to_wstring(config_.debugPort);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    const BOOL ok = CreateProcessW(nullptr,
                                   commandLine.data(),
                                   nullptr,
                                   nullptr,
                                   FALSE,
                                   CREATE_NO_WINDOW,
                                   nullptr,
                                   nullptr,
                                   &si,
                                   &pi);
    if (!ok) {
        logger_.Error(L"CreateProcessW failed when launching Edge.");
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool EdgeController::OpenTargetPage() {
    HINSTANCE instance = ShellExecuteW(nullptr, L"open", config_.edgePath.c_str(), config_.targetUrl.c_str(), nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(instance) <= 32) {
        logger_.Error(L"ShellExecuteW failed to open target page.");
        return false;
    }
    return true;
}

std::vector<EdgeController::TabInfo> EdgeController::FetchTabs() {
    const std::wstring url = L"http://127.0.0.1:" + std::to_wstring(config_.debugPort) + L"/json";
    const auto response = client_.Get(url, 2000);
    if (!response || response->statusCode != 200) {
        return {};
    }
    return ParseTabsJson(response->body);
}

bool EdgeController::ReloadTargetTab(const std::vector<TabInfo>& tabs) {
    for (const auto& tab : tabs) {
        if (tab.url.find(config_.targetUrl) == std::wstring::npos) {
            continue;
        }
        if (tab.webSocketDebuggerUrl.empty()) {
            continue;
        }

        std::wstring reloadUrl = tab.webSocketDebuggerUrl;
        const std::wstring wsPrefix = L"ws://";
        if (reloadUrl.rfind(wsPrefix, 0) == 0) {
            reloadUrl.replace(0, wsPrefix.size(), L"http://");
        } else if (reloadUrl.rfind(L"wss://", 0) == 0) {
            reloadUrl.replace(0, 6, L"https://");
        }
        reloadUrl += L"/reload";

        const auto response = client_.Post(reloadUrl, L"", 2000);
        return response.has_value() && response->statusCode >= 200 && response->statusCode < 300;
    }
    return false;
}

std::vector<EdgeController::TabInfo> EdgeController::ParseTabsJson(const std::wstring& json) {
    std::vector<TabInfo> tabs;
    size_t pos = 0;
    while (true) {
        const size_t start = json.find(L'{', pos);
        if (start == std::wstring::npos) {
            break;
        }
        const size_t end = json.find(L'}', start);
        if (end == std::wstring::npos) {
            break;
        }
        const std::wstring object = json.substr(start, end - start + 1);
        TabInfo tab;
        if (const auto url = ExtractField(object, L"url")) {
            tab.url = *url;
        }
        if (const auto ws = ExtractField(object, L"webSocketDebuggerUrl")) {
            tab.webSocketDebuggerUrl = *ws;
        }
        if (!tab.url.empty()) {
            tabs.push_back(std::move(tab));
        }
        pos = end + 1;
    }
    return tabs;
}
