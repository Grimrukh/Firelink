#pragma once

#include <map>
#include <memory>
#include <string>
#include <windows.h>

#include "Export.h"
#include "Logging.h"


namespace GrimHook {

    // Forward declarations.
    class BasePointer;
    class ChildPointer;

    /// @brief Concept that constrains memory read/write methods to types that are trivially copyable.
    template <typename T>
    concept MemReadWriteType = std::is_trivially_copyable_v<T>;

    /**
     * @brief Owns a Windows process handle and passes it to memory read/write methods.
     *
     * Ownership semantics:
     * - The handle is transferred to this class on construction.
     * - Copy semantics are disabled to avoid ownership duplication.
     * - Move semantics are enabled to transfer ownership.
     */
    class GRIMHOOK_API ManagedProcess
    {
    public:
        explicit ManagedProcess(void* processHandle);

        /// @brief Get a (non-owning) reference to the process handle.
        [[nodiscard]] void* GetHandle() const { return m_processHandle.get(); }

        /// @brief Get the main module base address.
        bool GetMainModuleBaseAddressAndSize(void*& baseAddress, SIZE_T& mainModuleSize) const;

        /// @brief Check if pointer overwrite is currently enabled.
        [[nodiscard]] bool GetAllowPointerOverwrite() const { return allowPointerOverwrite; }

        /// @brief Set pointer overwrite to true or false.
        void SetAllowPointerOverwrite(const bool allow) { allowPointerOverwrite = allow; }

        /// @brief Check if the process handle is still valid.
        [[nodiscard]] bool IsHandleValid() const;

        /// @brief Check if the wrapped process has terminated.
        [[nodiscard]] bool IsProcessTerminated() const;

        /// @brief Check if the given address is valid inside the process.
        [[nodiscard]] bool IsAddressValid(const void* address) const;

        /// @brief Retrieve a raw pointer to the named pointer.
        [[nodiscard]] const BasePointer* GetPointer(const std::string& name) const;

        /// @brief Delete the named pointer from the process. Logs an error if pointer is absent.
        void DeletePointer(const std::string& name);

        /// @brief Try to delete the named pointer from the process. Return true if pointer was deleted.
        bool TryDeletePointer(const std::string& name);

        // region Pointer Construction

        /// @Brief Basic constructor call for a pointer with a known base address and offsets from that address.
        BasePointer* CreatePointer(
            const std::string& name,
            const void* baseAddress,
            const std::vector<int>& offsets = {});

        /// @brief Pointer found by reading and executing a 32-bit jump `jumpRelativeOffset` bytes after `baseAddress`.
        BasePointer* CreatePointerWithJumpInstruction(
            const std::string& name,
            const void* baseAddress,
            int jumpRelativeOffset,
            const std::vector<int>& offsets = {});

        /// @brief Scan process memory for the given `aobPattern` and point to its start address.
        BasePointer* CreatePointerToAob(
            const std::string& name,
            const std::string& aobPattern,
            const std::vector<int>& offsets = {});

        /// @brief Scan processs memory for the given `aobPattern` then read and execute a jump near its start address.
        BasePointer* CreatePointerToAobWithJumpInstruction(
            const std::string& name,
            const std::string& aobPattern,
            int jumpRelativeOffset,
            const std::vector<int>& offsets = {});

        /// @brief Create a child pointer that dynamically starts at the resolved address of a parent pointer.
        ChildPointer* CreateChildPointer(
            const BasePointer& parent,
            const std::string& name,
            const std::vector<int>& offsets = {});

        /// @brief Create a child pointer that dynamically starts at the resolved address of a parent pointer.
        ChildPointer* CreateChildPointer(
            const std::string& parentName,
            const std::string& name,
            const std::vector<int>& offsets = {});

        // endregion

        // region Memory Read/Write

