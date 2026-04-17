// Binder (BND3/BND4) archive reader/writer implementation.

#include <FirelinkCore/Binder.h>
#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/Logging.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <numeric>

namespace Firelink
{
    using BinaryReadWrite::BufferReader;
    using BinaryReadWrite::BufferWriter;
    using BinaryReadWrite::Endian;

    // ========================================================================
    // BinderFlags
    // ========================================================================

    static std::uint8_t ReverseBits(std::uint8_t b)
    {
        std::uint8_t r = 0;
        for (int i = 0; i < 8; ++i)
        {
            r = (r << 1) | (b & 1);
            b >>= 1;
        }
        return r;
    }

    BinderFlags BinderFlags::FromByte(std::uint8_t raw, bool bit_big_endian)
    {
        BinderFlags f{raw};
        if (!bit_big_endian && !(f.is_big_endian() && !f.has_flag_7()))
            f.value = ReverseBits(raw);
        return f;
    }

    std::uint8_t BinderFlags::ToByte(bool bit_big_endian) const
    {
        if (!bit_big_endian && !(is_big_endian() && !has_flag_7()))
            return ReverseBits(value);
        return value;
    }

    // ========================================================================
    // String reading helpers
    // ========================================================================

    namespace
    {
        std::string ReadCString(const BufferReader& r, std::size_t offset)
        {
            auto bytes = r.ReadCStringAt(offset);
            return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
        }

        std::string ReadUTF16LEString(const BufferReader& r, std::size_t offset)
        {
            auto bytes = r.ReadUTF16LEStringAt(offset);
            return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
        }

        void WriteString(BufferWriter& w, const std::string& s, bool unicode)
        {
            w.WriteRaw(s.data(), s.size());
            w.WritePad(unicode ? 2 : 1);
        }

        // Entry flags bit reversal (same as binder flags).
        std::uint8_t EntryFlagsFromByte(std::uint8_t raw, bool bit_big_endian)
        {
            return bit_big_endian ? raw : ReverseBits(raw);
        }

        std::uint8_t EntryFlagsToByte(std::uint8_t flags, bool bit_big_endian)
        {
            return bit_big_endian ? flags : ReverseBits(flags);
        }

        bool IsEntryCompressed(std::uint8_t flags)
        {
            return flags & 0x01;
        }
    } // anonymous namespace

    // ========================================================================
    // Hash table
    // ========================================================================

    namespace
    {
        bool IsPrime(int p)
        {
            if (p < 2) return false;
            if (p == 2) return true;
            if (p % 2 == 0) return false;
            for (int i = 3; i * i <= p; i += 2)
                if (p % i == 0) return false;
            return true;
        }

        std::uint32_t PathHash(const std::string& path)
        {
            std::string hashable = path;
            for (auto& c : hashable)
                if (c == '\\') c = '/';
            if (hashable.empty() || hashable[0] != '/')
                hashable = "/" + hashable;

            std::uint32_t h = 0;
            for (std::size_t i = 0; i < hashable.size(); ++i)
                h += static_cast<std::uint32_t>(i) * 37 + static_cast<std::uint8_t>(hashable[i]);
            return h;
        }

