// TPF (Texture Pack File) format for FromSoftware file formats.
//
// TPFs contain one or more DDS textures bundled together.
// PC textures have standard DDS headers; console textures may be headerless.

#pragma once

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/DDS.h>
#include <FirelinkCore/Export.h>
#include <FirelinkCore/GameFile.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace Firelink
{
    /// @brief Error thrown during TPF operations.
    class TPFError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    /// @brief Game platform enum for a TPF. Can indicate swizzling.
    enum class TPFPlatform : std::uint8_t
    {
        PC = 0,
        Xbox360 = 1,
        PS3 = 2,
        PS4 = 4,
        XboxOne = 5,
    };

    /// @brief Basic type of TPF texture.
    enum class TextureType : std::uint8_t
    {
        Texture = 0,
        Cubemap = 1,
        Volume = 2,
    };

    /// @brief Metadata and raw bytes for a DDS texture stored inside a TPF container.
    struct TPFTexture
    {
        std::string stem;                   // texture name (no extension)
        std::uint8_t format = 1;            // internal TPF format enum
        TextureType texture_type = TextureType::Texture;
        std::uint8_t mipmap_count = 0;
        std::uint8_t texture_flags = 0;     // bit 1 = DCX/zlib compressed
        std::vector<std::byte> data;        // DDS data (decompressed)

        // Optional console info (for headerless console textures).
        struct ConsoleInfo
        {
            std::int16_t width = 0;
            std::int16_t height = 0;
            std::int32_t texture_count = 0;
            std::int32_t unk1 = 0;
            std::int32_t unk2 = 0;
            std::int32_t dxgi_format = 0;
        };
        std::optional<ConsoleInfo> console_info;

        // Optional unknown float struct.
        struct FloatStruct
        {
            std::int32_t unk0 = 0;
            std::vector<float> floats;
        };
        std::optional<FloatStruct> float_struct;

        /// @brief Platform of the owning TPF, copied in on read.
        /// @note Needed to interpret `format` and to know whether `data` is headerless.
        TPFPlatform platform = TPFPlatform::PC;

        /// @brief True if `data` already begins with the "DDS " magic.
        /// @note Console TPFs (PS3, Xbox 360, and some PS4/XboxOne) store the mip chain
        ///       alone, with no DDS header; such data cannot be loaded by DirectXTex
        ///       until `ToDDS()` rebuilds a header for it.
        [[nodiscard]] FIRELINK_CORE_API bool HasDDSHeader() const noexcept;

        /// @brief Construct a DDS from copied texture data.
        /// @note If the stored data is headerless, a DDS header is synthesized from this
        ///       texture's metadata (dimensions, `format`, `mipmap_count`, `texture_type`).
        /// @note Pixel data is *not* deswizzled; call `DDS::DeswizzlePS4()` on the result
        ///       for PS4 dumps. (PS3 TPF textures are not swizzled.)
        [[nodiscard]] FIRELINK_CORE_API DDS ToDDS() const;

        /// @brief Map the internal TPF `format` enum to a DXGI format.
        /// @return `DXGI_FORMAT_UNKNOWN` for unrecognized values.
        [[nodiscard]] FIRELINK_CORE_API static DXGI_FORMAT FormatToDXGI(std::uint8_t format) noexcept;
    };

    /// @brief Simple texture container file. May contain one or more DDS textures.
    class FIRELINK_CORE_API TPF : public GameFile<TPF>
    {
    public:

        // --- GAME FILE OVERRIDES ---

        /// @brief Get endianness of TPF.
        [[nodiscard]] BinaryReadWrite::Endian GetEndian() const noexcept;

        /// @brief Deserialize a TPF.
        void Deserialize(BinaryReadWrite::BufferReader& r);

        /// @brief Serialize this TPF.
        void Serialize(BinaryReadWrite::BufferWriter& w) const;

        // --- PUBLIC METHODS ---

        /// @brief Get texture count.
        [[nodiscard]] std::size_t TextureCount() const { return m_textures.size(); }

        /// @brief Find texture by stem (case-insensitive). Returns nullptr if not found.
        [[nodiscard]] const TPFTexture* FindTexture(const std::string& stem) const;
        [[nodiscard]] TPFTexture* FindTexture(const std::string& stem);

        /// @brief Get the texture at the given index.
        [[nodiscard]] const TPFTexture& GetTexture(const std::size_t index) const { return m_textures[index]; }
        [[nodiscard]] TPFTexture& GetTexture(const std::size_t index) { return m_textures[index]; }

        // --- PROPERTIES ---

        GAME_FILE_PROPERTY(TPFPlatform, m_platform, Platform, TPFPlatform::PC);
        GAME_FILE_PROPERTY(std::uint8_t, m_flags, Flags, 0);
        GAME_FILE_PROPERTY(std::uint8_t, m_encodingType, EncodingType, 0); // 0/2 = shift-jis, 1 = UTF-16
        GAME_FILE_PROPERTY_REF(std::vector<TPFTexture>, m_textures, Textures, );

    };

} // namespace Firelink
