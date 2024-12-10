#include "GrimHook/Logging.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

#include "GrimHook/MemoryUtils.h"

using namespace std;

namespace
{
    wstring LOG_FILE_PATH;
    wofstream LOG_FILE;
    once_flag LOG_MUTEX_INIT_FLAG;
    mutex LOG_MUTEX{};

    GrimHook::LogLevel LOG_LEVEL = GrimHook::LogLevel::INFO;

    // Helper function to get the current date and time as a wstring
    wstring GetCurrentDateTimeWide()
    {
        // Get current time
        const auto now = chrono::system_clock::now();
        const auto inTimeT = chrono::system_clock::to_time_t(now);

        // Create a tm structure to store local time
        tm timeInfo{};

        // Use localtime_s instead of localtime to avoid C4996 error
        const auto result = localtime_s(&timeInfo, &inTimeT);
        if (result != 0)
        {
            wcerr << L"Error getting current logging time: " << result << '\n';
            return L"";
        }

        // Format the time into a string
        wstringstream ss;
        ss << put_time(&timeInfo, L"%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // Ensure logging mutex is initialized
    void InitializeLogging() {
        std::call_once(LOG_MUTEX_INIT_FLAG, [] {
            LOG_MUTEX.lock();
            LOG_MUTEX.unlock();
        });
    }

    // Log a message with the given level.
    void Log(const wstring& level, const wstring& message, const bool toStdErr = false)
    {
        InitializeLogging();
        lock_guard<mutex> lock(LOG_MUTEX);

        const wstring logEntry = L"[" + GetCurrentDateTimeWide() + L"] " + level + message;

        if (LOG_FILE.is_open())
        {
            LOG_FILE << logEntry << '\n';
            LOG_FILE.flush();
        }
        else
        {
            (toStdErr ? wcerr : wcout) << logEntry << '\n';
        }
    }
}


// Enable logging to a file
void GrimHook::SetLogFile(const wstring& filePath)
{
    InitializeLogging();
    lock_guard<mutex> lock(LOG_MUTEX);

    if (LOG_FILE.is_open())
    {
        if (LOG_FILE_PATH == filePath) return;  // same file already enabled
        // Close previous log file so we can open the new one below.
        LOG_FILE.close();
    }

    LOG_FILE_PATH = filePath;

	// Open the log file (new file or overwrite existing)
    LOG_FILE.open(LOG_FILE_PATH.c_str(), ios::out);
    if (!LOG_FILE.is_open())
    {
        wcerr << L"Failed to open log file: " << LOG_FILE_PATH << '\n';
    }
    // NOTE: We CANNOT call a logging function here because it would cause a mutex deadlock.
}

// Disable logging to a file
void GrimHook::ClearLogFile()
{
    lock_guard<mutex> lock(LOG_MUTEX);
    if (LOG_FILE.is_open())
    {
        Info(L"Log file closing: " + LOG_FILE_PATH);
        LOG_FILE.close();
        Info(L"Log file closed.");
    }
}


void GrimHook::SetLogLevel(const LogLevel& level)
{
    LOG_LEVEL = level;
}

// Debug log
void GrimHook::Debug(const wstring& message)
{
    if (LOG_LEVEL > DEBUG) return;
    Log(L"  [DEBUG] ", message);
}
void GrimHook::Debug(const string& message)
{
    return Debug(wstring(message.begin(), message.end()));
}
void GrimHook::DebugPtr(const wstring& message, const LPCVOID& address)
{
    Debug(message + L" " + GrimHook::ToHexWstring(address));
}

// Info log
void GrimHook::Info(const wstring& message)
{
    if (LOG_LEVEL > INFO) return;
    Log(L"   [INFO] ", message);
}
void GrimHook::Info(const string& message)
{
    return Info(wstring(message.begin(), message.end()));
}
void GrimHook::InfoPtr(const wstring& message, const LPCVOID& address)
{
    Info(message + L" " + GrimHook::ToHexWstring(address));
}

// Warning log
void GrimHook::Warning(const wstring& message)
{
    if (LOG_LEVEL > WARNING) return;
    Log(L"[WARNING] ", message);
}
void GrimHook::Warning(const string& message)
{
    return Warning(wstring(message.begin(), message.end()));
}
void GrimHook::WarningPtr(const wstring& message, const LPCVOID& address)
{
    Warning(message + L" " + GrimHook::ToHexWstring(address));
}

// Error log
void GrimHook::Error(const wstring& message)
{
    if (LOG_LEVEL > ERR) return;
    Log(L"  [ERROR] ", message, true);
}
void GrimHook::Error(const string& message)
{
    return Error(wstring(message.begin(), message.end()));
}
void GrimHook::ErrorPtr(const wstring& message, const LPCVOID& address)
{
    Error(message + L" " + ToHexWstring(address));
}

// Windows error log
void GrimHook::WinError(const wstring& message)
{
    if (LOG_LEVEL > ERR) return;
    const DWORD errorCode = GetLastError();
    LPWSTR errorBuffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&errorBuffer), 0, nullptr);
    const wstring errorMessage = message + L" (WinError " + to_wstring(errorCode) + L"): " + errorBuffer;
    LocalFree(errorBuffer);
    Error(errorMessage);
}
void GrimHook::WinError(const string& message)
{
    return WinError(wstring(message.begin(), message.end()));
}
void GrimHook::WinErrorPtr(const wstring& message, const LPCVOID& address)
{
    WinError(message + L" " + ToHexWstring(address));
}
