// ImageImportManager implementation.

#include <FirelinkCore/ImageImportManager.h>
#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/DCX.h>
#include <FirelinkCore/DDS.h>
#include <FirelinkCore/Logging.h>
#include <FirelinkCore/Oodle.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <regex>

namespace Firelink
{
    namespace fs = std::filesystem;

    // ========================================================================
    // Static helpers
    // ========================================================================

    std::string ImageImportManager::ToLower(const std::string& s)
    {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
        return r;
    }

    std::string ImageImportManager::StemOf(const fs::path& p)
    {
        // Return minimal stem (before any dots): "c2300.chrbnd.dcx" → "c2300"
        auto name = p.filename().string();
        auto dot = name.find('.');
        return ToLower(dot == std::string::npos ? name : name.substr(0, dot));
    }

    std::string ImageImportManager::StemOf(const BinderEntry& e)
    {
        auto n = e.name();
        auto dot = n.find('.');
        return ToLower(dot == std::string::npos ? n : n.substr(0, dot));
    }

    std::vector<std::byte> ImageImportManager::ReadAndDecompress(const fs::path& path)
    {
        auto raw = BinaryReadWrite::ReadFileBytes(path);
        if (raw.size() >= 4 && IsDCX(raw.data(), raw.size()))
        {
            auto dcx_type = DetectDCX(raw.data(), raw.size());
            if (dcx_type == DCXType::DCX_KRAK && !Oodle::IsAvailable())
                throw std::runtime_error("Oodle DLL not available for DCX_KRAK decompression: " + path.string());
            return DecompressDCX(raw.data(), raw.size()).data;
        }
        return raw;
    }

