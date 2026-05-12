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

        bool IsTPFFileName(const std::string& name)
        {
            return name.ends_with(".tpf") || name.ends_with(".tpf.dcx");
        }

        std::string ToLowerStem(const std::filesystem::path& path)
        {
            return ToLower(StemOf(path));
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

        const auto source_name = flverSourcePath.filename().string();
        // Remove .dcx suffix for type detection.
        auto baseName = source_name;
        if (baseName.ends_with(".dcx"))
            baseName = baseName.substr(0, baseName.size() - 4);

        const auto modelStem = baseName.substr(0, baseName.find('.'));
        const auto source_dir = flverSourcePath.parent_path();

        // MAP PIECES
        if (modelStem.starts_with("m") && baseName.ends_with(".flver"))
        {
            // Loose Map Piece FLVER. Texture sources vary by game.
            RegisterMapTextures(source_dir);
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
            RegisterChrLooseTPFs(source_dir);
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
                        RegisterChrLooseTPFs(source_dir);
                        break;
                    case GameType::DarkSoulsPTDE:
                        RegisterChrLooseTPFs(source_dir / modelStem);
                        break;
                    case GameType::DarkSoulsDSR:
                        RegisterChrTPFBDTs(source_dir, *flverBinder);
                        break;
                    case GameType::Bloodborne:
                        break; // all TPFs inside CHRBND
                    case GameType::DarkSouls3:
                    case GameType::Sekiro:
                        RegisterChrTexbnd(source_dir, modelStem, "");
                        break;
                    case GameType::EldenRing:
                        RegisterChrTexbnd(source_dir, modelStem, preferHiRes ? "_h" : "_l");
                        break;
                }
            }

            // Check for cXXX9 shared texture CHRBND.
            if (modelStem.size() >= 5 && modelStem[4] != '9')
            {
                const auto c9_stem = modelStem.substr(0, 4) + "9";
                auto c9_name = c9_stem + ".chrbnd";
                if (flverSourcePath.extension() == ".dcx" || source_name.ends_with(".dcx"))
                    c9_name += ".dcx";
                const auto c9_path = source_dir / c9_name;
                const auto c9_key = ToLower(c9_path.string());
                if (!m_scannedBinderPaths.contains(c9_key) && fs::is_regular_file(c9_path))
                {
                    m_scannedBinderPaths.insert(c9_key);
                    const Binder::CPtr c9_binder = Binder::FromPath(c9_path);
                    ScanBinderForTPFs(*c9_binder);

                    if (m_game == GameType::DarkSoulsDSR)
                        RegisterChrTPFBDTs(source_dir, *c9_binder);
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
        const auto key = ToLower(textureStem);
        const auto model_key = ToLower(modelName);

        // 0. Fast path: check cache with shared lock.
        {
            std::shared_lock lock(m_mutex);
            const auto it = m_textureCache.find(key);
            if (it != m_textureCache.end())
                return &it->second;
        }

        // Slow path: unique lock for lazy loading.
        std::unique_lock lock(m_mutex);

        // 0. Double-check after acquiring unique lock.
        if (const auto it = m_textureCache.find(key); it != m_textureCache.end())
            return &it->second;

        // 0a. If texture is known to be missing, ignore it.
        if (m_missingStems.contains(key))
            return nullptr;

        // 1. Special 'global' texture cases recognized by name.
        if (m_game == GameType::EldenRing && key.starts_with("aet"))
        {
            // Asset AET texture.
            const auto aetPrefix = key.substr(0, 6);  // "aetXXX"
            const auto aetTpfStem = key.substr(0, 10); // "aetXXX_XXX"
            const fs::path aetTpfPath = m_dataRoot / "asset/aet" / aetPrefix / (aetTpfStem + ".tpf.dcx");
            if (!fs::is_regular_file(aetTpfPath))
                // This texture will not be found.
                return nullptr;

            if (!m_scannedTPFStems.contains(aetTpfStem))
                // Texture can be found below.
                m_pendingTPFs.try_emplace(aetTpfStem, aetTpfPath);
        }
        else if (m_game != GameType::EldenRing && key.starts_with("o"))
        {
            // Object texture. Can be used lazily by any FLVERs in the same map as the relevant object model.
            // We find the OBJBND and load its TPFs immediately.
            const auto objModelStem = key.substr(0, 5);  // "oXXXX"
            const auto fileName = objModelStem + ".objbnd" + (UsesBinderDcx(m_game) ? ".dcx" : "");
            const fs::path objbndPath = m_dataRoot / "obj" / fileName;
            const std::string lowerObjbndPath = ToLower(objbndPath.string());

            if (!m_scannedBinderPaths.contains(lowerObjbndPath))
            {
                if (fs::is_regular_file(objbndPath))
                {
                    const auto binder = Binder::FromPath(objbndPath);
                    ScanBinderForTPFs(*binder);
                    // Texture can be found below.
                }
                else
                {
                    Warning("Expected object binder not found for texture '{}': {}", key, objbndPath.string());
                    // Don't bother trying same Binder path again.
                    m_scannedBinderPaths.insert(lowerObjbndPath);
                    // This texture will not be found.
                    return nullptr;
                }
            }
        }
        // TODO: If characters can randomly share textures with each other, other than the known cXXX9 case, this
        //  would be where to check.

        // 2. Check pending TPFs: exact stem match.
        if (m_pendingTPFs.contains(key))
        {
            LoadTPF(key);
            if (const auto it = m_textureCache.find(key); it != m_textureCache.end())
                return &it->second;
        }

        // 3. Check pending TPFs: prefix match (multi-DDS TPFs).
        for (auto it = m_pendingTPFs.begin(); it != m_pendingTPFs.end(); )
        {
            auto& tpf_stem = it->first;
            if (key.starts_with(tpf_stem) || (!model_key.empty() && model_key.starts_with(tpf_stem)))
            {
                auto stem_copy = tpf_stem; // copy before invalidation
                ++it;
                LoadTPF(stem_copy);
                if (auto cit = m_textureCache.find(key); cit != m_textureCache.end())
                    return &cit->second;
            }
            else
            {
                ++it;
            }
        }

        // 4. Last resort: load all pending Binders.
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
                return ConvertDDSToPNG(tex->data.data(), tex->data.size());
            case ImageFormat::TGA:
                return ConvertDDSToTGA(tex->data.data(), tex->data.size());
        }
        return {};
    }

    // ========================================================================
    // Read-only getters
    // ========================================================================

    std::vector<std::string> TextureFinder::PendingTPFStems() const
    {
        std::vector<std::string> names;
        names.reserve(m_pendingTPFs.size());
        for (const auto& tpfStem : m_pendingTPFs | std::views::keys)
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
            auto name = ToLower(dirEntry.path().filename().string());

            if (name.ends_with(".tpfbhd"))
            {
                RegisterBinder(dirEntry);
            }
            else if (IsTPFFileName(name))
            {
                auto stem = ToLowerStem(dirEntry);
                if (!m_scannedTPFStems.contains(stem))
                {
                    // Multi-texture map TPF — load immediately (steal TPFTextures).
                    m_scannedTPFStems.insert(stem);
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

        const auto bdt_stem = bhdEntry->stem();
        const auto bdt_path = source_dir / (bdt_stem + ".chrtpfbdt");
        if (!fs::is_regular_file(bdt_path))
            return;

        try
        {
            const auto bdt_data = BinaryReadWrite::ReadFileBytes(bdt_path);
            auto bxf = Binder::FromSplitBytes(
                bhdEntry->data.data(), bhdEntry->data.size(),
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
        if (m_scannedBinderPaths.contains(lowerTexbndPath))
            return;
        m_scannedBinderPaths.insert(lowerTexbndPath);

        try
        {
            const auto texbnd = Binder::FromPath(texbndPath);

            for (auto& tpfEntry : texbnd->FindEntriesByNameRegex(TPF_RE_STR, /*fullMatch*/ true))
            {
                auto stem = ToLower(tpfEntry->stem());
                if (m_scannedTPFStems.contains(stem))
                    continue;
                m_scannedTPFStems.insert(stem);

                // Multi-texture TPF — load immediately and cache textures.
                const auto tpf = TPF::FromBytes(tpfEntry->data.data(), tpfEntry->data.size());
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
                if (m_scannedTPFStems.contains(stem))
                    continue;

                m_scannedTPFStems.insert(stem);
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

    void TextureFinder::RegisterBinder(const std::filesystem::path& binderPath)
    {
        if (const auto lowerPath = ToLower(binderPath.string()); !m_scannedBinderPaths.contains(lowerPath))
            m_pendingBinderPaths.try_emplace(ToLowerStem(binderPath), binderPath);
    }

    void TextureFinder::RegisterTPF(const std::filesystem::path& tpfPath)
    {
        if (const auto stem = ToLowerStem(tpfPath); !m_scannedTPFStems.contains(stem))
            m_pendingTPFs.try_emplace(stem, tpfPath);
    }

    void TextureFinder::RegisterTPF(const std::shared_ptr<BinderEntry>& tpfBinderEntry)
    {
        if (const auto stem = ToLower(tpfBinderEntry->stem()); !m_scannedTPFStems.contains(stem))
            m_pendingTPFs.try_emplace(stem, tpfBinderEntry);
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
        m_scannedBinderPaths.insert(ToLower(path.string()));

        try
        {
            // Check if it's a TPFBHD (split binder).
            const auto name = ToLower(path.filename().string());
            if (name.ends_with(".tpfbhd"))
            {
                // Find adjacent BDT.
                auto bdt_path = path;
                bdt_path.replace_extension(""); // remove .tpfbhd
                const auto bdt_stem = bdt_path.filename().string();
                bdt_path = path.parent_path() / (bdt_stem + ".tpfbdt");
                if (!fs::is_regular_file(bdt_path))
                    return;

                const auto bhd_data = BinaryReadWrite::ReadFileBytes(path);
                const auto bdt_data = BinaryReadWrite::ReadFileBytes(bdt_path);
                const auto binder = Binder::FromSplitBytes(
                    bhd_data.data(), bhd_data.size(),
                    bdt_data.data(), bdt_data.size());

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

    void TextureFinder::LoadTPF(const std::string& tpf_stem)
    {
        const auto it = m_pendingTPFs.find(tpf_stem);
        if (it == m_pendingTPFs.end()) return;

        const auto source = std::move(it->second);
        m_pendingTPFs.erase(it);
        m_scannedTPFStems.insert(tpf_stem);

        try
        {
            std::unique_ptr<TPF> tpf = nullptr;
            const std::byte* tpf_data = nullptr;
            std::size_t tpf_size = 0;
            std::vector<std::byte> buf;

            if (const auto* path = std::get_if<fs::path>(&source))
            {
                tpf = TPF::FromPath(*path);
            }
            else if (const auto& entry = std::get_if<std::shared_ptr<BinderEntry>>(&source))
            {
                tpf_data = (*entry)->data.data();
                tpf_size = (*entry)->data.size();
                tpf = TPF::FromBytes(tpf_data, tpf_size);
            }

            if (tpf)
            {
                for (auto& tex : tpf->Textures())
                    m_textureCache.try_emplace(ToLower(tex.stem), std::move(tex));
            }
        }
        catch (const std::exception& e)
        {
            Warning("[TextureFinder] Failed to load TPF '" + tpf_stem + "': " + e.what());
        }
    }

} // namespace Firelink
