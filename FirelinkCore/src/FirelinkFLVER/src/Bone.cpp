#include <FirelinkFLVER/Bone.h>
#include <FirelinkFLVER/Encodings.h>

#include <FirelinkCore/BinaryReadWrite.h>

namespace Firelink
{
    using namespace BinaryReadWrite;

    // Write a bone struct (128 bytes). Name offset is reserved.
    void Bone::Write(BufferWriter& w, const void* scope) const
    {
        w.Write<float>(translate.x);
        w.Write<float>(translate.y);
        w.Write<float>(translate.z);
        w.Reserve<std::uint32_t>("bone_name_offset", scope);
        w.Write<float>(rotate.x);
        w.Write<float>(rotate.y);
        w.Write<float>(rotate.z);
        w.Write<std::int16_t>(parent_bone_index);
        w.Write<std::int16_t>(child_bone_index);
        w.Write<float>(scale.x);
        w.Write<float>(scale.y);
        w.Write<float>(scale.z);
        w.Write<std::int16_t>(next_sibling_bone_index);
        w.Write<std::int16_t>(previous_sibling_bone_index);
        w.Write<float>(bounding_box.min.x);
        w.Write<float>(bounding_box.min.y);
        w.Write<float>(bounding_box.min.z);
        w.Write<std::int32_t>(usage_flags);
        w.Write<float>(bounding_box.max.x);
        w.Write<float>(bounding_box.max.y);
        w.Write<float>(bounding_box.max.z);
        w.WritePad(52);
    }

    Bone Bone::Read(BufferReader& r, const bool unicode_encoding)
    {
        Bone b;
        b.translate.x = r.Read<float>();
        b.translate.y = r.Read<float>();
        b.translate.z = r.Read<float>();
        const auto name_offset = r.Read<std::uint32_t>();
        b.rotate.x = r.Read<float>();
        b.rotate.y = r.Read<float>();
        b.rotate.z = r.Read<float>();
        b.parent_bone_index = r.Read<std::int16_t>();
        b.child_bone_index = r.Read<std::int16_t>();
        b.scale.x = r.Read<float>();
        b.scale.y = r.Read<float>();
        b.scale.z = r.Read<float>();
        b.next_sibling_bone_index = r.Read<std::int16_t>();
        b.previous_sibling_bone_index = r.Read<std::int16_t>();
        b.bounding_box.min.x = r.Read<float>();
        b.bounding_box.min.y = r.Read<float>();
        b.bounding_box.min.z = r.Read<float>();
        b.usage_flags = r.Read<std::int32_t>();
        b.bounding_box.max.x = r.Read<float>();
        b.bounding_box.max.y = r.Read<float>();
        b.bounding_box.max.z = r.Read<float>();
        r.AssertPad(52);

        b.name = DecodeFLVERString(r.ReadStringAt(name_offset, unicode_encoding), unicode_encoding);
        return b;
    }
}