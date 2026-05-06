#pragma once

#include <FirelinkCore/Havok/HKX.h>

#include <string_view>

namespace Firelink::Havok::EldenRing
{
    class EldenRingHKX : public HKX
    {
    protected:

        static constexpr std::string_view REQUIRED_HK_VERSION = "20180100";

        /// @brief Register Elden Ring Havok types in TagfileUnpacker.
        TagFileUnpacker CreateTagfileUnpacker() const noexcept override;

        /// @brief Optional override for subclasses to assert their Havok version.
        std::string_view RequiredHKVersion() const noexcept override { return REQUIRED_HK_VERSION; }
    };
}
