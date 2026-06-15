// TextureFinder implementation.
#include <FirelinkFLVER/TextureFinder.h>

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/DDS.h>
#include <FirelinkCore/Logging.h>
#include <FirelinkCore/Paths.h>

#include <regex>

#include <algorithm>

namespace Firelink
{
    namespace fs = std::filesystem;

    namespace
    {
        // Matches *.tpf or *.tpf.dcx (case-insensitive).
        const std::string TPF_RE_STR = R"(.*\.tpf(\.dcx)?$)";
        const std::regex TPF_RE(TPF_RE_STR, std::regex_constants::icase);

        // Matches mXX_* (case-sensitive).
        const std::regex MXX_TEX_RE(R"(m\d{2}_.+)");

        bool IsTPFFileName(const std::string& name)
        {
            return name.ends_with(".tpf") || name.ends_with(".tpf.dcx");
        }

        std::string ToLowerStem(const std::filesystem::path& path)
        {
            return ToLower(StemOf(path));
        }

        Binder::Ptr LoadTPFBXF(const fs::path& bhdPath)
        {
            // Find adjacent BDT.
            auto bdtPath = bhdPath;
            bdtPath.replace_extension(""); // remove .tpfbhd
            const auto bdt_stem = bdtPath.filename().string();
            bdtPath = bhdPath.parent_path() / (bdt_stem + ".tpfbdt");
            if (!fs::is_regular_file(bdtPath))
                return nullptr;

            const auto bhd_data = BinaryReadWrite::ReadFileBytes(bhdPath);
            const auto bdt_data = BinaryReadWrite::ReadFileBytes(bdtPath);
            Binder::Ptr binder = Binder::FromSplitBytes(
                bhd_data.data(), bhd_data.size(),
                bdt_data.data(), bdt_data.size());

            return std::move(binder);
        }


    }

    // ========================================================================
    // Constructor
    // ========================================================================

    TextureFinder::TextureFinder(const GameType game, fs::path dataRoot)
        : m_game(game), m_dataRoot(std::move(dataRoot))
    {
        // Eagerly register parts/Common*.tpf textures.
        const fs::path partsDir = m_dataRoot / "parts";
        if (fs::is_directory(partsDir))
            RegisterPartsCommonTPFs(partsDir);
    }

    // ========================================================================
    // RegisterFLVERSources
    // ========================================================================

    void TextureFinder::RegisterFLVERSources(
        const fs::path& flverSourcePath,
        const Binder* flverBinder,
        const bool preferHiRes)
    {
        // Multiple threads may use the same TextureFinder.
        std::unique_lock lock(m_mutex);

        const auto sourceName = flverSourcePath.filename().string();

        // Remove .dcx suffix for type detection.
        auto baseName = sourceName;
        if (baseName.ends_with(".dcx"))
            baseName = baseName.substr(0, baseName.size() - 4);

        const auto modelStem = baseName.substr(0, baseName.find('.'));
        const auto sourceDir = flverSourcePath.parent_path();

        // MAP PIECES
        if (modelStem.starts_with("m") && baseName.ends_with(".flver"))
        {
            // Loose Map Piece FLVER. Texture sources vary by game.
            RegisterMapTextures(sourceDir);
            return;
        }

        if (baseName.ends_with(".mapbnd") || baseName.ends_with(".geombnd"))
        {
            // Elden Ring Map Piece/Asset FLVERs use 'aet' textures, which are always checked.
            // Nothing additional to register.
            return;
        }

        // CHARACTERS
        if (modelStem.starts_with("c") && baseName.ends_with(".flver"))
        {
            // Loose Character FLVER.
            RegisterChrLooseTPFs(sourceDir);
            return;
        }

        if (baseName.ends_with(".chrbnd"))
        {
            // Character textures sources vary by game, the most variance out of any FLVER category.
            if (flverBinder)
            {
                ScanBinderForTPFs(*flverBinder);

                switch (m_game)
                {
                    case GameType::DemonsSouls:
                        RegisterChrLooseTPFs(sourceDir);
                        break;
                    case GameType::DarkSoulsPTDE:
                        RegisterChrLooseTPFs(sourceDir / modelStem);
                        break;
                    case GameType::DarkSoulsDSR:
                        RegisterChrTPFBDTs(sourceDir, *flverBinder);
                        break;
                    case GameType::Bloodborne:
                        break; // all TPFs inside CHRBND
                    case GameType::DarkSouls3:
                    case GameType::Sekiro:
                        RegisterChrTexbnd(sourceDir, modelStem, "");
                        break;
                    case GameType::EldenRing:
                        RegisterChrTexbnd(sourceDir, modelStem, preferHiRes ? "_h" : "_l");
                        break;
                }
            }

            // Check for cXXX9 shared texture CHRBND.
            if (modelStem.size() >= 5 && modelStem[4] != '9')
            {
                const auto c9_stem = modelStem.substr(0, 4) + "9";
                auto c9_name = c9_stem + ".chrbnd";
                if (flverSourcePath.extension() == ".dcx" || sourceName.ends_with(".dcx"))
                    c9_name += ".dcx";
                const auto c9_path = sourceDir / c9_name;
                const auto c9_key = ToLower(c9_path.string());
                if (!m_loadedBinderPaths.contains(c9_key) && fs::is_regular_file(c9_path))
                {
                    m_loadedBinderPaths.insert(c9_key);
                    const Binder::CPtr c9_binder = Binder::FromPath(c9_path);
                    ScanBinderForTPFs(*c9_binder);

                    if (m_game == GameType::DarkSoulsDSR)
                        RegisterChrTPFBDTs(sourceDir, *c9_binder);
                }
            }

            return;
        }
        // ALL OTHER BINDERS (including 'partsbnd' Equipment Binders)
        if (flverBinder)
            ScanBinderForTPFs(*flverBinder);
    }

