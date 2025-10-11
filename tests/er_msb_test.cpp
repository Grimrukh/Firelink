#include <../FirelinkCore/src/FirelinkCore/Firelink/Logging.h>
#include <../FirelinkER/src/Maps/MapStudio/EntryParam.h>
#include <../FirelinkER/src/Maps/MapStudio/MSB.h>
#include <FirelinkER/Maps/MapStudio/MSBFormatError.h>
#include <../FirelinkER/src/Maps/MapStudio/Part.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <vector>

using namespace std;
using namespace Firelink;
using namespace FirelinkER::Maps::MapStudio;

int main()
{
    // Test file (Stormveil Castle, DCX removed) is bundled with test.
    filesystem::path msbPath = "resources/elden_ring/m10_00_00_00.msb";
    msbPath = absolute(msbPath);
    if (!exists(msbPath))
    {
        Error(std::format("MSB file not found: {}", msbPath.string()));
        throw runtime_error("MSB file not found.");
    }

    // SetLogLevel(LogLevel::DEBUG);
    // SetLogFile(filesystem::path("er_msb_test.log"));

    Info(std::format("Opening MSB file: {}", msbPath.string()));
    unique_ptr<MSB> msb;
    try
    {
        const auto startTime = chrono::high_resolution_clock::now();
        msb = MSB::NewFromFilePath(msbPath);
        const auto endTime = chrono::high_resolution_clock::now();
        Info(
            std::format(
                "MSB loaded in {} ms.", chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count()));
    }
    catch (const MSBFormatError& e)
    {
        Error(std::format("MSB format error: {}", e.what()));
        throw;
    }
    catch (const exception& e)
    {
        Error(std::format("Error reading MSB: {}", e.what()));
        throw;
    }

    // Get first character name and translate.
    const auto& partParam = msb->GetPartParam();
    const auto allParts = partParam.GetAllEntries();
    const auto& firstPart = allParts.at(0);

    auto [x, y, z] = firstPart->GetTranslate();
    Info(std::format("First part: {} at ({}, {}, {})", string(*firstPart), x, y, z));

    const auto& characters = partParam.GetSubtypeEntries<CharacterPart>();
    if (characters.empty())
    {
        Error("No characters found in MSB.");
        throw runtime_error("No characters found in MSB.");
    }
    const auto& firstChr = characters[0];

    auto [cx, cy, cz] = firstChr->GetTranslate();
    Info(std::format("First character: {} at ({}, {}, {})", string(*firstChr), cx, cy, cz));

    if (const auto& chrModel = firstChr->GetModel())
        Info(std::format("First character's model: {}", string(*chrModel)));
    else
        Info("First character has no model set.");

    if (const auto& drawParent = firstChr->GetDrawParent())
    {
        Info(std::format("First character's draw parent: {}", string(*drawParent)));

        // Clear draw parent.
        firstChr->SetDrawParent(nullptr);

        if (const auto& nullDrawParent = firstChr->GetDrawParent(); !nullDrawParent)
            Info("First character's draw parent cleared.");
        else
            Error("Failed to clear first character's draw parent.");

        // Set new draw parent.
        firstChr->SetDrawParent(firstPart);

        if (const auto& newDrawParent = firstChr->GetDrawParent())
            Info(std::format("First character's draw parent set to first part: {}", string(*newDrawParent)));
        else
            Error("Failed to set first character's draw parent.");
    }
    else
        Info("First character has no draw parent Part set.");

    // Write MSB.
    filesystem::path outPath = "m10_00_00_00_out.msb";
    outPath = absolute(outPath);
    Info(std::format("Writing MSB to: {}", outPath.string()));
    msb->WriteToFilePath(outPath);
    Info("MSB written successfully.");

    Info("Re-reading MSB to verify.");
    try
    {
        const auto startTime = chrono::high_resolution_clock::now();
        MSB::NewFromFilePath(outPath);
        const auto endTime = chrono::high_resolution_clock::now();
        Info(
            std::format(
                "MSB reloaded in {} ms.", chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count()));
    }
    catch (const MSBFormatError& e)
    {
        Error(std::format("MSB format error on re-read: {}", e.what()));
        throw;
    }
    catch (const exception& e)
    {
        Error(std::format("Error re-reading MSB: {}", e.what()));
        throw;
    }

    Info("MSB read/write/re-read completed successfully.");
}