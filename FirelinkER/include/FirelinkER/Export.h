#pragma once

#ifdef _WIN32
    #ifdef FIRELINK_ER_EXPORTS
        #define FIRELINK_ER_API __declspec(dllexport)
    #else
        #define FIRELINK_ER_API __declspec(dllimport)
    #endif
#else
    #define FIRELINK_ER_API
#endif