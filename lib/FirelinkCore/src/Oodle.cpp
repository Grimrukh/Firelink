// Oodle (Kraken) compression/decompression via dynamic loading of oo2core_6_win64.dll.

#include <FirelinkCore/Oodle.h>
#include <FirelinkCore/DCX.h>
#include <FirelinkCore/Logging.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <filesystem>
#include <format>
#include <mutex>

namespace Firelink::Oodle
{
    // ========================================================================
    // Oodle ABI types
    // ========================================================================

    struct CompressOptions
    {
        unsigned int verbosity;
        int minMatchLen;
        int seekChunkReset;
        int seekChunkLen;
        int profile;
        int dictionarySize;
        int spaceSpeedTradeoffBytes;
        int maxHuffmansPerChunk;
        int sendQuantumCRCs;
        int maxLocalDictionarySize;
        int makeLongRangeMatcher;
        int matchTableSizeLog2;
    };

    // Oodle constants
    static constexpr int Compressor_Kraken = 8;
    static constexpr int Level_Optimal2 = 6;
    static constexpr int FuzzSafe_Yes = 1;
    static constexpr int CheckCRC_No = 0;
    static constexpr int Verbosity_None = 0;
    static constexpr int ThreadPhase_Unthreaded = 3;

    // Function pointer types
    using Fn_Compress = long(__stdcall*)(
        int, const void*, long, void*, int,
        CompressOptions*, const void*, const void*, void*, long);
    using Fn_Decompress = long(__stdcall*)(
        const void*, long, void*, long,
        int, int, int, void*, long, void*, void*, void*, long, int);
    using Fn_GetCompressedBufferSizeNeeded = long(__stdcall*)(long);
    using Fn_GetDecodeBufferSize = long(__stdcall*)(long, int);
    using Fn_CompressOptions_GetDefault = CompressOptions*(__stdcall*)(int, int);

    // ========================================================================
    // Module state
    // ========================================================================

    static std::once_flag s_initFlag;
    static std::mutex s_mutex;          // protects explicit LoadDLL after init
    static HMODULE s_dll = nullptr;
    static std::string s_loadedPath;
    static std::string s_userSearchPath; // set by SetDLLSearchPath

    static Fn_Compress s_compress = nullptr;
    static Fn_Decompress s_decompress = nullptr;
    static Fn_GetCompressedBufferSizeNeeded s_getCompressedSize = nullptr;
    static Fn_GetDecodeBufferSize s_getDecodeSize = nullptr;
    static Fn_CompressOptions_GetDefault s_getDefaultOptions = nullptr;

    static constexpr const char* DLL_NAME = "oo2core_6_win64.dll";

    // ========================================================================
    // Internal helpers
    // ========================================================================

    static bool TryLoadFrom(const std::string& dir)
    {
        namespace fs = std::filesystem;
        auto dllPath = (fs::path(dir) / DLL_NAME).string();
        if (!fs::exists(dllPath))
            return false;

        HMODULE h = LoadLibraryA(dllPath.c_str());
        if (!h)
            return false;

        auto fnC  = reinterpret_cast<Fn_Compress>(GetProcAddress(h, "OodleLZ_Compress"));
        auto fnD  = reinterpret_cast<Fn_Decompress>(GetProcAddress(h, "OodleLZ_Decompress"));
        auto fnGC = reinterpret_cast<Fn_GetCompressedBufferSizeNeeded>(
            GetProcAddress(h, "OodleLZ_GetCompressedBufferSizeNeeded"));
        auto fnGD = reinterpret_cast<Fn_GetDecodeBufferSize>(
            GetProcAddress(h, "OodleLZ_GetDecodeBufferSize"));
        auto fnOD = reinterpret_cast<Fn_CompressOptions_GetDefault>(
            GetProcAddress(h, "OodleLZ_CompressOptions_GetDefault"));

        if (!fnC || !fnD || !fnGC || !fnGD || !fnOD)
        {
            FreeLibrary(h);
            return false;
        }

        // If we already had a DLL loaded, free the old one.
        if (s_dll)
            FreeLibrary(s_dll);

        s_dll = h;
        s_compress = fnC;
        s_decompress = fnD;
        s_getCompressedSize = fnGC;
        s_getDecodeSize = fnGD;
        s_getDefaultOptions = fnOD;
        s_loadedPath = dllPath;
        return true;
    }

