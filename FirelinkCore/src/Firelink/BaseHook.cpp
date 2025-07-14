#include <format>
#include <string>

#include "Firelink/BaseHook.h"
#include "Firelink/Pointer.h"

using namespace std;


Firelink::BaseHook::BaseHook(std::shared_ptr<ManagedProcess> process) : m_process(move(process)) {}

Firelink::BaseHook::~BaseHook() = default;

Firelink::BasePointer Firelink::BaseHook::TempPointer(const std::string& name, const void* address) const
{
    return m_process->CreateTempPointer(name, address);
}

Firelink::BasePointer* Firelink::BaseHook::CreatePointerFromAobWithJump3(
    const string& pointerName,
    const string& aobPattern) const
{
    Info(format("Searching for '{}' AOB...", pointerName));
    if (const LPCVOID aobAddr = m_process->FindPattern(aobPattern))
    {
        Info(format("AOB address for pointer '{}' found.", pointerName));
        return m_process->CreatePointerWithJumpInstruction(pointerName, aobAddr, 0x3, { 0 });
    }
    else
    {
        Warning(format("AOB for pointer '{}' not found.", pointerName));
        return nullptr;
    }
}
