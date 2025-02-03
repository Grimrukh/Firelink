#pragma once

#include <filesystem>
#include <string>

#include "Export.h"

namespace Firelink
{
    enum class LogLevel
    {
        DEBUG,
        INFO,
        WARNING,
        ERR,
        NONE,
    };

    // While file logging is set, output will go to `filePath` instead of stdout or stderr.
    FIRELINK_API void SetLogFile(const std::filesystem::path& filePath);
    FIRELINK_API void ClearLogFile();

    FIRELINK_API void SetLogLevel(const LogLevel& level);

    FIRELINK_API void Debug(const std::string& message);
    FIRELINK_API void Info(const std::string& message);
    FIRELINK_API void Warning(const std::string& message);
    FIRELINK_API void Error(const std::string& message);
    // Calls `GetLastError()` and appends it automatically.
    FIRELINK_API void WinError(const std::string& message);
}
