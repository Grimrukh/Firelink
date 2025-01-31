#pragma once

#ifdef _WIN32
    #ifdef FIRELINKCORE_EXPORTS
        #define FIRELINK_API __declspec(dllexport)
    #else
        #define FIRELINK_API __declspec(dllimport)
    #endif
#else
    #define FIRELINK_API
#endif