
#include "pch.h"

// Entry point for the DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{

    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            // DLL is being loaded into the virtual address space of the current process
            DisableThreadLibraryCalls(hModule);
            break;
        case DLL_PROCESS_DETACH:
            // DLL is being unloaded
            break;
        default:
            break;
    }
    return TRUE;

}
