#pragma once

#include "FirelinkER/Export.h"
#include "EntryParam.h"
#include "Enums.h"
#include "Region.h"

#define CASE_MAKE_UNIQUE(ENUM_TYPE) \
    case static_cast<int>(RegionType::ENUM_TYPE): \
        newRegion = std::make_unique<ENUM_TYPE##Region>(); \
        break;


namespace FirelinkER::Maps::MapStudio
{
    class FIRELINKER_API RegionParam final : public EntryParam<Region>
    {
    public:
        RegionParam() : EntryParam(73, "POINT_PARAM_ST") {}

        /// @brief Create a new Region with no name.
        [[nodiscard]] Region* GetNewEntry(const int entrySubtype) override
        {
            std::unique_ptr<Region> newRegion;
            switch (entrySubtype)
            {
                CASE_MAKE_UNIQUE(InvasionPoint)
                CASE_MAKE_UNIQUE(EnvironmentMapPoint)
                CASE_MAKE_UNIQUE(Sound)
                CASE_MAKE_UNIQUE(VFX)
                CASE_MAKE_UNIQUE(WindVFX)
                CASE_MAKE_UNIQUE(SpawnPoint)
                CASE_MAKE_UNIQUE(Message)
                CASE_MAKE_UNIQUE(EnvironmentMapEffectBox)
                CASE_MAKE_UNIQUE(WindArea)
                CASE_MAKE_UNIQUE(Connection)
                CASE_MAKE_UNIQUE(PatrolRoute22)
                CASE_MAKE_UNIQUE(BuddySummonPoint)
                CASE_MAKE_UNIQUE(MufflingBox)
                CASE_MAKE_UNIQUE(MufflingPortal)
                CASE_MAKE_UNIQUE(OtherSound)
                CASE_MAKE_UNIQUE(MufflingPlane)
                CASE_MAKE_UNIQUE(PatrolRoute)
                CASE_MAKE_UNIQUE(MapPoint)
                CASE_MAKE_UNIQUE(WeatherOverride)
                CASE_MAKE_UNIQUE(AutoDrawGroupPoint)
                CASE_MAKE_UNIQUE(GroupDefeatReward)
                CASE_MAKE_UNIQUE(MapPointDiscoveryOverride)
                CASE_MAKE_UNIQUE(MapPointParticipationOverride)
                CASE_MAKE_UNIQUE(Hitset)
                CASE_MAKE_UNIQUE(FastTravelRestriction)
                CASE_MAKE_UNIQUE(WeatherCreateAssetPoint)
                CASE_MAKE_UNIQUE(PlayArea)
                CASE_MAKE_UNIQUE(EnvironmentMapOutput)
                CASE_MAKE_UNIQUE(MountJump)
                CASE_MAKE_UNIQUE(Dummy)
                CASE_MAKE_UNIQUE(FallPreventionRemoval)
                CASE_MAKE_UNIQUE(NavmeshCutting)
                CASE_MAKE_UNIQUE(MapNameOverride)
                CASE_MAKE_UNIQUE(MountJumpFall)
                CASE_MAKE_UNIQUE(HorseRideOverride)
                CASE_MAKE_UNIQUE(Other)
                default:
                    throw MSBFormatError(std::format(
                        "Invalid Region subtype: {}", static_cast<int>(entrySubtype)));
            }

            std::vector<std::unique_ptr<Region>>& subtypeVector = m_entriesBySubtype.at(static_cast<int>(entrySubtype));
            subtypeVector.push_back(std::move(newRegion));
            return subtypeVector.back().get();
        }
    };
}

#undef CASE_MAKE_UNIQUE