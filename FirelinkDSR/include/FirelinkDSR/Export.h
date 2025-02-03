#pragma once

#ifdef _WIN32
    #ifdef FIRELINK_DSR_EXPORTS
        #define FIRELINK_DSR_API __declspec(dllexport)
    #else
        #define FIRELINK_DSR_API __declspec(dllimport)
    #endif
#else
    #define FIRELINK_DSR_API
#endif