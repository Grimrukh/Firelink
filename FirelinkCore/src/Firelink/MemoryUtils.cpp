#include <Firelink/Logging.h>
#include <Firelink/MemoryUtils.h>

#include <sstream>

std::string Firelink::UTF16ToUTF8(const std::u16string& utf16)
{
    std::string str;
    str.reserve(utf16.size());
    for (const char16_t c : utf16)
    {
        if (c < 0x80)
        {
            str.push_back(static_cast<char>(c));
        }
        else if (c < 0x800)
        {
            str.push_back(static_cast<char>(0xC0 | (c >> 6)));
            str.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
        else
        {
            str.push_back(static_cast<char>(0xE0 | (c >> 12)));
            str.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            str.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return str;
}

std::string Firelink::UTF16ToUTF8(const std::wstring& utf16)
{
    return UTF16ToUTF8(std::u16string(utf16.begin(), utf16.end()));
}

const void* Firelink::GetOffsetPointer(const void* ptr, const int offset)
{
    return static_cast<const char*>(ptr) + offset;
}

bool Firelink::ParsePatternString(const std::string& patternString, std::vector<BYTE>& pattern, std::vector<bool>& wildcardMask)
{
    std::istringstream stream(patternString);
    std::string token;

    // Clear output std::vectors
    pattern.clear();
    wildcardMask.clear();

    while (stream >> token)
    {
        if (token == "?")
        {
            // Wildcard detected
            pattern.push_back(0x00); // Placeholder value (doesn't matter, as mask will ignore it)
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
                Error("Invalid token in pattern std::string: " + token);
                return false; // Parsing error
            }
        }
    }

    // Ensure both std::vectors have the same size, and return true for successful parsing
    return pattern.size() == wildcardMask.size();
}