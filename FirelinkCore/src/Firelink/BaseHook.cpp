#include <Firelink/BaseHook.h>
#include <Firelink/Pointer.h>

#include <format>
#include <string>

Firelink::BaseHook::BaseHook(std::shared_ptr<ManagedProcess> process)
: m_process(std::move(process))
{
}

Firelink::BaseHook::~BaseHook() = default;

Firelink::BasePointer Firelink::BaseHook::TempPointer(const std::string& name, const void* address) const
{
    return m_process->CreateTempPointer(name, address);
}

Firelink::BasePointer* Firelink::BaseHook::CreatePointerFromAobWithJump3(
    const std::string& pointerName, const std::string& aobPattern) const
{
    Info(std::format("Searching for '{}' AOB...", pointerName));
    if (const LPCVOID aobAddr = m_process->FindPattern(aobPattern))
    {
        Info(std::format("AOB address for pointer '{}' found.", pointerName));
        return m_process->CreatePointerWithJumpInstruction(pointerName, aobAddr, 0x3, {0});
    }
    else
    {
        Warning(std::format("AOB for pointer '{}' not found.", pointerName));
        return nullptr;
    }
}
