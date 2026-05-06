#pragma once

namespace Firelink
{
    /// @brief True if `n` is a power of two (used by alignment helpers).
    constexpr bool IsPowerOf2(const std::size_t n) noexcept
    {
        return n != 0 && (n & (n - 1)) == 0;
    }

    /// @brief Round `value` up to the nearest multiple of `align` (must be a power of two).
    constexpr std::size_t AlignUp(const std::size_t value, const std::size_t align) noexcept
    {
        return (value + (align - 1)) & ~(align - 1);
    }
} // namespace Firelink
