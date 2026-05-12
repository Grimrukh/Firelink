#pragma once

#include <string>

namespace Firelink
{
    /// @brief Decode a FromSoft string to UTF-8 using Shift-JIS or UTF-16.
    std::string DecodeString(const char* raw, size_t byteLen, bool isUTF16);
    std::string DecodeString(const std::string& raw, bool isUTF16);

    /// @brief Encode a FromSoft string as Shift-JIS or UTF-16.
    std::string EncodeString(const std::string& utf8, bool isUTF16);
}
