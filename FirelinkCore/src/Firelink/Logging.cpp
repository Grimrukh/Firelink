#include "Firelink/Logging.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

#include "Firelink/MemoryUtils.h"

using namespace std;

namespace
{
    filesystem::path LOG_FILE_PATH;
    ofstream LOG_FILE;
    mutex LOG_MUTEX;

    auto LOG_LEVEL = Firelink::LogLevel::INFO;

    // Helper function to get the current date and time as a wstring
    string GetCurrentDateTime()
    {
        // Get current time
        const auto now = chrono::system_clock::now();
        const auto inTimeT = chrono::system_clock::to_time_t(now);

        // Create a tm structure to store local time
        tm timeInfo{};

        // Use localtime_s instead of localtime to avoid C4996 error
        if (
            const auto result = localtime_s(&timeInfo, &inTimeT);
            result != 0)
        {
            cerr << "Error getting current logging time: " << result << '\n';
            return "";
        }

        // Format the time into a string
        stringstream ss;
        ss << put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // Log a message with the given level.
    void Log(const string& level, const string& message, const bool toStdErr = false)
    {
        lock_guard lock(LOG_MUTEX);

        const string logEntry = format("[{}] {} {}", GetCurrentDateTime(), level, message);

        if (LOG_FILE.is_open())
        {
            LOG_FILE << logEntry << '\n';
            LOG_FILE.flush();
            // cout << "LOGGED TO FILE: " << logEntry << '\n';
        }
        else
        {
            (toStdErr ? cerr : cout) << logEntry << '\n';
        }
    }
}


// Enable logging to a file
void Firelink::SetLogFile(const filesystem::path& filePath)
{
    unique_lock lock(LOG_MUTEX);

    if (LOG_FILE.is_open())
    {
        if (LOG_FILE_PATH == filePath)
            return;  // same file already set as log file
        // Close previous log file so we can open the new one below.
        LOG_FILE.close();
    }

    LOG_FILE_PATH = filePath;

	// Open the log file (new file or overwrite existing)
    LOG_FILE.open(LOG_FILE_PATH.c_str(), ios::out);
    if (!LOG_FILE.is_open())
    {
        cerr << format("Failed to open log file path: {}", LOG_FILE_PATH.string()) << '\n';
    }

    // Release mutex.
    lock.unlock();
    Debug(format("Log file opened: {}", LOG_FILE_PATH.string()));
}

// Disable logging to a file
void Firelink::ClearLogFile()
{
    lock_guard lock(LOG_MUTEX);
    if (LOG_FILE.is_open())
    {
        Info(format("Log file closing: {}", LOG_FILE_PATH.string()));
        LOG_FILE.close();
        Info("Log file closed.");
    }
}


void Firelink::SetLogLevel(const LogLevel& level)
{
    LOG_LEVEL = level;
}

void Firelink::Debug(const string& message)
{
    if (LOG_LEVEL > LogLevel::DEBUG)
        return;
    Log("  [DEBUG] ", message);
}

void Firelink::Info(const string& message)
{
    if (LOG_LEVEL > LogLevel::INFO)
        return;
    Log("   [INFO] ", message);
}

void Firelink::Warning(const string& message)
{
    if (LOG_LEVEL > LogLevel::WARNING)
        return;
    Log("[WARNING] ", message);
}

void Firelink::Error(const string& message)
{
    if (LOG_LEVEL > LogLevel::ERR)
        return;
    Log("  [ERROR] ", message, true);
}

void Firelink::WinError(const string& message)
{
    if (LOG_LEVEL > LogLevel::ERR)
        return;
    const DWORD errorCode = GetLastError();
    LPSTR errorBuffer = nullptr;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&errorBuffer), 0, nullptr);
    const string errorMessage = format("{} (WinError {}): {}", message, to_string(errorCode), errorBuffer);
    LocalFree(errorBuffer);
    Error(errorMessage);
}
