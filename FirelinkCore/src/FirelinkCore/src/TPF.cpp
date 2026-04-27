// TPF (Texture Pack File) reader/writer implementation.

#include <FirelinkCore/TPF.h>

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/DCX.h>
#include <FirelinkCore/Paths.h>

#include <algorithm>

namespace Firelink
{
    using BinaryReadWrite::BufferReader;
    using BinaryReadWrite::BufferWriter;
    using BinaryReadWrite::Endian;

    namespace
    {
        void WriteString(BufferWriter& w, const std::string& s, bool unicode)
        {
            w.WriteRaw(s.data(), s.size());
            w.WritePad(unicode ? 2 : 1);
        }

        bool IsBigEndianPlatform(TPFPlatform p)
        {
            return p == TPFPlatform::Xbox360 || p == TPFPlatform::PS3;
        }
    } // anonymous namespace

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
            tex.float_struct = std::move(th.float_struct);

            // Read stem.
            tex.stem = r.ReadStringAt(th.stem_offset, unicode_encoding);

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
            WriteString(w, m_textures[i].stem, unicode);
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
