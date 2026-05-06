#pragma once

// Internal helpers for MSB buffer-based serialization.
// Included only by .cpp files in MapStudio.

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/BinaryValidation.h>

#include <cstdint>
#include <string>

namespace Firelink::EldenRing::Maps::MapStudio::BufferHelpers
{
    using namespace Firelink::BinaryReadWrite;

    /// @brief Read a null-terminated UTF-16 LE string at the current reader position.
    inline std::u16string ReadUTF16String(BufferReader& reader)
    {
        std::u16string result;
        while (true)
        {
            const char16_t ch = reader.Read<char16_t>();
            if (ch == 0) break;
            result += ch;
        }
        return result;
    }

    /// @brief Read a null-terminated UTF-16 LE string at the given offset (does not advance cursor).
    inline std::u16string ReadUTF16StringAt(BufferReader& reader, std::size_t offset)
    {
        auto guard = reader.TempOffset(offset);
        return ReadUTF16String(reader);
    }

    /// @brief Write a null-terminated UTF-16 LE string.
    inline void WriteUTF16String(BufferWriter& writer, const std::u16string& str)
    {
        for (const char16_t ch : str)
            writer.Write(ch);
        writer.Write(char16_t{0});
    }

    /// @brief Read a trivially-copyable struct and call Validate().
    template <Validated T>
    T ReadValidatedStruct(BufferReader& reader)
    {
        T val = reader.Read<T>();
        val.Validate();
        return val;
    }

    /// @brief Validate and write a trivially-copyable struct.
    template <Validated T>
    void WriteValidatedStruct(BufferWriter& writer, const T& val)
    {
        val.Validate();
        writer.Write(val);
    }

    /// @brief Read a 32-bit enum value at an offset without advancing the cursor.
    template <typename EnumType>
    EnumType ReadEnum32At(BufferReader& reader, std::size_t offset)
    {
        auto guard = reader.TempOffset(offset);
        return static_cast<EnumType>(reader.Read<uint32_t>());
    }

    /// @brief Assert N values equal to a single expected value.
    template <PrimitiveType T, std::size_t N>
    void AssertReadValues(BufferReader& reader, const T& expected, const std::string& name)
    {
        for (std::size_t i = 0; i < N; ++i)
        {
            T val = reader.Read<T>();
            if (val != expected)
                throw BinaryAssertionError(
                    name + ": expected " + std::to_string(expected) + ", got " + std::to_string(val));
        }
    }

    /// @brief Assert values match an array of expected values.
    template <PrimitiveType T, std::size_t N>
    void AssertReadValues(BufferReader& reader, const std::array<T, N>& expected, const std::string& name)
    {
        for (std::size_t i = 0; i < N; ++i)
        {
            T val = reader.Read<T>();
            if (val != expected[i])
                throw BinaryAssertionError(
                    name + "[" + std::to_string(i) + "]: expected " + std::to_string(expected[i])
                    + ", got " + std::to_string(val));
        }
    }

    /// @brief Read N values into a std::array.
    template <PrimitiveType T, std::size_t N>
    std::array<T, N> ReadValues(BufferReader& reader)
    {
        std::array<T, N> values;
        for (std::size_t i = 0; i < N; ++i)
            values[i] = reader.Read<T>();
        return values;
    }

    /// @brief Write all values in a std::array.
    template <PrimitiveType T, std::size_t N>
    void WriteValues(BufferWriter& writer, const std::array<T, N>& values)
    {
        for (const T& v : values)
            writer.Write(v);
    }
} // namespace Firelink::EldenRing::Maps::MapStudio::BufferHelpers