    // ========================================================================
    // GetTexture
    // ========================================================================

    const TPFTexture* TextureFinder::GetTexture(
        const std::string& textureStem, const std::string& modelName)
    {
        const auto lowerTextureStem = ToLower(textureStem);
        const auto lowerModelName = ToLower(modelName);

        // 0. Fast path: check cache with shared lock.
        {
            std::shared_lock lock(m_mutex);
            const auto it = m_textureCache.find(lowerTextureStem);
            if (it != m_textureCache.end())
                return &it->second;
        }

        // Slow path: unique lock for lazy loading.
        std::unique_lock lock(m_mutex);

        // Double-check texture cache after acquiring unique lock.
        if (const auto it = m_textureCache.find(lowerTextureStem); it != m_textureCache.end())
            return &it->second;

        // If texture is known to be missing (non-findable), ignore it.
        // Only texture stems with globally findable patterns ('aet*', 'o*', 'm*') are added to this.
        if (m_missingStems.contains(lowerTextureStem))
            return nullptr;

        if (m_game == GameType::EldenRing && lowerTextureStem.starts_with("aet"))
        {
            if (!RegisterSpecificAssetTexture(lowerTextureStem))
            {
                m_missingStems.insert(lowerTextureStem);
                Warning("Asset AET texture not found for '{}'", textureStem);
                return nullptr;
            }
            // Otherwise, texture can probably be found below.
        }
        else if (m_game != GameType::EldenRing && lowerTextureStem.starts_with("o"))
        {
            if (!RegisterSpecificObjectTexture(lowerTextureStem))
            {
                m_missingStems.insert(lowerTextureStem);
                Warning("Expected object binder not found for texture '{}'.", textureStem);
                return nullptr;
            }
            // Otherwise, texture can probably be found below.
        }
        else if (m_game != GameType::EldenRing && std::regex_match(lowerTextureStem, MXX_TEX_RE))
        {
            if (!RegisterSpecificMapTexture(lowerTextureStem))
            {
                m_missingStems.insert(lowerTextureStem);
                Warning("Map texture not found in map: '{}'", textureStem);
                return nullptr;
            }
            // Otherwise, texture can probably be found below.
        }
        // TODO: If characters can randomly share textures with each other, other than the known cXXX9 case, this
        //  would be where to check.

        // Check pending TPFs for EXACT stem match.
        if (m_pendingTpfs.contains(lowerTextureStem))
        {
            LoadTPF(lowerTextureStem);
            if (const auto it = m_textureCache.find(lowerTextureStem); it != m_textureCache.end())
                return &it->second;
        }

        // Check pending TPFs for PREFIX stem match (multi-DDS TPFs).
        // If the texture stem or model name starts with the TPF stem, we open it.
        for (auto it = m_pendingTpfs.begin(); it != m_pendingTpfs.end(); )
        {
            auto& lowerPendingTpfStem = it->first;
            if (lowerTextureStem.starts_with(lowerPendingTpfStem) || (!lowerModelName.empty() && lowerModelName.starts_with(lowerPendingTpfStem)))
            {
                const std::string stemCopy = lowerPendingTpfStem; // copy before invalidation
                ++it;
                LoadTPF(stemCopy);
                if (auto cit = m_textureCache.find(lowerTextureStem); cit != m_textureCache.end())
                    return &cit->second;
            }
            else
            {
                ++it;
            }
        }

        // Last resort: load all pending Binders and try again.
        if (!m_pendingBinderPaths.empty())
        {
            auto binderStems = std::vector<std::string>();
            binderStems.reserve(m_pendingBinderPaths.size());
            for (const auto& stem : m_pendingBinderPaths | std::views::keys)
                binderStems.push_back(stem);

            for (auto& stem : binderStems)
                LoadPendingBinder(stem);

            // Retry with new TPF sources extracted from all pending Binders.
            lock.unlock();
            return GetTexture(textureStem, modelName);
        }

        return nullptr;
    }

