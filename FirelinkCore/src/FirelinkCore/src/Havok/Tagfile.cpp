#include <FirelinkCore/Havok/Tagfile.h>
#include <FirelinkCore/Havok/HkaiTypes.h>
#include <FirelinkCore/Havok/HkcdTypes.h>
#include <FirelinkCore/Logging.h>

#include <cstring>
#include <stdexcept>

namespace Firelink::Havok
{

    // =========================================================================
    // Constructor / dispatch setup
    // =========================================================================

    TagFileUnpacker::TagFileUnpacker()
    {
        RegisterDispatch();
    }

    void TagFileUnpacker::RegisterDispatch()
    {
        // Built-in type
        m_dispatch["hkRootLevelContainer"] =
            [](TagFileUnpacker& u, const TagFileItem& item) -> std::unique_ptr<HkObject>
            {
                return u.DeserializeRootLevelContainer(item);
            };

        // Generated type sets
        RegisterHkaiDispatch(*this);
        RegisterHkcdDispatch(*this);
    }

    // =========================================================================
    // Top-level Unpack
    // =========================================================================

    void TagFileUnpacker::Unpack(BinaryReadWrite::BufferReader& reader)
    {
        m_reader = &reader;

        // --- Outer container: TAG0 (object file) or TCM0 (compendium) ----------
        const auto rootSection = EnterSection(reader, "TAG0", "TCM0");

        // Check the actual magic that was consumed (4 bytes immediately before current pos).
        const bool isTag0 = (std::memcmp(
            reader.RawAt(reader.Position() - 4), "TAG0", 4) == 0);

        if (!isTag0)
        {
            isCompendium = true;
            throw std::runtime_error(
                "HKX tagfile: compendium (TCM0) files are not yet supported.");
        }

        isCompendium = false;

        // --- SDKV ---------------------------------------------------------------
        ParseSdkvSection();

        // --- DATA ---------------------------------------------------------------
        SkipDataSection();

        // --- TYPE ---------------------------------------------------------------
        ParseTypeSection();

        // --- INDX ---------------------------------------------------------------
        ParseIndexSection();

        // --- Deserialize root item (always item[1]) ----------------------------
        if (items.size() < 2 || items[1].IsNull())
            throw std::runtime_error("HKX tagfile: missing root item (index 1)");

        const std::string& rootTypeName = typeInfos[items[1].typeIndex].name;
        if (rootTypeName != "hkRootLevelContainer")
        {
            Warning(
                std::format("HKX tagfile: root item is '{}', expected 'hkRootLevelContainer'",
                        rootTypeName));
        }

        auto rootObj = DeserializeItem(1);
        root.reset(static_cast<hkRootLevelContainer*>(rootObj.release()));

        reader.Seek(rootSection.end);
    }

    // =========================================================================
    // Parsing phases
    // =========================================================================

    void TagFileUnpacker::ParseSdkvSection()
    {
        const auto sec = EnterSection(*m_reader, "SDKV");
        char versionBuf[9] = {};
        m_reader->ReadRaw(versionBuf, 8);
        hkVersion = std::string(versionBuf, 8);
        m_reader->Seek(sec.end);
    }

    void TagFileUnpacker::SkipDataSection()
    {
        const auto sec = EnterSection(*m_reader, "DATA");
        m_dataStart = sec.start;
        m_reader->Seek(sec.end);
    }

