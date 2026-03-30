#pragma once

#include "app_config.h"
#include "http_client.h"
#include "logger.h"
#include "network_probe.h"
#include "wifi_manager.h"

enum class CheckResult {
    Healthy,
    RemediationTriggered,
    Failed,
};

class EdgeController {
public:
    EdgeController(const AppConfig& config, Logger& logger);

    CheckResult ExecuteCheck();

private:
    struct TabInfo {
        std::wstring url;
        std::wstring webSocketDebuggerUrl;
    };

    bool IsInternetAvailable();
    void LogProbeDiagnostics();
    bool StartEdgeWithDebugPort();
    bool OpenTargetPage();
    std::vector<TabInfo> FetchTabs();
    bool ReloadTargetTab(const std::vector<TabInfo>& tabs);
    static std::vector<TabInfo> ParseTabsJson(const std::wstring& json);

    const AppConfig& config_;
    Logger& logger_;
    NetworkProbe probe_;
    WifiManager wifiManager_;
    HttpClient client_;
};