    // ========================================================================
    // GetTextureAs
    // ========================================================================

    std::vector<std::byte> TextureFinder::GetTextureAs(
        const std::string& textureStem, const ImageFormat format, const std::string& model_name)
    {
        const auto* tex = GetTexture(textureStem, model_name);
        if (!tex || tex->data.empty())
            return {};

        switch (format)
        {
            case ImageFormat::DDS:
                return tex->data; // copy
            case ImageFormat::PNG:
                return tex->ToDDS().ToPNG();
            case ImageFormat::TGA:
                return tex->ToDDS().ToTGA();
        }
        return {};
    }

    // ========================================================================
    // Read-only getters
    // ========================================================================

    std::vector<std::string> TextureFinder::PendingTPFStems() const
    {
        std::vector<std::string> names;
        names.reserve(m_pendingTpfs.size());
        for (const auto& tpfStem : m_pendingTpfs | std::views::keys)
            names.push_back(tpfStem);
        return names;
    }

    std::size_t TextureFinder::CachedTextureCount() const
    {
        std::shared_lock lock(m_mutex);
        return m_textureCache.size();
    }

    // ========================================================================
    // Registration helpers
    // ========================================================================

    void TextureFinder::RegisterMapTextures(const fs::path& sourceDir)
    {
        // Extract map area from directory name (mAA_BB_CC_DD → mAA).
        auto dir_name = sourceDir.filename().string();
        static const std::regex map_re(R"(^(m\d\d)_)");
        std::smatch match;
        if (!std::regex_search(dir_name, match, map_re))
            return;
        const auto area = match[1].str();

        const auto map_area_dir = (sourceDir.parent_path() / area).lexically_normal();
        RegisterMapAreaTextures(map_area_dir);

        if (m_game == GameType::DarkSoulsPTDE)
        {
            // PTDE: also check map/tx folder.
            const auto tx_dir = (sourceDir.parent_path() / "tx").lexically_normal();
            if (fs::is_directory(tx_dir))
                RegisterTPFsInDir(tx_dir);
        }
    }

    void TextureFinder::RegisterMapAreaTextures(const fs::path& mapAreaDir)
    {
        if (!fs::is_directory(mapAreaDir))
            return;

        for (auto& dirEntry : fs::directory_iterator(mapAreaDir))
        {
            if (!dirEntry.is_regular_file()) continue;
            const auto lowerPath = ToLower(dirEntry.path().string());
            const auto lowerName = ToLower(dirEntry.path().filename().string());

            if (lowerName.ends_with(".tpfbhd"))
            {
                if (m_loadedBinderPaths.contains(lowerPath))
                    continue;

                // Load all TPFs (as pending TPFs) from split Binder.
                m_loadedBinderPaths.insert(lowerPath);
                const auto binder = LoadTPFBXF(dirEntry.path());
                for (auto& entry : binder->FindEntriesByNameRegex(TPF_RE_STR, /*fullMatch*/ true))
                {
                    RegisterTPF(entry);
                }
            }
            else if (IsTPFFileName(lowerName))
            {
                auto stem = ToLowerStem(dirEntry);
                if (m_loadedTpfStems.contains(stem))
                    continue;

                // Multi-texture map TPF — load immediately (steal TPFTextures).
                m_loadedTpfStems.insert(stem);
                try
                {
                    for (auto& tex : TPF::FromPath(dirEntry)->Textures())
                        m_textureCache.try_emplace(ToLower(tex.stem), std::move(tex));
                }
                catch (const std::exception& e)
                {
                    Warning("[TextureFinder] Failed to load TPF: " + dirEntry.path().string() + " — " + e.what());
                }
            }
        }
    }

