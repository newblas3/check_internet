#pragma once

#include "common.h"
#include "logger.h"

class WifiManager {
public:
    explicit WifiManager(Logger& logger);

    bool EnsureConnected(const std::wstring& targetSsid);

private:
    struct HandleCloser {
        void operator()(HANDLE handle) const;
    };

    struct MemoryFreer {
        void operator()(void* ptr) const;
    };

    using ClientHandle = std::unique_ptr<void, HandleCloser>;

    std::optional<ClientHandle> OpenClient() const;
    bool EnableInterface(HANDLE clientHandle, const GUID& interfaceGuid) const;
    bool ConnectInterface(HANDLE clientHandle, const GUID& interfaceGuid, const std::wstring& targetSsid) const;
    bool IsTargetConnected(HANDLE clientHandle, const GUID& interfaceGuid, const std::wstring& targetSsid) const;
    static std::wstring SsidToString(const DOT11_SSID& ssid);

    Logger& logger_;
};
