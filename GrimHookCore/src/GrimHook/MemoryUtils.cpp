#include <sstream>
#include <iomanip>

#include "GrimHook/Logging.h"
#include "GrimHook/MemoryUtils.h"

using namespace std;

const void* GrimHook::GetOffsetPointer(const void* ptr, const int offset)
{
    return static_cast<const char*>(ptr) + offset;
}


bool GrimHook::ParsePatternString(const string& patternString, vector<BYTE>& pattern, vector<bool>& wildcardMask)
{
    istringstream stream(patternString);
    string token;

    // Clear output vectors
    pattern.clear();
    wildcardMask.clear();

    while (stream >> token)
    {
        if (token == "?")
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
                Error("Invalid token in pattern string: " + token);
                return false;  // Parsing error
            }
        }
    }

    // Ensure both vectors have the same size, and return true for successful parsing
    return pattern.size() == wildcardMask.size();
}