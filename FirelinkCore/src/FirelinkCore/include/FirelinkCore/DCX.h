// DCX compression / decompression for FromSoftware file formats.
//
// Supports all DCX variants. Oodle-based DCX_KRAK requires oo2core_6_win64.dll
// to be available at runtime (see Oodle.h for DLL search configuration).
//
// All header bytes in DCX/DCP files are big-endian.

#pragma once

#include <FirelinkCore/Endian.h>
#include <FirelinkCore/Export.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "BinaryReadWrite.h"

namespace Firelink
{
    namespace BinaryReadWrite
    {
        class BufferReader;
        class BufferWriter;
    }

    // --- DCXType ----------------------------------------------------------------

    enum class DCXType
    {
        Unknown = -1, // could not be detected
        Null = 0, // no compression
        Zlib = 1, // not really DCX but supported
        DCP_EDGE = 2, // DCP header, chunked deflate compression. Used in ACE:R TPFs.
        DCP_DFLT = 3, // DCP header, deflate compression. Used in DeS test maps.
        DCX_EDGE = 4, // DCX header, chunked deflate compression. Primarily used in DeS.
        DCX_DFLT_10000_24_9 = 5, // DCX header, deflate. Primarily used in DS1 and DS2.
        DCX_DFLT_10000_44_9 = 6, // DCX header, deflate. Primarily used in BB and DS3.
        DCX_DFLT_11000_44_8 = 7, // DCX header, deflate. DS3 save backup regulation.
        DCX_DFLT_11000_44_9 = 8, // DCX header, deflate. Used in Sekiro.
        DCX_DFLT_11000_44_9_15 = 9, // DCX header, deflate. Old ER regulation.
        DCX_KRAK = 10, // DCX header, Oodle compression. Used in Sekiro and Elden Ring.
        DCX_ZSTD = 11, // ZSTD compression. Used in new ER regulation.
    };

    // --- DCX-specific error ---

    class DCXError : public std::runtime_error
    {
    public:
        explicit DCXError(const std::string& msg) : std::runtime_error(msg)
        {
        }

        explicit DCXError(const char* msg) : std::runtime_error(msg)
        {
        }
    };

    // --- DCX result ---

    struct DCXResult
    {
        std::vector<std::byte> data; // decompressed payload
        DCXType type; // detected DCX variant
    };

    // --- Public API ---

    /// @brief Check if data appears to be DCX/DCP compressed (or raw zlib).
    [[nodiscard]] FIRELINK_CORE_API bool IsDCX(const std::byte* data, std::size_t size) noexcept;

    /// @brief Detect the DCXType from the header without decompressing.
    [[nodiscard]] FIRELINK_CORE_API DCXType DetectDCX(const std::byte* data, std::size_t size);

    /// @brief Decompress DCX/DCP/raw-zlib data.
    [[nodiscard]] FIRELINK_CORE_API DCXResult DecompressDCX(const std::byte* data, std::size_t size);

    /// @brief Compress data into the given DCX format.
    [[nodiscard]] FIRELINK_CORE_API std::vector<std::byte> CompressDCX(
        const std::byte* data, std::size_t size, DCXType type);

    /// @brief Decompress data if required and return a BufferReader and DCXType.
    /// @note  BufferReader will use `data` directly if already decompressed, or manage its own
    ///        decompressed storage if required. Caller must maintain `data` in the former case.
    [[nodiscard]] FIRELINK_CORE_API std::pair<BinaryReadWrite::BufferReader, DCXType> GetBufferReaderForDCX(
        const std::byte* data, size_t size, BinaryReadWrite::Endian endian = BinaryReadWrite::Endian::Little);

    /// @brief Decompress data if required and return a BufferReader and DCXType.
    /// @note  Takes ownership of storage. Will use new storage if decompression is needed.
    [[nodiscard]] FIRELINK_CORE_API std::pair<BinaryReadWrite::BufferReader, DCXType> GetBufferReaderForDCX(
        std::vector<std::byte>&& storage, BinaryReadWrite::Endian endian = BinaryReadWrite::Endian::Little);

    /// @brief Decompress data if required and return a BufferReader and DCXType.
    [[nodiscard]] FIRELINK_CORE_API std::pair<BinaryReadWrite::BufferReader, DCXType> GetBufferReaderForDCX(
        const std::filesystem::path& path, BinaryReadWrite::Endian endian = BinaryReadWrite::Endian::Little);
} // namespace Firelink
