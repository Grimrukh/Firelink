#include <FirelinkFLVER/Encodings.h>

#include <stdexcept>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

std::string Firelink::DecodeFLVERString(const char* raw, const size_t byteLen, const bool isUTF16)
{
#ifndef WIN32
    // Can only decode strings on Windows currently. Return raw.
    return std::string(raw);
#else
    if (byteLen == 0) return {};

    std::wstring wide;

    if (isUTF16)
    {
        // Raw bytes are already UTF-16-LE — just reinterpret
        wide.assign(
            reinterpret_cast<const wchar_t*>(raw),
            byteLen / sizeof(wchar_t)
        );
    }
    else
    {
        // Shift-JIS (codepage 932) -> wide
        int wide_len = MultiByteToWideChar(932, 0, raw, static_cast<int>(byteLen), nullptr, 0);
        if (wide_len == 0)
            throw std::runtime_error("MultiByteToWideChar (Shift-JIS) size query failed");
        wide.resize(wide_len);
        MultiByteToWideChar(932, 0, raw, static_cast<int>(byteLen), wide.data(), wide_len);
    }

    // Wide (UTF-16-LE) -> UTF-8
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (utf8_len == 0)
        throw std::runtime_error("WideCharToMultiByte (UTF-8) size query failed");
    std::string utf8(utf8_len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), utf8_len, nullptr, nullptr);

    return utf8;
#endif
}

std::string Firelink::DecodeFLVERString(const std::string& raw, const bool isUTF16)
{
    return DecodeFLVERString(raw.data(), raw.size(), isUTF16);
}

std::string Firelink::EncodeFLVERString(const std::string& utf8, const bool isUTF16) {
#ifndef WIN32
    // Can only encode strings on Windows currently. Return UTF8 (should still be raw).
    return std::string(utf8);
#else
    if (utf8.empty()) return {};

    // UTF-8 → wide
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (wide_len == 0)
        throw std::runtime_error("MultiByteToWideChar (UTF-8) size query failed");
    std::wstring wide(wide_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), wide_len);

    if (isUTF16)
    {
        // Reinterpret wide string as raw bytes (already UTF-16-LE on Windows)
        return std::string(
            reinterpret_cast<const char*>(wide.data()),
            wide.size() * sizeof(wchar_t)
        );
    }

    // Wide → Shift-JIS (codepage 932)
    BOOL used_default = FALSE;
    int sjis_len = WideCharToMultiByte(932, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, &used_default);
    if (sjis_len == 0)
        throw std::runtime_error("WideCharToMultiByte (Shift-JIS) size query failed");
    std::string sjis(sjis_len, '\0');
    WideCharToMultiByte(932, 0, wide.data(), static_cast<int>(wide.size()), sjis.data(), sjis_len, nullptr, &used_default);

    if (used_default)
        throw std::runtime_error("String contains characters not representable in Shift-JIS");

    return sjis;
#endif
}
