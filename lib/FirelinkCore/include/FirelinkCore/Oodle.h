// Oodle (Kraken) compression/decompression via dynamic loading of oo2core_6_win64.dll.
//
// The DLL is searched for in:
//   1. A path set at runtime via SetDLLSearchPath() or LoadDLL()
//   2. C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game/
//
// If not found, Compress/Decompress will throw DCXError when called.

#pragma once

#include <FirelinkCore/Export.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Firelink::Oodle
{
    /// @brief Set a directory to search for the DLL *before* the default paths.
    ///        Must be called before the first Compress/Decompress call to take effect.
    FIRELINK_CORE_API void SetDLLSearchPath(const std::string& directory);

    /// @brief Explicitly load the DLL from a specific directory. Returns true on success.
    ///        Can be called at any time; replaces a previously loaded DLL.
    FIRELINK_CORE_API bool LoadDLL(const std::string& directory);

    /// @brief Returns true if the Oodle DLL was loaded successfully.
    [[nodiscard]] FIRELINK_CORE_API bool IsAvailable() noexcept;

    /// @brief Returns the path the DLL was loaded from, or empty if not loaded.
    [[nodiscard]] FIRELINK_CORE_API const std::string& LoadedPath() noexcept;

    /// @brief Decompress Oodle-compressed data. Throws DCXError if DLL not available.
    [[nodiscard]] FIRELINK_CORE_API std::vector<std::byte> Decompress(
        const std::byte* compressed, std::size_t compressed_size,
        std::size_t decompressed_size);

    /// @brief Compress data with Oodle Kraken. Throws DCXError if DLL not available.
    [[nodiscard]] FIRELINK_CORE_API std::vector<std::byte> Compress(
        const std::byte* data, std::size_t size);

} // namespace Firelink::Oodle

