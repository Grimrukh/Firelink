// TextureFinder implementation.
#include <../../FirelinkFLVER/include/FirelinkFLVER/TextureFinder.h>

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/DCX.h>
#include <FirelinkCore/DDS.h>
#include <FirelinkCore/Logging.h>
#include <FirelinkCore/Paths.h>

#include <algorithm>
#include <regex>

namespace Firelink
{
    namespace fs = std::filesystem;

    // Check if name ends with ".tpf" or ".tpf.dcx".
    static bool IsTPFName(const std::string& name)
    {
        const std::string lower = ToLower(name);
        return lower.ends_with(".tpf") || lower.ends_with(".tpf.dcx");
    }

    // ========================================================================
    // Constructor
    // ========================================================================

    TextureFinder::TextureFinder(const GameType game, fs::path dataRoot)
        : m_game(game), m_dataRoot(std::move(dataRoot))
    {
        // Eagerly register parts/Common*.tpf textures.
        const auto parts_dir = m_dataRoot / "parts";
        if (fs::is_directory(parts_dir))
            RegisterPartsCommonTPFs(parts_dir);
    }

    // ========================================================================
    // RegisterFLVERSources
    // ========================================================================

    void TextureFinder::RegisterFLVERSources(
        const fs::path& flverSourcePath,
        const Binder* flverBinder,
        const bool preferHiRes)
    {
        std::unique_lock lock(m_mutex);

        const auto source_name = flverSourcePath.filename().string();
        // Remove .dcx suffix for type detection.
        auto base_name = source_name;
        if (base_name.ends_with(".dcx"))
            base_name = base_name.substr(0, base_name.size() - 4);

        const auto model_stem = base_name.substr(0, base_name.find('.'));
        const auto source_dir = flverSourcePath.parent_path();

        // MAP PIECES
        if (model_stem.starts_with("m") && base_name.ends_with(".flver"))
        {
            RegisterMapTextures(source_dir);
        }
        else if (base_name.ends_with(".mapbnd"))
        {
            // Asset FLVER textures are in 'asset/aet'.
            if (m_aetRoot.empty())
                m_aetRoot = (source_dir / "../../../asset/aet").lexically_normal();
        }
        // CHARACTERS
        else if (model_stem.starts_with("c") && base_name.ends_with(".flver"))
        {
            RegisterChrLooseTPFs(source_dir);
        }
        else if (base_name.ends_with(".chrbnd"))
        {
            if (flverBinder)
            {
                ScanBinderForTPFs(*flverBinder);

                switch (m_game)
                {
                    case GameType::DemonsSouls:
                        RegisterChrLooseTPFs(source_dir);
                        break;
                    case GameType::DarkSoulsPTDE:
                        RegisterChrLooseTPFs(source_dir / model_stem);
                        break;
                    case GameType::DarkSoulsDSR:
                        RegisterChrTPFBDTs(source_dir, *flverBinder);
                        break;
                    case GameType::Bloodborne:
                        break; // all TPFs inside CHRBND
                    case GameType::DarkSouls3:
                    case GameType::Sekiro:
                        RegisterChrTexbnd(source_dir, model_stem, "");
                        break;
                    case GameType::EldenRing:
                        RegisterChrTexbnd(source_dir, model_stem, preferHiRes ? "_h" : "_l");
                        break;
                }
            }

            // Check for cXXX9 shared texture CHRBND.
            if (model_stem.size() >= 5 && model_stem[4] != '9')
            {
                const auto c9_stem = model_stem.substr(0, 4) + "9";
                auto c9_name = c9_stem + ".chrbnd";
                if (flverSourcePath.extension() == ".dcx" || source_name.ends_with(".dcx"))
                    c9_name += ".dcx";
                const auto c9_path = source_dir / c9_name;
                const auto c9_key = ToLower(c9_path.string());
                if (!m_scannedBinders.contains(c9_key) && fs::is_regular_file(c9_path))
                {
                    m_scannedBinders.insert(c9_key);
                    const Binder::CPtr c9_binder = Binder::FromPath(c9_path);
                    ScanBinderForTPFs(*c9_binder);

                    if (m_game == GameType::DarkSoulsDSR)
                        RegisterChrTPFBDTs(source_dir, *c9_binder);
                }
            }
        }
        // EQUIPMENT
        else if (base_name.ends_with(".partsbnd"))
        {
            if (flverBinder)
                ScanBinderForTPFs(*flverBinder);
        }
        // ASSETS (Elden Ring)
        else if (base_name.ends_with(".geombnd"))
        {
            if (m_aetRoot.empty())
            {
                // Guess AET root as 'asset/aet', assuming GEOMBND is in 'asset/aeg/aegXXX'.
                m_aetRoot = (source_dir / "../../aet").lexically_normal();
            }
        }
        // GENERIC BINDERS
        else if (base_name.ends_with("bnd"))
        {
            if (flverBinder)
                ScanBinderForTPFs(*flverBinder);
        }
    }