    void TextureFinder::RegisterTPFsInDir(const fs::path& dir, const std::string& glob)
    {
        if (!fs::is_directory(dir))
            return;

        for (auto& dirEntry : fs::directory_iterator(dir))
        {
            if (!dirEntry.is_regular_file())
                continue;
            auto name = dirEntry.path().filename().string();
            if (MatchesGlob(name, glob) || MatchesGlob(name, glob + ".dcx"))
            {
                RegisterTPF(dirEntry);
            }
        }
    }

    void TextureFinder::RegisterChrLooseTPFs(const fs::path& dir)
    {
        if (!fs::is_directory(dir))
            return;

        for (auto& dirEntry : fs::directory_iterator(dir))
        {
            if (!dirEntry.is_regular_file())
                continue;
            if (std::regex_match(dirEntry.path().filename().string(), TPF_RE))
            {
                RegisterTPF(dirEntry);
            }
        }
    }

    void TextureFinder::RegisterChrTPFBDTs(const fs::path& source_dir, const Binder& chrbnd)
    {
        // Look for a .chrtpfbhd entry inside the CHRBND.
        static const std::string bhd_re(R"(\.chrtpfbhd$)");
        BinderEntry::Ptr bhdEntry;
        try
        {
            bhdEntry = chrbnd.FindEntryByNameRegex(bhd_re);
        }
        catch (BinderEntryNotFoundError&)
        {
            // No .chrtpfbhd entry — no split TPF/BDT, so nothing to register.
            return;
        }

        const auto bdt_stem = bhdEntry->GetPathStem();
        const auto bdt_path = source_dir / (bdt_stem + ".chrtpfbdt");
        if (!fs::is_regular_file(bdt_path))
            return;

        try
        {
            const auto bdt_data = BinaryReadWrite::ReadFileBytes(bdt_path);
            auto bxf = Binder::FromSplitBytes(
                bhdEntry->GetData().data(), bhdEntry->GetData().size(),
                bdt_data.data(), bdt_data.size());

            // Get new TPF entries from transient Binder.
            for (auto& tpfEntry : bxf->FindEntriesByNameRegex(TPF_RE_STR, /*fullMatch*/ true))
            {
                RegisterTPF(tpfEntry);
            }
        }
        catch (const std::exception& e)
        {
            Warning("[TextureFinder] Failed to read CHRTPFBDT: " + bdt_path.string() + " — " + e.what());
        }
    }

    void TextureFinder::RegisterChrTexbnd(
        const fs::path& source_dir, const std::string& model_stem, const std::string& res)
    {
        const fs::path texbndPath = source_dir / (model_stem + res + ".texbnd" + (UsesBinderDcx(m_game) ? ".dcx" : ""));
        if (!fs::is_regular_file(texbndPath))
            return;

        const auto lowerTexbndPath = ToLower(texbndPath.string());
        if (m_loadedBinderPaths.contains(lowerTexbndPath))
            return;
        m_loadedBinderPaths.insert(lowerTexbndPath);

        try
        {
            const auto texbnd = Binder::FromPath(texbndPath);

            for (auto& tpfEntry : texbnd->FindEntriesByNameRegex(TPF_RE_STR, /*fullMatch*/ true))
            {
                auto stem = ToLower(tpfEntry->GetPathStem());
                if (m_loadedTpfStems.contains(stem))
                    continue;
                m_loadedTpfStems.insert(stem);

                // Multi-texture TPF — load immediately and cache textures.
                const auto tpf = TPF::FromBytes(tpfEntry->GetData());
                for (auto& tex : tpf->Textures())
                    m_textureCache.try_emplace(ToLower(tex.stem), std::move(tex));
            }
        }
        catch (const std::exception& e)
        {
            Warning("[TextureFinder] Failed to read texbnd: {} — {}", texbndPath.string(), e.what());
        }
    }

