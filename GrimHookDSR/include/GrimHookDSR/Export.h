#pragma once

#ifdef _WIN32
    #ifdef GRIMHOOKDSR_EXPORTS
        #define GRIMHOOKDSR_API __declspec(dllexport)
    #else
        #define GRIMHOOKDSR_API __declspec(dllimport)
    #endif
#else
    #define GRIMHOOKDSR_API
#endif