    // ========================================================================
    // GetTexture
    // ========================================================================

    const TPFTexture* TextureFinder::GetTexture(
        const std::string& textureStem, const std::string& modelName)
    {
        const auto key = ToLower(textureStem);
        const auto model_key = ToLower(modelName);

        // Fast path: check cache with shared lock.
        {
            std::shared_lock lock(m_mutex);
            const auto it = m_textureCache.find(key);
            if (it != m_textureCache.end())
                return &it->second;
        }

        // Slow path: unique lock for lazy loading.
        std::unique_lock lock(m_mutex);

        // Double-check after acquiring unique lock.
        if (const auto it = m_textureCache.find(key); it != m_textureCache.end())
            return &it->second;

        // Check for AET texture.
        if (key.starts_with("aet") && !m_aetRoot.empty())
        {
            const auto aet_prefix = key.substr(0, 6);  // "aetXXX"
            const auto aet_tpf_prefix = key.substr(0, 10); // "aetXXX_XXX"
            auto aet_tpf_path = m_aetRoot / aet_prefix / (aet_tpf_prefix + ".tpf.dcx");
            if (!m_scannedTPFs.contains(aet_tpf_prefix) && fs::is_regular_file(aet_tpf_path))
                m_pendingTPFs.try_emplace(aet_tpf_prefix, aet_tpf_path);
        }

        // Check pending TPFs: exact stem match.
        if (m_pendingTPFs.contains(key))
        {
            LoadTPF(key);
            if (const auto it = m_textureCache.find(key); it != m_textureCache.end())
                return &it->second;
        }

        // Check pending TPFs: prefix match (multi-DDS TPFs).
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

        // Last resort: load all pending binders.
        if (!m_pendingBinders.empty())
        {
            auto binder_stems = std::vector<std::string>();
            binder_stems.reserve(m_pendingBinders.size());
            for (const auto& stem : m_pendingBinders | std::views::keys)
                binder_stems.push_back(stem);

            for (auto& stem : binder_stems)
                LoadBinder(stem);

            // Retry with new TPF sources.
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
    // CachedTextureCount
    // ========================================================================

    std::size_t TextureFinder::CachedTextureCount() const
    {
        std::shared_lock lock(m_mutex);
        return m_textureCache.size();
    }

    // ========================================================================
    // Registration helpers
    // ========================================================================

    void TextureFinder::RegisterMapTextures(const fs::path& source_dir)
    {
        // Extract map area from directory name (mAA_BB_CC_DD → mAA).
        const auto dir_name = source_dir.filename().string();
        static const std::regex map_re(R"(^m(\d\d)_)");
        std::smatch m;
        if (!std::regex_search(dir_name, m, map_re))
            return;

        const auto area = m[1].str();
        const auto map_area_dir = (source_dir / (".." ) / ("m" + area)).lexically_normal();
        RegisterMapAreaTextures(map_area_dir);

        if (m_game == GameType::DarkSoulsPTDE)
        {
            // PTDE: also check map/tx folder.
            const auto tx_dir = (source_dir / "../tx").lexically_normal();
            if (fs::is_directory(tx_dir))
                RegisterTPFsInDir(tx_dir);
        }
    }

    void TextureFinder::RegisterMapAreaTextures(const fs::path& map_area_dir)
    {
        if (!fs::is_directory(map_area_dir))
            return;

        for (auto& entry : fs::directory_iterator(map_area_dir))
        {
            if (!entry.is_regular_file()) continue;
            auto name = entry.path().filename().string();
            auto lower_name_str = ToLower(name);

            if (lower_name_str.ends_with(".tpfbhd"))
            {
                auto stem = StemOf(entry.path());
                if (!m_scannedBinders.contains(stem))
                    m_pendingBinders.try_emplace(stem, entry.path());
            }
            else if (IsTPFName(name))
            {
                auto stem = StemOf(entry.path());
                if (!m_scannedTPFs.contains(stem))
                {
                    // Multi-texture map TPF — load immediately.
                    m_scannedTPFs.insert(stem);
                    try
                    {
                        for (const TPF::CPtr tpf = TPF::FromPath(entry); auto& tex : tpf->textures)
                            m_textureCache.try_emplace(ToLower(tex.stem), std::move(tex));
                    }
                    catch (const std::exception& e)
                    {
                        Warning("[TextureFinder] Failed to load TPF: " + entry.path().string() + " — " + e.what());
                    }
                }
            }
        }
    }

    void TextureFinder::RegisterTPFsInDir(const fs::path& dir, const std::string& glob)
    {
        if (!fs::is_directory(dir)) return;
        for (auto& entry : fs::directory_iterator(dir))
        {
            if (!entry.is_regular_file()) continue;
            auto name = entry.path().filename().string();
            if (MatchesGlob(name, glob) || MatchesGlob(name, glob + ".dcx"))
            {
                auto stem = StemOf(entry.path());
                if (!m_scannedTPFs.contains(stem))
                    m_pendingTPFs.try_emplace(stem, entry.path());
            }
        }
    }

    void TextureFinder::RegisterChrLooseTPFs(const fs::path& dir)
    {
        if (!fs::is_directory(dir)) return;
        for (auto& entry : fs::directory_iterator(dir))
        {
            if (!entry.is_regular_file()) continue;
            if (IsTPFName(entry.path().filename().string()))
            {
                auto stem = StemOf(entry.path());
                if (!m_scannedTPFs.contains(stem))
                    m_pendingTPFs.try_emplace(stem, entry.path());
            }
        }
    }

    void TextureFinder::RegisterChrTPFBDTs(const fs::path& source_dir, const Binder& chrbnd)
    {
        // Look for a .chrtpfbhd entry inside the CHRBND.
        static const std::regex bhd_re(R"(\.chrtpfbhd$)", std::regex::icase);
        const BinderEntry* bhd_entry = nullptr;
        for (auto& entry : chrbnd.entries)
        {
            if (std::regex_search(entry.name(), bhd_re))
            {
                bhd_entry = &entry;
                break;
            }
        }
        if (!bhd_entry) return;

        const auto bdt_stem = bhd_entry->stem();
        const auto bdt_path = source_dir / (bdt_stem + ".chrtpfbdt");
        if (!fs::is_regular_file(bdt_path))
            return;

        try
        {
            const auto bdt_data = BinaryReadWrite::ReadFileBytes(bdt_path);
            const auto bxf = Binder::FromSplitBytes(
                bhd_entry->data.data(), bhd_entry->data.size(),
                bdt_data.data(), bdt_data.size());

            for (auto& tpf_entry : bxf->entries)
            {
                if (IsTPFName(tpf_entry.name()))
                {
                    auto stem = tpf_entry.stem();
                    if (!m_scannedTPFs.contains(stem))
                        m_pendingTPFs.try_emplace(stem, std::move(tpf_entry));
                }
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
        auto texbnd_path = source_dir / (model_stem + res + ".texbnd.dcx");
        if (!fs::is_regular_file(texbnd_path))
        {
            texbnd_path = source_dir / (model_stem + res + ".texbnd");
            if (!fs::is_regular_file(texbnd_path))
                return;
        }

        const auto texbnd_key = ToLower(texbnd_path.string());
        if (m_scannedBinders.contains(texbnd_key))
            return;
        m_scannedBinders.insert(texbnd_key);

        try
        {
            const auto texbnd = Binder::FromPath(texbnd_path);

            for (auto& tpf_entry : texbnd->entries)
            {
                if (!IsTPFName(tpf_entry.name())) continue;

                auto stem = tpf_entry.stem();
                if (m_scannedTPFs.contains(stem)) continue;
                m_scannedTPFs.insert(stem);

                // Multi-texture TPF — load immediately.
                const std::byte* tpf_data = tpf_entry.data.data();
                std::size_t tpf_size = tpf_entry.data.size();
                std::vector<std::byte> decompressed;
                if (IsDCX(tpf_data, tpf_size))
                {
                    decompressed = DecompressDCX(tpf_data, tpf_size).data;
                    tpf_data = decompressed.data();
                    tpf_size = decompressed.size();
                }
                const auto tpf = TPF::FromBytes(tpf_data, tpf_size);
                for (auto& tex : tpf->textures)
                    m_textureCache.try_emplace(ToLower(tex.stem), std::move(tex));
            }
        }
        catch (const std::exception& e)
        {
            Warning("[TextureFinder] Failed to read texbnd: " + texbnd_path.string() + " — " + e.what());
        }
    }

    void TextureFinder::RegisterPartsCommonTPFs(const fs::path& parts_dir)
    {
        if (!fs::is_directory(parts_dir)) return;
        for (auto& entry : fs::directory_iterator(parts_dir))
        {
            if (!entry.is_regular_file()) continue;
            auto name = entry.path().filename().string();
            if (MatchesGlob(name, "Common*.tpf") || MatchesGlob(name, "Common*.tpf.dcx")
                || MatchesGlob(name, "common*.tpf") || MatchesGlob(name, "common*.tpf.dcx"))
            {
                auto stem = StemOf(entry.path());
                if (m_scannedTPFs.contains(stem)) continue;
                m_scannedTPFs.insert(stem);
                try
                {
                    const auto tpf = TPF::FromPath(entry.path());
                    for (auto& tex : tpf->textures)
                        m_textureCache.try_emplace(ToLower(tex.stem), std::move(tex));
                }
                catch (const std::exception& e)
                {
                    Warning("[TextureFinder] Failed to load Common TPF: " + entry.path().string() + " — " + e.what());
                }
            }
        }
    }

    void TextureFinder::ScanBinderForTPFs(const Binder& binder)
    {
        for (auto& entry : binder.entries)
        {
            if (IsTPFName(entry.name()))
            {
                auto stem = entry.stem();
                if (!m_scannedTPFs.contains(stem))
                    m_pendingTPFs.try_emplace(stem, entry); // copy entry (owns data)
            }
        }
    }

    // ========================================================================
    // Lazy loading
    // ========================================================================

    void TextureFinder::LoadBinder(const std::string& binder_stem)
    {
        const auto it = m_pendingBinders.find(binder_stem);
        if (it == m_pendingBinders.end()) return;

        const auto path = std::move(it->second);
        m_pendingBinders.erase(it);
        m_scannedBinders.insert(ToLower(path.string()));

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

                for (auto& entry : binder->entries)
                {
                    if (IsTPFName(entry.name()))
                    {
                        auto stem = entry.stem();
                        if (!m_scannedTPFs.contains(stem))
                            m_pendingTPFs.try_emplace(stem, std::move(entry));
                    }
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
        m_scannedTPFs.insert(tpf_stem);

        try
        {
            const std::byte* tpf_data = nullptr;
            std::size_t tpf_size = 0;
            std::vector<std::byte> buf;

            if (const auto* path = std::get_if<fs::path>(&source))
            {
                const auto [storage, type] = TryDecompressDCX(*path);
                buf = std::move(storage);  // discard DCX type
                tpf_data = buf.data();
                tpf_size = buf.size();
            }
            else if (const auto* entry = std::get_if<BinderEntry>(&source))
            {
                tpf_data = entry->data.data();
                tpf_size = entry->data.size();

                // Entry data may itself be DCX-compressed.
                if (tpf_size >= 4 && IsDCX(tpf_data, tpf_size))
                {
                    buf = DecompressDCX(tpf_data, tpf_size).data;
                    tpf_data = buf.data();
                    tpf_size = buf.size();
                }
            }

            if (tpf_data && tpf_size >= 4)
            {
                const auto tpf = TPF::FromBytes(tpf_data, tpf_size);
                for (auto& tex : tpf->textures)
                    m_textureCache.try_emplace(ToLower(tex.stem), std::move(tex));
            }
        }
        catch (const std::exception& e)
        {
            Warning("[TextureFinder] Failed to load TPF '" + tpf_stem + "': " + e.what());
        }
    }

} // namespace Firelink
