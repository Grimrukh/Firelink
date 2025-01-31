#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <vector>

#include "Export.h"
#include "MemoryUtils.h"
#include "Process.h"


namespace Firelink
{
    /// @brief EXPORT: Manages a pointer to a memory address in a process, with read/write functions to use at that
    /// address.
    class FIRELINK_API BasePointer
    {
    public:
        /// @brief Default destructor (virtual class).
        virtual ~BasePointer() = default;

        /// @brief Get base address of pointer.
        [[nodiscard]] const void* GetBaseAddress() const { return m_baseAddress; }

        /// @brief Get name of pointer (unique in its process).
        [[nodiscard]] std::string GetName() const { return m_name; }

        /// @brief Get owning `ManagedProcess` of pointer.
        [[nodiscard]] const ManagedProcess& GetProcess() const { return m_process; }

        /// @brief Resolve the final pointer address by following its offsets from its base address.
        [[nodiscard]] virtual const void* Resolve() const;

        /// @brief Check if the pointer resolves to null (i.e. invalid pointer)
        [[nodiscard]] bool IsNull() const { return Resolve() == nullptr; }

        /// @brief Resolve the pointer's address and add `offset` to it (converting back to a pointer).
        [[nodiscard]] const void* ResolveWithOffset(int32_t offset) const;

        /// @brief Get hex string of resolved address.
        [[nodiscard]] std::string GetResolvedHex() const;

        /// @brief Read data from the owning process at the pointer's resolved address plus `offset`.
        /// Returns `true` if the read was successful.
        template <MemReadWriteType T>
        bool ReadInto(const int32_t offset, T& value) const
        {
            const void* address = ResolveWithOffset(offset);
            if (address == nullptr)
                return false;  // read failed (address invalid)

            return m_process.ReadInto<T>(address, value);
        }

        /// @brief Read data from the owning process at the pointer's resolved address plus `offset`.
        /// Returns the read value or a default value if the read fails.
        template <MemReadWriteType T>
        [[nodiscard]] T Read(const int32_t offset, T defaultValue = T{}) const
        {
            const void* address = ResolveWithOffset(offset);
            if (address == nullptr)
                return defaultValue;  // read failed (address invalid)

            return m_process.Read<T>(address, defaultValue);
        }

        /// @brief Write data to the owning process at the pointer's resolved address plus `offset`.
        template <MemReadWriteType T>
        bool Write(const int32_t offset, T value) const
        {
            const void* address = ResolveWithOffset(offset);
            if (address == nullptr)
                return false;  // write failed (address invalid)

            return m_process.Write(address, value);
        }

        /// @brief Reads an array of data from owning process at the pointer's resolved address plus `offset`.
        /// Returns `true` if the read was successful.
        template <MemReadWriteType T>
        bool ReadArrayInto(const int32_t offset, std::vector<T>& outData) const
        {
            const void* address = ResolveWithOffset(offset);
            if (address == nullptr)
                return false;  // read failed (address invalid)

            return m_process.ReadArrayInto<T>(address, outData);
        }

        /// @brief Reads an array of data from owning process at the pointer's resolved address plus `offset`.
        /// Returns the array vector directly, or a vector of default values.
        template <MemReadWriteType T>
        [[nodiscard]] std::vector<T> ReadArray(const int32_t offset, const size_t length, T defaultValue = T{}) const
        {
            const void* address = ResolveWithOffset(offset);
            if (address == nullptr)
                return {};  // read failed (address invalid)

            return m_process.ReadArray<T>(address, length, defaultValue);
        }

        /// @brief Write array of data to the owning process at the pointer's resolved address plus `offset`.
        template <MemReadWriteType T>
        bool WriteArray(const int32_t offset, const std::vector<T>& valueArray) const
        {
            const void* address = ResolveWithOffset(offset);
            if (address == nullptr)
                return false;  // write failed (address invalid)

            return m_process.WriteArray(address, valueArray);
        }

        /// @brief Read data of a potentially unvalidated type at the pointer's resolved address plus `offset`.
        /// Does not have a `Read()` equivalent that directly returns the read value.
        template <typename T>
        bool ReadAnyTypeInto(const int32_t offset, T& anyValue) const
        {
            const void* address = ResolveWithOffset(offset);
            if (address == nullptr)
                return false;  // read failed (address invalid)

            return m_process.ReadAnyTypeInto<T>(address, anyValue);
        }

