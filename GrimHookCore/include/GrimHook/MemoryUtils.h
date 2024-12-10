#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace GrimHook
{
    // Get a pointer offset by a number of bytes.
    LPCVOID GetOffsetPointer(LPCVOID ptr, SIZE_T offset);

    // Format a pointer as a hexadecimal string or '<NULL>'.
    std::string ToHexString(LPCVOID ptr);

    // Format a pointer as a wide hexadecimal string or '<NULL>'.
    std::wstring ToHexWstring(LPCVOID ptr);

    // Convert a string pattern, e.g. '45 2c ? 9a', into a pattern and wildcard mask.
    bool ParsePatternString(const std::wstring& patternString, std::vector<BYTE>& pattern, std::vector<bool>& wildcardMask);
}
