#pragma once

#include <FirelinkFLVER/Version.h>

#include <FirelinkCore/Collections.h>

#include <cstdint>

namespace Firelink
{
    namespace BinaryReadWrite
    {
        class BufferReader;
        class BufferWriter;
    }

    struct Dummy
    {
        Vector3 translate{};
        ColorRGBA color{}; // stored in on-disk byte order
        Vector3 forward{};
        Vector3 upward{};
        std::int16_t reference_id = 0;
        std::int16_t parent_bone_index = -1;
        std::int16_t attach_bone_index = -1;
        bool follows_attach_bone = false;
        bool use_upward_vector = false;
        std::int32_t unk_x30 = 0;
        std::int32_t unk_x34 = 0;

        bool operator==(const Dummy&) const = default;

        void Write(BinaryReadWrite::BufferWriter& w, FLVERVersion version) const;

        static Dummy Read(BinaryReadWrite::BufferReader& r, FLVERVersion version);
    };

} // namespace Firelink
