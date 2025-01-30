#include <set>
#include <sstream>
#include <stdexcept>

#include "GrimHook/BinaryReadWrite.h"
#include "GrimHook/Collections.h"

using namespace GrimHook::BinaryReadWrite;


template <std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT>::GroupBitSet() = default;

template <std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT>::GroupBitSet(const std::set<int>& bitSet)
{
    for (int bit : bitSet)
    {
        if (bit < 0 || bit >= BIT_COUNT)
        {
            throw std::out_of_range("Bit " + std::to_string(bit) + " is out of range for GroupBitSet.");
        }
        bits.set(bit);
    }
}


// Constructor from the appropriate number of `uint32_t`s in a filestream
template<std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT>::GroupBitSet(std::ifstream& stream)
{
    const size_t uintCount = BIT_COUNT / 32;
    for (std::size_t i = 0; i < uintCount; ++i)
        {
        const auto uint = ReadValue<uint32_t>(stream);
        for (int j = 0; j < 32; ++j)
        {
            if (uint >> j & 1)
                bits.set(i * 32 + j);
        }
    }
}



// Constructor from array<uint32_t, BIT_COUNT / 32> (i.e. from serialized data)
template <std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT>::GroupBitSet(const std::array<uint32_t, BIT_COUNT / 32>& uintArray)
{
    for (std::size_t i = 0; i < uintArray.size(); ++i)
    {
        for (int j = 0; j < 32; ++j)
        {
            if (uintArray[i] >> j & 1)
            {
                bits.set(i * 32 + j);
            }
        }
    }
}

// Constructor from vector<uint32_t> (i.e. from serialized data)
template <std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT>::GroupBitSet(const std::vector<uint32_t>& uintList)
{
    if (uintList.size() != BIT_COUNT / 32)
    {
        throw std::invalid_argument("Invalid uintList size for GroupBitSet.");
    }
    for (std::size_t i = 0; i < uintList.size(); ++i)
    {
        for (int j = 0; j < 32; ++j)
        {
            if (uintList[i] >> j & 1)
            {
                bits.set(i * 32 + j);
            }
        }
    }
}

template <std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT> GrimHook::GroupBitSet<BIT_COUNT>::FromRange(const int firstBit, const int lastBit)
{
    if (firstBit < 0 || lastBit >= BIT_COUNT || firstBit > lastBit)
    {
        throw std::invalid_argument("Invalid range for GroupBitSet.");
    }
    GroupBitSet result;
    for (int bit = firstBit; bit <= lastBit; ++bit)
    {
        result.bits.set(bit);
    }
    return result;
}

// All off
template <std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT> GrimHook::GroupBitSet<BIT_COUNT>::AllOff()
{
    return GroupBitSet();
}

// All on
template <std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT> GrimHook::GroupBitSet<BIT_COUNT>::AllOn()
{
    GroupBitSet result;
    result.bits.set();
    return result;
}

template <std::size_t BIT_COUNT>
std::vector<int> GrimHook::GroupBitSet<BIT_COUNT>::ToSortedBitList() const
{
    std::vector<int> result;
    for (std::size_t i = 0; i < BIT_COUNT; ++i)
    {
        if (bits.test(i))
        {
            result.push_back(static_cast<int>(i));
        }
    }
    return result;
}

template <std::size_t BIT_COUNT>
std::vector<uint32_t> GrimHook::GroupBitSet<BIT_COUNT>::ToUintList() const
{
    std::vector<uint32_t> uintList(BIT_COUNT / 32, 0);
    for (std::size_t i = 0; i < BIT_COUNT; ++i)
    {
        if (bits.test(i))
        {
            uintList[i / 32] |= 1U << i % 32;
        }
    }
    return uintList;
}

// Write to filestream (appropriate number of `uint32_t`s)
template<std::size_t BIT_COUNT>
void GrimHook::GroupBitSet<BIT_COUNT>::Write(std::ofstream& stream) const
{
    for (uint32_t value : ToUintList())
    {
        WriteValue(stream, value);
    }
}

// Access bit
template <std::size_t BIT_COUNT>
bool GrimHook::GroupBitSet<BIT_COUNT>::operator[](std::size_t index) const
{
    return bits.test(index);
}

// Add bit
template <std::size_t BIT_COUNT>
void GrimHook::GroupBitSet<BIT_COUNT>::Enable(int index)
{
    if (index < 0 || index >= BIT_COUNT)
    {
        throw std::out_of_range("Bit is out of range for GroupBitSet.");
    }
    bits.set(index);
}

// Remove bit
template <std::size_t BIT_COUNT>
void GrimHook::GroupBitSet<BIT_COUNT>::Disable(int index)
{
    if (index < 0 || index >= BIT_COUNT)
    {
        throw std::out_of_range("Bit is out of range for GroupBitSet.");
    }
    bits.reset(index);
}

// Intersection
template <std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT> GrimHook::GroupBitSet<BIT_COUNT>::Intersection(const GroupBitSet& other) const
{
    GroupBitSet result;
    result.bits = bits & other.bits;
    return result;
}

// Union
template <std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT> GrimHook::GroupBitSet<BIT_COUNT>::UnionWith(const GroupBitSet& other) const
{
    GroupBitSet result;
    result.bits = bits | other.bits;
    return result;
}

// Difference
template <std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT> GrimHook::GroupBitSet<BIT_COUNT>::Difference(const GroupBitSet& other) const
{
    GroupBitSet result;
    result.bits = bits & ~other.bits;
    return result;
}

// Equality
template <std::size_t BIT_COUNT>
bool GrimHook::GroupBitSet<BIT_COUNT>::operator==(const GroupBitSet& other) const
{
    return bits == other.bits;
}

// Inequality
template <std::size_t BIT_COUNT>
bool GrimHook::GroupBitSet<BIT_COUNT>::operator!=(const GroupBitSet& other) const
{
    return bits != other.bits;
}

template<std::size_t BIT_COUNT>
GrimHook::GroupBitSet<BIT_COUNT>::operator std::basic_string<char>() const
{
    const auto sortedBits = ToSortedBitList();
    std::ostringstream oss;
    oss << "GroupBitSet(";
    for (std::size_t i = 0; i < sortedBits.size(); ++i)
    {
        if (i > 0)
        {
            oss << ", ";
        }
        oss << sortedBits[i];
    }
    oss << ")";
    return oss.str();
}

// Explicit specifications.
template class GrimHook::GroupBitSet<128>;
template class GrimHook::GroupBitSet<256>;
template class GrimHook::GroupBitSet<1024>;
