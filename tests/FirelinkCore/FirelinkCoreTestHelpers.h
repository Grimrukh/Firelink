#pragma once

#include <FirelinkTestHelpers.h>
#include <FirelinkCore/Binder.h>
#include <FirelinkCore/Paths.h>

/// @brief Load a split binder whose BHD is an entry in `bnd_name` BND, with a separate BDT file.
inline Firelink::Binder::CPtr LoadSplitChrtpfbxf(const char* bnd_name, const char* bdt_name)
{
    auto bnd_path = GetResourcePath(bnd_name);
    auto bdt_path = GetResourcePath(bdt_name);
    auto bnd_raw = LoadFile(bnd_path);
    auto bdt_raw = LoadFile(bdt_path);

    // Find "bhd' entry in BND.
    auto stem = Firelink::StemOf(bnd_path);
    auto bhd_entry_name = std::format("{}.chrtpfbhd", stem);
    auto bnd = Firelink::Binder::FromPath(bnd_path);
    auto chrtpfbhd_entry = bnd->FindEntryByName(bhd_entry_name);

    return Firelink::Binder::FromSplitBytes(
        chrtpfbhd_entry->data.data(), chrtpfbhd_entry->data.size(),
        bdt_raw.data(), bdt_raw.size());
}
