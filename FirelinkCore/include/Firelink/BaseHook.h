#pragma once

#include <memory>

#include "Export.h"
#include "Process.h"


namespace Firelink
{
    /// @brief Base class for hooks.
    class FIRELINK_API BaseHook
    {
    public:
        /// @brief Construct a BaseHook using a shared pointer to a `ManagedProcess`.
        /// Derived class constructors will usually want to call `RefreshAllPointers()`.
        explicit BaseHook(std::shared_ptr<ManagedProcess> process);

        /// @brief Default destructor (virtual class).
        virtual ~BaseHook();

        /** @brief Get a shared pointer to the managed process associated with this hook.
         *
         * Useful for direct `Read()` and `Write()` calls that don't use `BasePointer` instances.
         */
        [[nodiscard]] std::shared_ptr<ManagedProcess> GetProcess() const { return m_process; }

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

        /// @brief Search for `aobPattern`, read a relative jump offset 3 bytes into it, and follow the found pointer.
        ///
        /// Specifically, calls `m_process->CreatePointerWithJumpInstruction(pointerName, aobAddr, 0x3, { 0 })`.
        ///
        /// @param pointerName Name of the pointer to create.
        /// @param aobPattern AOB pattern to search for.
        void CreatePointerFromAobWithJump3(const std::string& pointerName, const std::string& aobPattern) const;
    };
}