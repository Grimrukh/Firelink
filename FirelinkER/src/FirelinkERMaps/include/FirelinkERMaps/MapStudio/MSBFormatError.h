#pragma once

#include <FirelinkERMaps/Export.h>

#include <exception>
#include <string>
#include <utility>

namespace FirelinkER::Maps::MapStudio
{
    class FIRELINK_ER_MAPS_API MSBFormatError final : public std::exception
    {
    public:
        explicit MSBFormatError(std::string msg)
        : message(std::move(msg))
        {
        }

        [[nodiscard]] const char* what() const noexcept override { return message.c_str(); }

    private:
        std::string message;
    };

} // namespace FirelinkER::Maps::MapStudio
