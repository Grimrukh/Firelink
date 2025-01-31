#pragma once

#include <string>

#include "Export.h"

namespace Firelink
{
    enum LogLevel
    {
        DEBUG,
        INFO,
        WARNING,
        ERR,
        NONE,
    };

    // While file logging is set, output will go to `filePath` instead of `std::cout` or `std::cerr`.
    FIRELINK_API void SetLogFile(const std::wstring& filePath);
    FIRELINK_API void ClearLogFile();

    FIRELINK_API void SetLogLevel(const LogLevel& level);

    FIRELINK_API void Debug(const std::string& message);
    FIRELINK_API void Debug(const std::wstring& message);

    FIRELINK_API void Info(const std::string& message);
    FIRELINK_API void Info(const std::wstring& message);

    FIRELINK_API void Warning(const std::string& message);
    FIRELINK_API void Warning(const std::wstring& message);

    FIRELINK_API void Error(const std::string& message);
    FIRELINK_API void Error(const std::wstring& message);

    // These call `GetLastError()` and fill it in automatically.
    FIRELINK_API void WinError(const std::string& message);
    FIRELINK_API void WinError(const std::wstring& message);
}