    void TextureFinder::RegisterPartsCommonTPFs(const fs::path& partsDir)
    {
        if (!fs::is_directory(partsDir))
            return;

        for (auto& dirEntry : fs::directory_iterator(partsDir))
        {
            if (!dirEntry.is_regular_file())
                continue;

            auto name = dirEntry.path().filename().string();

            bool matchesGlob;
            if (UsesBinderDcx(m_game))
                matchesGlob = MatchesGlob(name, "Common*.tpf.dcx") || MatchesGlob(name, "common*.tpf.dcx");
            else
                matchesGlob = MatchesGlob(name, "Common*.tpf") || MatchesGlob(name, "common*.tpf");

            if (matchesGlob)
            {
                auto stem = ToLowerStem(dirEntry);
                if (m_loadedTpfStems.contains(stem))
                    continue;

                m_loadedTpfStems.insert(stem);
                try
                {
                    for (auto& tex : TPF::FromPath(dirEntry.path())->Textures())
                        m_textureCache.try_emplace(ToLower(tex.stem), std::move(tex));
                }
                catch (const std::exception& e)
                {
                    Warning("[TextureFinder] Failed to load Common TPF: " + dirEntry.path().string() + " — " + e.what());
                }
            }
        }
    }

    void TextureFinder::ScanBinderForTPFs(const Binder& binder)
    {
        for (auto& entry : binder.FindEntriesByNameRegex(TPF_RE_STR, /*fullMatch*/ true))
        {
            RegisterTPF(entry);
        }
    }

    bool TextureFinder::RegisterSpecificAssetTexture(const std::string& textureStem)
    {
        const auto aetPrefix = textureStem.substr(0, 6);  // "aetXXX"
        const auto aetTpfStem = textureStem.substr(0, 10); // "aetXXX_XXX"
        const fs::path aetTpfPath = m_dataRoot / "asset/aet" / aetPrefix / (aetTpfStem + ".tpf.dcx");

        // If the expected TPF doesn't exist, we definitely can't find this texture.
        if (!fs::is_regular_file(aetTpfPath))
        {
            return false;
        }
        // If the TPF exists but has already been scanned,

        if (!m_loadedTpfStems.contains(aetTpfStem))
            m_pendingTpfs.try_emplace(aetTpfStem, aetTpfPath);

        return true;
    }

    bool TextureFinder::RegisterSpecificObjectTexture(const std::string& textureStem)
    {
        // Object texture. Can be used lazily by any FLVERs in the same map as the relevant object model.
        // We find the OBJBND and load its TPFs immediately.
        const auto objModelStem = textureStem.substr(0, 5);  // "oXXXX"
        const auto fileName = objModelStem + ".objbnd" + (UsesBinderDcx(m_game) ? ".dcx" : "");
        const fs::path objbndPath = m_dataRoot / "obj" / fileName;
        const std::string lowerObjbndPath = ToLower(objbndPath.string());

        if (!fs::is_regular_file(objbndPath) || m_loadedBinderPaths.contains(lowerObjbndPath))
        {
            // Binder does not exist or has already been loaded. No chance of finding texture.
            return false;
        }

        m_loadedBinderPaths.insert(lowerObjbndPath);
        const auto binder = Binder::FromPath(objbndPath);
        bool anyTpfsFound = false;
        for (auto& entry : binder->FindEntriesByNameRegex(TPF_RE_STR, /*fullMatch*/ true))
        {
            RegisterTPF(entry);
            anyTpfsFound = true;
        }

        // Binder may contain multi-DDS TPFs, so we can't be 100% sure that the texture will be found.
        // It's worth looking inside the new TPFs though.
        return anyTpfsFound;
    }