        std::vector<std::byte> BuildHashTable(const std::vector<BinderEntry>& entries)
        {
            int group_count = static_cast<int>(entries.size()) / 7;
            while (!IsPrime(group_count)) ++group_count;

            // Build hash lists per group.
            std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t>>> hash_lists(group_count);
            for (std::uint32_t i = 0; i < entries.size(); ++i)
            {
                std::uint32_t h = PathHash(entries[i].path);
                hash_lists[h % group_count].emplace_back(h, i);
            }
            for (auto& hl : hash_lists)
                std::sort(hl.begin(), hl.end());

            // Flatten.
            struct HashGroup { std::int32_t length; std::int32_t index; };
            struct PathHashEntry { std::uint32_t hashed_value; std::int32_t entry_index; };

            std::vector<HashGroup> groups;
            std::vector<PathHashEntry> path_hashes;

            for (auto& hl : hash_lists)
            {
                auto first = static_cast<std::int32_t>(path_hashes.size());
                for (auto& [h, idx] : hl)
                    path_hashes.push_back({h, static_cast<std::int32_t>(idx)});
                groups.push_back({static_cast<std::int32_t>(path_hashes.size()) - first, first});
            }

            // Serialize: header (24 bytes) + groups + path hashes.
            BufferWriter w(Endian::Little);
            // Header: 8 pad bytes, path_hashes_offset (long), group_count (uint), 0x00080810 (int).
            w.WritePad(8);
            std::uint64_t path_hashes_offset = 24 + static_cast<std::uint64_t>(groups.size()) * 8;
            w.Write<std::uint64_t>(path_hashes_offset);
            w.Write<std::uint32_t>(static_cast<std::uint32_t>(group_count));
            w.Write<std::int32_t>(0x00080810);
            // Groups.
            for (auto& g : groups)
            {
                w.Write<std::int32_t>(g.length);
                w.Write<std::int32_t>(g.index);
            }
            // Path hashes.
            for (auto& ph : path_hashes)
            {
                w.Write<std::uint32_t>(ph.hashed_value);
                w.Write<std::int32_t>(ph.entry_index);
            }
            return w.Finalize();
        }
    } // anonymous namespace

    // ========================================================================
    // Entry header reading helpers
    // ========================================================================

    namespace
    {
        struct EntryHeader
        {
            std::uint8_t flags;
            std::int64_t compressed_size;
            std::int64_t data_offset;
            std::int32_t entry_id;
            std::string path;
            std::int64_t uncompressed_size;
            bool has_id;
            bool has_path;
            bool has_compression;
        };

        EntryHeader ReadEntryHeaderV3(
            BufferReader& r, const BinderFlags& bf, bool bit_big_endian)
        {
            EntryHeader eh{};
            eh.flags = EntryFlagsFromByte(r.Read<std::uint8_t>(), bit_big_endian);
            r.AssertPad(3);
            eh.compressed_size = r.Read<std::int32_t>();
            eh.data_offset = bf.has_long_offsets() ? r.Read<std::int64_t>() : r.Read<std::uint32_t>();
            eh.has_id = bf.has_ids();
            eh.entry_id = bf.has_ids() ? r.Read<std::int32_t>() : -1;
            eh.has_path = bf.has_names();
            if (bf.has_names())
            {
                auto path_offset = r.Read<std::uint32_t>();
                eh.path = ReadCString(r, path_offset);
            }
            eh.has_compression = bf.has_compression();
            eh.uncompressed_size = bf.has_compression() ? r.Read<std::int32_t>() : eh.compressed_size;
            return eh;
        }

        EntryHeader ReadEntryHeaderV4(
            BufferReader& r, const BinderFlags& bf, bool bit_big_endian, bool unicode)
        {
            EntryHeader eh{};
            eh.flags = EntryFlagsFromByte(r.Read<std::uint8_t>(), bit_big_endian);
            r.AssertPad(3);
            r.AssertValue<std::int32_t>(-1, "BND4 entry unk");
            eh.compressed_size = r.Read<std::int64_t>();
            eh.has_compression = bf.has_compression();
            eh.uncompressed_size = bf.has_compression() ? r.Read<std::int64_t>() : eh.compressed_size;
            eh.data_offset = bf.has_long_offsets() ? r.Read<std::int64_t>() : r.Read<std::uint32_t>();
            eh.has_id = bf.has_ids();
            eh.entry_id = bf.has_ids() ? r.Read<std::int32_t>() : -1;
            eh.has_path = bf.has_names();
            if (bf.has_names())
            {
                auto path_offset = r.Read<std::uint32_t>();
                eh.path = unicode ? ReadUTF16LEString(r, path_offset) : ReadCString(r, path_offset);
            }
            return eh;
        }
    } // anonymous namespace

    // ========================================================================
    // Binder::FromBytes
    // ========================================================================

