// Binder (BND3/BND4) archive reader/writer implementation.

#include <FirelinkCore/Binder.h>

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

    BinderFlags BinderFlags::FromByte(const std::uint8_t raw, const bool bit_big_endian)
    {
        BinderFlags f{raw};
        if (!bit_big_endian && !(f.is_big_endian() && !f.has_flag_7()))
            f.value = ReverseBits(raw);
        return f;
    }

    std::uint8_t BinderFlags::ToByte(const bool bit_big_endian) const
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
        std::string ReadCString(const BufferReader& r, const std::size_t offset)
        {
            const auto bytes = r.ReadCStringAt(offset);
            return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
        }

        std::string ReadUTF16LEString(const BufferReader& r, const std::size_t offset)
        {
            const auto bytes = r.ReadUTF16LEStringAt(offset);
            return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
        }

        void WriteString(BufferWriter& w, const std::string& s, const bool unicode)
        {
            w.WriteRaw(s.data(), s.size());
            w.WritePad(unicode ? 2 : 1);
        }

        // Entry flags bit reversal (same as binder flags).
        std::uint8_t EntryFlagsFromByte(const std::uint8_t raw, const bool bit_big_endian)
        {
            return bit_big_endian ? raw : ReverseBits(raw);
        }

        std::uint8_t EntryFlagsToByte(const std::uint8_t flags, const bool bit_big_endian)
        {
            return bit_big_endian ? flags : ReverseBits(flags);
        }

        bool IsEntryCompressed(const std::uint8_t flags)
        {
            return flags & 0x01;
        }
    } // anonymous namespace

    // ========================================================================
    // Hash table
    // ========================================================================

    namespace
    {
        bool IsPrime(const int p)
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
                const auto first = static_cast<std::int32_t>(path_hashes.size());
                for (auto& [h, idx] : hl)
                    path_hashes.push_back({h, static_cast<std::int32_t>(idx)});
                groups.push_back({static_cast<std::int32_t>(path_hashes.size()) - first, first});
            }

            // Serialize: header (24 bytes) + groups + path hashes.
            BufferWriter w(Endian::Little);
            // Header: 8 pad bytes, path_hashes_offset (long), group_count (uint), 0x00080810 (int).
            w.WritePad(8);
            const std::uint64_t path_hashes_offset = 24 + static_cast<std::uint64_t>(groups.size()) * 8;
            w.Write<std::uint64_t>(path_hashes_offset);
            w.Write<std::uint32_t>(static_cast<std::uint32_t>(group_count));
            w.Write<std::int32_t>(0x00080810);
            // Groups.
            for (const auto& g : groups)
            {
                w.Write<std::int32_t>(g.length);
                w.Write<std::int32_t>(g.index);
            }
            // Path hashes.
            for (const auto& ph : path_hashes)
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
            BufferReader& r, const BinderFlags& bf, const bool bit_big_endian)
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
                const auto path_offset = r.Read<std::uint32_t>();
                eh.path = ReadCString(r, path_offset);
            }
            eh.has_compression = bf.has_compression();
            eh.uncompressed_size = bf.has_compression() ? r.Read<std::int32_t>() : eh.compressed_size;
            return eh;
        }

        EntryHeader ReadEntryHeaderV4(
            BufferReader& r, const BinderFlags& bf, const bool bit_big_endian, const bool unicode)
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
                const auto path_offset = r.Read<std::uint32_t>();
                eh.path = unicode ? ReadUTF16LEString(r, path_offset) : ReadCString(r, path_offset);
            }
            return eh;
        }
    } // anonymous namespace

    void Binder::Deserialize(BufferReader& reader)
    {
        if (reader.size() < 4)
            throw BinderError("Data too small to be a Binder.");

        if (reader.IsRawAt("BND3"))
        {
            DeserializeV3(reader, reader);
            return;
        }
        if (reader.IsRawAt("BND4"))
        {
            DeserializeV4(reader, reader);
            return;
        }

        const int magic0 = reader.Read<std::uint8_t>();
        const int magic1 = reader.Read<std::uint8_t>();
        const int magic2 = reader.Read<std::uint8_t>();
        const int magic3 = reader.Read<std::uint8_t>();
        throw BinderError(std::format(
            "Unrecognized Binder magic: {:02X} {:02X} {:02X} {:02X}",
            magic0, magic1, magic2, magic3));
    }

    Binder::Ptr Binder::FromSplitBytes(std::vector<std::byte>&& bhdData, std::vector<std::byte>&& bdtData)
    {
        if (bhdData.size() < 4)
            throw BinderError("BHD data too small.");

        auto [bhdReader, bhdDcxType] = GetBufferReaderForDCX(std::move(bhdData));
        auto [bdtReader, bdtDcxType] = GetBufferReaderForDCX(std::move(bdtData));

        auto ptr = std::make_unique<Binder>();
        // We use BHD for DCX type.
        ptr->SetDCXType(bhdDcxType);

        if (bhdReader.IsRawAt("BHF3"))
        {
            bdtReader.Seek(0x10);  // skip useless BDT3 header
            ptr->DeserializeV3(bhdReader, bdtReader);
            return ptr;
        }
        if (bhdReader.IsRawAt("BHF4"))
        {
            bdtReader.Seek(0x30);  // skip useless BDT4 header
            ptr->DeserializeV4(bhdReader, bdtReader);
            return ptr;
        }

        const int magic0 = bhdReader.Read<std::uint8_t>();
        const int magic1 = bhdReader.Read<std::uint8_t>();
        const int magic2 = bhdReader.Read<std::uint8_t>();
        const int magic3 = bhdReader.Read<std::uint8_t>();
        throw BinderError(std::format(
            "Unrecognized split Binder magic: {:02X} {:02X} {:02X} {:02X}. "
            "Must be BHF3 or BHF4.",
            magic0, magic1, magic2, magic3));
    }

    Binder::Ptr Binder::FromSplitBytes(
        const std::byte* bhd_data, const std::size_t bhd_size,
        const std::byte* bdt_data, const std::size_t bdt_size)
    {
        if (bhd_size < 4)
            throw BinderError("BHD data too small.");

        auto [bhdReader, bhdDcxType] = GetBufferReaderForDCX(bhd_data, bhd_size);
        auto [bdtReader, bdtDcxType] = GetBufferReaderForDCX(bdt_data, bdt_size);

        auto ptr = std::make_unique<Binder>();
        // We use BHD for DCX type.
        ptr->SetDCXType(bhdDcxType);

        if (bhdReader.IsRawAt("BHF3"))
        {
            ptr->DeserializeV3(bhdReader, bdtReader);
            return ptr;
        }
        if (bhdReader.IsRawAt("BHF4"))
        {
            ptr->DeserializeV4(bhdReader, bdtReader);
            return ptr;
        }

        const int magic0 = bhdReader.Read<std::uint8_t>();
        const int magic1 = bhdReader.Read<std::uint8_t>();
        const int magic2 = bhdReader.Read<std::uint8_t>();
        const int magic3 = bhdReader.Read<std::uint8_t>();
        throw BinderError(std::format(
            "Unrecognized split Binder BHD magic at: {:02X} {:02X} {:02X} {:02X}. "
            "Must be BHF3 or BHF4.",
            magic0, magic1, magic2, magic3));
    }

    Endian Binder::GetEndian() const
    {
        return (m_isBigEndian || m_flags.is_big_endian()) ? Endian::Big : Endian::Little;
    }

    void Binder::DeserializeV3(BufferReader& reader, BufferReader& entryReader)
    {
        // Peek endian info at fixed offsets before creating a properly-endianized reader.
        this->m_isBigEndian = reader.ReadAt<bool>(0x0D);
        this->m_isBitBigEndian = reader.ReadAt<bool>(0x0E);

        this->m_flags = BinderFlags::FromByte(reader.ReadAt<std::uint8_t>(0x0C), m_isBitBigEndian);
        const Endian endian = (m_isBigEndian || m_flags.is_big_endian()) ? Endian::Big : Endian::Little;
        reader.SetEndian(endian);
        BufferReader& r = reader;

        // Header: magic(4) + signature(8) + flags(1) + big_endian(1) + bit_big_endian(1) + pad(1)
        //         + entry_count(4) + file_size(4) + pad(8) = 32 bytes
        // Accept both BND3 and BHF3.
        r.Skip(4); // magic already validated by caller

        char sig_buf[9]{};
        r.ReadRaw(sig_buf, 8);

        const auto raw_flags = r.Read<std::uint8_t>(); (void)raw_flags; // already read via peek
        r.Read<std::uint8_t>(); // big_endian
        r.Read<std::uint8_t>(); // bit_big_endian
        r.Skip(1); // pad

        const auto entry_count = r.Read<std::uint32_t>();
        const auto file_size = r.Read<std::uint32_t>(); (void)file_size;
        r.Skip(8); // pad

        this->m_version = BinderVersion::V3;
        this->m_signature = std::string(sig_buf);
        // Trim trailing nulls from signature.
        while (!this->m_signature.empty() && this->m_signature.back() == '\0')
            this->m_signature.pop_back();
        this->m_v4Info = std::nullopt;

        // Read entry headers.
        std::vector<EntryHeader> headers;
        headers.reserve(entry_count);
        for (std::uint32_t i = 0; i < entry_count; ++i)
            headers.push_back(ReadEntryHeaderV3(r, m_flags, m_isBitBigEndian));

        // Read entry data.
        this->m_entries.reserve(entry_count);
        for (const auto& eh : headers)
        {
            BinderEntry entry;
            entry.entry_id = eh.entry_id;
            entry.path = eh.path;
            entry.flags = eh.flags;
            entry.data.resize(static_cast<std::size_t>(eh.compressed_size));
            entryReader.ReadRawAt(eh.data_offset, entry.data.data(), entry.data.size());
            this->m_entries.push_back(std::move(entry));
        }
    }

    void Binder::DeserializeV4(BufferReader& reader, BufferReader& entryReader)
    {
        // Peek endian from offset 9 (big_endian byte in V4 header).
        const bool header_big_endian = reader.ReadAt<bool>(0x09);
        const Endian endian = header_big_endian ? Endian::Big : Endian::Little;
        reader.SetEndian(endian);
        BufferReader& r = reader;

        // BND4/BHF4 header (0x40 bytes).
        r.Skip(4); // magic already validated by caller
        const bool unknown1 = r.Read<std::uint8_t>() != 0;
        const bool unknown2 = r.Read<std::uint8_t>() != 0;
        r.Skip(3); // pad
        this->m_isBigEndian = r.Read<std::uint8_t>() != 0;
        const bool bit_little_endian = r.Read<std::uint8_t>() != 0;
        r.Skip(1); // pad
        const auto entry_count = r.Read<std::uint32_t>();
        r.AssertValue<std::int64_t>(0x40, "BND4 header size");

        char sig_buf[9]{};
        r.ReadRaw(sig_buf, 8);

        auto entry_header_size = r.Read<std::int64_t>();
        const auto data_offset = r.Read<std::int64_t>();
        const bool unicode = r.Read<std::uint8_t>() != 0;
        const auto raw_flags = r.Read<std::uint8_t>();
        const auto hash_table_type = r.Read<std::uint8_t>();
        r.Skip(5); // pad
        const auto hash_table_offset = r.Read<std::int64_t>();

        this->m_isBitBigEndian = !bit_little_endian;
        this->m_flags = BinderFlags::FromByte(raw_flags, m_isBitBigEndian);

        // Validate entry header size.
        if (static_cast<std::uint32_t>(entry_header_size) != m_flags.entry_header_size())
        {
            Warning(std::format(
                "[binder] Entry header size mismatch: header says {}, flags imply {}.",
                entry_header_size, m_flags.entry_header_size()));
        }

        // Read entry headers.
        std::vector<EntryHeader> headers;
        headers.reserve(entry_count);
        for (std::uint32_t i = 0; i < entry_count; ++i)
            headers.push_back(ReadEntryHeaderV4(r, m_flags, m_isBitBigEndian, unicode));

        // Read hash table if present.
        BinderVersion4Info v4;
        v4.unknown1 = unknown1;
        v4.unknown2 = unknown2;
        v4.unicode = unicode;
        v4.hash_table_type = hash_table_type;

        if (hash_table_type == 4 && hash_table_offset > 0)
        {
            const std::size_t ht_size = (data_offset > 0)
                ? static_cast<std::size_t>(data_offset - hash_table_offset)
                : reader.size() - hash_table_offset;
            v4.most_recent_hash_table.resize(ht_size);
            std::memcpy(v4.most_recent_hash_table.data(), reader.RawAt(hash_table_offset), ht_size);
        }

        this->m_version = BinderVersion::V4;
        this->m_signature = std::string(sig_buf);
        while (!this->m_signature.empty() && this->m_signature.back() == '\0')
            this->m_signature.pop_back();

        // Read entry data.
        this->m_entries.reserve(entry_count);
        for (const auto& eh : headers)
        {
            BinderEntry entry;
            entry.entry_id = eh.entry_id;
            entry.path = eh.path;
            entry.flags = eh.flags;
            entry.data.resize(static_cast<std::size_t>(eh.compressed_size));
            entryReader.ReadRawAt(eh.data_offset, entry.data.data(), entry.data.size());
            this->m_entries.push_back(std::move(entry));
        }

        v4.most_recent_entry_count = static_cast<std::uint32_t>(this->m_entries.size());
        for (auto& e : this->m_entries)
            v4.most_recent_paths.push_back(e.path);
        this->m_v4Info = std::move(v4);
    }

    void Binder::Serialize(BufferWriter& w) const
    {
        if (m_version == BinderVersion::V3)
            SerializeBND3(w);
        else if (m_version == BinderVersion::V4)
            SerializeBND4(w);
        else
            throw BinderError("Unsupported binder version for writing.");
    }

    void Binder::SerializeBND3(BufferWriter& w) const
    {
        // Sort entries by ID.
        std::vector<std::size_t> order(m_entries.size());
        std::iota(order.begin(), order.end(), 0);
        std::ranges::sort(order, [&](auto a, auto b) {
            return m_entries[a].entry_id < m_entries[b].entry_id;
        });

        // Header (32 bytes).
        w.WriteRaw("BND3", 4);
        // Signature: 8 bytes, null-padded.
        char sig[8]{};
        std::memcpy(sig, m_signature.data(), std::min(m_signature.size(), std::size_t(8)));
        w.WriteRaw(sig, 8);
        w.Write<std::uint8_t>(m_flags.ToByte(m_isBitBigEndian));
        w.Write<std::uint8_t>(m_isBigEndian ? 1 : 0);
        w.Write<std::uint8_t>(m_isBitBigEndian ? 1 : 0);
        w.WritePad(1);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(m_entries.size()));
        w.Reserve<std::uint32_t>("file_size");
        w.WritePad(8);

        // Entry headers.
        for (const auto idx : order)
        {
            const auto& e = m_entries[idx];
            w.Write<std::uint8_t>(EntryFlagsToByte(e.flags, m_isBitBigEndian));
            w.WritePad(3);
            w.Write<std::int32_t>(static_cast<std::int32_t>(e.data.size()));
            if (m_flags.has_long_offsets())
                w.Reserve<std::int64_t>("data_offset", &m_entries[idx]);
            else
                w.Reserve<std::uint32_t>("data_offset", &m_entries[idx]);
            if (m_flags.has_ids())
                w.Write<std::int32_t>(e.entry_id);
            if (m_flags.has_names())
                w.Reserve<std::uint32_t>("path_offset", &m_entries[idx]);
            if (m_flags.has_compression())
                w.Write<std::int32_t>(static_cast<std::int32_t>(e.data.size()));
        }

        // Paths.
        if (m_flags.has_names())
        {
            for (const auto idx : order)
            {
                const auto& e = m_entries[idx];
                w.Fill<std::uint32_t>("path_offset", static_cast<std::uint32_t>(w.Position()), &m_entries[idx]);
                WriteString(w, e.path, false); // V3 always shift-jis
            }
        }

        // Entry data.
        for (const auto idx : order)
        {
            const auto& e = m_entries[idx];
            w.PadAlign(16);
            if (m_flags.has_long_offsets())
                w.Fill<std::int64_t>("data_offset", static_cast<std::int64_t>(w.Position()), &m_entries[idx]);
            else
                w.Fill<std::uint32_t>("data_offset", static_cast<std::uint32_t>(w.Position()), &m_entries[idx]);
            w.WriteRaw(e.data.data(), e.data.size());
        }

        w.Fill<std::uint32_t>("file_size", static_cast<std::uint32_t>(w.Position()));
    }

    void Binder::SerializeBND4(BufferWriter& w) const
    {
        if (!m_v4Info.has_value())
            throw BinderError("BND4 requires v4_info.");

        // TODO: Is `flags.is_big_endian` not trusted for V4?

        const auto& v4 = m_v4Info.value();

        // Sort entries by ID.
        std::vector<std::size_t> order(m_entries.size());
        std::iota(order.begin(), order.end(), 0);
        std::ranges::sort(order, [&](auto a, auto b) {
            return m_entries[a].entry_id < m_entries[b].entry_id;
        });

        // Check if hash table needs rebuilding.
        bool rebuild_hash = v4.most_recent_hash_table.empty()
            || m_entries.size() != v4.most_recent_entry_count;
        if (!rebuild_hash)
        {
            for (std::size_t i = 0; i < m_entries.size(); ++i)
            {
                if (m_entries[i].path != v4.most_recent_paths[i])
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
        w.Write<std::uint8_t>(m_isBigEndian ? 1 : 0);
        w.Write<std::uint8_t>(m_isBitBigEndian ? 0 : 1); // bit_little_endian
        w.WritePad(1);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(m_entries.size()));
        w.Write<std::int64_t>(0x40); // header size
        char sig[8]{};
        std::memcpy(sig, m_signature.data(), std::min(m_signature.size(), std::size_t(8)));
        w.WriteRaw(sig, 8);
        w.Write<std::int64_t>(m_flags.entry_header_size());
        w.Reserve<std::int64_t>("data_offset");
        w.Write<std::uint8_t>(v4.unicode ? 1 : 0);
        w.Write<std::uint8_t>(m_flags.ToByte(m_isBitBigEndian));
        w.Write<std::uint8_t>(v4.hash_table_type);
        w.WritePad(5);
        w.Reserve<std::int64_t>("hash_table_offset");

        // Entry headers.
        for (const auto idx : order)
        {
            const auto& e = m_entries[idx];
            w.Write<std::uint8_t>(EntryFlagsToByte(e.flags, m_isBitBigEndian));
            w.WritePad(3);
            w.Write<std::int32_t>(-1);
            w.Write<std::int64_t>(static_cast<std::int64_t>(e.data.size()));
            if (m_flags.has_compression())
                w.Write<std::int64_t>(static_cast<std::int64_t>(e.data.size()));
            if (m_flags.has_long_offsets())
                w.Reserve<std::int64_t>("data_offset", &m_entries[idx]);
            else
                w.Reserve<std::uint32_t>("data_offset", &m_entries[idx]);
            if (m_flags.has_ids())
                w.Write<std::int32_t>(e.entry_id);
            if (m_flags.has_names())
                w.Reserve<std::uint32_t>("path_offset", &m_entries[idx]);
        }

        // Paths.
        if (m_flags.has_names())
        {
            for (const auto idx : order)
            {
                const auto& e = m_entries[idx];
                w.Fill<std::uint32_t>("path_offset", static_cast<std::uint32_t>(w.Position()), &m_entries[idx]);
                WriteString(w, e.path, v4.unicode);
            }
        }

        // Hash table.
        if (v4.hash_table_type == 4)
        {
            w.Fill<std::int64_t>("hash_table_offset", static_cast<std::int64_t>(w.Position()));
            if (rebuild_hash)
            {
                const auto ht = BuildHashTable(m_entries);
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
        for (const auto idx : order)
        {
            const auto& e = m_entries[idx];
            if (m_flags.has_long_offsets())
                w.Fill<std::int64_t>("data_offset", static_cast<std::int64_t>(w.Position()), &m_entries[idx]);
            else
                w.Fill<std::uint32_t>("data_offset", static_cast<std::uint32_t>(w.Position()), &m_entries[idx]);
            w.WriteRaw(e.data.data(), e.data.size());
            w.WritePad(10); // trailing null bytes for byte-perfect writes
        }
    }

    // ========================================================================
    // Find helpers
    // ========================================================================

    const BinderEntry* Binder::FindEntryByID(const std::int32_t id) const
    {
        for (auto& e : m_entries)
            if (e.entry_id == id) return &e;
        return nullptr;
    }

    BinderEntry* Binder::FindEntryByID(const std::int32_t id)
    {
        return const_cast<BinderEntry*>(std::as_const(*this).FindEntryByID(id));

    }

    const BinderEntry* Binder::FindEntryByName(const std::string& name) const
    {
        for (auto& e : m_entries)
            if (e.name() == name) return &e;
        return nullptr;
    }

    BinderEntry* Binder::FindEntryByName(const std::string& name)
    {
        return const_cast<BinderEntry*>(std::as_const(*this).FindEntryByName(name));
    }

    const BinderEntry* Binder::FindEntryByNameRegex(const RE2& regex, const bool fullMatch) const
    {
        BinderEntry const* candidate = nullptr;
        for (auto& entry : m_entries)
        {
            if (fullMatch ? RE2::FullMatch(entry.name(), regex) : RE2::PartialMatch(entry.name(), regex))
            {
                if (candidate != nullptr)
                    throw MultipleBinderEntriesFoundError(std::format(
                        "Multiple entries match regex '{}': '{}' and '{}'.",
                        regex.pattern(), candidate->path, entry.path));
                candidate = &entry;
            }
        }
        return nullptr;
    }

    BinderEntry* Binder::FindEntryByNameRegex(const RE2& regex, const bool fullMatch)
    {
        return const_cast<BinderEntry*>(std::as_const(*this).FindEntryByNameRegex(regex, fullMatch));
    }

    std::vector<const BinderEntry*> Binder::FindEntriesByNameRegex(const RE2& regex, const bool fullMatch) const
    {
        std::vector<const BinderEntry*> entries;
        for (auto& e : m_entries)
        {
            if (fullMatch ? RE2::FullMatch(e.name(), regex) : RE2::PartialMatch(e.name(), regex))
            {
                entries.push_back(&e);
            }
        }
        return entries;
    }

    std::vector<BinderEntry*> Binder::FindEntriesByNameRegex(const RE2& regex, const bool fullMatch)
    {
        std::vector<BinderEntry*> entries;
        for (auto& e : m_entries)
        {
            if (fullMatch ? RE2::FullMatch(e.name(), regex) : RE2::PartialMatch(e.name(), regex))
            {
                entries.push_back(&e);
            }
        }
        return entries;
    }

    const BinderEntry* Binder::FindEntryByFilter(const EntryFilter& filter) const
    {
        for (auto& e : m_entries)
            if (filter(e)) return &e;
        return nullptr;
    }

    BinderEntry* Binder::FindEntryByFilter(const EntryFilter& filter)
    {
        return const_cast<BinderEntry*>(std::as_const(*this).FindEntryByFilter(filter));
    }

    std::vector<const BinderEntry*> Binder::FindEntriesByFilter(const EntryFilter& filter) const noexcept
    {
        std::vector<const BinderEntry*> entries;
        for (auto& e : m_entries)
        {
            if (filter(e)) entries.push_back(&e);
        }
        return entries;
    }

    std::vector<BinderEntry*> Binder::FindEntriesByFilter(const EntryFilter& filter) noexcept
    {
        std::vector<BinderEntry*> entries;
        for (auto& e : m_entries)
        {
            if (filter(e)) entries.push_back(&e);
        }
        return entries;
    }
} // namespace Firelink
