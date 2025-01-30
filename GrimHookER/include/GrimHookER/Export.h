#pragma once

#ifdef _WIN32
    #ifdef GRIMHOOKER_EXPORTS
        #define GRIMHOOKER_API __declspec(dllexport)
    #else
        #define GRIMHOOKER_API __declspec(dllimport)
    #endif
#else
    #define GRIMHOOKER_API
#endif