#include <format>
#include <string>

#include "GrimHook/Logging.h"
#include "GrimHookDSR/DSRHook.h"


/// @brief Tests C++20 (format), GrimHookCore logging, and GrimHookDSR process name.
int main()
{
    GrimHook::Info("GrimHook logging success.");
    const std::wstring dsrMsg = std::format(L"DSR Process Name: '{}'", GrimHookDSR::DSR_PROCESS_NAME);
    GrimHook::Info(dsrMsg);
    return 0;
}
