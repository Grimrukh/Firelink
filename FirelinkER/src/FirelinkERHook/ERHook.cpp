#include <FirelinkERHook/ERHook.h>

#include <FirelinkERHook/EROffsets.h>

namespace
{
    constexpr char worldChrManAob[] = "48 8B 05 ? ? ? ? 48 85 C0 74 0F 48 39 88";
    constexpr char gameDataManAob[] = "48 8B 05 ? ? ? ? 48 85 C0 74 05 48 8B 40 58 C3 C3";
    constexpr char soloParamRepositoryAob[] = "48 8B 0D ? ? ? ? 48 85 C9 0F 84 ? ? ? ? 45 33 C0 BA 8E 00 00 00";
    constexpr char eventFlagManAob[] = "48 8B 3D ? ? ? ? 48 85 FF ? ? 32 C0 E9";
} // namespace

FirelinkER::ERHook::ERHook(std::shared_ptr<Firelink::ManagedProcess> process)
: BaseHook(std::move(process))
{
    ERHook::RefreshAllPointers();
}

void FirelinkER::ERHook::RefreshAllPointers()
{
    m_WorldChrMan = CreatePointerFromAobWithJump3("WorldChrMan", worldChrManAob);
    m_GameDataMan = CreatePointerFromAobWithJump3("GameDataMan", gameDataManAob);
    m_SoloParamRepository = CreatePointerFromAobWithJump3("SoloParamRepository", soloParamRepositoryAob);
    m_EventFlagMan = CreatePointerFromAobWithJump3("EventFlagMan", eventFlagManAob);
}

void FirelinkER::ERHook::RefreshChildPointers()
{
    m_PlayerIns = m_process->CreateChildPointer("WorldChrMan", "PlayerIns", {WORLD_CHR_MAN::PLAYER_INS});
}

bool FirelinkER::ERHook::IsGameLoaded() const
{
    // WorldChrMan only resolves to non-null if game is loaded.
    // TODO: True for DSR; still true for ER?
    return !m_WorldChrMan->IsNull();
}

uint32_t FirelinkER::ERHook::GetNGPlusLevel() const
{
    return m_GameDataMan->Read<uint32_t>(GAME_DATA_MAN::NG_PLUS_LEVEL);
}

void FirelinkER::ERHook::SetNGPlusLevel(uint32_t level) const
{
    if (!m_GameDataMan->Write<uint32_t>(GAME_DATA_MAN::NG_PLUS_LEVEL, level))
        Firelink::Error(std::format("Failed to set NG+ level to {}.", level));
}
