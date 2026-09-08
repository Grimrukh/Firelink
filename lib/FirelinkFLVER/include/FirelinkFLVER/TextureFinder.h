// TextureFinder — lazy texture discovery and caching for FromSoftware games.
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
        /// @param generosity  Generosity level: search in other locations that may be used lazily by assets.
        TextureFinder(GameType game, std::filesystem::path dataRoot, unsigned int generosity = 0);

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

        /// @brief Get the names of all pending TPFs.
        [[nodiscard]] std::vector<std::string> PendingTPFStems() const;

        /// @brief Get the number of cached textures.
        [[nodiscard]] std::size_t CachedTextureCount() const;

    private:
        // Game being searched, which determines custom texture resolution strategies.
        GameType m_game;

        // Game data root, required to find arbitrary textures (not bundled in/adjacent to FLVERs/BNDs).
        std::filesystem::path m_dataRoot;

        // Generosity level: search in other locations that may be used lazily by assets.
        unsigned int m_generosity = 0;

        // Pending binder file paths (lowercase stem -> full path). Not yet opened.
        std::unordered_map<std::string, std::filesystem::path> m_pendingBinderPaths;

        // Pending TPF sources (lowercase stem -> file path or BinderEntry shared pointer).
        std::unordered_map<std::string, std::variant<std::filesystem::path, std::shared_ptr<BinderEntry>>> m_pendingTpfs;

        // Loaded texture cache (lowercase stem -> TPFTexture).
        std::unordered_map<std::string, TPFTexture> m_textureCache;

        // Already-scanned TPF lowercase stems and lowercase full Binder paths to avoid re-scanning.
        // These are TPFs/Binders that have been popping from the pending map and fully searched.
        std::unordered_set<std::string> m_loadedTpfStems;
        std::unordered_set<std::string> m_loadedBinderPaths;

        // Directories that have already been fully scanned for TPFs (e.g. 'map/tx' in PTDE).
        std::unordered_set<std::string> m_scannedDirs;

        // Textures that could not be found and need not be searched for again (global process).
        std::unordered_set<std::string> m_missingStems;

        mutable std::shared_mutex m_mutex;

        // --- Registration helpers (no locking — caller must hold unique lock) ---

        //! @brief Register sources for all Map Piece models that may be in specific `mapBlockDir`.
        //! @param mapBlockDir  Directory containing Map Piece FLVER models (e.g. "{root}/map/m10_00_00_00").
        void RegisterSpecificMapTextures(const std::filesystem::path& mapBlockDir);

        //! @brief Register sources for all Map Piece models that may use shared textures in `mapAreaDir`.
        //! @details Looks for TPFBHD/BDT split binders and multi-texture TPF files (e.g. 'm10_9999.tpf').
        //! @param mapAreaDir  Directory containing shared map area textures (e.g. "{root}/map/m10").
        void RegisterMapAreaTextures(const std::filesystem::path& mapAreaDir);

        //! @brief Register all TPFs in a directory matching the given glob pattern.
        //! @details Used in PTDE ('map/tx' and 'chr/cXXXX' subfolders) and DeS ('chr') only.
        //! @returns List of lower-case file stems found in directory.
        std::vector<std::string> RegisterTPFsInDir(const std::filesystem::path& dir, const std::string& glob = "*.tpf");

        //! @brief Register all TPFs in split BND (TPFBHD/TPFBDT) for a given character model.
        //! @details Used in DSR only.
        void RegisterChrTPFBDTs(const std::filesystem::path& chrDir, const Binder& chrbnd);

        //! @brief Register all TPFs in TEXBND for a given character model (e.g. 'chr/c1234.texbnd').
        //! @details Used since Dark Souls 3 (so Sekiro, Elden Ring).
        void RegisterChrTexbnd(const std::filesystem::path& chrDir, const std::string& modelStem, const std::string& res);

        //! @brief Find and immediately load all textures in 'parts/Common*.tpf'.
        //! @details Always called on TextureFinder construction, since these textures can appear anywhere.
        void LoadPartsCommonTPFs(const std::filesystem::path& partsDir);

        //! @brief Scan a Binder for TPFs and register them as pending TPF sources, awaiting
        //! texture match later. Does not load TPFs immediately.
        void RegisterAllTPFsInBinder(const Binder& binder);

        // --- Registration helpers for specific texture stems that can be found from the data root ---

        //! @brief Find an 'aetXXX_*' texture in the '{data}/asset/aet' directory.
        //! @returns True if texture source is found and registered, false otherwise.
        bool RegisterSpecificAssetTexture(const std::string& textureStem);

        //! @brief Find an 'oXXXX*' texture in {data}/obj/oXXXX.objbnd[.dcx].
        //! @returns True if texture source is found and registered, false otherwise.
        bool RegisterSpecificObjectTexture(const std::string& textureStem);

        // --- First-time stem registration for Binders/TPFs ---

        //! @brief Add given Binder to pending Binder list.
        //! @details Pending bindings are only ever loaded as a last resort when a texture cannot
        //! be found in any loaded TPFs. This requires generosity level 1 or higher.
        //! @todo Not currently used anywhere, as all known relevant Binders are immediately loaded.
        void RegisterBinder(const std::filesystem::path& binderPath);

        //! @brief Add given TPF to pending TPF list (from path).
        //! @details Pending TPFs are loaded when their names are detected to be a good match
        //! for a requested texture (either an exact match or a prefix match). No generosity required.
        void RegisterTPF(const std::filesystem::path& tpfPath);

        //! @brief Add given TPF to pending TPF list (from Binder entry).
        //! @details Pending TPFs are loaded when their names are detected to be a good match
        //! for a requested texture (either an exact match or a prefix match). No generosity required.
        void RegisterTPF(const std::shared_ptr<BinderEntry>& tpfBinderEntry);

        // --- Lazy loading helpers (no locking — caller must hold unique lock) ---

        void LoadPendingBinder(const std::string& binderStem);
        void LoadTPF(const std::string& lowerTpfStem);
    };

} // namespace Firelink