    void TagFileUnpacker::ParseTypeSection()
    {
        const auto typeSection = EnterSection(*m_reader, "TYPE", "TCRF");

        // Detect TCRF by checking the magic bytes we just consumed.
        {
            const char* magic = reinterpret_cast<const char*>(
                m_reader->RawAt(typeSection.start - 4));
            if (std::memcmp(magic, "TCRF", 4) == 0)
            {
                throw std::runtime_error(
                    "HKX tagfile: TCRF (compendium-referenced) type sections are not yet supported.");
            }
        }

        // --- Optional TPTR (unused pointer table, skip) ---
        if (PeekSection(*m_reader, "TPTR"))
        {
            const auto s = EnterSection(*m_reader, "TPTR");
            m_reader->Seek(s.end);
        }

        // --- TSTR: null-separated type name strings ---
        std::vector<std::string> typeNames;
        {
            const auto sec = EnterSection(*m_reader, "TSTR");
            const size_t sz = sec.DataSize();
            std::string blob(sz, '\0');
            m_reader->ReadRaw(blob.data(), sz);

            size_t pos = 0;
            while (pos < sz)
            {
                const size_t end = blob.find('\0', pos);
                if (end == std::string::npos)
                {
                    typeNames.push_back(blob.substr(pos));
                    break;
                }
                typeNames.push_back(blob.substr(pos, end - pos));
                pos = end + 1;
            }
            m_reader->Seek(sec.end);
        }

        // --- TNAM / TNA1: type names + templates ---
        {
            const auto sec = EnterSection(*m_reader, "TNAM", "TNA1");
            const size_t fileTypeCount = static_cast<size_t>(ReadVarInt(*m_reader));

            typeInfos.clear();
            typeInfos.resize(1);  // index 0 = null sentinel

            for (size_t i = 1; i < fileTypeCount; ++i)
            {
                const int    nameIdx       = static_cast<int>(ReadVarInt(*m_reader));
                const size_t templateCount = static_cast<size_t>(ReadVarInt(*m_reader));

                TypeInfo ti;
                ti.name = (nameIdx >= 0 && nameIdx < static_cast<int>(typeNames.size()))
                           ? typeNames[nameIdx] : "";

                for (size_t t = 0; t < templateCount; ++t)
                {
                    TemplateInfo tmpl;
                    const int tmplNameIdx = static_cast<int>(ReadVarInt(*m_reader));
                    tmpl.name  = (tmplNameIdx >= 0 && tmplNameIdx < static_cast<int>(typeNames.size()))
                                  ? typeNames[tmplNameIdx] : "";
                    tmpl.value = static_cast<int>(ReadVarInt(*m_reader));
                    ti.templates.push_back(std::move(tmpl));
                }

                typeInfos.push_back(std::move(ti));
            }

            m_reader->Seek(sec.end);
        }

        // --- FSTR: null-separated member name strings ---
        std::vector<std::string> memberNames;
        {
            const auto sec = EnterSection(*m_reader, "FSTR");
            const size_t sz = sec.DataSize();
            std::string blob(sz, '\0');
            m_reader->ReadRaw(blob.data(), sz);

            size_t pos = 0;
            while (pos < sz)
            {
                const size_t end = blob.find('\0', pos);
                if (end == std::string::npos)
                {
                    memberNames.push_back(blob.substr(pos));
                    break;
                }
                memberNames.push_back(blob.substr(pos, end - pos));
                pos = end + 1;
            }
            m_reader->Seek(sec.end);
        }

        // --- TBOD / TBDY: type body ---
        {
            const auto sec = EnterSection(*m_reader, "TBOD", "TBDY");

            while (m_reader->Position() < sec.end)
            {
                const int typeIdx = static_cast<int>(ReadVarInt(*m_reader));
                if (typeIdx == 0)
                    continue;

                if (typeIdx >= static_cast<int>(typeInfos.size()))
                    throw std::runtime_error(
                        "HKX tagfile: TBOD type index " + std::to_string(typeIdx) + " out of range");

                TypeInfo& ti = typeInfos[typeIdx];
                ti.parentTypeIndex = static_cast<int>(ReadVarInt(*m_reader));
                ti.tagFormatFlags  = static_cast<uint32_t>(ReadVarInt(*m_reader));

                const auto tff = static_cast<TagFormatFlags>(ti.tagFormatFlags);
                uint32_t tagTypeFlags = 0;

                if (HasFlag(tff, TagFormatFlags::SubType))
                {
                    tagTypeFlags   = static_cast<uint32_t>(ReadVarInt(*m_reader));
                    ti.tagTypeFlags = tagTypeFlags;
                }

                if (HasFlag(tff, TagFormatFlags::Pointer) && (tagTypeFlags & 0xFu) >= 6u)
                    ti.pointerTypeIndex = static_cast<int>(ReadVarInt(*m_reader));

                if (HasFlag(tff, TagFormatFlags::Version))
                    ti.version = static_cast<uint32_t>(ReadVarInt(*m_reader));

                if (HasFlag(tff, TagFormatFlags::ByteSize))
                {
                    ti.byteSize  = static_cast<uint32_t>(ReadVarInt(*m_reader));
                    ti.alignment = static_cast<uint32_t>(ReadVarInt(*m_reader));
                }

                if (HasFlag(tff, TagFormatFlags::AbstractValue))
                    ti.abstractValue = static_cast<uint32_t>(ReadVarInt(*m_reader));

                if (HasFlag(tff, TagFormatFlags::Members))
                {
                    const size_t memberCount = static_cast<size_t>(ReadVarInt(*m_reader));
                    ti.members.resize(memberCount);

                    for (size_t m = 0; m < memberCount; ++m)
                    {
                        MemberInfo& mi   = ti.members[m];
                        const int mnIdx  = static_cast<int>(ReadVarInt(*m_reader));
                        mi.name      = (mnIdx >= 0 && mnIdx < static_cast<int>(memberNames.size()))
                                        ? memberNames[mnIdx] : "";
                        mi.flags     = static_cast<uint32_t>(ReadVarInt(*m_reader));
                        mi.offset    = static_cast<uint32_t>(ReadVarInt(*m_reader));
                        mi.typeIndex = static_cast<int>(ReadVarInt(*m_reader));
                    }
                }

                if (HasFlag(tff, TagFormatFlags::Interfaces))
                {
                    const size_t ifCount = static_cast<size_t>(ReadVarInt(*m_reader));
                    ti.interfaces.resize(ifCount);
                    for (size_t k = 0; k < ifCount; ++k)
                    {
                        ti.interfaces[k].typeIndex = static_cast<int>(ReadVarInt(*m_reader));
                        ti.interfaces[k].flags     = static_cast<uint32_t>(ReadVarInt(*m_reader));
                    }
                }

                if (HasFlag(tff, TagFormatFlags::Unknown))
                    throw std::runtime_error(
                        "HKX tagfile: type '" + ti.name
                        + "' has unsupported TagFormatFlags bit 0x80");
            }

            m_reader->Seek(sec.end);
        }

        // --- Optional THSH: type hashes ---
        if (PeekSection(*m_reader, "THSH"))
        {
            const auto sec = EnterSection(*m_reader, "THSH");
            const size_t hashCount = static_cast<size_t>(ReadVarInt(*m_reader));

            for (size_t h = 0; h < hashCount; ++h)
            {
                const int      idx = static_cast<int>(ReadVarInt(*m_reader));
                const uint32_t hsh = m_reader->Read<uint32_t>();
                if (idx > 0 && idx < static_cast<int>(typeInfos.size()))
                    typeInfos[idx].hsh = hsh;
            }

            m_reader->Seek(sec.end);
        }

        // --- Optional TPAD (ignored) ---
        if (PeekSection(*m_reader, "TPAD"))
        {
            const auto s = EnterSection(*m_reader, "TPAD");
            m_reader->Seek(s.end);
        }

        m_reader->Seek(typeSection.end);
    }

