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

    // =========================================================================
    // Primitive type aliases
    // =========================================================================

    using hkReal      = float;
    using hkUFloat8   = uint8_t;  ///< 8-bit float stored as uint8
    using hkHalf16    = uint16_t;  ///< 16-bit float stored as uint16
    using hkInt8      = int8_t;
    using hkInt16     = int16_t;
    using hkInt32     = int32_t;
    using hkInt64     = int64_t;
    using hkUint8     = uint8_t;
    using hkUint16    = uint16_t;
    using hkUint32    = uint32_t;
    using hkUint64    = uint64_t;
    using hkBool      = uint8_t;
    // NOTE: `hkUlong` is platform-sized, so we can read/write 32-bit HKX files.

    using hkStringPtr           = std::string;  ///< const char* that we store as string
    using hkReflectDetailOpaque = void;  ///< no data

    // =========================================================================
    // Math types
    // =========================================================================

    /// @brief 4-component float vector. 16-byte aligned for SIMD.
    struct alignas(16) hkVector4f
    {
        float x = 0.f, y = 0.f, z = 0.f, w = 0.f;
    };
    static_assert(sizeof(hkVector4f) == 16);
    using hkVector4 = hkVector4f;
    using hkQuaternion = hkVector4f;
    using hkQuaternionf = hkQuaternion;

    /// @brief Axis-aligned bounding box. 32 bytes (2 × hkVector4).
    struct hkAabb
    {
        hkVector4 min{};
        hkVector4 max{};
    };

    static_assert(sizeof(hkAabb) == 32);

    /// @brief 4×4 matrix stored as four hkVector4 columns. 64 bytes.
    struct hkMatrix4Impl
    {
        hkVector4 col[4]{};
    };
    using hkMatrix4f = hkMatrix4Impl;
    using hkMatrix4 = hkMatrix4f;

    /// @brief 3×3 matrix stored as three hkVector4 rows (with w padding). 48 bytes.
    struct hkMatrix3Impl
    {
        hkVector4 row[3]{};
    };
    static_assert(sizeof(hkMatrix3Impl) == 48);
    using hkMatrix3f = hkMatrix3Impl;
    using hkMatrix3 = hkMatrix3f;
    using hkRotationImpl = hkMatrix3;  // technically a different type, but identical
    using hkRotationf = hkRotationImpl;
    using hkRotation = hkRotationf;

    /// @brief Rigid-body transform: 3×4 rotation (hkRotation) + translation (hkVector4). 64 bytes.
    struct hkTransformf
    {
        hkRotationf rotation{};      ///< 48 bytes (3 × hkVector4 rows)
        hkVector4f  translation{};   ///< 16 bytes
    };
    static_assert(sizeof(hkTransformf) == 64);
    using hkTransform = hkTransformf;

    /// @brief Quaternion-scale-transform: translation + quaternion + scale. 48 bytes.
    struct hkQsTransformf
    {
        hkVector4f    translation{};
        hkQuaternionf rotation{};
        hkVector4f    scale{};
    };
    static_assert(sizeof(hkQsTransformf) == 48);
    using hkQsTransform = hkQsTransformf;

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
        hkStringPtr name;
        hkStringPtr className;
        std::shared_ptr<HkObject> variant;      ///< hkRefVariant — shared; the same object may be
                                                ///< referenced by multiple variants or refptrs.
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

