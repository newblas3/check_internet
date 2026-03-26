#include "http_client.h"

std::optional<HttpResponse> HttpClient::Get(const std::wstring& url, uint32_t timeoutMs) const {
    return Request(L"GET", url, L"", timeoutMs);
}

std::optional<HttpResponse> HttpClient::Post(const std::wstring& url, const std::wstring& body, uint32_t timeoutMs) const {
    return Request(L"POST", url, body, timeoutMs);
}

std::optional<HttpResponse> HttpClient::Request(const std::wstring& method,
                                                const std::wstring& url,
                                                const std::wstring& body,
                                                uint32_t timeoutMs) const {
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    std::wstring mutableUrl = url;
    if (!WinHttpCrackUrl(mutableUrl.data(), 0, 0, &components)) {
        return std::nullopt;
    }

    const std::wstring host(components.lpszHostName, components.dwHostNameLength);
    const INTERNET_PORT port = components.nPort;
    const std::wstring path = std::wstring(components.lpszUrlPath, components.dwUrlPathLength) +
                              std::wstring(components.lpszExtraInfo, components.dwExtraInfoLength);

    HINTERNET session = WinHttpOpen(L"EdgeNetworkGuard/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return std::nullopt;
    }
    WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        return std::nullopt;
    }

    DWORD flags = 0;
    if (components.nScheme == INTERNET_SCHEME_HTTPS) {
        flags |= WINHTTP_FLAG_SECURE;
    }

    HINTERNET request = WinHttpOpenRequest(connect, method.c_str(), path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return std::nullopt;
    }

    const std::string bodyUtf8 = ToUtf8(body);
    const void* bodyPointer = bodyUtf8.empty() ? WINHTTP_NO_REQUEST_DATA : bodyUtf8.data();
    const BOOL sent = WinHttpSendRequest(request,
                                         WINHTTP_NO_ADDITIONAL_HEADERS,
                                         0,
                                         const_cast<void*>(bodyPointer),
                                         static_cast<DWORD>(bodyUtf8.size()),
                                         static_cast<DWORD>(bodyUtf8.size()),
                                         0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return std::nullopt;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    std::string responseBody;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(request, &bytesAvailable) && bytesAvailable > 0) {
        std::string chunk(bytesAvailable, '\0');
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request, chunk.data(), bytesAvailable, &bytesRead) || bytesRead == 0) {
            break;
        }
        chunk.resize(bytesRead);
        responseBody += chunk;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    HttpResponse response;
    response.statusCode = statusCode;
    response.body = ToWide(responseBody);
    return response;
}
