#include <format>
#include <string>

#include "Firelink/Logging.h"
#include "FirelinkDSR/DSRHook.h"


/// @brief Tests C++20 (format), FirelinkCore logging, and FirelinkDSR process name.
int main()
{
    Firelink::Info("Firelink logging success.");
    const std::wstring dsrMsg = std::format(L"DSR Process Name: '{}'", FirelinkDSR::DSR_PROCESS_NAME);
    Firelink::Info(dsrMsg);
    return 0;
}
