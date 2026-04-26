// ImageImportManager — lazy texture discovery and caching for FromSoftware games.
//
// Given a game type and data root, registers known texture source locations
// (Binders, TPFs, loose files) for a given FLVER file, then lazily loads
// textures on demand and caches them for reuse across multiple FLVERs.

#pragma once

#include <FirelinkCore/Export.h>
#include <FirelinkCore/Binder.h>
#include <FirelinkCore/GameType.h>
#include <FirelinkCore/TPF.h>

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace Firelink
{

    /// @brief Image format enum.
    enum class ImageFormat : std::uint8_t
    {
        DDS = 0,
        PNG = 1,
        TGA = 2,
    };

    /// @brief Manager for importing and converting DDS textures used by FLVER models.
    ///
    /// @details Knows the standard locations of texture TPFs for FLVERs of varying types.
    class FIRELINK_CORE_API TextureFinder
    {
    public:
        /// @brief Construct a manager for the given game.
        /// @param game       Which FromSoftware game.
        /// @param dataRoot  Root of unpacked game data (e.g. "DARK SOULS REMASTERED", "ELDEN RING/Game").
        TextureFinder(GameType game, std::filesystem::path dataRoot);

        /// @brief Register texture source locations for a FLVER loaded from `flver_source_path`.
        ///
        /// `flver_source_path` is the path to the file the FLVER was loaded from
        /// (e.g. "chr/c2300.chrbnd.dcx" or a loose "map/.../m1234.flver").
        ///
        /// If the FLVER came from a Binder that has already been opened, pass it as
        /// `flver_binder` so its TPF entries can be scanned without re-reading the file.
        void RegisterFLVERSources(
            const std::filesystem::path& flverSourcePath,
            const Binder* flverBinder = nullptr,
            bool preferHiRes = true);

        /// @brief Look up a texture by stem. Lazily loads pending TPFs and Binders as needed.
        ///
        /// Returns nullptr if not found.
        ///
        /// @note Texture stems are not case-sensitive.
        const TPFTexture* GetTexture(
            const std::string& textureStem,
            const std::string& modelName = "");

        /// @brief Get texture data converted to the requested format.
        ///
        /// Returns empty vector if texture not found.
        ///
        /// @note Texture stems are not case-sensitive.
        std::vector<std::byte> GetTextureAs(
            const std::string& textureStem,
            ImageFormat format,
            const std::string& model_name = "");

        /// @brief Manually set the AET root directory for asset texture lookups (Elden Ring).
        void SetAETRoot(const std::filesystem::path& aetRoot)
        {
            m_aetRoot = aetRoot;
        }

        /// @brief Get the number of cached textures.
        [[nodiscard]] std::size_t CachedTextureCount() const;

    private:
        GameType m_game;
        std::filesystem::path m_dataRoot;
        std::filesystem::path m_aetRoot;

        // Pending binder file paths (lowercase stem -> path). Not yet opened.
        std::unordered_map<std::string, std::filesystem::path> m_pendingBinders;

        // Pending TPF sources (lowercase stem -> file path or BinderEntry).
        // BinderEntry is stored by value (owns its data) for simple lifetime management.
        std::unordered_map<std::string, std::variant<std::filesystem::path, BinderEntry>> m_pendingTPFs;

        // Loaded texture cache (lowercase stem -> TPFTexture).
        std::unordered_map<std::string, TPFTexture> m_textureCache;

        // Already-scanned paths to avoid re-scanning.
        std::unordered_set<std::string> m_scannedBinders;
        std::unordered_set<std::string> m_scannedTPFs;

        mutable std::shared_mutex m_mutex;

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
