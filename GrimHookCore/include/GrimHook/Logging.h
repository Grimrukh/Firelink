#pragma once

#include <string>
#include <windows.h>

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
    __declspec(dllexport) void SetLogFile(const std::wstring& filePath);
    __declspec(dllexport) void ClearLogFile();

    __declspec(dllexport) void SetLogLevel(const LogLevel& level);

    __declspec(dllexport) void Debug(const std::string& message);
    __declspec(dllexport) void Debug(const std::wstring& message);
    __declspec(dllexport) void DebugPtr(const std::wstring& message, const LPCVOID& address);

    __declspec(dllexport) void Info(const std::string& message);
    __declspec(dllexport) void Info(const std::wstring& message);
    __declspec(dllexport) void InfoPtr(const std::wstring& message, const LPCVOID& address);

    __declspec(dllexport) void Warning(const std::string& message);
    __declspec(dllexport) void Warning(const std::wstring& message);
    __declspec(dllexport) void WarningPtr(const std::wstring& message, const LPCVOID& address);

    __declspec(dllexport) void Error(const std::string& message);
    __declspec(dllexport) void Error(const std::wstring& message);
    __declspec(dllexport) void ErrorPtr(const std::wstring& message, const LPCVOID& address);

    // These call `GetLastError()` and fill it in automatically.
    __declspec(dllexport) void WinError(const std::string& message);
    __declspec(dllexport) void WinError(const std::wstring& message);
    __declspec(dllexport) void WinErrorPtr(const std::wstring& message, const LPCVOID& address);
}
