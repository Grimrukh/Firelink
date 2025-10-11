#include <Firelink/BaseHook.h>
#include <Firelink/Pointer.h>
#include <Firelink/Process.h>
#include <FirelinkDSRHook/DSRHook.h>
#include <FirelinkDSRHook/DSROffsets.h>

#include <memory>

using namespace Firelink;
using namespace FirelinkDSR;

namespace
{
    constexpr char worldChrManAob[] = "48 8B 05 ? ? ? ? 48 8B 48 68 48 85 C9 0F 84 ? ? ? ? 48 39 5E 10 0F 84 ? ? ? ? 48";
    // constexpr char chrClassBaseAob[] = "48 8B 05 ? ? ? ? 48 85 C0 ? ? F3 0F 58 80 AC 00 00 00";
    // constexpr char chrClassWarpAob[] = "48 8B 05 ? ? ? ? 66 0F 7F 80 ? ? ? ? 0F 28 02 66 0F 7F 80 ? ? ? ? C6 80";
} // namespace

DSRHook::DSRHook(std::shared_ptr<ManagedProcess> process)
: BaseHook(std::move(process))
{
    DSRHook::RefreshAllPointers();
}

void DSRHook::RefreshAllPointers()
{
    m_WorldChrMan = CreatePointerFromAobWithJump3("WorldChrMan", worldChrManAob);
    // m_ChrClassBase = CreatePointerFromAobWithJump3("ChrClassBase", chrClassBaseAob);
    // TODO: ChrClassWarp.
    // TODO: More AoBs.

    RefreshChildPointers();
}

void DSRHook::RefreshChildPointers()
{
    m_PlayerIns = m_process->CreateChildPointer("WorldChrMan", "PlayerIns", {WORLD_CHR_MAN::PLAYER_INS});

    m_PlayerGameData = m_process->CreateChildPointer("PlayerIns", "PlayerGameData", {PLAYER_INS::PLAYER_GAME_DATA_PTR});

    m_ClientPlayerIns = m_process->CreateChildPointer(
        "PlayerIns",
        "ClientPlayerIns",
        {
            PLAYER_INS::CHR_INS_NO_VTABLE + CHR_INS_NO_VTABLE::CONNECTED_PLAYERS_CHR_SLOT_ARRAY,
            CHR_SLOT::PLAYER_INS_PTR,
        });
}

bool DSRHook::IsGameLoaded() const
{
    // WorldChrMan only resolves to non-null if game is loaded.
    return !m_WorldChrMan->IsNull();
}
