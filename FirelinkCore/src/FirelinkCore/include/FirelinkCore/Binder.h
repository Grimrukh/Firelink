// Binder (BND3/BND4) archive format for FromSoftware file formats.
//
// Binders are multi-file archives that contain entries with IDs, paths, and data.
// Two versions exist: V3 (pre-DS2) and V4 (DS2+).
// Files are usually DCX-compressed on disk.

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
    // --- BinderError ---

    class BinderError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
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
            auto pos = path.find_last_of("\\/");
            return (pos == std::string::npos) ? path : path.substr(pos + 1);
        }
    };

    // --- Binder ---

    class FIRELINK_CORE_API Binder
    {
    public:
        BinderVersion version = BinderVersion::V4;
        std::string signature = "07D7R6";
        BinderFlags flags;
        bool big_endian = false;
        bool bit_big_endian = false;
        std::optional<BinderVersion4Info> v4_info = BinderVersion4Info{};

        std::vector<BinderEntry> entries;

        /// @brief Parse a BND3 or BND4 archive from raw (already decompressed) bytes.
        static Binder FromBytes(const std::byte* data, std::size_t size);

        /// @brief Parse a split BHF3/BHF4 header + BDT data pair.
        static Binder FromSplitBytes(
            const std::byte* bhd_data, std::size_t bhd_size,
            const std::byte* bdt_data, std::size_t bdt_size);

        /// @brief Serialize this Binder back to BND3 or BND4 bytes.
        [[nodiscard]] std::vector<std::byte> ToBytes() const;

        /// @brief Get entry count.
        [[nodiscard]] std::size_t EntryCount() const { return entries.size(); }

        /// @brief Find entry by ID. Returns nullptr if not found.
        [[nodiscard]] const BinderEntry* FindEntryByID(std::int32_t id) const;
        [[nodiscard]] BinderEntry* FindEntryByID(std::int32_t id);

        /// @brief Find entry by name (basename). Returns nullptr if not found.
        [[nodiscard]] const BinderEntry* FindEntryByName(const std::string& name) const;
        [[nodiscard]] BinderEntry* FindEntryByName(const std::string& name);

    private:
        static Binder ReadV3(const std::byte* data, std::size_t size,
                             const std::byte* entry_data, std::size_t entry_data_size);
        static Binder ReadV4(const std::byte* data, std::size_t size,
                             const std::byte* entry_data, std::size_t entry_data_size);
        [[nodiscard]] std::vector<std::byte> WriteBND3() const;
        [[nodiscard]] std::vector<std::byte> WriteBND4() const;
    };

} // namespace Firelink

