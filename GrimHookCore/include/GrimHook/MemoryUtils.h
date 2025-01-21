#pragma once

#include <string>
#include <vector>
#include <windows.h>

#include "Export.h"

namespace GrimHook
{
    /// @brief Get a pointer offset by a number of bytes.
    GRIMHOOK_API const void* GetOffsetPointer(const void* ptr, int offset);

    /// @brief Convert a string pattern, e.g. '45 2c ? 9a', into a pattern and wildcard mask.
    ///
    /// Returns true if pattern is valid. `pattern` and `wildcardMask` are output parameters.
    GRIMHOOK_API bool ParsePatternString(const std::string& patternString, std::vector<BYTE>& pattern, std::vector<bool>& wildcardMask);
}
