#pragma once

#ifndef TEST_RESOURCES_DIR
#define TEST_RESOURCES_DIR ""
#endif

#include <FirelinkCore/Oodle.h>

#include <filesystem>
#include <fstream>
#include <vector>

inline std::vector<std::byte> LoadFile(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    const auto size = static_cast<std::size_t>(f.tellg());
    f.seekg(0);
    std::vector<std::byte> buf(size);
    f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
    return buf;
}

inline std::filesystem::path GetResourcePath(const std::filesystem::path& filename)
{
    return std::filesystem::path(TEST_RESOURCES_DIR) / filename;
}

inline bool oodle_available()
{
    return Firelink::Oodle::IsAvailable();
}