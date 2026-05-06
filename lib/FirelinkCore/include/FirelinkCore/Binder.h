// Binder (BND3/BND4) archive format for FromSoftware file formats.
//
// Binders are multi-file archives that contain entries with IDs, paths, and data.
// Two versions exist: V3 (pre-DS2) and V4 (DS2+).
// Files are usually DCX-compressed on disk.

#pragma once

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/Endian.h>
#include <FirelinkCore/Export.h>
#include <FirelinkCore/GameFile.h>

#include <re2/re2.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace Firelink
{
    // --- BinderError ---

    class BinderError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    class BinderEntryNotFoundError : public BinderError
    {
    public:
        using BinderError::BinderError;
    };

    class MultipleBinderEntriesFoundError : public BinderError
    {
    public:
        using BinderError::BinderError;
    };

    // --- BinderVersion ---

    enum class BinderVersion : std::uint8_t
    {
        V3 = 3, // BND3 — used before Dark Souls 2
        V4 = 4, // BND4 — used from Dark Souls 2 onwards
    };

    // --- BinderFlags ---
    // Bit flags from the binder header. Stored in "big-endian bit order" internally.

    struct BinderFlags
    {
        std::uint8_t value = 0b00101110; // most common default

        [[nodiscard]] bool is_big_endian()    const { return value & 0b0000'0001; }
        [[nodiscard]] bool has_ids()          const { return value & 0b0000'0010; }
        [[nodiscard]] bool has_names_1()      const { return value & 0b0000'0100; }
        [[nodiscard]] bool has_names_2()      const { return value & 0b0000'1000; }
        [[nodiscard]] bool has_names()        const { return value & (0b0000'0100 | 0b0000'1000); }
        [[nodiscard]] bool has_long_offsets() const { return value & 0b0001'0000; }
        [[nodiscard]] bool has_compression()  const { return value & 0b0010'0000; }
        [[nodiscard]] bool has_flag_6()       const { return value & 0b0100'0000; }
        [[nodiscard]] bool has_flag_7()       const { return value & 0b1000'0000; }

        [[nodiscard]] std::uint32_t entry_header_size() const
        {
            std::uint32_t size = 16;
            if (has_ids()) size += 4;
            if (has_names()) size += 4;
            if (has_compression()) size += 8;
            size += has_long_offsets() ? 8 : 4;
            return size;
        }

        /// Read raw byte and convert from wire bit order.
        static BinderFlags FromByte(std::uint8_t raw, bool bit_big_endian);
        /// Convert to wire byte for writing.
        [[nodiscard]] std::uint8_t ToByte(bool bit_big_endian) const;
    };

    // --- BinderVersion4Info ---

    struct BinderVersion4Info
    {
        bool unknown1 = false;
        bool unknown2 = false;
        bool unicode = true;
        std::uint8_t hash_table_type = 4;

        // Cached hash table from read (reused on write if entries haven't changed).
        std::vector<std::byte> most_recent_hash_table;
        std::uint32_t most_recent_entry_count = 0;
        std::vector<std::string> most_recent_paths;
    };

    // --- BinderEntry ---

    struct BinderEntry
    {
        std::int32_t entry_id = -1;
        std::string path;                   // full internal path (raw bytes, shift-jis or UTF-16 LE round-trip)
        std::vector<std::byte> data;        // entry payload (may be zlib-compressed per entry flags)
        std::uint8_t flags = 0x02;          // entry flags (bit 0 = compressed)

        [[nodiscard]] std::string name() const
        {
            const auto pos = path.find_last_of("\\/");
            return pos == std::string::npos ? path : path.substr(pos + 1);
        }

        [[nodiscard]] std::string stem() const
        {
            const std::string _name = name();
            // Get substring before first '.' in name.
            const auto pos = _name.find_first_of('.');
            return pos == std::string::npos ? path : _name.substr(0, pos);
        }
    };

    // --- Binder ---

    class FIRELINK_CORE_API Binder : public GameFile<Binder>
    {
    public:

        // --- ADDITIONAL FACTORIES ---

        /// @brief Parse a split BHF3/BHF4 header + BDT data pair (managed storage).
        static Ptr FromSplitBytes(std::vector<std::byte>&& bhdData, std::vector<std::byte>&& bdtData);

        /// @brief Parse a split BHF3/BHF4 header + BDT data pair (raw data).
        static Ptr FromSplitBytes(
            const std::byte* bhd_data, std::size_t bhd_size,
            const std::byte* bdt_data, std::size_t bdt_size);

        // --- GAME FILE OVERRIDES ---

        /// @brief Get endianness of Binder.
        [[nodiscard]] BinaryReadWrite::Endian GetEndian() const;

        /// @brief Deserialize a Binder. Dispatches to V3 or V4 method.
        void Deserialize(BinaryReadWrite::BufferReader& reader);

        /// @brief Serialize a Binder. Dispatches to V3 or V4 method.
        void Serialize(BinaryReadWrite::BufferWriter& w) const;

        // --- PUBLIC METHODS ---

        /// @brief Get entry count.
        [[nodiscard]] std::size_t EntryCount() const { return m_entries.size(); }

        /// @brief Find entry by ID. Returns nullptr if not found.
        [[nodiscard]] const BinderEntry* FindEntryByID(std::int32_t id) const;
        [[nodiscard]] BinderEntry* FindEntryByID(std::int32_t id);

        /// @brief Find entry by name (basename). Returns nullptr if not found.
        [[nodiscard]] const BinderEntry* FindEntryByName(const std::string& name) const;
        [[nodiscard]] BinderEntry* FindEntryByName(const std::string& name);

        /// @brief Find entry by regex match to name (basename). Returns nullptr if not found.
        /// @note Exactly one entry must match, or a `MultipleBinderEntriesFoundError` will be thrown.
        [[nodiscard]] const BinderEntry* FindEntryByNameRegex(const RE2& regex, bool fullMatch = false) const;
        [[nodiscard]] BinderEntry* FindEntryByNameRegex(const RE2& regex, bool fullMatch = false);

        /// @brief Find all entries whose names match the given regex.
        [[nodiscard]] std::vector<const BinderEntry*> FindEntriesByNameRegex(
            const RE2& regex, bool fullMatch = false) const;
        [[nodiscard]] std::vector<BinderEntry*> FindEntriesByNameRegex(
            const RE2& regex, bool fullMatch = false);

        /// @brief Alias for entry filter function.
        using EntryFilter = std::function<bool(const BinderEntry&)>;

        /// @brief Find entry by any entry-filtering function. Returns nullptr if not found.
        /// @note Exactly one entry must match, or a `MultipleBinderEntriesFoundError` will be thrown.
        [[nodiscard]] const BinderEntry* FindEntryByFilter(const EntryFilter& filter) const;
        [[nodiscard]] BinderEntry* FindEntryByFilter(const EntryFilter& filter);

        /// @brief Find all entries that match an entry-filtering function.
        [[nodiscard]] std::vector<const BinderEntry*> FindEntriesByFilter(const EntryFilter& filter) const noexcept;
        [[nodiscard]] std::vector<BinderEntry*> FindEntriesByFilter(const EntryFilter& filter) noexcept;

        // --- PROPERTIES ---

        GAME_FILE_PROPERTY(BinderVersion, m_version, Version, BinderVersion::V4);
        GAME_FILE_PROPERTY(std::string, m_signature, Signature, "07D7R6");
        GAME_FILE_PROPERTY(BinderFlags, m_flags, Flags, );
        GAME_FILE_PROPERTY(bool, m_isBigEndian, BigEndian, false);
        GAME_FILE_PROPERTY(bool, m_isBitBigEndian, BitBigEndian, false);
        GAME_FILE_PROPERTY(std::optional<BinderVersion4Info>, m_v4Info, V4Info, BinderVersion4Info{});
        GAME_FILE_PROPERTY_REF(std::vector<BinderEntry>, m_entries, Entries, );

    private:

        // Version-dispatched serialization/deserialization.

        void DeserializeV3(BinaryReadWrite::BufferReader& reader, BinaryReadWrite::BufferReader& entryReader);
        void DeserializeV4(BinaryReadWrite::BufferReader& reader, BinaryReadWrite::BufferReader& entryReader);
        void SerializeBND3(BinaryReadWrite::BufferWriter& w) const;
        void SerializeBND4(BinaryReadWrite::BufferWriter& writer) const;
    };

} // namespace Firelink
