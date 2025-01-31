#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>

#include "Export.h"
#include "Logging.h"


namespace Firelink::BinaryReadWrite
{
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
            throw std::runtime_error("Failed to read value from stream.");
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
            throw std::runtime_error("Failed to read value from stream.");
        if (value != assertedValue)
        {
            throw std::runtime_error(
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
            throw std::runtime_error("Failed to read value from stream.");
        }
        for (std::size_t i = 0; i < N; ++i)
        {
            if (values[i] != assertedValues[i])
            {
                throw std::runtime_error(
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
            throw std::runtime_error("Failed to read value from stream.");
        }
        for (std::size_t i = 0; i < N; ++i)
        {
            if (values[i] != assertedValue)
            {
                throw std::runtime_error(
                    fieldName + ": Read value " + std::to_string(values[i]) + " does not match expected value "
                    + std::to_string(assertedValue));
            }
        }
    }

    /// @brief Assert a certain number of bytes of a constant value (default 0).
    FIRELINK_API void AssertReadPadBytes(std::ifstream& stream, const int& padSize, const std::string& fieldName, const uint8_t& padValue = 0);

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
            throw std::runtime_error("Failed to read values from stream.");
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

    FIRELINK_API void WritePadBytes(std::ofstream& stream, const int& padSize, const uint8_t& padValue = 0);

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
    FIRELINK_API std::string ReadString(std::ifstream& stream);
    /// @brief Reads a null-terminated UTF-8 string from the stream at a specific `offset`.
    FIRELINK_API std::string ReadString(std::ifstream& stream, const std::streampos& offset);

    /// @brief Writes a null-terminated UTF-8 string to the stream.
    FIRELINK_API void WriteString(std::ofstream& stream, const std::string& str);
    /// @brief Writes a null-terminated UTF-8 string to the stream at a specific `offset`.
    FIRELINK_API void WriteString(std::ofstream& stream, const std::streampos& offset, const std::string& str);

    /// @brief Reads a null-terminated UTF-16 string from the stream.
    FIRELINK_API std::u16string ReadUTF16String(std::ifstream& stream);
    /// @brief Reads a null-terminated UTF-16 string from the stream at a specific `offset`.
    FIRELINK_API std::u16string ReadUTF16String(std::ifstream& stream, const std::streampos& offset);

    /// @brief Writes a null-terminated UTF-16 string to the stream.
    FIRELINK_API void WriteUTF16String(std::ofstream& stream, const std::u16string& str);
    /// @brief Writes a null-terminated UTF-16 string to the stream at a specific `offset`.
    FIRELINK_API void WriteUTF16String(std::ofstream& stream, const std::streampos& offset, const std::u16string& str);

    /// @brief Writes a null-terminated UTF-16 string to the stream (converted from UTF-8)
    FIRELINK_API void WriteUTF16String(std::ofstream& stream, const std::string& str);
    /// @brief Writes a null-terminated UTF-16 string to the stream at a specific `offset` (converted from UTF-8)
    FIRELINK_API void WriteUTF16String(std::ofstream& stream, const std::streampos& offset, const std::string& str);

    // STRUCTS

    template<typename T>
    concept Validated = requires(T t)
    {
        t.Validate();  // must have a `validate` method
    };

    // Reads a type with a `validate()` member from the stream and calls `validate()` on it
    template <Validated T>
    T ReadValidatedStruct(std::ifstream& stream)
    {
        T vStruct;
        stream.read(reinterpret_cast<char*>(&vStruct), sizeof(T));
        if (!stream)
        {
            throw std::runtime_error("Failed to read value from stream.");
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
    FIRELINK_API void AlignStream(std::ifstream& stream, const std::streamsize& alignment);
    FIRELINK_API void AlignStream(std::ofstream& stream, const std::streamsize& alignment);

    // STREAM MANIPULATION

    /**
     * Temporarily seek to a specific position in the stream, execute the provided function,
     * and then restore the stream's original position.
     *
     * @param stream The input/output stream to manipulate.
     * @param position The temporary position to seek to.
     * @param func A function or lambda to execute while at the specified position.
     */
    FIRELINK_API void WithStreamPosition(std::iostream& stream, const std::streampos& position, const std::function<void()>& func);

    // RESERVER

    // Stores reserved offsets, under labels, for later filling. Call `Finish()` to prevent further reserving and
    // validate that no offsets remain unfilled. Currently only handles offsets (32-bit or 64-bit, default).
    class FIRELINK_API Reserver
    {
        std::ofstream& stream;
        std::map<std::string, std::streampos> reservedOffsetOffsets;
        std::map<std::string, std::pair<std::streampos, std::size_t>> reservedStructOffsetsSizes;
        bool finished = false;
        bool is64Bit = true;
        std::streampos relativePositionStart = 0;

    public:
        explicit Reserver(std::ofstream& stream, const bool is64Bit = true) : stream(stream), is64Bit(is64Bit) {}

        // Ensure that the reserver is finished before destruction.
        ~Reserver()
        {
            if (!finished)
            {
                Error("Reserver was not finished before its destruction.");
            }
        }

        // Reserves space for a 64-bit offset at the current position.
        void ReserveOffset(const std::string& label);
        // Reserves space for a struct of the given size. (Can only be filled with `ValidatedStruct` instances.)
        void ReserveValidatedStruct(const std::string& label, const std::streamsize& size);

        // Fills a previously reserved 64-bit offset in the output stream.
        void FillOffset(const std::string& label, int64_t offset);
        // Fills a previously reserved 32-bit offset in the output stream.
        void FillOffset(const std::string& label, int32_t offset);

        // Fills a previously reserved 64-bit offset in the output stream using the current stream offset.
        void FillOffsetWithPosition(const std::string& label);
        // Fills a previously reserved 64-bit offset in the output stream using the current stream offset, relative to
        // (i.e. after subtracting) the `relativePositionStart` set by `SetRelativePositionStart()`.
        void FillOffsetWithRelativePosition(const std::string& label);

        // Fills a previously reserved `ValidatedStruct` in the output stream.
        template <Validated T>
        void FillValidatedStruct(const std::string& label, const T& vStruct)
        {
            if (finished)
            {
                throw std::runtime_error("Cannot reserve more struct offsets after calling Finish().");
            }

            if (!reservedStructOffsetsSizes.contains(label))
                throw std::runtime_error("No `ValidatedStruct` reserved with label: " + label);
            auto [offset, size] = reservedStructOffsetsSizes.at(label);
            if (size != sizeof(T))
                throw std::runtime_error("Reserved struct size does not match size of given `ValidatedStruct`.");

            const std::streampos currentPos = stream.tellp();
            stream.seekp(offset);
            WriteValidatedStruct(stream, vStruct);
            stream.seekp(currentPos);
        }

        // Set the relative position to use when filling offsets with relative positions.
        void SetRelativePositionStart(const std::streampos offset) { relativePositionStart = offset; }
        // Clear the relative position start.
        void ClearRelativePositionStart() { relativePositionStart = 0; }

        // Prevent further reserving and validate that all offsets have been filled.
        void Finish();
    };

}