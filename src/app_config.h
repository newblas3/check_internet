#pragma once

#include "common.h"

struct AppConfig {
    std::wstring testHost = L"8.8.8.8";
    std::vector<std::wstring> testHosts{L"8.8.8.8"};
    std::wstring targetUrl = L"http://192.168.2.135";
    std::wstring edgePath = L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
    std::wstring wifiSsid;
    uint16_t debugPort = 9222;
    uint32_t checkIntervalSeconds = 60;
    uint32_t wifiReconnectWaitSeconds = 8;
    std::wstring startupScope = L"logon";
    std::wstring logFile = L"edge_network_guard.log";
};

class ConfigManager {
public:
    explicit ConfigManager(std::filesystem::path baseDir);

    bool Load(AppConfig& config, std::wstring& error) const;
    const std::filesystem::path& ConfigPath() const noexcept;

private:
    std::filesystem::path configPath_;
};
