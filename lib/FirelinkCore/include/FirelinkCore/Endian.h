#pragma once

#include <cstdint>

namespace Firelink::BinaryReadWrite
{
    /// @brief Byte order for in-memory buffer readers/writers.
    enum class Endian : std::uint8_t { Little, Big };
}
