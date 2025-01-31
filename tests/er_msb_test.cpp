#include <filesystem>
#include <format>

#include "GrimHook/Logging.h"
#include "GrimHookER/Maps/MapStudio/MSB.h"
#include "GrimHookER/Maps/MapStudio/MSBFormatError.h"

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
        msb = MSB::NewFromFilePath(msbPath);
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

    const auto& firstPart = msb->GetPartParam().GetAllEntries().at(0);

    string name = firstPart->GetName();
    auto [x, y, z] = firstPart->GetTranslate();
    Info(format("First character: '{}' at ({}, {}, {})", name, x, y, z));
}