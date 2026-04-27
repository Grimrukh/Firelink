#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Firelink::Havok
{
    // =========================================================================
    // Runtime object base classes
    // =========================================================================

    /// @brief Polymorphic base for all deserialized Havok objects.
    struct HkObject
    {
        virtual ~HkObject() = default;
        [[nodiscard]] virtual std::string_view GetTypeName() const noexcept = 0;
    };

    /// @brief Fallback for types not yet hardcoded in C++.
    /// Preserves the raw DATA bytes of the item so the caller can inspect them.
    struct HkUnknownObject : HkObject
    {
        std::string typeName;
        std::vector<std::byte> rawData;

        HkUnknownObject() = default;
        explicit HkUnknownObject(std::string name, std::vector<std::byte> data)
            : typeName(std::move(name)), rawData(std::move(data))
        {
        }

        [[nodiscard]] std::string_view GetTypeName() const noexcept override { return typeName; }
    };

    /// @brief Base for reference-counted Havok objects (mirrors hkReferencedObject).
    /// Binary layout: offset 0 vtable (8 B), offset 8 memSizeAndRefCount (2 B),
    ///                offset 10 referenceCount (2 B), offset 12 padding (4 B). Total = 16 B.
    struct HkReferencedObject : HkObject
    {
        uint16_t memSizeAndRefCount = 0;
        uint16_t referenceCount    = 0;
    };

    // =========================================================================
    // Primitive type aliases  (match Python hk* names)
    // =========================================================================

    using hkReal    = float;
    using hkHalf16  = uint16_t;  ///< 16-bit float stored as uint16
    using hkInt8    = int8_t;
    using hkInt16   = int16_t;
    using hkInt32   = int32_t;
    using hkInt64   = int64_t;
    using hkUint8   = uint8_t;
    using hkUint16  = uint16_t;
    using hkUint32  = uint32_t;
    using hkUint64  = uint64_t;
    using hkUlong   = uint64_t;  ///< Platform-size unsigned long (8 bytes in 64-bit tagfiles)
    using hkBool    = uint8_t;

    // =========================================================================
    // Math types
    // =========================================================================

    /// @brief 4-component float vector. 16-byte aligned for SIMD.
    struct alignas(16) hkVector4
    {
        float x = 0.f, y = 0.f, z = 0.f, w = 0.f;
    };

    static_assert(sizeof(hkVector4) == 16);

    /// @brief Axis-aligned bounding box. 32 bytes (2 × hkVector4).
    struct hkAabb
    {
        hkVector4 min{};
        hkVector4 max{};
    };

    static_assert(sizeof(hkAabb) == 32);

    /// @brief 4×4 matrix stored as four hkVector4 columns. 64 bytes.
    struct hkMatrix4
    {
        hkVector4 col[4]{};
    };

    /// @brief 3×3 rotation matrix stored as three hkVector4 rows (with w padding). 48 bytes.
    struct hkMatrix3
    {
        hkVector4 row[3]{};
    };

    static_assert(sizeof(hkMatrix3) == 48);

    /// @brief Rotation matrix alias (same layout as hkMatrix3). 48 bytes.
    using hkRotation = hkMatrix3;

    /// @brief Unit quaternion. 16-byte aligned, stored as 4 floats (x, y, z, w).
    struct alignas(16) hkQuaternion
    {
        float x = 0.f, y = 0.f, z = 0.f, w = 1.f;
    };

    static_assert(sizeof(hkQuaternion) == 16);

    /// @brief Rigid-body transform: 3×4 rotation (hkRotation) + translation (hkVector4). 64 bytes.
    struct hkTransform
    {
        hkRotation rotation{};      ///< 48 bytes (3 × hkVector4 rows)
        hkVector4  translation{};   ///< 16 bytes
    };

    static_assert(sizeof(hkTransform) == 64);

    /// @brief Quaternion-scale-transform: translation + quaternion + scale. 48 bytes.
    struct hkQsTransform
    {
        hkVector4    translation{};
        hkQuaternion rotation{};
        hkVector4    scale{};
    };

    static_assert(sizeof(hkQsTransform) == 48);

    // =========================================================================
    // File-layout helper POD structs
    // These mirror the binary layout of composite members inside DATA items,
    // making it easy to read them via reinterpret_cast / ReadAt.
    // They are only used transiently during (de)serialization.
    // =========================================================================

    /// @brief Binary layout of an hkArray member inside DATA.
    /// offset 0: uint64 — 1-based ITEM INDEX of the array-element data item
    ///                     (element count = items[itemIndex].length, NOT stored here)
    /// offset 8: int32 — always 0 in tagfiles (element count is in the ITEM record)
    /// offset 12: int32 — always 0 in tagfiles
    struct HkArrayLayout
    {
        uint64_t dataPtr        = 0;  ///< 1-based item index (not a buffer offset)
        int32_t  size           = 0;  ///< unused in tagfile; count comes from item.length
        int32_t  capacityFlags  = 0;
    };

    static_assert(sizeof(HkArrayLayout) == 16);

    /// @brief Binary layout of a Ptr / hkRefPtr / hkViewPtr member inside DATA.
    /// Just an 8-byte absolute DATA offset.
    struct HkPtrLayout
    {
        uint64_t ptr = 0;
    };

    static_assert(sizeof(HkPtrLayout) == 8);

    /// @brief Binary layout of an hkRelArray member inside DATA.
    /// offset 0: element count (uint16)
    /// offset 2: forward jump in bytes from the start of this header (uint16)
    struct HkRelArrayLayout
    {
        uint16_t length = 0;
        uint16_t jump   = 0;
    };

    static_assert(sizeof(HkRelArrayLayout) == 4);

    // =========================================================================
    // Concrete runtime types
    // =========================================================================

    /// @brief Named variant stored inside hkRootLevelContainer.
    /// Binary layout per element: { namePtr(8), classNamePtr(8), variantPtr(8) } = 24 bytes.
    struct hkRootLevelContainerNamedVariant
    {
        std::string name;                     ///< hkStringPtr
        std::string className;                ///< hkStringPtr
        std::unique_ptr<HkObject> variant;    ///< hkRefVariant → any HkReferencedObject subclass

        hkRootLevelContainerNamedVariant() = default;

        // Non-copyable due to unique_ptr member.
        hkRootLevelContainerNamedVariant(const hkRootLevelContainerNamedVariant&) = delete;
        hkRootLevelContainerNamedVariant& operator=(const hkRootLevelContainerNamedVariant&) = delete;

        hkRootLevelContainerNamedVariant(hkRootLevelContainerNamedVariant&&) noexcept = default;
        hkRootLevelContainerNamedVariant& operator=(hkRootLevelContainerNamedVariant&&) noexcept = default;
    };

    /// @brief Root object present in every HKX tagfile (byte_size = 16 in binary).
    /// Binary layout: { hkArray[hkRootLevelContainerNamedVariant](at offset 0) }.
    struct hkRootLevelContainer : HkObject
    {
        static constexpr std::string_view TypeName = "hkRootLevelContainer";
        [[nodiscard]] std::string_view GetTypeName() const noexcept override { return TypeName; }

        std::vector<hkRootLevelContainerNamedVariant> namedVariants;
    };

    /// @brief Typed helper to retrieve a hkRootLevelContainer variant pointer of type T.
    template<typename T>
    T* GetVariant(hkRootLevelContainer& root)
    {
        for (auto& v : root.namedVariants)
            if (v.variant && v.variant->GetTypeName() == T::TypeName)
                return static_cast<T*>(v.variant.get());
        return nullptr;
    }

    // =========================================================================
    // Binary-layout element size constants
    // (used by TagFileUnpacker when iterating over array items in DATA)
    // =========================================================================

    inline constexpr size_t kNamedVariantElementSize = 24;  ///< 3 pointers × 8 bytes

} // namespace Firelink::Havok

