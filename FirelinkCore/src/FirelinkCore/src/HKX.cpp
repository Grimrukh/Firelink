#include <FirelinkCore/HKX.h>
#include <FirelinkCore/Havok/Tagfile.h>

#include <cstring>
#include <stdexcept>

namespace Firelink
{
    void HKX::Deserialize(BinaryReadWrite::BufferReader& reader)
    {
        // Detect format by peeking at bytes 4–7 (the section magic, after the 4-byte size field).
        // Tagfile: bytes 4–7 are "TAG0" or "TCM0".
        // Packfile: bytes 0–7 are 0x57 0xE0 0xE0 0x57 0x10 0xC0 0xC0 0x10.
        if (reader.Size() < 8)
            throw std::runtime_error("HKX: file too small to contain a valid header");

        const auto* header = reader.RawAt(0);

        // Check packfile magic first.
        static constexpr uint8_t kPackfileMagic[8] =
            { 0x57, 0xE0, 0xE0, 0x57, 0x10, 0xC0, 0xC0, 0x10 };
        if (std::memcmp(header, kPackfileMagic, 8) == 0)
            throw std::runtime_error("HKX: packfile format is not yet supported");

        // Check tagfile magic at bytes 4–7.
        const bool isTagfile =
            std::memcmp(header + 4, "TAG0", 4) == 0 ||
            std::memcmp(header + 4, "TCM0", 4) == 0;

        if (!isTagfile)
            throw std::runtime_error("HKX: unrecognised file format (not a packfile or tagfile)");

        Havok::TagFileUnpacker unpacker;
        unpacker.Unpack(reader);

        hkVersion = std::move(unpacker.hkVersion);
        root      = std::move(unpacker.root);
    }

    void HKX::Serialize(BinaryReadWrite::BufferWriter& /*writer*/) const
    {
        throw std::logic_error("HKX::Serialize: tagfile packing is not yet implemented");
    }
}

