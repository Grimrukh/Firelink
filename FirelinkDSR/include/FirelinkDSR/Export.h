#pragma once

#ifdef _WIN32
    #ifdef FIRELINKDSR_EXPORTS
        #define FIRELINKDSR_API __declspec(dllexport)
    #else
        #define FIRELINKDSR_API __declspec(dllimport)
    #endif
#else
    #define FIRELINKDSR_API
#endif