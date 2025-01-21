#include <format>
#include <string>

#include "GrimHook/Logging.h"
#include "GrimHookDSR/DSRHook.h"


int main(int argc, char* argv[])
{
    const std::wstring msg = std::format(L"Logging Test. DSR Process Name: {}", GrimHookDSR::DSR_PROCESS_NAME);
    GrimHook::Info(msg);
    return 0;
}
