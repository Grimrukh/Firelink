#include <FirelinkERHavok/EldenRingHKX.h>

#include <FirelinkERHavok/Types/base.h>
#include <FirelinkERHavok/Types/hcl.h>
#include <FirelinkERHavok/Types/hka.h>
#include <FirelinkERHavok/Types/hkai.h>
#include <FirelinkERHavok/Types/hkb.h>
#include <FirelinkERHavok/Types/hkcd.h>
#include <FirelinkERHavok/Types/hknp.h>
#include <FirelinkERHavok/Types/hkx.h>

#include <FirelinkCore/Havok/TagfileUnpacker.h>

namespace Firelink::Havok::EldenRing
{
    TagFileUnpacker EldenRingHKX::CreateTagfileUnpacker() const noexcept
    {
        TagFileUnpacker unpacker;
        RegisterBaseDispatch(unpacker);
        RegisterHclDispatch(unpacker);
        RegisterHkaDispatch(unpacker);
        RegisterHkaiDispatch(unpacker);
        RegisterHkbDispatch(unpacker);
        RegisterHkcdDispatch(unpacker);
        RegisterHknpDispatch(unpacker);
        RegisterHkxDispatch(unpacker);
        return unpacker;
    }
}
