#pragma once

#include <array>
#include <bitset>
#include <set>
#include <vector>

#include "Export.h"

namespace GrimHook
{
    /// @brief Vector3 struct for positions, Euler rotations, etc.
    /// Note that Y is "up" in FromSoftware games and rotation is left-handed.
    struct GRIMHOOK_API Vector3
    {
        float x, y, z;
    };

    /**
     * @brief A bitset with a fixed number of bits, used for grouping and filtering.
     *
     * This data is written to MSB fields as bit flags over the required number of `uint32_t` values, but is more
     * convenient to store here as a collection of enabled bits. Constructors are provided for both (`set<int>` enabled
     * bits or `vector<uint32_t>` serialized data).
     *
     * @tparam BIT_COUNT The number of bits in the bitset. Only 128, 256, and 1024 are permitted (game-dependent).
     */
    template <std::size_t BIT_COUNT>
    class GRIMHOOK_API GroupBitSet
    {
    public:
        /// @brief Default constructor (no enabled bits).
        GroupBitSet();

        /// @brief Construct by enabling all bits in `bitSet`.
        explicit GroupBitSet(const std::set<int>& bitSet);

        /// @brief Read the appropriate number of `uint32_t`s from `stream` and enable all bits.
        explicit GroupBitSet(std::ifstream& stream);

        /// @brief Construct from a vector of serialized `uint32_t` values. Size of vector must be correct.
        explicit GroupBitSet(const std::vector<uint32_t>& uintList);

        /// @brief Construct from array<uint32_t, BIT_COUNT / 32> (i.e. from serialized data).
        explicit GroupBitSet(const std::array<uint32_t, BIT_COUNT / 32>& uintArray);

        /// @brief Create by enabling inclusive range `[firstBit, lastBit]`.
        static GroupBitSet FromRange(int firstBit, int lastBit);

        /// @brief Create by disabling all bits.
        static GroupBitSet AllOff();

        /// @brief Create by enabling all bits.
        static GroupBitSet AllOn();

        /// @brief Construct a vector of enabled bits in ascending order.
        [[nodiscard]] std::vector<int> ToSortedBitList() const;

        /// @brief Construct a vector of the appropriate number of `uint32_t`s to hold all bits (for serialization).
        [[nodiscard]] std::vector<uint32_t> ToUintList() const;

        /// @brief Write the appropriate number of `uint32_t`s to `stream` (for serialization).
        ///
        /// Calls `ToUintList()` and writes each `uint32_t` to the stream.
        void Write(std::ofstream& stream) const;

        bool operator[](std::size_t index) const;

        /// @brief Enable group bit `index`.
        void Enable(int index);

        /// @brief Disable group bit `index`.
        void Disable(int index);

        [[nodiscard]] GroupBitSet Intersection(const GroupBitSet& other) const;

        [[nodiscard]] GroupBitSet UnionWith(const GroupBitSet& other) const;

        [[nodiscard]] GroupBitSet Difference(const GroupBitSet& other) const;

        bool operator==(const GroupBitSet& other) const;

        bool operator!=(const GroupBitSet& other) const;

        explicit operator std::string() const;

    private:

        /// @brief Underlying bitset that records which bits are enabled.
        std::bitset<BIT_COUNT> bits;
    };

    // Explicit instantiations (defined uniquely in `Collections.cpp`).
    extern template class GroupBitSet<128>;
    extern template class GroupBitSet<256>;
    extern template class GroupBitSet<1024>;
}
