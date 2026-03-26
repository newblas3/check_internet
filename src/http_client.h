#pragma once

#include "common.h"

struct HttpResponse {
    DWORD statusCode = 0;
    std::wstring body;
};

class HttpClient {
public:
    std::optional<HttpResponse> Get(const std::wstring& url, uint32_t timeoutMs) const;
    std::optional<HttpResponse> Post(const std::wstring& url, const std::wstring& body, uint32_t timeoutMs) const;

private:
    std::optional<HttpResponse> Request(const std::wstring& method,
                                        const std::wstring& url,
                                        const std::wstring& body,
                                        uint32_t timeoutMs) const;
};