        /// @brief Write data of a potentially unvalidated type at the pointer's resolved address plus `offset`.
        template <typename T>
        bool WriteAnyType(const int32_t offset, const T& value) const
        {
            const void* address = ResolveWithOffset(offset);
            if (address == nullptr)
                return false;  // write failed (address invalid)

            return m_process.Write(address, value);
        }

        /// @brief Read a string of known `length` from the pointer's resolved address plus `offset`.
        [[nodiscard]] std::string ReadString(int32_t offset, size_t length) const;

        /// @brief Write a string to the pointer's resolved address plus `offset`.
        /// Does NOT append a null terminator automatically.
        [[nodiscard]] bool WriteString(int32_t offset, const std::string& str) const;

        /// @brief Read a UTF-16 (wide) string of known `length` (in wide characters, not bytes) from the pointer's
        /// resolved address plus `offset`.
        [[nodiscard]] std::u16string ReadUTF16String(int32_t offset, size_t length) const;

        /// @brief Write a UTF-16 (wide) string to the pointer's resolved address plus `offset`.
        /// Does NOT append a null terminator automatically.
        [[nodiscard]] bool WriteUTF16String(int32_t offset, const std::u16string& str) const;

        /// @brief Read and log (as hex) an array of `size` bytes from the pointer's resolved address plus `offset`.
        void ReadAndLogByteArray(int32_t offset, size_t length) const;

    protected:

        // Pointer's process.
        const ManagedProcess& m_process;

        // Pointer's name for retrieval and debugging. Also used as unique key in `m_process.m_pointers`.
        std::string m_name;

        // Base address of pointer in process memory.
        const void* m_baseAddress;

        // Chain of relative offsets to follow from `m_baseAddress` to resolve pointer address.
        std::vector<int> m_offsets;

        /// @brief Protected constructor (straightforward) is available only to `ManagedProcess` via friend functions.
        BasePointer(
            const ManagedProcess& process,
            const void* address,
            std::string name,
            const std::vector<int>& offsets = {})
            : m_process(process), m_name(std::move(name)), m_baseAddress(address), m_offsets(offsets) {}

        // These friend functions in `ManagedProcess` are the only way to create pointers.

        friend BasePointer* ManagedProcess::CreatePointer(
            const std::string& name,
            const void* baseAddress,
            const std::vector<int>& offsets);

        friend BasePointer* ManagedProcess::CreatePointerWithJumpInstruction(
            const std::string& name,
            const void* baseAddress,
            int jumpRelativeOffset,
            const std::vector<int>& offsets);

        friend BasePointer* ManagedProcess::CreatePointerToAob(
            const std::string& name,
            const std::string& aobPattern,
            const std::vector<int>& offsets);

        friend BasePointer* ManagedProcess::CreatePointerToAobWithJumpInstruction(
            const std::string& name,
            const std::string& aobPattern,
            int jumpRelativeOffset,
            const std::vector<int>& offsets);
    };

    /// @brief EXPORT: Child pointer class whose base address is taken from the resolved address of a parent pointer.
    class FIRELINK_API ChildPointer final : public BasePointer
    {

    public:

        /// @brief Resolved by resolving the parent pointer first to find a dynamic base address for this pointer.
        [[nodiscard]] const void* Resolve() const override;

    private:

        // Parent pointer to follow before applying additional offsets.
        const BasePointer& m_parentPointer;

        /// @brief Constructor taking a parent pointer and additional offsets. Process is copied from the parent.
        ChildPointer(const BasePointer& parent, const std::string& name, const std::vector<int>& offsets)
            : BasePointer(parent.GetProcess(), nullptr, name, offsets),
              m_parentPointer(parent) {}

        /// @brief This friend function in `ManagedProcess` is the only way to create a `ChildPointer`.
        friend ChildPointer* ManagedProcess::CreateChildPointer(
            const BasePointer& parent,
            const std::string& name,
            const std::vector<int>& offsets);

    };

}
