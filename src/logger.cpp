#include "logger.h"

namespace {

std::wstring Timestamp() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::wostringstream stream;
    stream << std::setfill(L'0')
           << st.wYear << L'-'
           << std::setw(2) << st.wMonth << L'-'
           << std::setw(2) << st.wDay << L' '
           << std::setw(2) << st.wHour << L':'
           << std::setw(2) << st.wMinute << L':'
           << std::setw(2) << st.wSecond;
    return stream.str();
}

} // namespace

Logger::Logger(std::filesystem::path path) : path_(std::move(path)) {
}

void Logger::Info(const std::wstring& message) {
    Write(L"INFO", message);
}

void Logger::Error(const std::wstring& message) {
    Write(L"ERROR", message);
}

void Logger::Write(const std::wstring& level, const std::wstring& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::wofstream file(path_, std::ios::app);
    file.imbue(std::locale(file.getloc(), new std::codecvt_utf8_utf16<wchar_t>));
    if (!file) {
        return;
    }
    file << L'[' << Timestamp() << L"] [" << level << L"] " << message << L'\n';
}