    Binder Binder::FromBytes(const std::byte* data, std::size_t size)
    {
        if (size < 4)
            throw BinderError("Data too small to be a Binder.");

        if (std::memcmp(data, "BND3", 4) == 0)
            return ReadV3(data, size, data, size);
        if (std::memcmp(data, "BND4", 4) == 0)
            return ReadV4(data, size, data, size);

        throw BinderError(std::format(
            "Unrecognized Binder magic: {:02X} {:02X} {:02X} {:02X}",
            static_cast<int>(data[0]), static_cast<int>(data[1]),
            static_cast<int>(data[2]), static_cast<int>(data[3])));
    }

    // ========================================================================
    // Binder::FromSplitBytes (BHF header + BDT data)
    // ========================================================================

    Binder Binder::FromSplitBytes(
        const std::byte* bhd_data, std::size_t bhd_size,
        const std::byte* bdt_data, std::size_t bdt_size)
    {
        if (bhd_size < 4)
            throw BinderError("BHD data too small.");

        if (std::memcmp(bhd_data, "BHF3", 4) == 0)
            return ReadV3(bhd_data, bhd_size, bdt_data, bdt_size);
        if (std::memcmp(bhd_data, "BHF4", 4) == 0)
            return ReadV4(bhd_data, bhd_size, bdt_data, bdt_size);

        throw BinderError("Unrecognized BHF magic.");
    }

    // ========================================================================
    // ReadV3
    // ========================================================================

    Binder Binder::ReadV3(const std::byte* data, std::size_t size,
                          const std::byte* entry_data, std::size_t entry_data_size)
    {
        // Peek endian info at fixed offsets before creating a properly-endianized reader.
        bool big_endian = static_cast<bool>(data[0x0D]);
        bool bit_big_endian = static_cast<bool>(data[0x0E]);

        auto flags = BinderFlags::FromByte(static_cast<std::uint8_t>(data[0x0C]), bit_big_endian);
        Endian endian = (big_endian || flags.is_big_endian()) ? Endian::Big : Endian::Little;

        BufferReader r(data, size, endian);

        // Header: magic(4) + signature(8) + flags(1) + big_endian(1) + bit_big_endian(1) + pad(1)
        //         + entry_count(4) + file_size(4) + pad(8) = 32 bytes
        // Accept both BND3 and BHF3.
        r.Skip(4); // magic already validated by caller

        char sig_buf[9]{};
        r.ReadRaw(sig_buf, 8);

        std::uint8_t raw_flags = r.Read<std::uint8_t>();
        (void)raw_flags; // already read via peek
        r.Read<std::uint8_t>(); // big_endian
        r.Read<std::uint8_t>(); // bit_big_endian
        r.Skip(1); // pad

        auto entry_count = r.Read<std::uint32_t>();
        auto file_size = r.Read<std::uint32_t>();
        (void)file_size;
        r.Skip(8); // pad

        Binder binder;
        binder.version = BinderVersion::V3;
        binder.signature = std::string(sig_buf);
        // Trim trailing nulls from signature.
        while (!binder.signature.empty() && binder.signature.back() == '\0')
            binder.signature.pop_back();
        binder.flags = flags;
        binder.big_endian = big_endian;
        binder.bit_big_endian = bit_big_endian;
        binder.v4_info = std::nullopt;

        // Read entry headers.
        std::vector<EntryHeader> headers;
        headers.reserve(entry_count);
        for (std::uint32_t i = 0; i < entry_count; ++i)
            headers.push_back(ReadEntryHeaderV3(r, flags, bit_big_endian));

        // Read entry data.
        binder.entries.reserve(entry_count);
        for (auto& eh : headers)
        {
            BinderEntry entry;
            entry.entry_id = eh.entry_id;
            entry.path = eh.path;
            entry.flags = eh.flags;
            entry.data.resize(static_cast<std::size_t>(eh.compressed_size));
            std::memcpy(entry.data.data(), entry_data + eh.data_offset, entry.data.size());
            binder.entries.push_back(std::move(entry));
        }

        return binder;
    }

    // ========================================================================
    // ReadV4
    // ========================================================================

