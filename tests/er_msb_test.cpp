#include <chrono>
#include <filesystem>
#include <format>
#include <vector>

#include "GrimHook/Logging.h"
#include "GrimHookER/Maps/MapStudio/EntryParam.h"
#include "GrimHookER/Maps/MapStudio/MSB.h"
#include "GrimHookER/Maps/MapStudio/MSBFormatError.h"
#include "GrimHookER/Maps/MapStudio/Part.h"

using namespace std;
using namespace GrimHook;
using namespace GrimHookER::Maps::MapStudio;


int main()
{
    // Test file (Stormveil Castle, DCX removed) is bundled with test.
    const filesystem::path msbPath = "resources/elden_ring/m10_00_00_00.msb";
    if (!exists(msbPath))
    {
        Error(format("MSB file not found: {}", msbPath.string()));
        throw runtime_error("MSB file not found.");
    }

    unique_ptr<MSB> msb;
    try
    {
        const auto startTime = chrono::high_resolution_clock::now();
        msb = MSB::NewFromFilePath(msbPath);
        const auto endTime = chrono::high_resolution_clock::now();
        Info(format("MSB loaded in {} ms.", chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count()));
    }
    catch (const MSBFormatError& e)
    {
        Error(format("MSB format error: {}", e.what()));
        throw;
    }
    catch (const exception& e)
    {
        Error(format("Error reading MSB: {}", e.what()));
        throw;
    }

    // Get first character name and translate.
    const auto& partParam = msb->GetPartParam();
    const auto allParts = partParam.GetAllEntries();
    const auto& firstPart = allParts.at(0);

    string name = firstPart->GetName();
    auto [x, y, z] = firstPart->GetTranslate();
    Info(format("First part: '{}' at ({}, {}, {})", name, x, y, z));

    const auto& characters = partParam.GetSubtypeEntries<CharacterPart>();
    if (characters.empty())
    {
        Error("No characters found in MSB.");
        throw runtime_error("No characters found in MSB.");
    }
    const auto& firstChr = characters[0];

    auto [cx, cy, cz] = firstChr->GetTranslate();
    Info(format("First character: '{}' at ({}, {}, {})", firstChr->GetName(), cx, cy, cz));
}