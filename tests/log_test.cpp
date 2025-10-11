#include <Firelink/Logging.h>
#include <Firelink/MemoryUtils.h>
#include <FirelinkDSRHook/DSRHook.h>

#include <format>
#include <string>

/// @brief Tests C++20 (format), FirelinkCore logging, and FirelinkDSR process name.
int main()
{
    Firelink::Info("Firelink logging success.");
    const std::string dsrMsg =
        std::format("DSR Process Name: '{}'", Firelink::UTF16ToUTF8(FirelinkDSR::DSR_PROCESS_NAME));
    Firelink::Info(dsrMsg);
    return 0;
}