    void TagFileUnpacker::ParseIndexSection()
    {
        const auto indxSection = EnterSection(*m_reader, "INDX");

        // --- ITEM ---
        {
            const auto sec = EnterSection(*m_reader, "ITEM");

            items.clear();
            // NOTE: Do NOT pre-add a null sentinel here. The file's first ITEM record is always
            // itemInfo==0 (the null item), which naturally becomes items[0]. Following items are
            // at indices 1, 2, … matching the 1-indexed convention used everywhere else.

            while (m_reader->Position() < sec.end)
            {
                const uint32_t itemInfo  = m_reader->Read<uint32_t>();
                const uint32_t relOffset = m_reader->Read<uint32_t>();
                const uint32_t length    = m_reader->Read<uint32_t>();

                if (itemInfo == 0)
                {
                    items.push_back({});  // null item → becomes index 0
                    continue;
                }

                    const int  typeIndex = static_cast<int>(itemInfo & 0x00FFFFFFu);
                    const bool isPtr     = ((itemInfo >> 24) & 0x10u) != 0u;

                    TagFileItem item;
                    item.typeIndex          = typeIndex;
                    item.absoluteDataOffset = m_dataStart + relOffset;
                    item.length             = static_cast<int>(length);
                    item.isPtr              = isPtr;

                    const int idx = static_cast<int>(items.size());
                    m_itemByOffset[item.absoluteDataOffset] = idx;
                    items.push_back(item);
            }

            m_reader->Seek(sec.end);
        }

        // --- PTCH (skip, not needed for reading) ---
        if (PeekSection(*m_reader, "PTCH"))
        {
            const auto s = EnterSection(*m_reader, "PTCH");
            m_reader->Seek(s.end);
        }

        m_reader->Seek(indxSection.end);
    }

    // =========================================================================
    // Deserialization helpers
    // =========================================================================

    uint64_t TagFileUnpacker::ReadU64At(const size_t absOffset) const
    {
        return m_reader->ReadAt<uint64_t>(absOffset);
    }