    static void InitOnce()
    {
        // 1. User-specified search path (via SetDLLSearchPath)
        if (!s_userSearchPath.empty() && TryLoadFrom(s_userSearchPath))
        {
            Firelink::Info(std::format("[oodle] Loaded Oodle DLL from user path: {}", s_loadedPath));
            return;
        }

        // 2. Default: Elden Ring Steam install
        if (TryLoadFrom("C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game"))
        {
            Firelink::Info(std::format("[oodle] Loaded Oodle DLL from: {}", s_loadedPath));
            return;
        }

        Firelink::Warning(std::format("[oodle] {} not found. DCX_KRAK compression/decompression will be unavailable.", DLL_NAME));
    }

    static void EnsureInit()
    {
        std::call_once(s_initFlag, InitOnce);
    }

    static void RequireDLL()
    {
        EnsureInit();
        if (!s_dll)
            throw DCXError(
                "Oodle DLL (oo2core_6_win64.dll) not found. "
                "Call Oodle::SetDLLSearchPath() or Oodle::LoadDLL() with the directory containing the DLL, "
                "or place it in your Elden Ring Game folder.");
    }

    // ========================================================================
    // Public API
    // ========================================================================

    void SetDLLSearchPath(const std::string& directory)
    {
        std::lock_guard lock(s_mutex);
        s_userSearchPath = directory;
    }

    bool LoadDLL(const std::string& directory)
    {
        std::lock_guard lock(s_mutex);
        // Force past the once_flag by doing the load directly.
        // Also ensure the once_flag won't re-run its default search.
        std::call_once(s_initFlag, [] {});

        if (TryLoadFrom(directory))
        {
            Firelink::Info(std::format("[oodle] Loaded Oodle DLL from: {}", s_loadedPath));
            return true;
        }
        Firelink::Warning(std::format("[oodle] Failed to load Oodle DLL from: {}", directory));
        return false;
    }

    bool IsAvailable() noexcept
    {
        EnsureInit();
        return s_dll != nullptr;
    }

    const std::string& LoadedPath() noexcept
    {
        EnsureInit();
        static const std::string empty;
        return s_dll ? s_loadedPath : empty;
    }

    std::vector<std::byte> Decompress(
        const std::byte* compressed, const std::size_t compressed_size,
        const std::size_t decompressed_size)
    {
        RequireDLL();

        const long bufSize = s_getDecodeSize(static_cast<long>(decompressed_size), 1);
        std::vector<std::byte> out(static_cast<std::size_t>(bufSize));

        const long actual = s_decompress(
            compressed, static_cast<long>(compressed_size),
            out.data(), static_cast<long>(decompressed_size),
            FuzzSafe_Yes, CheckCRC_No, Verbosity_None,
            nullptr, 0, nullptr, nullptr, nullptr, 0, ThreadPhase_Unthreaded);

        if (actual <= 0)
        {
            Firelink::Warning(std::format("[oodle] Decompression returned {}. Using expected size {}.", actual, decompressed_size));
            out.resize(decompressed_size);
        }
        else
        {
            out.resize(static_cast<std::size_t>(actual));
        }
        return out;
    }

    std::vector<std::byte> Compress(const std::byte* data, const std::size_t size)
    {
        RequireDLL();

        const long maxSize = s_getCompressedSize(static_cast<long>(size));
        std::vector<std::byte> out(static_cast<std::size_t>(maxSize));

        CompressOptions* opts = s_getDefaultOptions(Compressor_Kraken, Level_Optimal2);
        opts->seekChunkReset = 1;     // required for game compatibility
        opts->seekChunkLen = 0x40000; // default, but explicit for authenticity

        const long actual = s_compress(
            Compressor_Kraken, data, static_cast<long>(size),
            out.data(), Level_Optimal2,
            opts, nullptr, nullptr, nullptr, 0);

        if (actual <= 0)
            throw DCXError("Oodle compression failed (returned " + std::to_string(actual) + ").");

        out.resize(static_cast<std::size_t>(actual));
        return out;
    }

} // namespace Firelink::Oodle