    // Helper: check if a filename matches a simple glob like "*.tpf" or "Common*.tpf".
    static bool MatchesGlob(const std::string& filename, const std::string& glob)
    {
        // Only support "*.ext" and "prefix*.ext" patterns.
        auto star = glob.find('*');
        if (star == std::string::npos) return filename == glob;

        std::string prefix = glob.substr(0, star);
        std::string suffix = glob.substr(star + 1);

        auto lower_name = filename;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        auto lower_prefix = prefix;
        std::transform(lower_prefix.begin(), lower_prefix.end(), lower_prefix.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        auto lower_suffix = suffix;
        std::transform(lower_suffix.begin(), lower_suffix.end(), lower_suffix.begin(),
                        [](unsigned char c) { return std::tolower(c); });

        return lower_name.size() >= lower_prefix.size() + lower_suffix.size()
            && lower_name.compare(0, lower_prefix.size(), lower_prefix) == 0
            && lower_name.compare(lower_name.size() - lower_suffix.size(), lower_suffix.size(), lower_suffix) == 0;
    }

    // Check if name ends with ".tpf" or ".tpf.dcx".
    static bool IsTPFName(const std::string& name)
    {
        auto lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        return lower.ends_with(".tpf") || lower.ends_with(".tpf.dcx");
    }

    // ========================================================================
    // Constructor
    // ========================================================================

    ImageImportManager::ImageImportManager(GameType game, fs::path data_root)
        : game_(game), data_root_(std::move(data_root))
    {
        // Eagerly register parts/Common*.tpf textures.
        auto parts_dir = data_root_ / "parts";
        if (fs::is_directory(parts_dir))
            RegisterPartsCommonTPFs(parts_dir);
    }

    // ========================================================================
    // RegisterFLVERSources
    // ========================================================================

    void ImageImportManager::RegisterFLVERSources(
        const fs::path& flver_source_path,
        const Binder* flver_binder,
        bool prefer_hi_res)
    {
        std::unique_lock lock(mutex_);

        auto source_name = flver_source_path.filename().string();
        // Remove .dcx suffix for type detection.
        auto base_name = source_name;
        if (base_name.ends_with(".dcx"))
            base_name = base_name.substr(0, base_name.size() - 4);

        auto model_stem = base_name.substr(0, base_name.find('.'));
        auto source_dir = flver_source_path.parent_path();

        // MAP PIECES
        if (model_stem.starts_with("m") && base_name.ends_with(".flver"))
        {
            RegisterMapTextures(source_dir);
        }
        else if (base_name.ends_with(".mapbnd"))
        {
            // Asset FLVER textures are in 'asset/aet'.
            if (aet_root_.empty())
                aet_root_ = (source_dir / "../../../asset/aet").lexically_normal();
        }
        // CHARACTERS
        else if (model_stem.starts_with("c") && base_name.ends_with(".flver"))
        {
            RegisterChrLooseTPFs(source_dir);
        }
        else if (base_name.ends_with(".chrbnd"))
        {
            if (flver_binder)
            {
                ScanBinderForTPFs(*flver_binder);

                switch (game_)
                {
                    case GameType::DemonsSouls:
                        RegisterChrLooseTPFs(source_dir);
                        break;
                    case GameType::DarkSoulsPTDE:
                        RegisterChrLooseTPFs(source_dir / model_stem);
                        break;
                    case GameType::DarkSoulsDSR:
                        RegisterChrTPFBDTs(source_dir, *flver_binder);
                        break;
                    case GameType::Bloodborne:
                        break; // all TPFs inside CHRBND
                    case GameType::DarkSouls3:
                    case GameType::Sekiro:
                        RegisterChrTexbnd(source_dir, model_stem, "");
                        break;
                    case GameType::EldenRing:
                        RegisterChrTexbnd(source_dir, model_stem, prefer_hi_res ? "_h" : "_l");
                        break;
                }
            }

            // Check for cXXX9 shared texture CHRBND.
            if (model_stem.size() >= 5 && model_stem[4] != '9')
            {
                auto c9_stem = model_stem.substr(0, 4) + "9";
                auto c9_name = c9_stem + ".chrbnd";
                if (flver_source_path.extension() == ".dcx" || source_name.ends_with(".dcx"))
                    c9_name += ".dcx";
                auto c9_path = source_dir / c9_name;
                auto c9_key = ToLower(c9_path.string());
                if (!scanned_binders_.contains(c9_key) && fs::is_regular_file(c9_path))
                {
                    scanned_binders_.insert(c9_key);
                    auto data = ReadAndDecompress(c9_path);
                    auto c9_binder = Binder::FromBytes(data.data(), data.size());
                    ScanBinderForTPFs(c9_binder);

                    if (game_ == GameType::DarkSoulsDSR)
                        RegisterChrTPFBDTs(source_dir, c9_binder);
                }
            }
        }
        // EQUIPMENT
        else if (base_name.ends_with(".partsbnd"))
        {
            if (flver_binder)
                ScanBinderForTPFs(*flver_binder);
        }
        // ASSETS (Elden Ring)
        else if (base_name.ends_with(".geombnd"))
        {
            if (aet_root_.empty())
                aet_root_ = (source_dir / "../../aet").lexically_normal();
        }
        // GENERIC BINDERS
        else if (base_name.ends_with("bnd"))
        {
            if (flver_binder)
                ScanBinderForTPFs(*flver_binder);
        }
    }

    // ========================================================================
    // GetTexture
    // ========================================================================

    const TPFTexture* ImageImportManager::GetTexture(
        const std::string& texture_stem, const std::string& model_name)
    {
        auto key = ToLower(texture_stem);
        auto model_key = ToLower(model_name);

        // Fast path: check cache with shared lock.
        {
            std::shared_lock lock(mutex_);
            auto it = texture_cache_.find(key);
            if (it != texture_cache_.end())
                return &it->second;
        }

        // Slow path: unique lock for lazy loading.
        std::unique_lock lock(mutex_);

        // Double-check after acquiring unique lock.
        if (auto it = texture_cache_.find(key); it != texture_cache_.end())
            return &it->second;

        // Check for AET texture.
        if (key.starts_with("aet") && !aet_root_.empty())
        {
            auto aet_prefix = key.substr(0, 6);  // "aetXXX"
            auto aet_tpf_prefix = key.substr(0, 10); // "aetXXX_XXX"
            auto aet_tpf_path = aet_root_ / aet_prefix / (aet_tpf_prefix + ".tpf.dcx");
            if (!scanned_tpfs_.contains(aet_tpf_prefix) && fs::is_regular_file(aet_tpf_path))
                pending_tpfs_.try_emplace(aet_tpf_prefix, aet_tpf_path);
        }

        // Check pending TPFs: exact stem match.
        if (pending_tpfs_.contains(key))
        {
            LoadTPF(key);
            if (auto it = texture_cache_.find(key); it != texture_cache_.end())
                return &it->second;
        }

        // Check pending TPFs: prefix match (multi-DDS TPFs).
        for (auto it = pending_tpfs_.begin(); it != pending_tpfs_.end(); )
        {
            auto& tpf_stem = it->first;
            if (key.starts_with(tpf_stem) || (!model_key.empty() && model_key.starts_with(tpf_stem)))
            {
                auto stem_copy = tpf_stem; // copy before invalidation
                ++it;
                LoadTPF(stem_copy);
                if (auto cit = texture_cache_.find(key); cit != texture_cache_.end())
                    return &cit->second;
            }
            else
            {
                ++it;
            }
        }

        // Last resort: load all pending binders.
        if (!pending_binders_.empty())
        {
            auto binder_stems = std::vector<std::string>();
            binder_stems.reserve(pending_binders_.size());
            for (auto& [stem, _] : pending_binders_)
                binder_stems.push_back(stem);

            for (auto& stem : binder_stems)
                LoadBinder(stem);

            // Retry with new TPF sources.
            lock.unlock();
            return GetTexture(texture_stem, model_name);
        }

        return nullptr;
    }

    // ========================================================================
    // GetTextureAs
    // ========================================================================

    std::vector<std::byte> ImageImportManager::GetTextureAs(
        const std::string& texture_stem, ImageFormat format, const std::string& model_name)
    {
        const auto* tex = GetTexture(texture_stem, model_name);
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

    std::size_t ImageImportManager::CachedTextureCount() const
    {
        std::shared_lock lock(mutex_);
        return texture_cache_.size();
    }

    // ========================================================================
    // Registration helpers
    // ========================================================================

    void ImageImportManager::RegisterMapTextures(const fs::path& source_dir)
    {
        // Extract map area from directory name (mAA_BB_CC_DD → mAA).
        auto dir_name = source_dir.filename().string();
        static const std::regex map_re(R"(^m(\d\d)_)");
        std::smatch m;
        if (!std::regex_search(dir_name, m, map_re))
            return;

        auto area = m[1].str();
        auto map_area_dir = (source_dir / (".." ) / ("m" + area)).lexically_normal();
        RegisterMapAreaTextures(map_area_dir);

        // PTDE: also check map/tx folder.
        auto tx_dir = (source_dir / "../tx").lexically_normal();
        if (fs::is_directory(tx_dir))
            RegisterTPFsInDir(tx_dir);
    }

    void ImageImportManager::RegisterMapAreaTextures(const fs::path& map_area_dir)
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
                if (!scanned_binders_.contains(stem))
                    pending_binders_.try_emplace(stem, entry.path());
            }
            else if (IsTPFName(name))
            {
                auto stem = StemOf(entry.path());
                if (!scanned_tpfs_.contains(stem))
                {
                    // Multi-texture map TPF — load immediately.
                    scanned_tpfs_.insert(stem);
                    try
                    {
                        auto data = ReadAndDecompress(entry.path());
                        auto tpf = TPF::FromBytes(data.data(), data.size());
                        for (auto& tex : tpf.textures)
                            texture_cache_.try_emplace(ToLower(tex.stem), std::move(tex));
                    }
                    catch (const std::exception& e)
                    {
                        Warning("[ImageImportManager] Failed to load TPF: " + entry.path().string() + " — " + e.what());
                    }
                }
            }
        }
    }

    void ImageImportManager::RegisterTPFsInDir(const fs::path& dir, const std::string& glob)
    {
        if (!fs::is_directory(dir)) return;
        for (auto& entry : fs::directory_iterator(dir))
        {
            if (!entry.is_regular_file()) continue;
            auto name = entry.path().filename().string();
            if (MatchesGlob(name, glob) || MatchesGlob(name, glob + ".dcx"))
            {
                auto stem = StemOf(entry.path());
                if (!scanned_tpfs_.contains(stem))
                    pending_tpfs_.try_emplace(stem, entry.path());
            }
        }
    }

    void ImageImportManager::RegisterChrLooseTPFs(const fs::path& dir)
    {
        if (!fs::is_directory(dir)) return;
        for (auto& entry : fs::directory_iterator(dir))
        {
            if (!entry.is_regular_file()) continue;
            if (IsTPFName(entry.path().filename().string()))
            {
                auto stem = StemOf(entry.path());
                if (!scanned_tpfs_.contains(stem))
                    pending_tpfs_.try_emplace(stem, entry.path());
            }
        }
    }

    void ImageImportManager::RegisterChrTPFBDTs(const fs::path& source_dir, const Binder& chrbnd)
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

        auto bdt_stem = StemOf(*bhd_entry);
        auto bdt_path = source_dir / (bdt_stem + ".chrtpfbdt");
        if (!fs::is_regular_file(bdt_path))
            return;

        try
        {
            auto bdt_data = BinaryReadWrite::ReadFileBytes(bdt_path);
            auto bxf = Binder::FromSplitBytes(
                bhd_entry->data.data(), bhd_entry->data.size(),
                bdt_data.data(), bdt_data.size());

            for (auto& tpf_entry : bxf.entries)
            {
                if (IsTPFName(tpf_entry.name()))
                {
                    auto stem = StemOf(tpf_entry);
                    if (!scanned_tpfs_.contains(stem))
                        pending_tpfs_.try_emplace(stem, std::move(tpf_entry));
                }
            }
        }
        catch (const std::exception& e)
        {
            Warning("[ImageImportManager] Failed to read CHRTPFBDT: " + bdt_path.string() + " — " + e.what());
        }
    }

    void ImageImportManager::RegisterChrTexbnd(
        const fs::path& source_dir, const std::string& model_stem, const std::string& res)
    {
        auto texbnd_path = source_dir / (model_stem + res + ".texbnd.dcx");
        if (!fs::is_regular_file(texbnd_path))
        {
            texbnd_path = source_dir / (model_stem + res + ".texbnd");
            if (!fs::is_regular_file(texbnd_path))
                return;
        }

        auto texbnd_key = ToLower(texbnd_path.string());
        if (scanned_binders_.contains(texbnd_key))
            return;
        scanned_binders_.insert(texbnd_key);

        try
        {
            auto data = ReadAndDecompress(texbnd_path);
            auto texbnd = Binder::FromBytes(data.data(), data.size());

            for (auto& tpf_entry : texbnd.entries)
            {
                if (!IsTPFName(tpf_entry.name())) continue;

                auto stem = StemOf(tpf_entry);
                if (scanned_tpfs_.contains(stem)) continue;
                scanned_tpfs_.insert(stem);

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
                auto tpf = TPF::FromBytes(tpf_data, tpf_size);
                for (auto& tex : tpf.textures)
                    texture_cache_.try_emplace(ToLower(tex.stem), std::move(tex));
            }
        }
        catch (const std::exception& e)
        {
            Warning("[ImageImportManager] Failed to read texbnd: " + texbnd_path.string() + " — " + e.what());
        }
    }

    void ImageImportManager::RegisterPartsCommonTPFs(const fs::path& parts_dir)
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
                if (scanned_tpfs_.contains(stem)) continue;
                scanned_tpfs_.insert(stem);
                try
                {
                    auto data = ReadAndDecompress(entry.path());
                    auto tpf = TPF::FromBytes(data.data(), data.size());
                    for (auto& tex : tpf.textures)
                        texture_cache_.try_emplace(ToLower(tex.stem), std::move(tex));
                }
                catch (const std::exception& e)
                {
                    Warning("[ImageImportManager] Failed to load Common TPF: " + entry.path().string() + " — " + e.what());
                }
            }
        }
    }

    void ImageImportManager::ScanBinderForTPFs(const Binder& binder)
    {
        for (auto& entry : binder.entries)
        {
            if (IsTPFName(entry.name()))
            {
                auto stem = StemOf(entry);
                if (!scanned_tpfs_.contains(stem))
                    pending_tpfs_.try_emplace(stem, entry); // copy entry (owns data)
            }
        }
    }

    // ========================================================================
    // Lazy loading
    // ========================================================================

    void ImageImportManager::LoadBinder(const std::string& binder_stem)
    {
        auto it = pending_binders_.find(binder_stem);
        if (it == pending_binders_.end()) return;

        auto path = std::move(it->second);
        pending_binders_.erase(it);
        scanned_binders_.insert(ToLower(path.string()));

        try
        {
            // Check if it's a TPFBHD (split binder).
            auto name = ToLower(path.filename().string());
            if (name.ends_with(".tpfbhd"))
            {
                // Find adjacent BDT.
                auto bdt_path = path;
                bdt_path.replace_extension(""); // remove .tpfbhd
                auto bdt_stem = bdt_path.filename().string();
                bdt_path = path.parent_path() / (bdt_stem + ".tpfbdt");
                if (!fs::is_regular_file(bdt_path))
                    return;

                auto bhd_data = BinaryReadWrite::ReadFileBytes(path);
                auto bdt_data = BinaryReadWrite::ReadFileBytes(bdt_path);
                auto binder = Binder::FromSplitBytes(
                    bhd_data.data(), bhd_data.size(),
                    bdt_data.data(), bdt_data.size());

                for (auto& entry : binder.entries)
                {
                    if (IsTPFName(entry.name()))
                    {
                        auto stem = StemOf(entry);
                        if (!scanned_tpfs_.contains(stem))
                            pending_tpfs_.try_emplace(stem, std::move(entry));
                    }
                }
            }
            else
            {
                auto data = ReadAndDecompress(path);
                auto binder = Binder::FromBytes(data.data(), data.size());
                ScanBinderForTPFs(binder);
            }
        }
        catch (const std::exception& e)
        {
            Warning("[ImageImportManager] Failed to load binder: " + path.string() + " — " + e.what());
        }
    }

    void ImageImportManager::LoadTPF(const std::string& tpf_stem)
    {
        auto it = pending_tpfs_.find(tpf_stem);
        if (it == pending_tpfs_.end()) return;

        auto source = std::move(it->second);
        pending_tpfs_.erase(it);
        scanned_tpfs_.insert(tpf_stem);

        try
        {
            const std::byte* tpf_data = nullptr;
            std::size_t tpf_size = 0;
            std::vector<std::byte> buf;

            if (auto* path = std::get_if<fs::path>(&source))
            {
                buf = ReadAndDecompress(*path);
                tpf_data = buf.data();
                tpf_size = buf.size();
            }
            else if (auto* entry = std::get_if<BinderEntry>(&source))
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
                auto tpf = TPF::FromBytes(tpf_data, tpf_size);
                for (auto& tex : tpf.textures)
                    texture_cache_.try_emplace(ToLower(tex.stem), std::move(tex));
            }
        }
        catch (const std::exception& e)
        {
            Warning("[ImageImportManager] Failed to load TPF '" + tpf_stem + "': " + e.what());
        }
    }

} // namespace Firelink

