#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/DevTools.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace std::filesystem;
using namespace Firelink::BinaryReadWrite;
using namespace Firelink::DevTools;

// ---------------------------------------------------------------------------
// Tee — write to stdout and a report file simultaneously.
// ---------------------------------------------------------------------------

struct Tee
{
    std::ostream& a;
    std::ostream& b;

    Tee& operator<<(const std::string& s) { a << s; b << s; return *this; }
    Tee& operator<<(const char* s)        { a << s; b << s; return *this; }
    Tee& operator<<(std::size_t v)        { a << v; b << v; return *this; }
    Tee& operator<<(int v)                { a << v; b << v; return *this; }
    Tee& operator<<(char c)               { a << c; b << c; return *this; }
};

static void Section(Tee& out, const std::string& title)
{
    out << "\n============================================================\n";
    out << "  " + title + "\n";
    out << "============================================================\n";
}

static void Sub(Tee& out, const std::string& title)
{
    out << "\n--- " + title + " ---\n";
}

// ---------------------------------------------------------------------------
// Analyse — run all RE tools on a single file.
// ---------------------------------------------------------------------------

static void Analyse(
    Tee& out,
    const std::string& label,
    const path& filePath,
    const std::vector<std::byte>* otherData = nullptr,
    const std::string& otherLabel = "")
{
    Section(out, label + "  [" + filePath.filename().string() + "]");

    BufferReader reader(filePath);
    const std::byte* data = reader.RawAt(0);
    const std::size_t size = reader.Size();

    // File size.
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "File size: %zu bytes  (0x%zX)\n", size, size);
        out << buf;
    }

    // ---- Entropy / histogram ------------------------------------------------
    Sub(out, "Byte histogram & entropy");
    out << FormatHistogram(ComputeHistogram(data, size), 12);

    // ---- ASCII strings ------------------------------------------------------
    Sub(out, "Printable ASCII strings (min 5 chars)");
    const auto strings = FindStrings(data, size, 5);
    if (strings.empty())
    {
        out << "(none found)\n";
    }
    else
    {
        for (const auto& s : strings)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "  0x%08zX  ", s.offset);
            out << buf;
            out << s.text + "\n";
        }
    }

    // ---- Header hex dump (first 512 bytes) ----------------------------------
    Sub(out, "Header hex dump (0x0000 - 0x01FF)");
    out << HexDump(data, size, 0, std::min(size, std::size_t{512}));

    // ---- InspectAt every 8 bytes across first 0x80 -------------------------
    Sub(out, "InspectAt every 8 bytes (0x00 - 0x78)");
    for (std::size_t off = 0; off < std::min(size, std::size_t{0x80}); off += 8)
        out << InspectAt(data, size, off);

    // ---- Offset table scan — 32-bit -----------------------------------------
    Sub(out, "Candidate 32-bit offsets in header (0x0000 - 0x01FF)");
    out << FormatOffsetCandidates(ScanOffsetTable(
        data, size,
        0, std::min(size, std::size_t{512}),
        4,
        0x10,
        static_cast<uint64_t>(size),
        4));

    // ---- Offset table scan — 64-bit -----------------------------------------
    Sub(out, "Candidate 64-bit offsets in header (0x0000 - 0x01FF)");
    out << FormatOffsetCandidates(ScanOffsetTable(
        data, size,
        0, std::min(size, std::size_t{512}),
        8,
        0x10,
        static_cast<uint64_t>(size),
        8));

    // ---- Cross-file diff ----------------------------------------------------
    if (otherData)
    {
        Sub(out, "Diff vs " + otherLabel);
        const auto diff = Diff(data, size, otherData->data(), otherData->size());

        {
            char buf[64];
            snprintf(buf, sizeof(buf), "Total differing bytes: %zu\n", diff.size());
            out << buf;
        }

        // Full diff (capped at 500 entries to keep the report readable).
        std::vector<DiffEntry> capped(
            diff.begin(),
            diff.begin() + static_cast<ptrdiff_t>(std::min(diff.size(), std::size_t{500})));
        out << FormatDiff(capped);
        if (diff.size() > 500)
            out << "  ... (truncated)\n";

        // Header-only diff.
        std::vector<DiffEntry> headerDiff;
        for (const auto& d : diff)
            if (d.offset < 0x200) headerDiff.push_back(d);

        if (!headerDiff.empty())
        {
            out << "\nDiff restricted to header (0x0000 - 0x01FF):\n";
            out << FormatDiff(headerDiff);
        }
        else
        {
            out << "(headers are byte-identical)\n";
        }
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    const path hkx1000Path = "n10_01_00_00_001000.hkx";
    const path hkx1700Path = "n10_01_00_00_001700.hkx";
    const path reportPath  = "navmesh_re_report.txt";

    std::ofstream reportFile(reportPath);
    if (!reportFile)
    {
        std::cerr << "Could not open report file: " << reportPath << "\n";
        return 1;
    }

    Tee out{std::cout, reportFile};

    out << "Elden Ring Navmesh RE Report\n";
    out << "Files: " + hkx1000Path.string() + ",  " + hkx1700Path.string() + "\n";

    // Load both files upfront so we can pass raw bytes for cross-diff.
    const auto bytes1000 = ReadFileBytes(hkx1000Path);
    const auto bytes1700 = ReadFileBytes(hkx1700Path);

    Analyse(out, "FILE 1", hkx1000Path, &bytes1700, hkx1700Path.filename().string());
    Analyse(out, "FILE 2", hkx1700Path, &bytes1000, hkx1000Path.filename().string());

    Section(out, "Done — report written to " + reportPath.string());
    out << "\n";

    return 0;
}