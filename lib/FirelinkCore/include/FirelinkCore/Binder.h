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

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace Firelink
{
    // --- BinderError ---

    class FIRELINK_CORE_API BinderError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    class FIRELINK_CORE_API BinderEntryNotFoundError : public BinderError
    {
    public:
        using BinderError::BinderError;
    };

    class FIRELINK_CORE_API MultipleBinderEntriesFoundError : public BinderError
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

    struct FIRELINK_CORE_API BinderFlags
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

    struct FIRELINK_CORE_API BinderVersion4Info
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

    /// @class BinderEntry
    /// @brief Single entry inside a Binder.
    /// @details Entry data may or may not be decompressed (ZLIB level 7). Data is only decompressed on access,
    ///  as Binders may frequently be loaded with only specific Entries in mind for use.
    class FIRELINK_CORE_API BinderEntry
    {
    public:
        using Ptr = std::shared_ptr<BinderEntry>;

        BinderEntry() = default;
        BinderEntry(std::int32_t id, std::string path, std::vector<std::byte> rawData, std::uint8_t flags);

        // Binder entry ID.
        GAME_FILE_PROPERTY(std::int32_t, m_entryID, EntryID, -1);
        // Full internal path (string).
        GAME_FILE_PROPERTY(std::string, m_path, Path, /*default = {}*/);
        // Entry bit flags. Only one bit flag's purpose is currently known (compression == 0b1).
        GAME_FILE_PROPERTY(std::uint8_t, m_flags, Flags, 0b00000010);

        /// @brief Get basename component of entry path.
        [[nodiscard]] std::string GetPathName() const;

        [[nodiscard]] std::string GetPathStem() const;

        /// @brief Get data, decompressing it first (on first access) if required.
        /// @note We only check if data has been decompressed - it doesn't matter what current flags are.
        ///       It only matters what the flags were when the entry was created.
        [[nodiscard]] const std::vector<std::byte>& GetData() const;

        /// @brief Get data for serialization, compressing it first if required.
        /// @note Checks current entry flags for compression requirement.
        [[nodiscard]] const std::vector<std::byte>& GetDataForSerialization() const;

        /// @brief Set uncompressed data of entry.
        /// @note Compressed data cache will be cleared and regenerated later for serialization as needed.
        void SetData(std::vector<std::byte> decompressedData);

    private:

        // Tracks which field is currently the source of truth.
        bool m_decompressedIsSourceOfTruth = false;

        // Compressed form. Ground truth when m_decompressedIsSourceOfTruth == false; lazy cache otherwise.
        mutable std::optional<std::vector<std::byte>> m_compressedData;
        // Decompressed form. Ground truth when m_decompressedIsSourceOfTruth == true; lazy cache otherwise.
        mutable std::optional<std::vector<std::byte>> m_decompressedData;

        /// @brief Decompress `data` using `zlib`. Only called on data confirmed to be compressed.
        [[nodiscard]] static std::vector<std::byte> DecompressEntryData(const std::vector<std::byte>& data);

        /// @brief Compress `data` using `zlib`. Only called on data confirmed to be decompressed.
        [[nodiscard]] static std::vector<std::byte> CompressEntryData(const std::vector<std::byte>& data);
    };

    // --- Binder ---

    class FIRELINK_CORE_API Binder : public GameFile<Binder>
    {
    public:

        // --- ADDITIONAL FACTORIES ---

        /// @brief Parse a split BHF3/BHF4 header + BDT data pair (managed storage).
        static Ptr FromSplitBytes(
            const std::vector<std::byte>& bhdData,
            const std::vector<std::byte>& bdtData);

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

        /// @brief Find entry by ID. Throws `BinderEntryNotFoundError` if not found.
        [[nodiscard]] std::shared_ptr<BinderEntry> FindEntryByID(std::int32_t id) const;

        /// @brief Find entry by exact path. Throws `BinderEntryNotFoundError` if not found.
        [[nodiscard]] std::shared_ptr<BinderEntry> FindEntryByPath(const std::string& pathString) const;

        /// @brief Find entry by regex match to path.
        /// @note Throws `BinderEntryNotFoundError` if none match, `MultipleBinderEntriesFoundError` if more than one matches.
        [[nodiscard]] std::shared_ptr<BinderEntry> FindEntryByPathRegex(const std::string& pattern, bool fullMatch = false) const;

        /// @brief Find all entries whose paths match the given regex pattern string.
        [[nodiscard]] std::vector<std::shared_ptr<BinderEntry>> FindEntriesByPathRegex(
            const std::string& pattern, bool fullMatch = false) const;

        /// @brief Find entry by name (basename). Throws `BinderEntryNotFoundError` if not found.
        [[nodiscard]] std::shared_ptr<BinderEntry> FindEntryByName(const std::string& name) const;

        /// @brief Find entry by regex match to name (basename).
        /// @note Throws `BinderEntryNotFoundError` if none match, `MultipleBinderEntriesFoundError` if more than one matches.
        [[nodiscard]] std::shared_ptr<BinderEntry> FindEntryByNameRegex(const std::string& pattern, bool fullMatch = false) const;

        /// @brief Find all entries whose names match the given regex pattern string.
        [[nodiscard]] std::vector<std::shared_ptr<BinderEntry>> FindEntriesByNameRegex(
            const std::string& pattern, bool fullMatch = false) const;

        /// @brief Alias for entry filter function.
        using EntryFilter = std::function<bool(const BinderEntry&)>;

        /// @brief Find entry matching a filter function.
        /// @note Throws `BinderEntryNotFoundError` if none match, `MultipleBinderEntriesFoundError` if more than one matches.
        [[nodiscard]] std::shared_ptr<BinderEntry> FindEntryByFilter(const EntryFilter& filter) const;

        /// @brief Find all entries that match an entry-filtering function.
        [[nodiscard]] std::vector<std::shared_ptr<BinderEntry>> FindEntriesByFilter(const EntryFilter& filter) const;

        // --- PROPERTIES ---

        GAME_FILE_PROPERTY(BinderVersion, m_version, Version, BinderVersion::V4);
        GAME_FILE_PROPERTY(std::string, m_signature, Signature, "07D7R6");
        GAME_FILE_PROPERTY(BinderFlags, m_flags, Flags, /*default {}*/);
        GAME_FILE_PROPERTY(bool, m_isBigEndian, BigEndian, false);
        GAME_FILE_PROPERTY(bool, m_isBitBigEndian, BitBigEndian, false);
        GAME_FILE_PROPERTY(std::optional<BinderVersion4Info>, m_v4Info, V4Info, BinderVersion4Info{});
        GAME_FILE_PROPERTY_REF(std::vector<std::shared_ptr<BinderEntry>>, m_entries, Entries, /*default {}*/);

    private:

        // Version-dispatched serialization/deserialization.

        void DeserializeV3(BinaryReadWrite::BufferReader& reader, BinaryReadWrite::BufferReader& entryReader);
        void DeserializeV4(BinaryReadWrite::BufferReader& reader, BinaryReadWrite::BufferReader& entryReader);
        void SerializeBND3(BinaryReadWrite::BufferWriter& w) const;
        void SerializeBND4(BinaryReadWrite::BufferWriter& writer) const;
    };

} // namespace Firelink
