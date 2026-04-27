#include <doctest/doctest.h>

#include <FirelinkERHavok/EldenRingHKX.h>
#include <FirelinkCore/Logging.h>
#include <FirelinkTestHelpers.h>

#include <FirelinkERHavok/Types/hkcd.h>
#include <FirelinkERHavok/Types/hknp.h>

using namespace Firelink;
using namespace Firelink::Havok;
using namespace Firelink::Havok::EldenRing;

TEST_CASE("Collision: Load Chapel of Anticipation hi collision 001000 without error")
{
    EldenRingHKX::Ptr hkx = EldenRingHKX::FromPath(GetResourcePath("eldenring/h10_01_00_00_001000.hkx.dcx"));
    auto& namedVariants = hkx->GetRoot()->namedVariants;
    Info("HKX named variants count: {}", namedVariants.size());

    for (const auto& v : namedVariants)
    {
        Info("Variant name / className: {}, {}", v.name, v.className);
    }
}