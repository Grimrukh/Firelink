#include <FirelinkCore/Encodings.h>

#include <FirelinkCore/Logging.h>

#include <stdexcept>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

std::string Firelink::ToString(const FSEncoding encoding)
{
    switch (encoding)
    {
        case FSEncoding::UTF_16:
            return "UTF-16";
        case FSEncoding::SHIFT_JIS:
            return "Shift-JIS";
        default:
            return "<Unknown>";
    }
}

std::string Firelink::DecodeString(const char* encoded, const size_t byteLen, const FSEncoding encoding)
{
#ifndef WIN32
    // Can only decode strings on Windows currently. Return raw.
    return std::string(encoded);
#else
    if (byteLen == 0) return {};

    std::wstring wide;

    if (encoding == FSEncoding::UTF_16)
    {
        // Raw bytes are already UTF-16-LE wide (byte pairs) — just reinterpret.
        // Big-endian encoded strings are byte-swapped by reader.
        wide.assign(
            reinterpret_cast<const wchar_t*>(encoded),
            byteLen / sizeof(wchar_t)
        );
    }
    else
    {
        // All other strings are assumed to be multibyte and are converted to generic wide here.
        const std::uint32_t codePage = static_cast<std::uint32_t>(encoding);
        const int wideLen = MultiByteToWideChar(codePage, 0, encoded, static_cast<int>(byteLen), nullptr, 0);
        if (wideLen == 0)
            throw std::runtime_error(std::format("MultiByteToWideChar ({}) size query failed", ToString(encoding)));
        wide.resize(wideLen);
        MultiByteToWideChar(codePage, 0, encoded, static_cast<int>(byteLen), wide.data(), wideLen);
    }

    // Wide (UTF-16-LE) -> UTF-8
    const int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (utf8Len == 0)
        throw std::runtime_error("WideCharToMultiByte (UTF-8) size query failed");
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), utf8Len, nullptr, nullptr);

    return utf8;
#endif
}

std::string Firelink::DecodeString(const std::string& encoded, const FSEncoding encoding)
{
    return DecodeString(encoded.data(), encoded.size(), encoding);
}

std::string Firelink::EncodeString(const std::string& decoded, const FSEncoding encoding)
{
#ifndef WIN32
    // Can only encode strings on Windows currently. Return UTF8 (should still be raw).
    return std::string(decoded);
#else
    if (decoded.empty()) return {};

    // UTF-8 → wide
    const int wideLen = MultiByteToWideChar(CP_UTF8, 0, decoded.data(), static_cast<int>(decoded.size()), nullptr, 0);
    if (wideLen == 0)
        throw std::runtime_error("MultiByteToWideChar (UTF-8) size query failed");
    std::wstring wide(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, decoded.data(), static_cast<int>(decoded.size()), wide.data(), wideLen);

    if (encoding == FSEncoding::UTF_16)
    {
        // Reinterpret wide string as raw bytes (already UTF-16-LE on Windows).
        return std::string{reinterpret_cast<const char*>(wide.data()), wide.size() * sizeof(wchar_t)};
    }

    // Use multibyte code page.
    const std::uint32_t codePage = static_cast<std::uint32_t>(encoding);
    BOOL used_default = FALSE;
    const int multiByteLen = WideCharToMultiByte(codePage, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, &used_default);
    if (multiByteLen == 0)
        throw std::runtime_error(std::format("WideCharToMultiByte ({}) size query failed", ToString(encoding)));
    std::string encoded(multiByteLen, '\0');
    WideCharToMultiByte(codePage, 0, wide.data(), static_cast<int>(wide.size()), encoded.data(), multiByteLen, nullptr, &used_default);

    if (used_default)
        throw std::runtime_error("String contains characters not representable in given encoding");

    return encoded;
#endif
}
