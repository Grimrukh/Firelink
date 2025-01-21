#pragma once

#include <string>

#include "Export.h"

namespace GrimHook
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
    GRIMHOOK_API void SetLogFile(const std::wstring& filePath);
    GRIMHOOK_API void ClearLogFile();

    GRIMHOOK_API void SetLogLevel(const LogLevel& level);

    GRIMHOOK_API void Debug(const std::string& message);
    GRIMHOOK_API void Debug(const std::wstring& message);

    GRIMHOOK_API void Info(const std::string& message);
    GRIMHOOK_API void Info(const std::wstring& message);

    GRIMHOOK_API void Warning(const std::string& message);
    GRIMHOOK_API void Warning(const std::wstring& message);

    GRIMHOOK_API void Error(const std::string& message);
    GRIMHOOK_API void Error(const std::wstring& message);

    // These call `GetLastError()` and fill it in automatically.
    GRIMHOOK_API void WinError(const std::string& message);
    GRIMHOOK_API void WinError(const std::wstring& message);
}
