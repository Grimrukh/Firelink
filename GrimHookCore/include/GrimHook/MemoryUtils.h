#pragma once

#include <string>
#include <vector>
#include <windows.h>

#include "Export.h"

namespace GrimHook
{
    /// @brief Concept that constrains memory read/write methods to types that are trivially copyable.
    template <typename T>
    concept MemReadWriteType = std::is_trivially_copyable_v<T>;

    /// @brief Convert a UTF-16 string to a standard UTF-8 string without warning about data loss.
    GRIMHOOK_API std::string UTF16ToUTF8(const std::u16string& utf16);

    /// @brief Get a pointer offset by a number of bytes.
    GRIMHOOK_API const void* GetOffsetPointer(const void* ptr, int offset);

    /// @brief Convert a string pattern, e.g. '45 2c ? 9a', into a pattern and wildcard mask.
    ///
    /// Returns true if pattern is valid. `pattern` and `wildcardMask` are output parameters.
    GRIMHOOK_API bool ParsePatternString(const std::string& patternString, std::vector<BYTE>& pattern, std::vector<bool>& wildcardMask);
}
