#pragma once

#include <FirelinkCore/Collections.h>

#include <cstdint>
#include <string>

namespace Firelink
{
    namespace BinaryReadWrite
    {
        class BufferReader;
        class BufferWriter;
    }

    // --- BoneUsageFlags ----------------------------------------------------
    // Bit flags on Bone.usage_flags. "UNUSED" marks a bone that only exists
    // for skeleton hierarchy reasons; used bones have usage_flags == 0.
    namespace BoneUsageFlags
    {
        constexpr std::uint32_t UNUSED = 0b0001; // 1
        constexpr std::uint32_t DUMMY = 0b0010; // 2
        constexpr std::uint32_t cXXXX = 0b0100; // 4 — references another skeleton
        constexpr std::uint32_t MESH = 0b1000; // 8
    }

    struct Bone
    {
        std::string name;
        Vector3 translate = Vector3::Zero();
        EulerRad rotate = EulerRad::Zero();
        Vector3 scale = Vector3::One();
        AABB bounding_box{};
        std::int32_t usage_flags = 0;

        // Related bone indices.
        std::int16_t parent_bone_index = -1;
        std::int16_t child_bone_index = -1;
        std::int16_t next_sibling_bone_index = -1;
        std::int16_t previous_sibling_bone_index = -1;

        bool operator==(const Bone&) const = default;

        void Write(BinaryReadWrite::BufferWriter& w, const void* scope) const;

        static Bone Read(BinaryReadWrite::BufferReader& r, bool unicode_encoding);
    };
} // namespace Firelink
