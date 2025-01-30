#include "GrimHookER/ERHook.h"
#include "GrimHookER/EROffsets.h"

using namespace std;


namespace
{
    constexpr char worldChrManAob[] = "48 8B 05 ? ? ? ? 48 85 C0 74 0F 48 39 88";
    constexpr char gameDataManAob[] = "48 8B 05 ? ? ? ? 48 85 C0 74 05 48 8B 40 58 C3 C3";
    constexpr char soloParamRepositoryAob[] = "48 8B 0D ? ? ? ? 48 85 C9 0F 84 ? ? ? ? 45 33 C0 BA 8E 00 00 00";
    constexpr char eventFlagManAob[] = "48 8B 3D ? ? ? ? 48 85 FF ? ? 32 C0 E9";
}


GrimHookER::ERHook::ERHook(shared_ptr<GrimHook::ManagedProcess> process) : BaseHook(move(process))
{
    ERHook::RefreshAllPointers();
}

void GrimHookER::ERHook::RefreshAllPointers()
{
    CreatePointerFromAobWithJump3("WorldChrMan", worldChrManAob);
    CreatePointerFromAobWithJump3("GameDataMan", gameDataManAob);
    CreatePointerFromAobWithJump3("SoloParamRepository", soloParamRepositoryAob);
    CreatePointerFromAobWithJump3("EventFlagMan", eventFlagManAob);
}

void GrimHookER::ERHook::RefreshChildPointers()
{
    m_process->CreateChildPointer("WorldChrMan", "PlayerIns", { world_chr_man_offsets::PLAYER_INS });
}

bool GrimHookER::ERHook::IsGameLoaded() const
{
    // WorldChrMan only resolves to non-null if game is loaded.
    // TODO: True for DSR; still true for ER?
    return !(*this)["WorldChrMan"]->IsNull();
}

uint32_t GrimHookER::ERHook::GetNGPlusLevel() const
{
    return (*this)["GameDataMan"]->Read<uint32_t>(game_data_man_offsets::NG_PLUS_LEVEL);
}

void GrimHookER::ERHook::SetNGPlusLevel(uint32_t level) const
{
    if (!(*this)["GameDataMan"]->Write<uint32_t>(game_data_man_offsets::NG_PLUS_LEVEL, level))
        GrimHook::Error(format("Failed to set NG+ level to {}.", level));
}

