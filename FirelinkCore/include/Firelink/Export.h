#pragma once

#ifdef _WIN32
    #ifdef FIRELINK_CORE_EXPORTS
        #define FIRELINK_API __declspec(dllexport)
    #else
        #define FIRELINK_API __declspec(dllimport)
    #endif
#else
    #define FIRELINK_API
#endif