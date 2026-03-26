#pragma once

#include "common.h"

class Logger {
public:
    explicit Logger(std::filesystem::path path);

    void Info(const std::wstring& message);
    void Error(const std::wstring& message);

private:
    void Write(const std::wstring& level, const std::wstring& message);

    std::filesystem::path path_;
    std::mutex mutex_;
};
