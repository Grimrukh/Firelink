// TPF (Texture Pack File) format for FromSoftware file formats.
//
// TPFs contain one or more DDS textures bundled together.
// PC textures have standard DDS headers; console textures may be headerless.

#pragma once

#include <FirelinkCore/Export.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace Firelink
{
    // --- TPFError ---

    class TPFError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // --- TPFPlatform ---

    enum class TPFPlatform : std::uint8_t
    {
        PC = 0,
        Xbox360 = 1,
        PS3 = 2,
        PS4 = 4,
        XboxOne = 5,
    };

    // --- TextureType ---

    enum class TextureType : std::uint8_t
    {
        Texture = 0,
        Cubemap = 1,
        Volume = 2,
    };

    // --- TPFTexture ---

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
    };

    // --- TPF ---

    class FIRELINK_CORE_API TPF
    {
    public:
        TPFPlatform platform = TPFPlatform::PC;
        std::uint8_t tpf_flags = 0;
        std::uint8_t encoding_type = 0; // 0/2 = shift-jis, 1 = UTF-16
        std::vector<TPFTexture> textures;

        /// @brief Parse a TPF from raw (already decompressed) bytes.
        static TPF FromBytes(const std::byte* data, std::size_t size);

        /// @brief Serialize this TPF back to bytes.
        [[nodiscard]] std::vector<std::byte> ToBytes() const;

        /// @brief Get texture count.
        [[nodiscard]] std::size_t TextureCount() const { return textures.size(); }

        /// @brief Find texture by stem (case-insensitive). Returns nullptr if not found.
        [[nodiscard]] const TPFTexture* FindTexture(const std::string& stem) const;
        [[nodiscard]] TPFTexture* FindTexture(const std::string& stem);
    };

} // namespace Firelink