        /// @brief Generic read method that returns a success bool and uses an out `value` parameter.
        /// Supports primitive 'trivially copyable' types: int, float, double, bool, void*, arrays thereof, etc.
        template <MemReadWriteType T>
        bool ReadInto(const void* address, T& value) const
        {
            return ReadProcessBytes(address, &value, sizeof(T));
        }

        /// @brief Generic read method with direct return of read value or a default value (zero, empty array, etc.).
        /// Note that when using this method, a read failure cannot be distinguished from the default value being read.
        template <MemReadWriteType T>
        T Read(const void* address, T defaultValue = T{}) const
        {
            T value;
            return ReadInto(address, value) ? value : defaultValue;
        }

        /// @brief Generic write method.
        /// Supports primitive 'trivially copyable' types: int, float, double, bool, void*, arrays thereof, etc.
        template <MemReadWriteType T>
        bool Write(const void* address, const T& value) const
        {
            return WriteProcessBytes(address, &value, sizeof(T));
        }


        /// @brief Generic array read method that returns a success bool and uses an `outData` vector parameter.
        /// Supports primitive 'trivially copyable' types: int, float, double, bool, void*, arrays thereof, etc.
        /// Size of `outData` must already be set to the desired array length.
        template <MemReadWriteType T>
        bool ReadArrayInto(const void* address, std::vector<T>& outData) const
        {
            if (outData.empty())
            {
                Warning(L"Tried to read array into an empty vector (no-op).");
                return false;
            }
            return ReadProcessBytes(address, outData.data(), sizeof(T) * outData.size());
        }

        /// @brief Generic array read method that returns the read vector directly, or a default empty vector.
        /// Supports primitive 'trivially copyable' types: int, float, double, bool, void*, arrays thereof, etc.
        /// Note that when using this method, a read failure cannot be distinguished from the default array being read.
        template <MemReadWriteType T>
        [[nodiscard]] std::vector<T> ReadArray(const void* address, const size_t length, T defaultValue = T{}) const
        {
            if (length == 0)
            {
                Warning("Tried to read an array of length 0 (no-op). Returning empty vector.");
                return {};  // return empty vector
            }
            std::vector<T> outData(length, defaultValue);
            if (!ReadProcessBytes(address, outData.data(), sizeof(T) * length))
                Warning("Failed to read array data. Returning default vector.");  // read failed

            return outData;
        }

        /// @brief Generic write method for arrays (as vectors).
        /// Supports primitive 'trivially copyable' element types: int, float, double, bool, void*, arrays thereof, etc.
        template <MemReadWriteType T>
        bool WriteArray(const void* address, const std::vector<T>& data) const
        {
            if (data.empty())
            {
                Warning("Tried to write an empty array vector (no-op).");
                return false;
            }
            return WriteProcessBytes(address, data.data(), sizeof(T) * data.size());
        }


        /// @brief Read a string of fixed size from memory.
        [[nodiscard]] std::string ReadString(const void* address, size_t length) const;

        /// @brief Write a string to memory. Null terminator is NOT added automatically.
        bool WriteString(const void* address, const std::string& str) const;


        /// @brief Read a fixed-length UTF-16 string from memory to a `u16string` (`length` in characters).
        [[nodiscard]] std::u16string ReadUTF16String(const void* address, size_t length) const;

        /// @brief Write a UTF-16 (wide) string to memory.
        /// NOTE: Does not append a null terminator automatically.
        bool WriteUTF16String(const void* address, const std::u16string& u16str) const;


        /// @brief Read any type from memory.
        /// Distinguished from `ReadInto()` to ensure the user is aware that the type may not be binary-appropriate.
        /// Does not have a `Read()` equivalent that directly returns the read value.
        template <typename T>
        [[nodiscard]] bool ReadAnyTypeInto(const void* address, T& anyValue) const
        {
            return ReadProcessBytes(address, &anyValue, sizeof(T));
        }

        /// @brief Write any type to memory.
        /// Distinguished from `Write()` to ensure the user is aware that the type may not be binary-appropriate.
        template <typename T>
        bool WriteAnyType(const void* address, const T& anyValue) const
        {
            return WriteProcessBytes(address, &anyValue, sizeof(T));
        }

