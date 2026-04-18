// ImageImportManager — lazy texture discovery and caching for FromSoftware games.
//
// Given a game type and data root, registers known texture source locations
// (Binders, TPFs, loose files) for a given FLVER file, then lazily loads
// textures on demand and caches them for reuse across multiple FLVERs.

#pragma once

#include <FirelinkCore/Export.h>
#include <FirelinkCore/Binder.h>
#include <FirelinkCore/TPF.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace Firelink
{
    // --- GameType ----------------------------------------------------------------

    enum class GameType : std::uint8_t
    {
        DemonsSouls = 0,
        DarkSoulsPTDE = 1,
        DarkSoulsDSR = 2,
        Bloodborne = 3,
        DarkSouls3 = 4,
        Sekiro = 5,
        EldenRing = 6,
    };

    // --- ImageFormat --------------------------------------------------------------

    enum class ImageFormat : std::uint8_t
    {
        DDS = 0,
        PNG = 1,
        TGA = 2,
    };

    // --- ImageImportManager -------------------------------------------------------

    class FIRELINK_CORE_API ImageImportManager
    {
    public:
        /// @brief Construct a manager for the given game.
        /// @param game       Which FromSoftware game.
        /// @param data_root  Root of unpacked game data (e.g. "DARK SOULS REMASTERED", "ELDEN RING/Game").
        ImageImportManager(GameType game, std::filesystem::path data_root);

        /// @brief Register texture source locations for a FLVER loaded from `flver_source_path`.
        ///
        /// `flver_source_path` is the path to the file the FLVER was loaded from
        /// (e.g. "chr/c2300.chrbnd.dcx" or a loose "map/.../m1234.flver").
        ///
        /// If the FLVER came from a Binder that has already been opened, pass it as
        /// `flver_binder` so its TPF entries can be scanned without re-reading the file.
        void RegisterFLVERSources(
            const std::filesystem::path& flver_source_path,
            const Binder* flver_binder = nullptr,
            bool prefer_hi_res = true);

        /// @brief Look up a texture by stem (case-insensitive). Returns nullptr if not found.
        ///
        /// Lazily loads pending TPFs and Binders as needed.
        const TPFTexture* GetTexture(const std::string& texture_stem,
                                     const std::string& model_name = "");

        /// @brief Get texture data converted to the requested format.
        ///
        /// Returns empty vector if texture not found.
        std::vector<std::byte> GetTextureAs(const std::string& texture_stem,
                                            ImageFormat format,
                                            const std::string& model_name = "");

        /// @brief Manually set the AET root directory for asset texture lookups.
        void SetAETRoot(const std::filesystem::path& aet_root) { aet_root_ = aet_root; }

        /// @brief Get the number of cached textures.
        [[nodiscard]] std::size_t CachedTextureCount() const;

    private:
        GameType game_;
        std::filesystem::path data_root_;
        std::filesystem::path aet_root_;

        // Pending binder file paths (lowercase stem → path). Not yet opened.
        std::unordered_map<std::string, std::filesystem::path> pending_binders_;

        // Pending TPF sources (lowercase stem → file path or BinderEntry).
        // BinderEntry is stored by value (owns its data) for simple lifetime management.
        std::unordered_map<std::string, std::variant<std::filesystem::path, BinderEntry>> pending_tpfs_;

        // Loaded texture cache (lowercase stem → TPFTexture).
        std::unordered_map<std::string, TPFTexture> texture_cache_;

        // Already-scanned paths to avoid re-scanning.
        std::unordered_set<std::string> scanned_binders_;
        std::unordered_set<std::string> scanned_tpfs_;

        mutable std::shared_mutex mutex_;

        // --- Helpers ---

        static std::string ToLower(const std::string& s);
        static std::string StemOf(const std::filesystem::path& p);
        static std::string StemOf(const BinderEntry& e);

        /// Read a file and decompress DCX if needed.
        static std::vector<std::byte> ReadAndDecompress(const std::filesystem::path& path);

        // --- Registration helpers (no locking — caller must hold unique lock) ---

        void RegisterMapTextures(const std::filesystem::path& source_dir);
        void RegisterMapAreaTextures(const std::filesystem::path& map_area_dir);
        void RegisterTPFsInDir(const std::filesystem::path& dir, const std::string& glob = "*.tpf");
        void RegisterChrLooseTPFs(const std::filesystem::path& dir);
        void RegisterChrTPFBDTs(const std::filesystem::path& source_dir, const Binder& chrbnd);
        void RegisterChrTexbnd(const std::filesystem::path& source_dir, const std::string& model_stem, const std::string& res);
        void RegisterPartsCommonTPFs(const std::filesystem::path& parts_dir);
        void ScanBinderForTPFs(const Binder& binder);

        // --- Lazy loading helpers (no locking — caller must hold unique lock) ---

        void LoadBinder(const std::string& binder_stem);
        void LoadTPF(const std::string& tpf_stem);
    };

} // namespace Firelink

