#include "GrimHook/MemoryUtils.h"

#include <sstream>
#include <iomanip>

#include "GrimHook/Logging.h"

using namespace std;

// Get a pointer offset by a number of bytes
LPCVOID GrimHook::GetOffsetPointer(const LPCVOID ptr, const SIZE_T offset)
{
    return static_cast<const char*>(ptr) + offset;
}

// Format a pointer as a hexadecimal string or '<NULL>'
string GrimHook::ToHexString(LPCVOID ptr)
{
    if (ptr == nullptr)
    {
        return "<NULL>";
    }
    ostringstream oss;
    oss << "0x"
        << hex << uppercase << setw(sizeof(void*) * 2)
        << setfill('0')
        << reinterpret_cast<uintptr_t>(ptr);
    return oss.str();
}

// Format a pointer as a wide hexadecimal string or '<NULL>'
wstring GrimHook::ToHexWstring(LPCVOID ptr)
{
    if (ptr == nullptr)
    {
        return L"<NULL>";
    }
    wostringstream oss;
    oss << L"0x"
        << hex << uppercase << setw(sizeof(void*) * 2)
        << setfill(L'0')
        << reinterpret_cast<uintptr_t>(ptr);
    return oss.str();
}

// Parses a pattern string like "45 2c ? 9A" into pattern and wildcard mask arrays.
// Returns true if pattern is valid. `pattern` and `wildcardMask` are output parameters.
bool GrimHook::ParsePatternString(const wstring& patternString, vector<BYTE>& pattern, vector<bool>& wildcardMask)
{
    wistringstream stream(patternString);
    wstring token;

    // Clear output vectors
    pattern.clear();
    wildcardMask.clear();

    while (stream >> token)
    {
        if (token == L"?")
        {
            // Wildcard detected
            pattern.push_back(0x00);  // Placeholder value (doesn't matter, as mask will ignore it)
            wildcardMask.push_back(true);
        }
        else
        {
            // Parse hex byte
            try
            {
                BYTE byte = static_cast<BYTE>(stoul(token, nullptr, 16));
                pattern.push_back(byte);
                wildcardMask.push_back(false);
            }
            catch (...)
            {
                Error(L"Invalid token in pattern string: " + token);
                return false;  // Parsing error
            }
        }
    }

    // Ensure both vectors have the same size, and return true for successful parsing
    return pattern.size() == wildcardMask.size();
}