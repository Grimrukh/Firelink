#pragma once

#include <FirelinkCore/Export.h>

#include <cstddef>
#include <vector>

#ifdef _WIN32
#include <dxgiformat.h>    // DXGI_FORMAT enum
#endif

namespace Firelink
{
    // --- DDS -> image -----------------------------------------------------------

    [[nodiscard]] FIRELINK_CORE_API std::vector<std::byte> ConvertDDSToTGA(const std::byte* data, size_t size);
    [[nodiscard]] FIRELINK_CORE_API std::vector<std::byte> ConvertDDSToPNG(const std::byte* data, size_t size);

    // --- Image -> DDS -----------------------------------------------------------
    // Convert a TGA or PNG image to DDS with the given DXGI pixel format.
    // Block-compressed formats (BC1–BC7) are supported; the source pixels are
    // automatically compressed.  Use TEX_COMPRESS_PARALLEL internally for speed.

    [[nodiscard]] FIRELINK_CORE_API std::vector<std::byte> ConvertTGAToDDS(const std::byte* data, size_t size, DXGI_FORMAT targetFormat);
    [[nodiscard]] FIRELINK_CORE_API std::vector<std::byte> ConvertPNGToDDS(const std::byte* data, size_t size, DXGI_FORMAT targetFormat);
}