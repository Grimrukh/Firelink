#pragma once

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/Havok/Helpers.h>
#include <FirelinkCore/Havok/Types.h>

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Firelink::Havok
{

    /// @brief Deserializes a Havok "tagfile" (TAG0/TCM0 format, used from DSR onwards) from a BufferReader.
    ///
    /// @par Usage
    /// @code
    ///   TagFileUnpacker unpacker;
    ///   unpacker.Unpack(reader);
    ///   auto& container = *unpacker.root;
    /// @endcode
    ///
    /// @par Adding concrete types
    /// Register a `DeserializeFn` in `RegisterDispatch()` (Tagfile.cpp) for any Havok type you want
    /// as a typed C++ object. Types without a registered deserializer fall back to `HkUnknownObject`,
    /// which preserves the raw item bytes.
    class TagFileUnpacker
    {
    public:
        // ----- Type alias -------------------------------------------------------

        /// @brief Signature for a per-type deserializer registered in m_dispatch.
        /// Receives the unpacker (for helper access) and the item to deserialize.
        using DeserializeFn = std::function<
            std::unique_ptr<HkObject>(TagFileUnpacker&, const TagFileItem&)>;

        // -----------------------------------------------------------------------

        TagFileUnpacker();

        /// @brief Parse and fully deserialize a tagfile from `reader`.
        /// On return, `root` holds the `hkRootLevelContainer` and `hkVersion` is set.
        void Unpack(BinaryReadWrite::BufferReader& reader);

        // --- Results (valid after Unpack()) ---

        /// @brief Deserialized root level container.
        std::unique_ptr<hkRootLevelContainer> root;

        /// @brief Havok SDK version string from SDKV section (e.g. "20180100").
        std::string hkVersion;

        /// @brief True when the file is a compendium (TCM0) rather than a regular object file.
        bool isCompendium = false;

        /// @brief Parsed type table (1-indexed; entry 0 is a null sentinel).
        /// Exposed for inspection; not needed for normal usage.
        std::vector<TypeInfo> typeInfos;

        /// @brief Parsed item table (1-indexed; entry 0 is a null sentinel).
        std::vector<TagFileItem> items;

        // --- Public helpers usable by generated deserializers ---

        /// @brief Register a custom deserializer for the given Havok type name.
        /// Generated RegisterXxxDispatch() functions call this.
        void Register(std::string typeName, DeserializeFn fn)
        {
            m_dispatch[std::move(typeName)] = std::move(fn);
        }

        /// @brief Follow an 8-byte item-index pointer in DATA and deserialize the target.
        /// Returns nullptr for null/invalid indices.
        std::unique_ptr<HkObject> FollowObjectPtr(uint64_t itemIdx);

        /// @brief Follow a string-item-index pointer in DATA and return the string contents.
        std::string FollowStringPtr(uint64_t itemIdx) const;

        /// @brief Read a trivially-copyable value T at an absolute DATA offset.
        template<typename T>
        T ReadPodAt(size_t absOffset) const
        {
            T v{};
            std::memcpy(&v, m_reader->RawAt(absOffset), sizeof(T));
            return v;
        }

        /// @brief Read an hkArray of plain-old-data structs.
        /// @param arrLayoutAbsOffset  Absolute DATA offset of the 16-byte HkArrayLayout.
        /// @return Vector populated by bulk-copying from the array item's DATA bytes.
        template<typename T>
        std::vector<T> ReadPodArray(size_t arrLayoutAbsOffset) const
        {
            const HkArrayLayout layout = ReadArrayLayout(arrLayoutAbsOffset);
            const int idx = static_cast<int>(layout.dataPtr);
            if (idx <= 0 || idx >= static_cast<int>(items.size()))
                return {};
            const TagFileItem& arrItem = items[idx];
            const int count = arrItem.length;
            if (count <= 0)
                return {};
            std::vector<T> result(static_cast<size_t>(count));
            std::memcpy(result.data(),
                        m_reader->RawAt(arrItem.absoluteDataOffset),
                        static_cast<size_t>(count) * sizeof(T));
            return result;
        }

    private:

        // ----- State during Unpack() --------------------------------------------

        BinaryReadWrite::BufferReader* m_reader = nullptr;

        size_t m_dataStart = 0;   ///< absolute reader offset of the first DATA byte

        /// Maps absolute DATA offsets → item index (for pointer resolution).
        std::unordered_map<size_t, int> m_itemByOffset;

        /// Dispatch table: Havok type name → deserializer.
        std::unordered_map<std::string, DeserializeFn> m_dispatch;

        /// Tracks item indices currently being deserialized (cycle detection for hkViewPtr).
        std::unordered_set<int> m_inProgress;

        // ----- Parsing phases ---------------------------------------------------

        void ParseSdkvSection();
        void SkipDataSection();
        void ParseTypeSection();
        void ParseIndexSection();

        // ----- Deserialization --------------------------------------------------

        /// @brief Deserialize item at `idx` using the dispatch table.
        /// Returns `HkUnknownObject` for unregistered types.
        std::unique_ptr<HkObject> DeserializeItem(int idx);


        /// @brief Read the hkArray header (ptr + size + cap) at `absOffset` within DATA.
        HkArrayLayout ReadArrayLayout(size_t absOffset) const;

        /// @brief Read a uint64 from an absolute DATA offset without advancing main cursor.
        uint64_t ReadU64At(size_t absOffset) const;

        // ----- Concrete deserializers (called via m_dispatch) -------------------

        void RegisterDispatch();

        std::unique_ptr<HkObject> DeserializeRootLevelContainer(const TagFileItem& item);
    };

} // namespace Firelink::Havok

