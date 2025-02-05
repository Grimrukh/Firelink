﻿#pragma once

#include <exception>
#include <string>
#include <utility>

#include "../../Export.h"

namespace FirelinkER::Maps::MapStudio
{
    class FIRELINK_ER_API MSBFormatError final : public std::exception
    {
    public:
        explicit MSBFormatError(std::string msg) : message(std::move(msg)) {}

        [[nodiscard]] const char* what() const noexcept override
        {
            return message.c_str();
        }

    private:
        std::string message;
    };

}
