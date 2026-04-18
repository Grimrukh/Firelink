#pragma once

#include <FirelinkCore/Endian.h>
#include <FirelinkCore/Export.h>
#include <FirelinkCore/Logging.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace Firelink::BinaryReadWrite
{
    // Exception for binary read operations.
    class FIRELINK_CORE_API BinaryReadError : public std::runtime_error
    {
    public:
        explicit BinaryReadError(const std::string& msg) : std::runtime_error(msg)
        {
        }

        explicit BinaryReadError(const char* msg) : std::runtime_error(msg)
        {
        }
    };

    // Exception for binary write operations.
    class FIRELINK_CORE_API BinaryWriteError : public std::runtime_error
    {
    public:
        explicit BinaryWriteError(const std::string& msg) : std::runtime_error(msg)
        {
        }

        explicit BinaryWriteError(const char* msg) : std::runtime_error(msg)
        {
        }
    };

    // Exception for binary assertions.
    class FIRELINK_CORE_API BinaryAssertionError : public std::runtime_error
    {
    public:
        explicit BinaryAssertionError(const std::string& msg) : std::runtime_error(msg)
        {
        }

        explicit BinaryAssertionError(const char* msg) : std::runtime_error(msg)
        {
        }
    };

    // WHOLE FILES

    /// @brief Read a file path into a vector of bytes.
    FIRELINK_CORE_API std::vector<std::byte> ReadFileBytes(const std::string& path);

    /// @brief Read a file path into a vector of bytes.
    FIRELINK_CORE_API std::vector<std::byte> ReadFileBytes(const std::filesystem::path& path);

    // BASIC VALUES

    /// @brief Define a concept of readable primitive types to restrict `ReadValue` and `WriteValue` below.
    /// TODO: Just use `is_trivially_copyable`.
    template <typename T>
    concept PrimitiveType = std::integral<T> || std::floating_point<T> || std::same_as<T, bool>;

    /// @brief Reads a value of primitive type T from the stream.
    template <PrimitiveType T>
    T ReadValue(std::ifstream& stream)
    {
        T value;
        stream.read(reinterpret_cast<char*>(&value), sizeof(T));
        if (!stream)
            throw BinaryReadError("Failed to read value from stream.");
        return value;
    }

    /// @brief Reads a value of primitive type T from the stream at a specific `offset`.
    template <PrimitiveType T>
    T ReadValue(std::ifstream& stream, const std::streampos offset)
    {
        const std::streampos currentPos = stream.tellg();
        stream.seekg(offset);
        T value = ReadValue<T>(stream);
        stream.seekg(currentPos);
        return value;
    }

    /// @Reads and asserts a value of primitive type T from the stream. Does not return the value.
    /// (There is no offset-jump version of this function to avoid `offset/assertedValue` confusion.)
    template <PrimitiveType T>
    void AssertReadValue(std::ifstream& stream, const T& assertedValue, const std::string& fieldName)
    {
        T value;
        stream.read(reinterpret_cast<char*>(&value), sizeof(T));
        if (!stream)
            throw BinaryReadError("Failed to read value from stream.");
        if (value != assertedValue)
        {
            throw BinaryReadError(
                fieldName + ": Read value " + std::to_string(value) + " does not match expected value "
                + std::to_string(assertedValue));
        }
    }

    /// @brief Reads and asserts an array of values of primitive type T from the stream. Does not return the array.
    /// (There is no offset-jump version of this function to avoid `offset/assertedValues` confusion.)
    template <PrimitiveType T, std::size_t N>
    void AssertReadValues(std::ifstream& stream, const std::array<T, N>& assertedValues, const std::string& fieldName)
    {
        std::array<T, N> values;
        stream.read(reinterpret_cast<char*>(&values), N * sizeof(T));
        if (!stream)
        {
            throw BinaryReadError("Failed to read value from stream.");
        }
        for (std::size_t i = 0; i < N; ++i)
        {
            if (values[i] != assertedValues[i])
            {
                throw BinaryReadError(
                    fieldName + ": Read value " + std::to_string(values[i]) + " does not match expected value "
                    + std::to_string(assertedValues[i]));
            }
        }
    }

    /// @Brief Variant that asserts a single value against an array of values.
    template <PrimitiveType T, std::size_t N>
    void AssertReadValues(std::ifstream& stream, const T& assertedValue, const std::string& fieldName)
    {
        std::array<T, N> values;
        stream.read(reinterpret_cast<char*>(&values), N * sizeof(T));
        if (!stream)
        {
            throw BinaryReadError("Failed to read value from stream.");
        }
        for (std::size_t i = 0; i < N; ++i)
        {
            if (values[i] != assertedValue)
            {
                throw BinaryReadError(
                    fieldName + ": Read value " + std::to_string(values[i]) + " does not match expected value "
                    + std::to_string(assertedValue));
            }
        }
    }

    /// @brief Assert a certain number of bytes of a constant value (default 0).
    FIRELINK_CORE_API void AssertReadPadBytes(
        std::ifstream& stream, const int& padSize, const std::string& fieldName, const uint8_t& padValue = 0);

    /// @brief Writes a value of primitive type T to the stream
    template <PrimitiveType T>
    void WriteValue(std::ofstream& stream, const T& value)
    {
        stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    template <PrimitiveType T>
    void WriteValue(std::ofstream& stream, const std::streampos offset, const T& value)
    {
        const std::streampos currentPos = stream.tellp();
        stream.seekp(offset);
        WriteValue<T>(stream, value);
        stream.seekp(currentPos);
    }

    /// @brief Reads multiple values of primitive type T from the stream
    template <PrimitiveType T, std::size_t N>
    std::array<T, N> ReadValues(std::ifstream& stream)
    {
        std::array<T, N> values{};
        stream.read(reinterpret_cast<char*>(values.data()), N * sizeof(T));
        if (!stream)
        {
            throw BinaryReadError("Failed to read values from stream.");
        }
        return values;
    }

    /// @brief Reads multiple values of primitive type T from the stream at a specific `offset`.
    template <PrimitiveType T, std::size_t N>
    std::array<T, N> ReadValues(std::ifstream& stream, const std::streampos offset)
    {
        const std::streampos currentPos = stream.tellg();
        stream.seekg(offset);
        std::array<T, N> values = ReadValues<T, N>(stream);
        stream.seekg(currentPos);
        return values;
    }

    /// @brief Writes multiple values of primitive type T to the stream
    template <PrimitiveType T, std::size_t N>
    void WriteValues(std::ofstream& stream, const std::array<T, N>& values)
    {
        stream.write(reinterpret_cast<const char*>(values.data()), values.size() * sizeof(T));
    }

    template <PrimitiveType T, std::size_t N>
    void WriteValues(std::ofstream& stream, const std::streampos offset, const std::array<T, N>& values)
    {
        const std::streampos currentPos = stream.tellp();
        stream.seekp(offset);
        WriteValues<T, N>(stream, values);
        stream.seekp(currentPos);
    }

    FIRELINK_CORE_API void WritePadBytes(std::ofstream& stream, const int& padSize, const uint8_t& padValue = 0);

    // ENUMS

    // Reads a 32-bit enum value from the stream (template definition must be in header)
    template <typename EnumType>
    EnumType ReadEnum32(std::ifstream& stream)
    {
        uint32_t value;
        stream.read(reinterpret_cast<char*>(&value), sizeof(uint32_t));
        return static_cast<EnumType>(value);
    }

    template <typename EnumType>
    EnumType ReadEnum32(std::ifstream& stream, const std::streampos offset)
    {
        const std::streampos currentPos = stream.tellg();
        stream.seekg(offset);
        EnumType enumType = ReadEnum32<EnumType>(stream);
        stream.seekg(currentPos);
        return enumType;
    }

    // Writes a 32-bit enum value to the stream (template definition must be in header)
    template <typename EnumType>
    void WriteEnum32(std::ofstream& stream, EnumType value)
    {
        const auto intValue = static_cast<uint32_t>(value);
        stream.write(reinterpret_cast<const char*>(&intValue), sizeof(uint32_t));
    }

    template <typename EnumType>
    void WriteEnum32(std::ofstream& stream, const std::streampos offset, EnumType value)
    {
        const std::streampos currentPos = stream.tellp();
        stream.seekp(offset);
        WriteEnum32<EnumType>(stream, value);
        stream.seekp(currentPos);
    }

    // STRINGS

    /// @brief Reads a null-terminated UTF-8 string from the stream.
    FIRELINK_CORE_API std::string ReadString(std::ifstream& stream);
    /// @brief Reads a null-terminated UTF-8 string from the stream at a specific `offset`.
    FIRELINK_CORE_API std::string ReadString(std::ifstream& stream, const std::streampos& offset);

    /// @brief Writes a null-terminated UTF-8 string to the stream.
    FIRELINK_CORE_API void WriteString(std::ofstream& stream, const std::string& str);
    /// @brief Writes a null-terminated UTF-8 string to the stream at a specific `offset`.
    FIRELINK_CORE_API void WriteString(std::ofstream& stream, const std::streampos& offset, const std::string& str);

    /// @brief Reads a null-terminated UTF-16 string from the stream.
    FIRELINK_CORE_API std::u16string ReadUTF16String(std::ifstream& stream);
    /// @brief Reads a null-terminated UTF-16 string from the stream at a specific `offset`.
    FIRELINK_CORE_API std::u16string ReadUTF16String(std::ifstream& stream, const std::streampos& offset);

    /// @brief Writes a null-terminated UTF-16 string to the stream.
    FIRELINK_CORE_API void WriteUTF16String(std::ofstream& stream, const std::u16string& str);
    /// @brief Writes a null-terminated UTF-16 string to the stream at a specific `offset`.
    FIRELINK_CORE_API void WriteUTF16String(
        std::ofstream& stream, const std::streampos& offset, const std::u16string& str);

    /// @brief Writes a null-terminated UTF-16 string to the stream (converted from UTF-8)
    FIRELINK_CORE_API void WriteUTF16String(std::ofstream& stream, const std::string& str);
    /// @brief Writes a null-terminated UTF-16 string to the stream at a specific `offset` (converted from UTF-8)
    FIRELINK_CORE_API void WriteUTF16String(
        std::ofstream& stream, const std::streampos& offset, const std::string& str);

    // STRUCTS

    template <typename T>
    concept Validated = requires(T t)
    {
        t.Validate(); // must have a `validate` method
    };

    // Reads a type with a `validate()` member from the stream and calls `validate()` on it
    template <Validated T>
    T ReadValidatedStruct(std::ifstream& stream)
    {
        T vStruct;
        stream.read(reinterpret_cast<char*>(&vStruct), sizeof(T));
        if (!stream)
        {
            throw BinaryReadError("Failed to read value from stream.");
        }
        vStruct.Validate();
        return vStruct;
    }

    template <Validated T>
    T ReadValidatedStruct(std::ifstream& stream, const std::streampos offset)
    {
        const std::streampos currentPos = stream.tellg();
        stream.seekg(offset);
        T value = ReadValidatedStruct<T>(stream);
        stream.seekg(currentPos);
        return value;
    }

    // Writes an instance of a type with a `validate()` member to the stream after calling `validate()` on it
    template <Validated T>
    void WriteValidatedStruct(std::ofstream& stream, const T& value)
    {
        value.Validate();
        stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    template <Validated T>
    void WriteValidatedStruct(std::ofstream& stream, const std::streampos offset, const T& vStruct)
    {
        const std::streampos currentPos = stream.tellp();
        stream.seekp(offset);
        WriteValidatedStruct<T>(stream, vStruct);
        stream.seekp(currentPos);
    }

    // ALIGNMENT

    // Aligns the stream to the specified boundary
    FIRELINK_CORE_API void AlignStream(std::ifstream& stream, const std::streamsize& alignment);
    FIRELINK_CORE_API void AlignStream(std::ofstream& stream, const std::streamsize& alignment);

    // STREAM MANIPULATION

    /*!
     * Temporarily seek to a specific position in the stream, execute the provided function,
     * and then restore the stream's original position.
     *
     * @param stream The input/output stream to manipulate.
     * @param position The temporary position to seek to.
     * @param func A function or lambda to execute while at the specified position.
     */
    FIRELINK_CORE_API void WithStreamPosition(
        std::iostream& stream, const std::streampos& position, const std::function<void()>& func);

    // RESERVER

    // Stores reserved offsets, under labels, for later filling. Call `Finish()` to prevent further reserving and
    // validate that no offsets remain unfilled. Currently only handles offsets (32-bit or 64-bit, default).
    class FIRELINK_CORE_API Reserver
    {
    public:
        explicit Reserver(std::ofstream& stream, const bool is64Bit = true)
            : m_stream(stream)
              , m_is64Bit(is64Bit)
        {
        }

        // Ensure that the reserver is finished before destruction.
        ~Reserver()
        {
            if (!m_finished)
            {
                Error("Reserver was not finished before its destruction.");
            }
        }

        /// @brief Reserves space for a 64-bit (or 32-bit if specified in `Reserver`) offset at the current position.
        void ReserveOffset(const std::string& label);
        /// @brief Reserves space for a 32-bit signed `int` at the current position.
        void ReserveInt32(const std::string& label);
        /// @brief Reserves space for a struct of the given size. (Can only be filled with `ValidatedStruct` instances.)
        void ReserveValidatedStruct(const std::string& label, const std::streamsize& size);

        /// @brief Fills a previously reserved 64-bit offset in the output stream. 64-bit `Reserver` only.
        void FillOffset(const std::string& label, int64_t offset);
        /// @brief Fills a previously reserved 32-bit offset in the output stream. 32-bit `Reserver` only.
        void FillOffset(const std::string& label, int32_t offset);

        /// @brief Fills a previously reserved offset in the output stream using the current stream offset.
        void FillOffsetWithPosition(const std::string& label);

        /// @brief Fills a previously reserved 64-bit offset in the output stream.
        /// Uses the current stream offset, relative to the `relativePositionStart` set by `SetRelativePositionStart()`.
        void FillOffsetWithRelativePosition(const std::string& label);

        /// @brief Fills a previously reserved `int32_t` in the output stream.
        void FillInt32(const std::string& label, int32_t value);

        /// @brief Fills a previously reserved `ValidatedStruct` in the output stream.
        template <Validated T>
        void FillValidatedStruct(const std::string& label, const T& vStruct)
        {
            if (m_finished)
            {
                throw BinaryWriteError("Cannot reserve more struct offsets after calling Finish().");
            }

            if (!m_reservedStructOffsetsSizes.contains(label))
                throw BinaryWriteError("No `ValidatedStruct` reserved with label: " + label);
            auto [offset, size] = m_reservedStructOffsetsSizes.at(label);
            if (size != sizeof(T))
                throw BinaryWriteError("Reserved struct size does not match size of given `ValidatedStruct`.");

            const std::streampos currentPos = m_stream.tellp();
            m_stream.seekp(offset);
            WriteValidatedStruct(m_stream, vStruct);
            m_stream.seekp(currentPos);

            m_reservedStructOffsetsSizes.erase(label);
        }

        /// @brief Set the relative position to use when filling offsets with relative positions.
        void SetRelativePositionStart(const std::streampos& offset) { m_relativePositionStart = offset; }
        /// @brief Clear the relative position start.
        void ClearRelativePositionStart() { m_relativePositionStart = 0; }

        /// @brief Prevent further reserving and validate that all offsets have been filled.
        void Finish();

    private:
        std::ofstream& m_stream;
        std::map<std::string, std::streampos> m_reservedOffsetOffsets;
        std::map<std::string, std::streampos> m_reservedInt32Offsets;
        std::map<std::string, std::pair<std::streampos, std::size_t>> m_reservedStructOffsetsSizes;
        bool m_finished = false;
        bool m_is64Bit = true;
        std::streampos m_relativePositionStart = 0;
    };

    // =========================================================================
    // BUFFER-BASED I/O (in-memory byte buffers with endian support)
    // =========================================================================

    namespace detail
    {
        /// @brief Reverse the bytes of a primitive value (for endian conversion).
        template <PrimitiveType T>
        T ByteSwap(T value)
        {
            static_assert(std::endian::native == std::endian::little, "ByteSwap assumes little-endian host");
            if constexpr (sizeof(T) == 1) return value;
            else
            {
                T result;
                auto* src = reinterpret_cast<const std::uint8_t*>(&value);
                auto* dst = reinterpret_cast<std::uint8_t*>(&result);
                for (std::size_t i = 0; i < sizeof(T); ++i)
                    dst[i] = src[sizeof(T) - 1 - i];
                return result;
            }
        }
    } // namespace detail

    // -------------------------------------------------------------------------
    // BufferReader — reads primitives from an in-memory byte buffer.
    // -------------------------------------------------------------------------

    class BufferReader
    {
    public:
        /// @brief Get a BufferReader into data managed by the caller.
        BufferReader(const std::byte* data, const std::size_t size, const Endian endian = Endian::Little)
            : m_data(data), m_size(size), m_endian(endian)
        {
        }

        /// @brief Get a BufferReader with its own managed storage.
        explicit BufferReader(std::vector<std::byte>&& storage, const Endian endian = Endian::Little);

        /// @brief Get a BufferReader from a file path with its own managed storage.
        explicit BufferReader(const std::filesystem::path& part, Endian endian = Endian::Little);

        // Non-copyable (m_data may alias m_storage).
        BufferReader(const BufferReader&) = delete;
        BufferReader& operator=(const BufferReader&) = delete;

        /// @brief Move constructor. Fixes up m_data when storage is owned.
        BufferReader(BufferReader&& other) noexcept
            : m_storage(std::move(other.m_storage))
            , m_data(m_storage.empty() ? other.m_data : m_storage.data())
            , m_size(other.m_size)
            , m_position(other.m_position)
            , m_endian(other.m_endian)
        {
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_position = 0;
        }

        /// @brief Move assignment. Fixes up m_data when storage is owned.
        BufferReader& operator=(BufferReader&& other) noexcept
        {
            if (this != &other)
            {
                m_storage = std::move(other.m_storage);
                m_data = m_storage.empty() ? other.m_data : m_storage.data();
                m_size = other.m_size;
                m_position = other.m_position;
                m_endian = other.m_endian;
                other.m_data = nullptr;
                other.m_size = 0;
                other.m_position = 0;
            }
            return *this;
        }

        /// @brief Read a trivially-copyable value, byte-swapping if big-endian.
        /// For scalars and small uniform-width POD structs. For mixed-width
        /// structs, read each field individually so the swap is per-field.
        template <typename T>
            requires std::is_trivially_copyable_v<T>
        T Read()
        {
            if (m_position + sizeof(T) > m_size)
                throw BinaryReadError("BufferReader::Read past end of buffer");
            T value;
            std::memcpy(&value, m_data + m_position, sizeof(T));
            m_position += sizeof(T);
            if constexpr (PrimitiveType<T>)
            {
                if (m_endian == Endian::Big && sizeof(T) > 1)
                    value = detail::ByteSwap(value);
            }
            return value;
        }

        /// @brief Read N elements into a vector (applies per-element endian swap for primitives).
        template <typename T>
            requires std::is_trivially_copyable_v<T>
        [[nodiscard]] std::vector<T> ReadArray(std::size_t count)
        {
            std::vector<T> out(count);
            for (std::size_t i = 0; i < count; ++i)
                out[i] = Read<T>();
            return out;
        }

        /// @brief Read raw bytes into a destination buffer.
        void ReadRaw(void* dest, const std::size_t count)
        {
            if (m_position + count > m_size)
                throw BinaryReadError("BufferReader::ReadRaw past end of buffer");
            std::memcpy(dest, m_data + m_position, count);
            m_position += count;
        }

        /// @brief Skip over bytes without reading.
        void Skip(const std::size_t count)
        {
            if (m_position + count > m_size)
                throw BinaryReadError("BufferReader::Skip past end of buffer");
            m_position += count;
        }

        /// @brief Seek to an absolute position.
        void Seek(const std::size_t pos)
        {
            if (pos > m_size)
                throw BinaryReadError("BufferReader::Seek past end of buffer");
            m_position = pos;
        }

        [[nodiscard]] std::size_t Position() const noexcept { return m_position; }
        [[nodiscard]] std::size_t Size() const noexcept { return m_size; }
        [[nodiscard]] std::size_t Remaining() const noexcept { return m_size - m_position; }

        /// @brief Assert that the cursor is at the expected position.
        void AssertPosition(const std::size_t expected) const
        {
            if (m_position != expected)
                throw BinaryAssertionError(
                    "BufferReader position " + std::to_string(m_position)
                    + " != expected " + std::to_string(expected));
        }

        void SetEndian(const Endian e) noexcept { m_endian = e; }
        [[nodiscard]] Endian GetEndian() const noexcept { return m_endian; }

        /// @brief Get a raw pointer into the buffer at the given offset.
        [[nodiscard]] const std::byte* RawAt(const std::size_t offset) const
        {
            if (offset > m_size)
                throw BinaryReadError("BufferReader::RawAt offset past end of buffer");
            return m_data + offset;
        }

        /// @brief Assert that the next `count` bytes are all zero, advancing past them.
        void AssertPad(const std::size_t count)
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                if (m_position >= m_size)
                    throw BinaryReadError("BufferReader::AssertPad past end of buffer");
                if (m_data[m_position] != std::byte{0})
                    throw BinaryAssertionError(
                        "BufferReader::AssertPad non-zero byte at offset " + std::to_string(m_position));
                ++m_position;
            }
        }

        /// @brief Assert that the next bytes match `expected`, advancing past them.
        void AssertBytes(const void* expected, const std::size_t count, const std::string& name)
        {
            if (m_position + count > m_size)
                throw BinaryReadError(name + ": AssertBytes past end of buffer");
            if (std::memcmp(m_data + m_position, expected, count) != 0)
                throw BinaryAssertionError(name + ": bytes do not match expected");
            m_position += count;
        }

        /// @brief Read a value and assert it matches `expected`.
        template <PrimitiveType T>
        void AssertValue(T expected, const std::string& name)
        {
            T value = Read<T>();
            if (value != expected)
                throw BinaryAssertionError(
                    name + ": expected " + std::to_string(expected) + ", got " + std::to_string(value));
        }

        // -- RAII guard for temporary seek ---

        class TempOffsetGuard
        {
        public:
            ~TempOffsetGuard() { if (m_active) m_reader.Seek(m_saved); }
            TempOffsetGuard(const TempOffsetGuard&) = delete;
            TempOffsetGuard& operator=(const TempOffsetGuard&) = delete;

            TempOffsetGuard(TempOffsetGuard&& o) noexcept
                : m_reader(o.m_reader), m_saved(o.m_saved), m_active(o.m_active)
            {
                o.m_active = false;
            }

            TempOffsetGuard& operator=(TempOffsetGuard&&) = delete;

        private:
            friend class BufferReader;

            TempOffsetGuard(BufferReader& r, const std::size_t saved) : m_reader(r), m_saved(saved)
            {
            }

            BufferReader& m_reader;
            std::size_t m_saved;
            bool m_active = true;
        };

        /// @brief Temporarily seek to `offset`, returning a guard that restores position on destruction.
        [[nodiscard]] TempOffsetGuard TempOffset(const std::size_t offset)
        {
            auto saved = m_position;
            Seek(offset);
            return {*this, saved};
        }

        // -- String reading (const — does not affect current position) ---

        /// @brief Read a null-terminated byte string at `offset` (does not move the cursor).
        [[nodiscard]] std::vector<std::byte> ReadCStringAt(std::size_t offset) const
        {
            std::vector<std::byte> result;
            while (offset < m_size && m_data[offset] != std::byte{0})
            {
                result.push_back(m_data[offset]);
                ++offset;
            }
            return result;
        }

        /// @brief Read a null-terminated UTF-16 LE string at `offset` as raw bytes (does not move the cursor).
        [[nodiscard]] std::vector<std::byte> ReadUTF16LEStringAt(std::size_t offset) const
        {
            std::vector<std::byte> result;
            while (offset + 1 < m_size)
            {
                auto lo = m_data[offset];
                auto hi = m_data[offset + 1];
                if (lo == std::byte{0} && hi == std::byte{0})
                    break;
                result.push_back(lo);
                result.push_back(hi);
                offset += 2;
            }
            return result;
        }

        [[nodiscard]] size_t size() const noexcept { return m_size; }

    private:
        std::vector<std::byte> m_storage{};
        const std::byte* m_data;
        std::size_t m_size;
        std::size_t m_position = 0;
        Endian m_endian = Endian::Little;
    };

    // -------------------------------------------------------------------------
    // BufferWriter — writes primitives into an in-memory byte buffer.
    // -------------------------------------------------------------------------

    class BufferWriter
    {
    public:
        explicit BufferWriter(const Endian endian = Endian::Little) : m_endian(endian)
        {
        }

        [[nodiscard]] Endian GetEndian() const noexcept { return m_endian; }

        void ReserveCapacity(const std::size_t bytes) { m_buffer.reserve(bytes); }

        /// @brief Write a trivially-copyable value, byte-swapping primitives for big-endian output.
        template <typename T>
            requires std::is_trivially_copyable_v<T>
        void Write(T value)
        {
            if constexpr (PrimitiveType<T>)
            {
                if (m_endian == Endian::Big && sizeof(T) > 1)
                    value = detail::ByteSwap(value);
            }
            EnsureSize(m_position + sizeof(T));
            std::memcpy(m_buffer.data() + m_position, &value, sizeof(T));
            m_position += sizeof(T);
        }

        /// @brief Write an array of trivially-copyable values (per-element endian swap for primitives).
        template <typename T>
            requires std::is_trivially_copyable_v<T>
        void WriteArray(const T* values, const std::size_t count)
        {
            for (std::size_t i = 0; i < count; ++i)
                Write<T>(values[i]);
        }

        /// @brief Write raw bytes.
        void WriteRaw(const void* data, const std::size_t size)
        {
            EnsureSize(m_position + size);
            std::memcpy(m_buffer.data() + m_position, data, size);
            m_position += size;
        }

        /// @brief Write `count` zero bytes.
        void WritePad(const std::size_t count)
        {
            EnsureSize(m_position + count);
            std::memset(m_buffer.data() + m_position, 0, count);
            m_position += count;
        }

        /// @brief Reserve space for a value (writes zeros), to be filled later with Fill().
        /// Optional `scope` pointer creates a scoped key ("hex:label") to avoid name collisions.
        template <typename T>
            requires std::is_trivially_copyable_v<T>
        void Reserve(const std::string& label, const void* scope = nullptr)
        {
            const std::string key = MakeKey(label, scope);
            if (m_reserved.contains(key))
                throw BinaryWriteError("BufferWriter::Reserve: label already reserved: " + key);
            m_reserved[key] = m_position;
            WritePad(sizeof(T));
        }

        /// @brief Fill a previously reserved slot with a value.
        template <typename T>
            requires std::is_trivially_copyable_v<T>
        void Fill(const std::string& label, T value, const void* scope = nullptr)
        {
            const std::string key = MakeKey(label, scope);
            const auto it = m_reserved.find(key);
            if (it == m_reserved.end())
                throw BinaryWriteError("BufferWriter::Fill: label not reserved: " + key);
            if constexpr (PrimitiveType<T>)
            {
                if (m_endian == Endian::Big && sizeof(T) > 1)
                    value = detail::ByteSwap(value);
            }
            std::memcpy(m_buffer.data() + it->second, &value, sizeof(T));
            m_reserved.erase(it);
        }

        /// @brief Fill a reserved slot with the current write position.
        template <typename T>
            requires std::is_trivially_copyable_v<T>
        void FillWithPosition(const std::string& label, const void* scope = nullptr)
        {
            Fill<T>(label, static_cast<T>(m_position), scope);
        }

        /// @brief Returns true if the given (label, scope) reservation is still pending.
        [[nodiscard]] bool HasPending(const std::string& label, const void* scope = nullptr) const
        {
            return m_reserved.contains(MakeKey(label, scope));
        }

        /// @brief Number of unfilled reservations.
        [[nodiscard]] std::size_t PendingCount() const noexcept { return m_reserved.size(); }

        /// @brief Pad current position up to the given alignment boundary.
        void PadAlign(const std::size_t alignment)
        {
            if (const std::size_t rem = m_position % alignment; rem != 0)
                WritePad(alignment - rem);
        }

        [[nodiscard]] std::size_t Position() const noexcept { return m_position; }

        /// @brief Return the written buffer content. Throws if any reservations are unfilled.
        [[nodiscard]] std::vector<std::byte> Finalize()
        {
            if (!m_reserved.empty())
            {
                std::string msg = "BufferWriter::Finalize with unfilled reservations:";
                for (const auto& key : m_reserved | std::views::keys)
                {
                    msg += ' ';
                    msg += key;
                }
                throw BinaryWriteError(msg);
            }
            m_buffer.resize(m_position);
            return std::move(m_buffer);
        }

    private:
        void EnsureSize(const std::size_t required)
        {
            if (required > m_buffer.size())
                m_buffer.resize(required, std::byte{0});
        }

        static std::string MakeKey(const std::string& label, const void* scope)
        {
            if (scope == nullptr)
                return label;
            // Prefix with hex address for scoped keys.
            char buf[2 + sizeof(void*) * 2 + 2]; // "0x" + hex + ":" + '\0'
            const auto v = reinterpret_cast<std::uintptr_t>(scope);
            constexpr char digits[] = "0123456789abcdef";
            constexpr std::size_t n = sizeof(void*) * 2;
            std::size_t pos = 0;
            for (std::size_t i = 0; i < n; ++i)
                buf[pos++] = digits[(v >> ((n - 1 - i) * 4)) & 0xF];
            buf[pos++] = ':';
            buf[pos] = '\0';
            return std::string(buf, pos) + label;
        }

        std::vector<std::byte> m_buffer;
        std::size_t m_position = 0;
        Endian m_endian;
        std::map<std::string, std::size_t> m_reserved;
    };
} // namespace Firelink::BinaryReadWrite
