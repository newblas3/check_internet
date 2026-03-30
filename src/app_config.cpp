#include "app_config.h"

namespace {

std::optional<std::wstring> ExtractString(const std::wstring& json, const std::wstring& key) {
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::wstring::npos) {
        return std::nullopt;
    }
    const size_t colon = json.find(L':', keyPos + pattern.size());
    if (colon == std::wstring::npos) {
        return std::nullopt;
    }
    size_t firstQuote = json.find(L'"', colon + 1);
    if (firstQuote == std::wstring::npos) {
        return std::nullopt;
    }
    ++firstQuote;
    std::wstring value;
    bool escape = false;
    for (size_t i = firstQuote; i < json.size(); ++i) {
        const wchar_t ch = json[i];
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

std::optional<uint32_t> ExtractUInt(const std::wstring& json, const std::wstring& key) {
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::wstring::npos) {
        return std::nullopt;
    }
    const size_t colon = json.find(L':', keyPos + pattern.size());
    if (colon == std::wstring::npos) {
        return std::nullopt;
    }
    size_t begin = colon + 1;
    while (begin < json.size() && iswspace(json[begin])) {
        ++begin;
    }
    size_t end = begin;
    while (end < json.size() && iswdigit(json[end])) {
        ++end;
    }
    if (begin == end) {
        return std::nullopt;
    }
    return static_cast<uint32_t>(std::stoul(json.substr(begin, end - begin)));
}

std::vector<std::wstring> ExtractStringArray(const std::wstring& json, const std::wstring& key) {
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::wstring::npos) {
        return {};
    }
    const size_t colon = json.find(L':', keyPos + pattern.size());
    if (colon == std::wstring::npos) {
        return {};
    }
    const size_t arrayStart = json.find(L'[', colon + 1);
    if (arrayStart == std::wstring::npos) {
        return {};
    }
    const size_t arrayEnd = json.find(L']', arrayStart + 1);
    if (arrayEnd == std::wstring::npos) {
        return {};
    }

    std::vector<std::wstring> values;
    size_t pos = arrayStart + 1;
    while (pos < arrayEnd) {
        const size_t quoteStart = json.find(L'"', pos);
        if (quoteStart == std::wstring::npos || quoteStart >= arrayEnd) {
            break;
        }
        const size_t quoteEnd = json.find(L'"', quoteStart + 1);
        if (quoteEnd == std::wstring::npos || quoteEnd > arrayEnd) {
            break;
        }
        values.push_back(json.substr(quoteStart + 1, quoteEnd - quoteStart - 1));
        pos = quoteEnd + 1;
    }
    return values;
}

} // namespace

ConfigManager::ConfigManager(std::filesystem::path baseDir)
    : configPath_(std::move(baseDir) / "check_network_config.json") {
}

bool ConfigManager::Load(AppConfig& config, std::wstring& error) const {
    std::wifstream file(configPath_);
    file.imbue(std::locale(file.getloc(), new std::codecvt_utf8_utf16<wchar_t>));
    if (!file) {
        error = L"Unable to open config file: " + configPath_.wstring();
        return false;
    }

    std::wstringstream buffer;
    buffer << file.rdbuf();
    const std::wstring json = buffer.str();

    if (const auto value = ExtractString(json, L"test_host")) {
        config.testHost = *value;
    }
    const auto hostArray = ExtractStringArray(json, L"test_hosts");
    if (!hostArray.empty()) {
        config.testHosts = hostArray;
    } else if (!config.testHost.empty()) {
        config.testHosts = {config.testHost};
    }
    if (const auto value = ExtractString(json, L"connectivity_check_url")) {
        config.connectivityCheckUrl = *value;
    }
    if (const auto value = ExtractUInt(json, L"connectivity_expected_status")) {
        config.connectivityExpectedStatus = static_cast<uint16_t>(*value);
    }
    if (const auto value = ExtractString(json, L"target_url")) {
        config.targetUrl = *value;
    }
    if (const auto value = ExtractString(json, L"edge_path")) {
        config.edgePath = *value;
    }
    if (const auto value = ExtractString(json, L"wifi_ssid")) {
        config.wifiSsid = *value;
    }
    if (const auto value = ExtractString(json, L"startup_scope")) {
        config.startupScope = *value;
    }
    if (const auto value = ExtractString(json, L"log_file")) {
        config.logFile = *value;
    }
    if (const auto value = ExtractUInt(json, L"debug_port")) {
        config.debugPort = static_cast<uint16_t>(*value);
    }
    if (const auto value = ExtractUInt(json, L"check_interval_seconds")) {
        config.checkIntervalSeconds = *value;
    }
    if (const auto value = ExtractUInt(json, L"wifi_reconnect_wait_seconds")) {
        config.wifiReconnectWaitSeconds = *value;
    }

    if (config.targetUrl.empty()) {
        error = L"Config field target_url is required.";
        return false;
    }
    if (config.edgePath.empty()) {
        error = L"Config field edge_path is required.";
        return false;
    }
    if (config.testHosts.empty()) {
        error = L"At least one probe target is required in test_host or test_hosts.";
        return false;
    }
    if (config.connectivityExpectedStatus == 0) {
        error = L"Config field connectivity_expected_status must be greater than zero.";
        return false;
    }
    if (config.checkIntervalSeconds == 0) {
        error = L"Config field check_interval_seconds must be greater than zero.";
        return false;
    }
    if (config.wifiReconnectWaitSeconds == 0) {
        config.wifiReconnectWaitSeconds = 8;
    }

    return true;
}

const std::filesystem::path& ConfigManager::ConfigPath() const noexcept {
    return configPath_;
}
