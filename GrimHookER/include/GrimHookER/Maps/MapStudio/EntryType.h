#pragma once

#include "Entry.h"

namespace GrimHookER::Maps::MapStudio
{
    /// @brief Concept for `Entry` subtype templates.
    template <typename T>
    concept EntryType = std::is_base_of_v<Entry, T>;
}
