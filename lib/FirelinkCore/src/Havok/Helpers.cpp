#include <FirelinkCore/Havok/Helpers.h>

#include <cstring>
#include <stdexcept>

namespace Firelink::Havok
{

    // =========================================================================
    // Internal helper
    // =========================================================================

    static uint32_t ReadBigEndianU32(BinaryReadWrite::BufferReader& reader)
    {
        uint8_t b[4];
        reader.ReadRaw(b, 4);
        return (static_cast<uint32_t>(b[0]) << 24)
             | (static_cast<uint32_t>(b[1]) << 16)
             | (static_cast<uint32_t>(b[2]) <<  8)
             |  static_cast<uint32_t>(b[3]);
    }

    // =========================================================================
    // Section helpers
    // =========================================================================

    SectionView EnterSection(
        BinaryReadWrite::BufferReader& reader,
        std::string_view magic1,
        std::string_view magic2)
    {
        // Section header: big-endian uint32 total size (incl. 8-byte header) + 4-byte magic.
        const uint32_t rawSize  = ReadBigEndianU32(reader);
        const size_t dataSize   = (rawSize & 0x3FFFFFFFu) - 8;

        char magicBuf[4];
        reader.ReadRaw(magicBuf, 4);
        const std::string_view got(magicBuf, 4);

        if (got != magic1 && (magic2.empty() || got != magic2))
        {
            throw std::runtime_error(
                "HKX tagfile: expected section '"
                + std::string(magic1)
                + (magic2.empty() ? "" : ("' or '" + std::string(magic2)))
                + "', got '" + std::string(got) + "'");
        }

        const size_t start = reader.Position();
        return { start, start + dataSize };
    }

    bool PeekSection(
        const BinaryReadWrite::BufferReader& reader,
        std::string_view magic)
    {
        if (reader.Remaining() < 8)
            return false;

        uint8_t b[8];
        // Non-advancing peek: copy raw bytes without moving the cursor.
        std::memcpy(b, reader.RawAt(reader.Position()), 8);

        // Magic is at bytes 4–7.
        return std::string_view(reinterpret_cast<const char*>(b + 4), 4) == magic;
    }

    // =========================================================================
    // Variable-length integer
    // =========================================================================

    uint64_t ReadVarInt(BinaryReadWrite::BufferReader& reader)
    {
        const uint8_t b1 = reader.Read<uint8_t>();

        // 7-bit fast path.
        if ((b1 & 0x80u) == 0u)
            return b1 & 0x7Fu;

        // Special-case literal 0xC3 observed in Elden Ring files.
        if (b1 == 0xC3u)
        {
            const uint8_t b2 = reader.Read<uint8_t>();
            const uint8_t b3 = reader.Read<uint8_t>();
            return (static_cast<uint64_t>(b2) << 8) | b3;
        }

        const uint8_t marker = b1 >> 3u;

        if (marker >= 0x10u && marker < 0x18u)
        {
            // 14-bit (2 bytes big-endian), mask 0x3FFF.
            const uint8_t b2 = reader.Read<uint8_t>();
            return 0x3FFFu & ((static_cast<uint32_t>(b1) << 8) | b2);
        }

        if (marker >= 0x18u && marker < 0x1Cu)
        {
            // 21-bit (3 bytes big-endian), mask 0x1FFFFF.
            const uint8_t b2 = reader.Read<uint8_t>();
            const uint8_t b3 = reader.Read<uint8_t>();
            return 0x1FFFFFu & ((static_cast<uint32_t>(b1) << 16)
                              | (static_cast<uint32_t>(b2) <<  8)
                              |  static_cast<uint32_t>(b3));
        }

        if (marker == 0x1Cu)
        {
            // 27-bit: rewind 1 byte, read LE uint32, mask 0x07FFFFFF.
            reader.Seek(reader.Position() - 1);
            return reader.Read<uint32_t>() & 0x07FFFFFFu;
        }

        if (marker == 0x1Du)
        {
            // 35-bit (byte_1 + 4 more big-endian), mask 0x07FFFFFFFF.
            const uint8_t b2 = reader.Read<uint8_t>();
            const uint8_t b3 = reader.Read<uint8_t>();
            const uint8_t b4 = reader.Read<uint8_t>();
            const uint8_t b5 = reader.Read<uint8_t>();
            return 0x07FFFFFFFFull & ((static_cast<uint64_t>(b1) << 32)
                                    | (static_cast<uint64_t>(b2) << 24)
                                    | (static_cast<uint64_t>(b3) << 16)
                                    | (static_cast<uint64_t>(b4) <<  8)
                                    |  static_cast<uint64_t>(b5));
        }

        if (marker == 0x1Eu)
        {
            // 59-bit: rewind 1 byte, read LE uint64, mask 0x07FFFFFFFFFFFFFF.
            reader.Seek(reader.Position() - 1);
            return reader.Read<uint64_t>() & 0x07FFFFFFFFFFFFFFull;
        }

        throw std::runtime_error(
            "HKX tagfile: unrecognised varint marker byte 0x"
            + [b1]{ char buf[3]; std::snprintf(buf, 3, "%02X", b1); return std::string(buf); }());
    }

    void WriteVarInt(BinaryReadWrite::BufferWriter& writer, uint64_t value)
    {
        // All multi-byte forms are big-endian; individual bytes are written explicitly.
        if (value < 0x80ull)
        {
            writer.Write<uint8_t>(static_cast<uint8_t>(value));
        }
        else if (value < 0x4000ull)
        {
            const auto v = static_cast<uint16_t>(value | 0x8000u);
            writer.Write<uint8_t>(static_cast<uint8_t>(v >> 8));
            writer.Write<uint8_t>(static_cast<uint8_t>(v & 0xFFu));
        }
        else if (value < 0x200000ull)
        {
            writer.Write<uint8_t>(static_cast<uint8_t>((value >> 16) | 0xC0u));
            writer.Write<uint8_t>(static_cast<uint8_t>((value >>  8) & 0xFFu));
            writer.Write<uint8_t>(static_cast<uint8_t>( value        & 0xFFu));
        }
        else if (value < 0x8000000ull)
        {
            const auto v = static_cast<uint32_t>(value | 0xE0000000u);
            writer.Write<uint8_t>(static_cast<uint8_t>(v >> 24));
            writer.Write<uint8_t>(static_cast<uint8_t>((v >> 16) & 0xFFu));
            writer.Write<uint8_t>(static_cast<uint8_t>((v >>  8) & 0xFFu));
            writer.Write<uint8_t>(static_cast<uint8_t>( v        & 0xFFu));
        }
        else
        {
            throw std::runtime_error(
                "HKX tagfile: VarInt value " + std::to_string(value) + " exceeds supported range");
        }
    }

} // namespace Firelink::Havok