        // endregion

        // region AoB Pattern Scanning

        /**
         * @brief Find a byte pattern in the process memory.
         *
         * @param pattern The byte (unsigned char) pattern to search for.
         * @param patternSize The size of the pattern in bytes.
         * @param wildcardMask An optional mask of booleans to ignore certain bytes in the pattern.
         * @param pageExecuteOnly If true, only search memory pages with 'EXECUTE*' protection.
        **/
        [[nodiscard]] const void* FindPattern(const BYTE* pattern, SIZE_T patternSize, const bool* wildcardMask = nullptr, bool pageExecuteOnly = true) const;

        /**
         * @brief Find a wide string hexadecimal pattern in the process memory.
         *
         * Converts the string pattern to a BYTE pattern and a wildcard mask, then calls `FindPattern` overload.
         *
         * @param patternString The pattern to search for, e.g. "45 2c ? 9a".
         * @param pageExecuteOnly If true, only search memory pages with 'EXECUTE*' protection.
        **/
        [[nodiscard]] const void* FindPattern(const std::string& patternString, bool pageExecuteOnly = true) const;

        /** @brief Static function to find and open a process by name.
         *
         * Ownership is left up to the caller.
         *
         * Returns `true` if the process search was valid, even if no `outProcess` was found. To determine if the
         * process was found, check if `outProcess` is `nullptr`.
         *
         * @param processName The name of the process to search for.
         * @param outProcess A pointer to a `ManagedProcess` pointer to assign the found process to.
         */
        static bool FindProcessByName(const std::wstring& processName, ManagedProcess*& outProcess);

        /** @brief Static function to find and open a process by window title.
         *
         * Ownership is left up to the caller.
         *
         * Returns `true` if the process search was valid, even if no `outProcess` was found. To determine if the
         * process was found, check if `outProcess` is `nullptr`.
         *
         * @param windowTitle The title of the window to search for.
         * @param outProcess A pointer to a `ManagedProcess` pointer to assign the found process to.
         */
        static bool FindProcessByWindowTitle(const std::wstring& windowTitle, ManagedProcess*& outProcess);

        /** @brief Blocks until the given process name is found and returned, `timeout` ms passes (returns nullptr),
         *  or the `stopFlag` is set to `true` (returns nullptr).
         *
         *  Ownership is left up to the caller.
         *
         *  Assigns the found process to `outProcess` if found within the timeout. If not found, `outProcess` is
         *  `nullptr`.
         *
         *  @param processName The name of the process to search for.
         *  @param timeoutMs The maximum time to wait for the process to be found (ms).
         *  @param refreshIntervalMs The time to wait between process searches (ms).
         *  @param stopFlag An atomic boolean flag to stop the search early.
         */
        static std::unique_ptr<ManagedProcess> WaitForProcess(
            const std::wstring& processName,
            int timeoutMs,
            int refreshIntervalMs,
            const std::atomic<bool>& stopFlag);

    private:

        // Unique pointer for process handle (`void*` or typedef `HANDLE`) that automatically closes the handle.
        std::unique_ptr<void, decltype(&CloseHandle)> m_processHandle;

        // Map of pointers (on stack) owned by this process. Each pointer has a unique (wide) name. (Pointers are
        // polymorphic, so they can't be stored as value types here.)
        std::map<std::string, std::unique_ptr<BasePointer>> m_pointers;

        // Array of loaded module handles for this process.
        HMODULE m_hModules[1024] = { nullptr };

        // If true, allow pointer creation to overwrite existing pointers with the same name.
        bool allowPointerOverwrite = true;

        /// @brief Helper function to read bytes from memory.
        [[nodiscard]] bool ReadProcessBytes(const void* address, void* buffer, size_t size) const;

        /// @brief Helper function to write bytes to memory.
        bool WriteProcessBytes(const void* address, const void* data, size_t size) const;
    };
}
