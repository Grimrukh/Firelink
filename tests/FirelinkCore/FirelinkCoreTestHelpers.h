#pragma once

#include <FirelinkTestHelpers.h>
#include <FirelinkCore/Binder.h>
#include <FirelinkCore/Paths.h>

/// @brief Load a split binder whose BHD is an entry in `bnd_name` BND, with a separate BDT file.
inline Firelink::Binder::CPtr LoadSplitChrtpfbxf(const char* bnd_name, const char* bdt_name)
{
    const auto bnd_path = GetResourcePath(bnd_name);
    const auto bdt_path = GetResourcePath(bdt_name);
    const auto bnd_raw = LoadFile(bnd_path);
    const auto bdt_raw = LoadFile(bdt_path);

    // Find "bhd' entry in BND.
    const auto stem = Firelink::StemOf(bnd_path);
    const auto bhd_entry_name = std::format("{}.chrtpfbhd", stem);
    const auto bnd = Firelink::Binder::FromPath(bnd_path);
    const auto chrtpfbhd_entry = bnd->FindEntryByName(bhd_entry_name);

    const auto& chrtpfbhdData = chrtpfbhd_entry->GetData();
    return Firelink::Binder::FromSplitBytes(
        chrtpfbhdData.data(), chrtpfbhdData.size(),
        bdt_raw.data(), bdt_raw.size());
}