    HkArrayLayout TagFileUnpacker::ReadArrayLayout(const size_t absOffset) const
    {
        HkArrayLayout layout;
        layout.dataPtr       = m_reader->ReadAt<uint64_t>(absOffset);
        layout.size          = m_reader->ReadAt<int32_t> (absOffset + 8);
        layout.capacityFlags = m_reader->ReadAt<int32_t> (absOffset + 12);
        return layout;
    }

    std::string TagFileUnpacker::FollowStringPtr(const uint64_t itemIdx) const
    {
        // In HKX tagfiles the DATA "pointer" field is a 1-based ITEM INDEX,
        // not a buffer offset.
        if (itemIdx == 0 || itemIdx >= static_cast<uint64_t>(items.size()))
            return {};

        const TagFileItem& strItem = items[static_cast<int>(itemIdx)];
        if (strItem.IsNull())
            return {};

        const auto* raw = reinterpret_cast<const char*>(
            m_reader->RawAt(strItem.absoluteDataOffset));
        return std::string(raw);
    }

    std::unique_ptr<HkObject> TagFileUnpacker::FollowObjectPtr(const uint64_t itemIdx)
    {
        // Same: DATA "pointer" field is a 1-based ITEM INDEX.
        if (itemIdx == 0)
            return nullptr;

        return DeserializeItem(static_cast<int>(itemIdx));
    }

    std::unique_ptr<HkObject> TagFileUnpacker::DeserializeItem(const int idx)
    {
        if (idx <= 0 || idx >= static_cast<int>(items.size()))
            return nullptr;

        const TagFileItem& item = items[idx];
        if (item.IsNull())
            return nullptr;

        // Cycle guard (hkViewPtr back-references).
        if (m_inProgress.contains(idx))
            return nullptr;

        if (item.typeIndex <= 0 || item.typeIndex >= static_cast<int>(typeInfos.size()))
            return nullptr;

        const std::string& typeName = typeInfos[item.typeIndex].name;

        const auto dispatchIt = m_dispatch.find(typeName);
        if (dispatchIt != m_dispatch.end())
        {
            m_inProgress.insert(idx);
            auto obj = dispatchIt->second(*this, item);
            m_inProgress.erase(idx);
            return obj;
        }

        // Fallback: copy raw bytes into HkUnknownObject.
        const uint32_t elemSize = typeInfos[item.typeIndex].byteSize;
        const size_t rawSize = (elemSize > 0)
            ? static_cast<size_t>(item.length) * elemSize
            : 0;

        std::vector<std::byte> raw(rawSize);
        if (rawSize > 0)
        {
            std::memcpy(raw.data(),
                        m_reader->RawAt(item.absoluteDataOffset),
                        rawSize);
        }

        return std::make_unique<HkUnknownObject>(typeName, std::move(raw));
    }

    // =========================================================================
    // Concrete deserializers
    // =========================================================================

    std::unique_ptr<HkObject> TagFileUnpacker::DeserializeRootLevelContainer(
        const TagFileItem& item)
    {
        auto result = std::make_unique<hkRootLevelContainer>();

        // hkRootLevelContainer binary layout:
        //   offset 0: uint64 → 1-based ITEM INDEX of the namedVariants array data
        //             (the array element count is items[itemIndex].length, NOT stored in DATA)

        const auto arrItemIdx = static_cast<int>(
            m_reader->ReadAt<uint64_t>(item.absoluteDataOffset));

        if (arrItemIdx <= 0 || arrItemIdx >= static_cast<int>(items.size()))
            return result;

        const TagFileItem& arrItem = items[arrItemIdx];
        const int count = arrItem.length;

        if (count <= 0)
            return result;

        // hkRootLevelContainerNamedVariant binary layout per element (24 bytes):
        //   offset  0: namePtr      (uint64 → 1-based item index of a string item)
        //   offset  8: classNamePtr (uint64 → 1-based item index of a string item)
        //   offset 16: variantPtr   (uint64 → 1-based item index of hkReferencedObject)

        for (int i = 0; i < count; ++i)
        {
            const size_t base = arrItem.absoluteDataOffset
                              + static_cast<size_t>(i) * kNamedVariantElementSize;

            const uint64_t namePtr      = ReadU64At(base + 0);
            const uint64_t classNamePtr = ReadU64At(base + 8);
            const uint64_t variantPtr   = ReadU64At(base + 16);

            hkRootLevelContainerNamedVariant v;
            v.name      = FollowStringPtr(namePtr);
            v.className = FollowStringPtr(classNamePtr);
            v.variant   = FollowObjectPtr(variantPtr);

            result->namedVariants.push_back(std::move(v));
        }

        return result;
    }

} // namespace Firelink::Havok