    Binder Binder::ReadV4(const std::byte* data, std::size_t size,
                          const std::byte* entry_data, std::size_t entry_data_size)
    {
        // Peek endian from offset 9 (big_endian byte in V4 header).
        bool header_big_endian = static_cast<bool>(data[0x09]);
        Endian endian = header_big_endian ? Endian::Big : Endian::Little;

        BufferReader r(data, size, endian);

        // BND4/BHF4 header (0x40 bytes).
        r.Skip(4); // magic already validated by caller
        bool unknown1 = r.Read<std::uint8_t>() != 0;
        bool unknown2 = r.Read<std::uint8_t>() != 0;
        r.Skip(3); // pad
        bool big_endian = r.Read<std::uint8_t>() != 0;
        bool bit_little_endian = r.Read<std::uint8_t>() != 0;
        r.Skip(1); // pad
        auto entry_count = r.Read<std::uint32_t>();
        r.AssertValue<std::int64_t>(0x40, "BND4 header size");

        char sig_buf[9]{};
        r.ReadRaw(sig_buf, 8);

        auto entry_header_size = r.Read<std::int64_t>();
        auto data_offset = r.Read<std::int64_t>();
        bool unicode = r.Read<std::uint8_t>() != 0;
        auto raw_flags = r.Read<std::uint8_t>();
        auto hash_table_type = r.Read<std::uint8_t>();
        r.Skip(5); // pad
        auto hash_table_offset = r.Read<std::int64_t>();

        bool bit_big_endian = !bit_little_endian;
        auto flags = BinderFlags::FromByte(raw_flags, bit_big_endian);

        // Validate entry header size.
        if (static_cast<std::uint32_t>(entry_header_size) != flags.entry_header_size())
        {
            Firelink::Warning(std::format(
                "[binder] Entry header size mismatch: header says {}, flags imply {}.",
                entry_header_size, flags.entry_header_size()));
        }

        // Read entry headers.
        std::vector<EntryHeader> headers;
        headers.reserve(entry_count);
        for (std::uint32_t i = 0; i < entry_count; ++i)
            headers.push_back(ReadEntryHeaderV4(r, flags, bit_big_endian, unicode));

        // Read hash table if present.
        BinderVersion4Info v4;
        v4.unknown1 = unknown1;
        v4.unknown2 = unknown2;
        v4.unicode = unicode;
        v4.hash_table_type = hash_table_type;

        if (hash_table_type == 4 && hash_table_offset > 0)
        {
            std::size_t ht_size = (data_offset > 0)
                ? static_cast<std::size_t>(data_offset - hash_table_offset)
                : static_cast<std::size_t>(size - hash_table_offset);
            v4.most_recent_hash_table.resize(ht_size);
            std::memcpy(v4.most_recent_hash_table.data(), data + hash_table_offset, ht_size);
        }

        Binder binder;
        binder.version = BinderVersion::V4;
        binder.signature = std::string(sig_buf);
        while (!binder.signature.empty() && binder.signature.back() == '\0')
            binder.signature.pop_back();
        binder.flags = flags;
        binder.big_endian = big_endian;
        binder.bit_big_endian = bit_big_endian;

        // Read entry data.
        binder.entries.reserve(entry_count);
        for (auto& eh : headers)
        {
            BinderEntry entry;
            entry.entry_id = eh.entry_id;
            entry.path = eh.path;
            entry.flags = eh.flags;
            entry.data.resize(static_cast<std::size_t>(eh.compressed_size));
            std::memcpy(entry.data.data(), entry_data + eh.data_offset, entry.data.size());
            binder.entries.push_back(std::move(entry));
        }

        v4.most_recent_entry_count = static_cast<std::uint32_t>(binder.entries.size());
        for (auto& e : binder.entries)
            v4.most_recent_paths.push_back(e.path);
        binder.v4_info = std::move(v4);

        return binder;
    }

    // ========================================================================
    // Binder::ToBytes
    // ========================================================================

    std::vector<std::byte> Binder::ToBytes() const
    {
        if (version == BinderVersion::V3)
            return WriteBND3();
        if (version == BinderVersion::V4)
            return WriteBND4();
        throw BinderError("Unsupported binder version for writing.");
    }

    // ========================================================================
    // WriteBND3
    // ========================================================================

