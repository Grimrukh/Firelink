#pragma once

#include <Firelink/Export.h>

#include <filesystem>
#include <string>

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
    FIRELINK_CORE_API void SetLogFile(const std::filesystem::path& filePath);
    FIRELINK_CORE_API void ClearLogFile();

    FIRELINK_CORE_API void SetLogLevel(const LogLevel& level);

    FIRELINK_CORE_API void Debug(const std::string& message);
    FIRELINK_CORE_API void Info(const std::string& message);
    FIRELINK_CORE_API void Warning(const std::string& message);
    FIRELINK_CORE_API void Error(const std::string& message);
    // Calls `GetLastError()` and appends it automatically.
    FIRELINK_CORE_API void WinError(const std::string& message);
} // namespace Firelink
