#pragma once

#ifdef _WIN32
    #ifdef GRIMHOOKCORE_EXPORTS
        #define GRIMHOOK_API __declspec(dllexport)
    #else
        #define GRIMHOOK_API __declspec(dllimport)
    #endif
#else
    #define GRIMHOOK_API
#endif