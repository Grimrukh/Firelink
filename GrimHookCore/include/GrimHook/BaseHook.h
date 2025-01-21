#pragma once

#include "Export.h"
#include "Pointer.h"
#include "Process.h"


namespace GrimHook
{
    /// @brief Base class for hooks.
    class GRIMHOOK_API BaseHook
    {
    public:
        /// @brief Construct a BaseHook using a shared pointer to a `ManagedProcess`.
        /// Derived class constructors will usually want to call `RefreshAllPointers()`.
        explicit BaseHook(std::shared_ptr<ManagedProcess> process);

        /// @brief Default destructor (virtual class).
        virtual ~BaseHook();

        /// @brief Get the managed process associated with this hook, e.g. for direct `Read()` and `Write()` calls.
        [[nodiscard]] ManagedProcess* GetProcess() const { return m_process.get(); }

        /// @brief Convenient name access to pointers.
        [[nodiscard]] const BasePointer* operator [](const std::string& name) const
        {
            return m_process->GetPointer(name);
        }

        /// @brief Refresh ALL pointers in the hook, including slow AoB-based pointers and all child pointers.
        virtual void RefreshAllPointers() = 0;

        /// @brief Refresh all child pointers in the hook, but not base pointers.
        virtual void RefreshChildPointers() = 0;

    protected:
        std::shared_ptr<ManagedProcess> m_process;
    };
}