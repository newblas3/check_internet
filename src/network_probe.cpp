#include "network_probe.h"

bool NetworkProbe::PingHost(const std::wstring& host, uint32_t timeoutMs) const {
    addrinfoW hints{};
    hints.ai_family = AF_INET;

    addrinfoW* result = nullptr;
    if (GetAddrInfoW(host.c_str(), nullptr, &hints, &result) != 0 || !result) {
        return false;
    }

    sockaddr_in addr = *reinterpret_cast<sockaddr_in*>(result->ai_addr);
    FreeAddrInfoW(result);

    HANDLE handle = IcmpCreateFile();
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    char sendData[] = "edge-network-guard";
    const DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8;
    std::vector<unsigned char> replyBuffer(replySize);
    const DWORD ret = IcmpSendEcho(handle,
                                   addr.sin_addr.S_un.S_addr,
                                   sendData,
                                   static_cast<WORD>(sizeof(sendData)),
                                   nullptr,
                                   replyBuffer.data(),
                                   replySize,
                                   timeoutMs);
    IcmpCloseHandle(handle);
    return ret > 0;
}

bool NetworkProbe::IsTcpPortOpen(const std::wstring& host, uint16_t port, uint32_t timeoutMs) const {
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    addrinfoW hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfoW* result = nullptr;
    const std::wstring service = std::to_wstring(port);
    if (GetAddrInfoW(host.c_str(), service.c_str(), &hints, &result) != 0 || !result) {
        WSACleanup();
        return false;
    }

    SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        FreeAddrInfoW(result);
        WSACleanup();
        return false;
    }

    u_long nonBlocking = 1;
    ioctlsocket(sock, FIONBIO, &nonBlocking);
    connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen));

    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(sock, &writeSet);

    TIMEVAL timeout{};
    timeout.tv_sec = static_cast<LONG>(timeoutMs / 1000);
    timeout.tv_usec = static_cast<LONG>((timeoutMs % 1000) * 1000);

    const int selectResult = select(0, nullptr, &writeSet, nullptr, &timeout);
    bool isOpen = false;
    if (selectResult > 0 && FD_ISSET(sock, &writeSet)) {
        int error = 0;
        int errorLength = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &errorLength) == 0 && error == 0) {
            isOpen = true;
        }
    }

    closesocket(sock);
    FreeAddrInfoW(result);
    WSACleanup();
    return isOpen;
}

bool NetworkProbe::IsHostReachable(const std::wstring& host, uint32_t timeoutMs) const {
    if (PingHost(host, timeoutMs)) {
        return true;
    }

    static constexpr uint16_t kFallbackPorts[] = {53, 443};
    for (const auto port : kFallbackPorts) {
        if (IsTcpPortOpen(host, port, timeoutMs)) {
            return true;
        }
    }

    return false;
}
