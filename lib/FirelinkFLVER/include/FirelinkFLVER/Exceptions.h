// flver/common.hpp
//
// Top-level compile-time configuration for the flver_cpp library.
// Included (directly or transitively) by every other public header.

#pragma once

#include <stdexcept>
#include <string>

namespace Firelink
{
    class FLVERError : public std::runtime_error
    {
    public:
        explicit FLVERError(const std::string& msg) : std::runtime_error(msg)
        {
        }

        explicit FLVERError(const char* msg) : std::runtime_error(msg)
        {
        }
    };

    class FLVERReadError : public FLVERError
    {
    public:
        explicit FLVERReadError(const std::string& msg) : FLVERError(msg)
        {
        }
    };
} // namespace Firelink
