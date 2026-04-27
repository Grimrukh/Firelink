#pragma once

#include <FirelinkCore/Export.h>
#include <FirelinkCore/GameFile.h>
#include <FirelinkCore/Havok/Types.h>

#include <memory>
#include <string>

namespace Firelink
{
    /// @brief A Havok HKX file — currently supports tagfile format (DSR / Sekiro / Elden Ring).
    ///
    /// Inherits `GameFile<HKX>` for DCX handling and path-based I/O.
    ///
    /// @par Tagfile versions
    ///   - "20150100" → Dark Souls Remastered
    ///   - "20160200" → Sekiro
    ///   - "20180100" → Elden Ring
    ///
    /// @par Adding support for new types
    /// Register a `DeserializeFn` in `Havok::TagFileUnpacker::RegisterDispatch()` for any Havok
    /// type you want as a concrete C++ object. Unknown types become `Havok::HkUnknownObject`.
    class FIRELINK_CORE_API HKX : public GameFile<HKX>
    {
    public:
        // --- GameFile contract ---

        /// @brief HKX files use little-endian byte order.
        [[nodiscard]] static BinaryReadWrite::Endian GetEndian() noexcept
        {
            return BinaryReadWrite::Endian::Little;
        }

        /// @brief Deserialize from a raw buffer.
        /// Detects tagfile magic (TAG0/TCM0) and dispatches accordingly.
        /// @throws std::runtime_error if the data is not a recognised HKX tagfile.
        void Deserialize(BinaryReadWrite::BufferReader& reader);

        /// @brief Serialization stub — tagfile packing is not yet implemented.
        /// @throws std::logic_error always.
        void Serialize(BinaryReadWrite::BufferWriter& writer) const;

        // --- Data ---

        /// @brief Root level container (valid after a successful Deserialize).
        std::unique_ptr<Havok::hkRootLevelContainer> root;

        /// @brief Havok SDK version string from the file (e.g. "20180100").
        std::string hkVersion;
    };
}