    std::vector<std::byte> Binder::WriteBND3() const
    {
        Endian endian = (big_endian || flags.is_big_endian()) ? Endian::Big : Endian::Little;
        BufferWriter w(endian);

        // Sort entries by ID.
        std::vector<std::size_t> order(entries.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](auto a, auto b) {
            return entries[a].entry_id < entries[b].entry_id;
        });

        // Header (32 bytes).
        w.WriteRaw("BND3", 4);
        // Signature: 8 bytes, null-padded.
        char sig[8]{};
        std::memcpy(sig, signature.data(), std::min(signature.size(), std::size_t(8)));
        w.WriteRaw(sig, 8);
        w.Write<std::uint8_t>(flags.ToByte(bit_big_endian));
        w.Write<std::uint8_t>(big_endian ? 1 : 0);
        w.Write<std::uint8_t>(bit_big_endian ? 1 : 0);
        w.WritePad(1);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(entries.size()));
        w.Reserve<std::uint32_t>("file_size");
        w.WritePad(8);

        // Entry headers.
        for (auto idx : order)
        {
            const auto& e = entries[idx];
            w.Write<std::uint8_t>(EntryFlagsToByte(e.flags, bit_big_endian));
            w.WritePad(3);
            w.Write<std::int32_t>(static_cast<std::int32_t>(e.data.size()));
            if (flags.has_long_offsets())
                w.Reserve<std::int64_t>("data_offset", &entries[idx]);
            else
                w.Reserve<std::uint32_t>("data_offset", &entries[idx]);
            if (flags.has_ids())
                w.Write<std::int32_t>(e.entry_id);
            if (flags.has_names())
                w.Reserve<std::uint32_t>("path_offset", &entries[idx]);
            if (flags.has_compression())
                w.Write<std::int32_t>(static_cast<std::int32_t>(e.data.size()));
        }

        // Paths.
        if (flags.has_names())
        {
            for (auto idx : order)
            {
                const auto& e = entries[idx];
                w.Fill<std::uint32_t>("path_offset", static_cast<std::uint32_t>(w.Position()), &entries[idx]);
                WriteString(w, e.path, false); // V3 always shift-jis
            }
        }

        // Entry data.
        for (auto idx : order)
        {
            const auto& e = entries[idx];
            w.PadAlign(16);
            if (flags.has_long_offsets())
                w.Fill<std::int64_t>("data_offset", static_cast<std::int64_t>(w.Position()), &entries[idx]);
            else
                w.Fill<std::uint32_t>("data_offset", static_cast<std::uint32_t>(w.Position()), &entries[idx]);
            w.WriteRaw(e.data.data(), e.data.size());
        }

        w.Fill<std::uint32_t>("file_size", static_cast<std::uint32_t>(w.Position()));
        return w.Finalize();
    }

    // ========================================================================
    // WriteBND4
    // ========================================================================

    std::vector<std::byte> Binder::WriteBND4() const
    {
        if (!v4_info.has_value())
            throw BinderError("BND4 requires v4_info.");

        const auto& v4 = v4_info.value();
        Endian endian = big_endian ? Endian::Big : Endian::Little;
        BufferWriter w(endian);

        // Sort entries by ID.
        std::vector<std::size_t> order(entries.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](auto a, auto b) {
            return entries[a].entry_id < entries[b].entry_id;
        });

        // Check if hash table needs rebuilding.
        bool rebuild_hash = v4.most_recent_hash_table.empty()
            || entries.size() != v4.most_recent_entry_count;
        if (!rebuild_hash)
        {
            for (std::size_t i = 0; i < entries.size(); ++i)
            {
                if (entries[i].path != v4.most_recent_paths[i])
                {
                    rebuild_hash = true;
                    break;
                }
            }
        }

        // Header (0x40 bytes).
        w.WriteRaw("BND4", 4);
        w.Write<std::uint8_t>(v4.unknown1 ? 1 : 0);
        w.Write<std::uint8_t>(v4.unknown2 ? 1 : 0);
        w.WritePad(3);
        w.Write<std::uint8_t>(big_endian ? 1 : 0);
        w.Write<std::uint8_t>(bit_big_endian ? 0 : 1); // bit_little_endian
        w.WritePad(1);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(entries.size()));
        w.Write<std::int64_t>(0x40); // header size
        char sig[8]{};
        std::memcpy(sig, signature.data(), std::min(signature.size(), std::size_t(8)));
        w.WriteRaw(sig, 8);
        w.Write<std::int64_t>(static_cast<std::int64_t>(flags.entry_header_size()));
        w.Reserve<std::int64_t>("data_offset");
        w.Write<std::uint8_t>(v4.unicode ? 1 : 0);
        w.Write<std::uint8_t>(flags.ToByte(bit_big_endian));
        w.Write<std::uint8_t>(v4.hash_table_type);
        w.WritePad(5);
        w.Reserve<std::int64_t>("hash_table_offset");

        // Entry headers.
        for (auto idx : order)
        {
            const auto& e = entries[idx];
            w.Write<std::uint8_t>(EntryFlagsToByte(e.flags, bit_big_endian));
            w.WritePad(3);
            w.Write<std::int32_t>(-1);
            w.Write<std::int64_t>(static_cast<std::int64_t>(e.data.size()));
            if (flags.has_compression())
                w.Write<std::int64_t>(static_cast<std::int64_t>(e.data.size()));
            if (flags.has_long_offsets())
                w.Reserve<std::int64_t>("data_offset", &entries[idx]);
            else
                w.Reserve<std::uint32_t>("data_offset", &entries[idx]);
            if (flags.has_ids())
                w.Write<std::int32_t>(e.entry_id);
            if (flags.has_names())
                w.Reserve<std::uint32_t>("path_offset", &entries[idx]);
        }

        // Paths.
        if (flags.has_names())
        {
            for (auto idx : order)
            {
                const auto& e = entries[idx];
                w.Fill<std::uint32_t>("path_offset", static_cast<std::uint32_t>(w.Position()), &entries[idx]);
                WriteString(w, e.path, v4.unicode);
            }
        }

        // Hash table.
        if (v4.hash_table_type == 4)
        {
            w.Fill<std::int64_t>("hash_table_offset", static_cast<std::int64_t>(w.Position()));
            if (rebuild_hash)
            {
                auto ht = BuildHashTable(entries);
                w.WriteRaw(ht.data(), ht.size());
            }
            else
            {
                w.WriteRaw(v4.most_recent_hash_table.data(), v4.most_recent_hash_table.size());
            }
        }
        else
        {
            w.Fill<std::int64_t>("hash_table_offset", 0);
        }

        // Data offset.
        w.Fill<std::int64_t>("data_offset", static_cast<std::int64_t>(w.Position()));

        // Entry data (with 10 trailing null bytes per entry, matching Python).
        for (auto idx : order)
        {
            const auto& e = entries[idx];
            if (flags.has_long_offsets())
                w.Fill<std::int64_t>("data_offset", static_cast<std::int64_t>(w.Position()), &entries[idx]);
            else
                w.Fill<std::uint32_t>("data_offset", static_cast<std::uint32_t>(w.Position()), &entries[idx]);
            w.WriteRaw(e.data.data(), e.data.size());
            w.WritePad(10); // trailing null bytes for byte-perfect writes
        }

        return w.Finalize();
    }

    // ========================================================================
    // Find helpers
    // ========================================================================

    const BinderEntry* Binder::FindEntryByID(std::int32_t id) const
    {
        for (auto& e : entries)
            if (e.entry_id == id) return &e;
        return nullptr;
    }

    BinderEntry* Binder::FindEntryByID(std::int32_t id)
    {
        for (auto& e : entries)
            if (e.entry_id == id) return &e;
        return nullptr;
    }

    const BinderEntry* Binder::FindEntryByName(const std::string& n) const
    {
        for (auto& e : entries)
            if (e.name() == n) return &e;
        return nullptr;
    }

    BinderEntry* Binder::FindEntryByName(const std::string& n)
    {
        for (auto& e : entries)
            if (e.name() == n) return &e;
        return nullptr;
    }

} // namespace Firelink