    bool TextureFinder::RegisterSpecificMapTexture(const std::string& textureStem)
    {
        // Many object FLVERs use textures from the map they expect to be used in.
        // Some vanilla map piece FLVERs also use textures from maps they expect to be loaded at the same
        // time. This is especially common in DS1.

        const std::string mapArea = textureStem.substr(0, 3);  // 'mXX'
        const fs::path mapAreaDir = m_dataRoot / "map" / mapArea;
        if (!fs::is_directory(mapAreaDir))
            return false;

        RegisterMapAreaTextures(mapAreaDir);

        // PTDE has textures in 'map/tx'.
        const fs::path looseTxDirPath = m_dataRoot / "map/tx";
        if (fs::is_directory(looseTxDirPath))
            RegisterTPFsInDir(looseTxDirPath);

        // We guess that the texture may be found.
        return true;
    }

    void TextureFinder::RegisterBinder(const std::filesystem::path& binderPath)
    {
        if (const auto lowerPath = ToLower(binderPath.string()); !m_loadedBinderPaths.contains(lowerPath))
            m_pendingBinderPaths.try_emplace(ToLowerStem(binderPath), binderPath);
    }

    void TextureFinder::RegisterTPF(const std::filesystem::path& tpfPath)
    {
        if (const auto stem = ToLowerStem(tpfPath); !m_loadedTpfStems.contains(stem))
            m_pendingTpfs.try_emplace(stem, tpfPath);
    }

    void TextureFinder::RegisterTPF(const std::shared_ptr<BinderEntry>& tpfBinderEntry)
    {
        if (const auto stem = ToLower(tpfBinderEntry->GetPathStem()); !m_loadedTpfStems.contains(stem))
            m_pendingTpfs.try_emplace(stem, tpfBinderEntry);
    }

    // ========================================================================
    // Lazy loading
    // ========================================================================

    void TextureFinder::LoadPendingBinder(const std::string& binderStem)
    {
        const auto it = m_pendingBinderPaths.find(binderStem);
        if (it == m_pendingBinderPaths.end()) return;

        const auto path = std::move(it->second);
        m_pendingBinderPaths.erase(it);
        m_loadedBinderPaths.insert(ToLower(path.string()));

        Info("[TextureFinder] Loading binder: {}", path.string());

        try
        {
            // Check if it's a TPFBHD (split binder).
            const auto name = ToLower(path.filename().string());
            if (name.ends_with(".tpfbhd"))
            {
                const Binder::CPtr binder = LoadTPFBXF(path);
                // Get new TPF entries from transient Binder.
                for (auto& entry : binder->FindEntriesByNameRegex(TPF_RE_STR, /*fullMatch*/ true))
                {
                    RegisterTPF(entry);
                }
            }
            else
            {
                const auto binder = Binder::FromPath(path);
                ScanBinderForTPFs(*binder);
            }
        }
        catch (const std::exception& e)
        {
            Warning("[TextureFinder] Failed to load binder: " + path.string() + " — " + e.what());
        }
    }

    void TextureFinder::LoadTPF(const std::string& lowerTpfStem)
    {
        const auto it = m_pendingTpfs.find(lowerTpfStem);
        if (it == m_pendingTpfs.end()) return;

        const auto source = std::move(it->second);
        m_pendingTpfs.erase(it);
        m_loadedTpfStems.insert(lowerTpfStem);

        try
        {
            std::unique_ptr<TPF> tpf = nullptr;
            std::vector<std::byte> buf;

            if (const auto* path = std::get_if<fs::path>(&source))
            {
                // Load TPF from path.
                tpf = TPF::FromPath(*path);
            }
            else if (const auto& entry = std::get_if<std::shared_ptr<BinderEntry>>(&source))
            {
                // Load TPF from borrowed entry bytes.
                const auto entryData = (*entry)->GetData();
                const std::byte* tpfData = entryData.data();
                const std::size_t tpfSize = entryData.size();
                tpf = TPF::FromBytes(tpfData, tpfSize);
            }

            if (tpf)
            {
                for (auto& tex : tpf->Textures())
                    m_textureCache.try_emplace(ToLower(tex.stem), std::move(tex));
            }
        }
        catch (const std::exception& e)
        {
            Warning("[TextureFinder] Failed to load TPF '" + lowerTpfStem + "': " + e.what());
        }
    }

} // namespace Firelink
