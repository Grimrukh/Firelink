// TPF (Texture Pack File) reader/writer implementation.

#include <FirelinkCore/TPF.h>

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/DCX.h>
#include <FirelinkCore/Encodings.h>
#include <FirelinkCore/Paths.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace Firelink
{
    using BinaryReadWrite::BufferReader;
    using BinaryReadWrite::BufferWriter;
    using BinaryReadWrite::Endian;

    namespace
    {
        //! @brief Texture stems are stored as UTF-16 or Shift-JIS, but held in memory as UTF-8.
        FSEncoding StemEncoding(const bool unicode)
        {
            return unicode ? FSEncoding::UTF_16 : FSEncoding::SHIFT_JIS;
        }

        bool IsBigEndianPlatform(TPFPlatform p)
        {
            return p == TPFPlatform::Xbox360 || p == TPFPlatform::PS3;
        }

        //! @brief FourCC code, bytes per block/pixel, and compression flag for a TPF format enum.
        //!
        //! The `TPFTexture::format` byte is a FromSoft-internal enum, not a DXGI format. Console
        //! TPFs need it to rebuild the DDS header they don't store. A null fourCC means the
        //! format is described by channel bit masks instead (see `PixelFormatMasks` below).
        struct FormatInfo
        {
            std::array<char, 4> fourCC{};
            int bytesPerBlock = 0;
            bool isCompressed = false;
        };

        FormatInfo GetFormatInfo(const std::uint8_t format)
        {
            constexpr auto FourCC = [](const char (&s)[5])
            {
                return std::array{s[0], s[1], s[2], s[3]};
            };

            switch (format)
            {
                case 0: case 1: case 24: case 25: case 108: case 109:
                    return {FourCC("DXT1"), 8, true};
                case 3:
                    return {FourCC("DXT3"), 16, true};
                case 5: case 23: case 33: case 110:
                    return {FourCC("DXT5"), 16, true};
                case 6:
                    return {FourCC("DX10"), 2, false};   // B5G5R5A1_UNORM
                case 9: case 10: case 105:
                    return {{}, 4, false};               // 32-bit RGBA variants
                case 16:
                    return {{}, 1, false};               // A8_UNORM
                case 22:
                    return {std::array{'\x71', '\0', '\0', '\0'}, 8, false};  // fourCC 0x71: R16G16B16A16_UNORM
                case 100: case 113:
                    return {FourCC("DX10"), 16, true};   // BC6H_UF16
                case 102: case 106: case 107: case 112:
                    return {FourCC("DX10"), 16, true};   // BC7_UNORM(_SRGB)
                case 103:
                    return {FourCC("ATI1"), 8, true};    // BC4
                case 104:
                    return {FourCC("ATI2"), 16, true};   // BC5
                default:
                    throw TPFError(
                        "Cannot rebuild a DDS header for TPF texture format "
                        + std::to_string(format) + " (inferred DXGI format "
                        + std::to_string(static_cast<int>(TPFTexture::FormatToDXGI(format)))
                        + "); this format has not been seen in a headerless texture yet.");
            }
        }

        //! @brief Fill in the DDPF flags / channel bit masks for mask-described TPF formats.
        void SetPixelFormatMasks(DDSHeaderParams& params, const std::uint8_t format)
        {
            switch (format)
            {
                case 6:  // B5G5R5A1_UNORM
                    params.pixelFormatFlags |= DDPF_ALPHAPIXELS | DDPF_RGB;
                    params.rgbBitCount = 16;
                    params.rBitMask = 0x7C00;
                    params.gBitMask = 0x03E0;
                    params.bBitMask = 0x001F;
                    params.aBitMask = 0x8000;
                    break;
                case 9:  // B8G8R8A8
                    params.pixelFormatFlags |= DDPF_ALPHAPIXELS | DDPF_RGB;
                    params.rgbBitCount = 32;
                    params.rBitMask = 0x00FF0000;
                    params.gBitMask = 0x0000FF00;
                    params.bBitMask = 0x000000FF;
                    params.aBitMask = 0xFF000000;
                    break;
                case 10:  // B8G8R8X8 (no alpha)
                    params.pixelFormatFlags |= DDPF_RGB;
                    params.rgbBitCount = 32;
                    params.rBitMask = 0x00FF0000;
                    params.gBitMask = 0x0000FF00;
                    params.bBitMask = 0x000000FF;
                    break;
                case 16:  // A8
                    params.pixelFormatFlags |= DDPF_ALPHA;
                    params.rgbBitCount = 8;
                    params.aBitMask = 0x000000FF;
                    break;
                case 105:  // R8G8B8A8
                    params.pixelFormatFlags |= DDPF_ALPHAPIXELS | DDPF_RGB;
                    params.rgbBitCount = 32;
                    params.rBitMask = 0x000000FF;
                    params.gBitMask = 0x0000FF00;
                    params.bBitMask = 0x00FF0000;
                    params.aBitMask = 0xFF000000;
                    break;
                default:
                    break;  // fourCC-described; no masks needed
            }
        }
    } // anonymous namespace

    // ========================================================================
    // TPFTexture
    // ========================================================================

    DXGI_FORMAT TPFTexture::FormatToDXGI(const std::uint8_t format) noexcept
    {
        switch (format)
        {
            // 24 is BC1 here to agree with `GetFormatInfo`, which writes it a "DXT1" fourCC.
            // (Soulstruct's equivalent table calls 24 BC4; the two contradict each other there.)
            case 0: case 1: case 24: case 25: case 29: case 108: case 109:
                                                                  return DXGI_FORMAT_BC1_UNORM;
            case 3:                                               return DXGI_FORMAT_BC2_UNORM;
            case 5: case 23: case 33: case 110:                   return DXGI_FORMAT_BC3_UNORM;
            case 6:                                               return DXGI_FORMAT_B5G5R5A1_UNORM;
            case 8: case 10: case 105:                            return DXGI_FORMAT_R8G8B8A8_UNORM;
            case 9:                                               return DXGI_FORMAT_B8G8R8A8_UNORM;
            case 16:                                              return DXGI_FORMAT_A8_UNORM;
            case 22:                                              return DXGI_FORMAT_R16G16B16A16_UNORM;
            case 103:                                             return DXGI_FORMAT_BC4_UNORM;
            case 104:                                             return DXGI_FORMAT_BC5_UNORM;
            case 100: case 113: case 115:                         return DXGI_FORMAT_BC6H_UF16;
            case 102: case 106: case 107:                         return DXGI_FORMAT_BC7_UNORM;
            case 112:                                             return DXGI_FORMAT_BC7_UNORM_SRGB;
            default:                                              return DXGI_FORMAT_UNKNOWN;
        }
    }

    bool TPFTexture::HasDDSHeader() const noexcept
    {
        return data.size() >= 4 && std::memcmp(data.data(), "DDS ", 4) == 0;
    }

    DDS TPFTexture::ToDDS() const
    {
        if (HasDDSHeader())
            return DDS(data.data(), data.size());

        if (data.empty())
            throw TPFError("TPF texture '" + stem + "' has no data.");
        if (!console_info.has_value())
            throw TPFError(
                "TPF texture '" + stem + "' is headerless but has no console info, so a DDS "
                "header cannot be rebuilt for it.");

        const auto& ci = *console_info;
        if (ci.width <= 0 || ci.height <= 0)
            throw TPFError(
                "TPF texture '" + stem + "' is headerless but its console info gives invalid "
                "dimensions (" + std::to_string(ci.width) + "x" + std::to_string(ci.height) + ").");

        const FormatInfo info = GetFormatInfo(format);

        DDSHeaderParams params;
        params.width = ci.width;
        params.height = ci.height;
        params.mipCount = mipmap_count;  // 0 means "derive the full chain"
        params.isCubemap = texture_type == TextureType::Cubemap;
        params.isVolume = texture_type == TextureType::Volume;
        params.fourCC = info.fourCC;
        params.bytesPerBlock = info.bytesPerBlock;
        params.isCompressed = info.isCompressed;
        SetPixelFormatMasks(params, format);

        // PS4/XboxOne store the DXGI format directly; everything else is mapped from `format`.
        params.dxgiFormat = ci.dxgi_format != 0
            ? static_cast<DXGI_FORMAT>(ci.dxgi_format)
            : FormatToDXGI(format);

        return DDS::FromHeaderlessData(data.data(), data.size(), params);
    }

    // ========================================================================
    // TPF::FromBytes
    // ========================================================================

    void TPF::Deserialize(BufferReader& r)
    {
        if (r.size() < 16)
            throw TPFError("Data too small to be a TPF.");

        // Peek platform byte at offset 0x0C to determine endianness.
        this->m_platform = r.ReadAt<TPFPlatform>(0x0C);
        Endian endian = IsBigEndianPlatform(m_platform) ? Endian::Big : Endian::Little;
        r.SetEndian(endian);

        // Header: "TPF\0"(4) + data_size(4) + file_count(4) + platform(1) + tpf_flags(1) + encoding_type(1) + pad(1)
        r.AssertBytes("TPF\0", 4, "TPF magic");
        auto data_size = r.Read<std::int32_t>();
        (void)data_size;
        auto file_count = r.Read<std::int32_t>();
        r.Read<std::uint8_t>(); // platform (already peeked)
        this->m_flags = r.Read<std::uint8_t>();
        this->m_encodingType = r.Read<std::uint8_t>();
        r.Skip(1); // pad

        bool unicode_encoding = (m_encodingType == 1);

        m_textures.reserve(file_count);

        // Texture struct reading.
        struct TextureHeader
        {
            std::uint32_t data_offset;
            std::int32_t data_size;
            std::uint8_t format;
            TextureType texture_type;
            std::uint8_t mipmap_count;
            std::uint8_t texture_flags;
            std::uint32_t stem_offset;
            bool has_float_struct;
            std::optional<TPFTexture::ConsoleInfo> console_info;
            std::optional<TPFTexture::FloatStruct> float_struct;
        };

        std::vector<TextureHeader> headers;
        headers.reserve(file_count);

        for (std::int32_t i = 0; i < file_count; ++i)
        {
            TextureHeader th{};
            th.data_offset = r.Read<std::uint32_t>();
            th.data_size = r.Read<std::int32_t>();
            th.format = r.Read<std::uint8_t>();
            th.texture_type = static_cast<TextureType>(r.Read<std::uint8_t>());
            th.mipmap_count = r.Read<std::uint8_t>();
            th.texture_flags = r.Read<std::uint8_t>();

            if (m_platform != TPFPlatform::PC)
            {
                TPFTexture::ConsoleInfo ci{};
                ci.width = r.Read<std::int16_t>();
                ci.height = r.Read<std::int16_t>();

                // Only PS4/XboxOne store a DXGI format of their own (read further below);
                // for the older consoles it has to be derived from the internal format enum.
                ci.dxgi_format = TPFTexture::FormatToDXGI(th.format);

                if (m_platform == TPFPlatform::Xbox360)
                {
                    r.Skip(4);
                }
                else if (m_platform == TPFPlatform::PS3)
                {
                    ci.unk1 = r.Read<std::int32_t>();
                    if (m_flags != 0)
                        ci.unk2 = r.Read<std::int32_t>();
                }
                else if (m_platform == TPFPlatform::PS4 || m_platform == TPFPlatform::XboxOne)
                {
                    ci.texture_count = r.Read<std::int32_t>();
                    ci.unk2 = r.Read<std::int32_t>();
                }
                th.console_info = ci;
            }

            th.stem_offset = r.Read<std::uint32_t>();
            th.has_float_struct = (r.Read<std::int32_t>() == 1);

            if (m_platform == TPFPlatform::PS4 || m_platform == TPFPlatform::XboxOne)
            {
                if (th.console_info.has_value())
                    th.console_info->dxgi_format = r.Read<std::int32_t>();
            }

            if (th.has_float_struct)
            {
                TPFTexture::FloatStruct fs{};
                fs.unk0 = r.Read<std::int32_t>();
                auto float_byte_size = r.Read<std::int32_t>();
                auto float_count = float_byte_size / 4;
                fs.floats.resize(float_count);
                for (int fi = 0; fi < float_count; ++fi)
                    fs.floats[fi] = r.Read<float>();
                th.float_struct = std::move(fs);
            }

            headers.push_back(std::move(th));
        }

        // Build textures.
        for (auto& th : headers)
        {
            TPFTexture tex;
            tex.format = th.format;
            tex.texture_type = th.texture_type;
            tex.mipmap_count = th.mipmap_count;
            tex.texture_flags = th.texture_flags;
            tex.console_info = th.console_info;
            tex.platform = m_platform;
            tex.float_struct = std::move(th.float_struct);

            // Read stem.
            tex.stem = r.ReadDecodedStringAt(th.stem_offset, StemEncoding(unicode_encoding));

            // Read data.
            const std::byte* tex_data = r.RawAt(th.data_offset);
            auto tex_size = static_cast<std::size_t>(th.data_size);

            if (th.texture_flags == 2 || th.texture_flags == 3)
            {
                // Data is DCX-compressed.
                auto result = DecompressDCX(tex_data, tex_size);
                tex.data = std::move(result.data);
            }
            else
            {
                tex.data.assign(tex_data, tex_data + tex_size);
            }

            m_textures.push_back(std::move(tex));
        }
    }

    void TPF::Serialize(BufferWriter& w) const
    {
        const bool unicode = (m_encodingType == 1);

        // Header.
        w.WriteRaw("TPF\0", 4);
        w.Reserve<std::int32_t>("data_size");
        w.Write<std::int32_t>(static_cast<std::int32_t>(m_textures.size()));
        w.Write<std::uint8_t>(static_cast<std::uint8_t>(m_platform));
        w.Write<std::uint8_t>(m_flags);
        w.Write<std::uint8_t>(m_encodingType);
        w.WritePad(1);

        // Texture structs.
        for (std::size_t i = 0; i < m_textures.size(); ++i)
        {
            const auto& tex = m_textures[i];
            const void* scope = &m_textures[i];

            w.Reserve<std::uint32_t>("tex_data_offset", scope);
            w.Reserve<std::int32_t>("tex_data_size", scope);
            w.Write<std::uint8_t>(tex.format);
            w.Write<std::uint8_t>(static_cast<std::uint8_t>(tex.texture_type));
            w.Write<std::uint8_t>(tex.mipmap_count);
            w.Write<std::uint8_t>(tex.texture_flags);

            if (m_platform != TPFPlatform::PC && tex.console_info.has_value())
            {
                const auto& ci = *tex.console_info;
                w.Write<std::int16_t>(ci.width);
                w.Write<std::int16_t>(ci.height);
                if (m_platform == TPFPlatform::Xbox360)
                    w.WritePad(4);
                else if (m_platform == TPFPlatform::PS3)
                {
                    w.Write<std::int32_t>(ci.unk1);
                    if (m_flags != 0)
                        w.Write<std::int32_t>(ci.unk2);
                }
                else if (m_platform == TPFPlatform::PS4 || m_platform == TPFPlatform::XboxOne)
                {
                    w.Write<std::int32_t>(ci.texture_count);
                    w.Write<std::int32_t>(ci.unk2);
                }
            }

            w.Reserve<std::uint32_t>("tex_stem_offset", scope);
            w.Write<std::int32_t>(tex.float_struct.has_value() ? 1 : 0);

            if (m_platform == TPFPlatform::PS4 || m_platform == TPFPlatform::XboxOne)
            {
                if (tex.console_info.has_value())
                    w.Write<std::int32_t>(tex.console_info->dxgi_format);
            }

            if (tex.float_struct.has_value())
            {
                const auto& fs = *tex.float_struct;
                w.Write<std::int32_t>(fs.unk0);
                w.Write<std::int32_t>(static_cast<std::int32_t>(fs.floats.size() * 4));
                for (float f : fs.floats)
                    w.Write<float>(f);
            }
        }

        // Stems.
        for (std::size_t i = 0; i < m_textures.size(); ++i)
        {
            const void* scope = &m_textures[i];
            w.Fill<std::uint32_t>("tex_stem_offset", static_cast<std::uint32_t>(w.Position()), scope);
            w.WriteDecodedString(m_textures[i].stem, StemEncoding(unicode));
        }

        // Data.
        const auto data_start = w.Position();
        for (std::size_t i = 0; i < m_textures.size(); ++i)
        {
            const auto& tex = m_textures[i];
            const void* scope = &m_textures[i];

            if (!tex.data.empty())
                w.PadAlign(4);

            w.Fill<std::uint32_t>("tex_data_offset", static_cast<std::uint32_t>(w.Position()), scope);

            if (tex.texture_flags == 2 || tex.texture_flags == 3)
            {
                // Compress with DCP_DFLT (zlib).
                auto compressed = CompressDCX(tex.data.data(), tex.data.size(), DCXType::DCP_DFLT);
                w.Fill<std::int32_t>("tex_data_size", static_cast<std::int32_t>(compressed.size()), scope);
                w.WriteRaw(compressed.data(), compressed.size());
            }
            else
            {
                w.Fill<std::int32_t>("tex_data_size", static_cast<std::int32_t>(tex.data.size()), scope);
                w.WriteRaw(tex.data.data(), tex.data.size());
            }
        }

        w.Fill<std::int32_t>("data_size", static_cast<std::int32_t>(w.Position() - data_start));
    }

    Endian TPF::GetEndian() const noexcept
    {
        return IsBigEndianPlatform(m_platform) ? Endian::Big : Endian::Little;
    }

    const TPFTexture* TPF::FindTexture(const std::string& stem) const
    {
        const auto target = ToLower(stem);
        for (auto& t : m_textures)
            if (ToLower(t.stem) == target) return &t;
        return nullptr;
    }

    TPFTexture* TPF::FindTexture(const std::string& stem)
    {
        return const_cast<TPFTexture*>(const_cast<const TPF*>(this)->FindTexture(stem));
    }

} // namespace Firelink
