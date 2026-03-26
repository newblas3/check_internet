#pragma once

#include "common.h"

class NetworkProbe {
public:
    bool PingHost(const std::wstring& host, uint32_t timeoutMs) const;
    bool IsTcpPortOpen(const std::wstring& host, uint16_t port, uint32_t timeoutMs) const;
    bool IsHostReachable(const std::wstring& host, uint32_t timeoutMs) const;
};
