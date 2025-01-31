#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace GrimHook::BinaryValidation
{

    // Template alias for variable-size padding (array of uint8_t)
    template <std::size_t N>
    using padding = std::array<uint8_t, N>;

    // Confirms that the bytes in the (padding) array are all equal to the specified value (default 0).
    template<std::size_t N>
    bool IsPaddingValid(std::array<uint8_t, N> padding, const uint8_t& padValue = 0)
    {
        return std::ranges::all_of(padding, [padValue](const uint8_t byte) { return byte == padValue; });
    }

    template <std::size_t Size>
    void ValidateSinglePadding(const std::string& typeName, const padding<Size>& pad)
    {
        if (!IsPaddingValid(pad, 0))
        {
            throw std::runtime_error(typeName + ": Non-zero padding field detected.");
        }
    }

    // Function to validate multiple (zero) padding fields. Pads of non-zero values must be validated separately.
    template <std::size_t... Sizes>
    void AssertPadding(const std::string& typeName, const padding<Sizes>&... paddings)
    {
        (ValidateSinglePadding(typeName, paddings), ...);
    }

    template <std::integral R, std::integral T>
    void AssertValue(const std::string& typeName, const R& reference, const T& value)
    {
        if (reference != value)
        {
            throw std::runtime_error(typeName + ": Value validation failed (expected "
                + std::to_string(reference) + ", got " + std::to_string(value) + ")");
        }
    }

    template <std::integral R, std::integral T>
    void AssertNotValue(const std::string& typeName, const R& reference, const T& value)
    {
        if (reference == value)
            throw std::runtime_error(typeName + ": Value validation failed (expected NOT "
                + std::to_string(reference) + ")");
    }

    // Function template to validate that given array value is equal to a reference array.
    template <std::integral T, std::size_t N>
    void ValidateArray(const std::string& fieldName, const std::array<T, N>& reference, const std::array<T, N>& arrayValue)
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (arrayValue[i] != reference[i])
            {
                throw std::runtime_error(
                    fieldName + ": Array validation failed: Value[" + std::to_string(i) + "] ("
                    + std::to_string(arrayValue[i]) + ") is not equal to " + std::to_string(reference[i]));
            }
        }
    }
}