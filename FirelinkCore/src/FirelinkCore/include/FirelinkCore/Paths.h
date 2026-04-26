#pragma once

#include <FirelinkCore/Export.h>

#include <filesystem>
#include <string>

namespace Firelink
{
    /// @brief Return a lower-case copy of a string.
    std::string FIRELINK_CORE_API ToLower(const std::string& s);;

    /// @brief Return an upper-case copy of a string.
    std::string FIRELINK_CORE_API ToUpper(const std::string& s);;

    /// @brief Get the 'minimal stem' of a path (before FIRST period in file name).
    /// @details Example: "c2300.chrbnd.dcx" -> "c2300"
    std::string FIRELINK_CORE_API StemOf(const std::filesystem::path& p);

    /// @brief Check if a filename matches a glob.
    /// @note Only supports "*.ext" and "prefix*.ext" patterns at the moment.
    bool FIRELINK_CORE_API MatchesGlob(const std::string& filename, const std::string& glob);

}