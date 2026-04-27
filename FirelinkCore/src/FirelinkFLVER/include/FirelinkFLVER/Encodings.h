#pragma once

#include <string>

namespace Firelink
{
    /// @brief Decode a FLVER string (encoding varies with FLVER version).
    std::string DecodeFLVERString(const char* raw, size_t byteLen, bool isUTF16);
    std::string DecodeFLVERString(const std::string& raw, bool isUTF16);

    /// @brief Encode a FLVER string (encoding varies with FLVER version).
    std::string EncodeFLVERString(const std::string& utf8, bool isUTF16);
}
