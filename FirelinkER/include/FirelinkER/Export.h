#pragma once

#ifdef _WIN32
    #ifdef FIRELINKER_EXPORTS
        #define FIRELINKER_API __declspec(dllexport)
    #else
        #define FIRELINKER_API __declspec(dllimport)
    #endif
#else
    #define FIRELINKER_API
#endif