#include <FirelinkFLVER/Dummy.h>

#include <FirelinkCore/BinaryReadWrite.h>

#include <cstdint>

namespace Firelink
{
    using namespace BinaryReadWrite;

    void Dummy::Write(BufferWriter& w, const FLVERVersion version) const
    {
        w.Write<float>(translate.x);
        w.Write<float>(translate.y);
        w.Write<float>(translate.z);
        if (version == FLVERVersion::DarkSouls2)
        {
            // This one version stores color as BGRA on disk, but we store as RGBA in memory.
            w.Write<std::uint8_t>(color.b);
            w.Write<std::uint8_t>(color.g);
            w.Write<std::uint8_t>(color.r);
            w.Write<std::uint8_t>(color.a);
        }
        else
        {
            w.Write<std::uint8_t>(color.r);
            w.Write<std::uint8_t>(color.g);
            w.Write<std::uint8_t>(color.b);
            w.Write<std::uint8_t>(color.a);
        }
        w.Write<float>(forward.x);
        w.Write<float>(forward.y);
        w.Write<float>(forward.z);
        w.Write<std::int16_t>(reference_id);
        w.Write<std::int16_t>(parent_bone_index);
        w.Write<float>(upward.x);
        w.Write<float>(upward.y);
        w.Write<float>(upward.z);
        w.Write<std::int16_t>(attach_bone_index);
        w.Write<std::uint8_t>(follows_attach_bone ? 1 : 0);
        w.Write<std::uint8_t>(use_upward_vector ? 1 : 0);
        w.Write<std::int32_t>(unk_x30);
        w.Write<std::int32_t>(unk_x34);
        w.WritePad(8);
    }

    Dummy Dummy::Read(BufferReader& r, const FLVERVersion version)
    {
        Dummy d;
        d.translate.x = r.Read<float>();
        d.translate.y = r.Read<float>();
        d.translate.z = r.Read<float>();
        if (version == FLVERVersion::DarkSouls2)
        {
            // This one version stores color as BGRA.
            d.color.b = r.Read<std::uint8_t>();
            d.color.g = r.Read<std::uint8_t>();
            d.color.r = r.Read<std::uint8_t>();
            d.color.a = r.Read<std::uint8_t>();
        }
        else
        {
            d.color.r = r.Read<std::uint8_t>();
            d.color.g = r.Read<std::uint8_t>();
            d.color.b = r.Read<std::uint8_t>();
            d.color.a = r.Read<std::uint8_t>();
        }
        d.forward.x = r.Read<float>();
        d.forward.y = r.Read<float>();
        d.forward.z = r.Read<float>();
        d.reference_id = r.Read<std::int16_t>();
        d.parent_bone_index = r.Read<std::int16_t>();
        d.upward.x = r.Read<float>();
        d.upward.y = r.Read<float>();
        d.upward.z = r.Read<float>();
        d.attach_bone_index = r.Read<std::int16_t>();
        d.follows_attach_bone = r.Read<std::uint8_t>() != 0;
        d.use_upward_vector = r.Read<std::uint8_t>() != 0;
        d.unk_x30 = r.Read<std::int32_t>();
        d.unk_x34 = r.Read<std::int32_t>();
        r.AssertPad(8);
        return d;
    }
}
