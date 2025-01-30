#include <format>
#include <string>

#include "GrimHook/BaseHook.h"

using namespace std;


GrimHook::BaseHook::BaseHook(std::shared_ptr<ManagedProcess> process) : m_process(move(process)) {}

GrimHook::BaseHook::~BaseHook() = default;


void GrimHook::BaseHook::CreatePointerFromAobWithJump3(const string& pointerName, const string& aobPattern) const
{
    Info(format("Searching for '{}' AOB...", pointerName));
    if (const LPCVOID aobAddr = m_process->FindPattern(aobPattern))
    {
        m_process->CreatePointerWithJumpInstruction(pointerName, aobAddr, 0x3, { 0 });
        Info(format("AOB address for pointer '{}' found.", pointerName));
    }
    else
    {
        Warning(format("AOB for pointer '{}' not found.", pointerName));
    }
}
