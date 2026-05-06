// Repairs known-broken DS1R vertex layouts that QLOC shipped with incorrect
// tangent/bitangent fields baked into the vertex data but not reflected in
// the stored layout.

#include "../include/FirelinkFLVER/LayoutRepair.h"

#include <FirelinkFLVER/FLVER.h>

#include <FirelinkCore/Logging.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Firelink
{
    namespace
    {
        // =============================================================
        // Helpers to build repaired VertexDataType / VertexArrayLayout
        // =============================================================

        VertexDataType make_type(
            const VertexUsage usage, const VertexDataFormatEnum format, const std::uint32_t instance = 0)
        {
            VertexDataType dt;
            dt.usage = usage;
            dt.format = format;
            dt.instance_index = instance;
            dt.unk_x00 = 0;
            dt.data_offset = 0;
            return dt;
        }

        // Convenience aliases matching the Python class names.
        VertexDataType VPos() { return make_type(VertexUsage::Position, VertexDataFormatEnum::ThreeFloats); }
        VertexDataType VBoneI() { return make_type(VertexUsage::BoneIndices, VertexDataFormatEnum::FourBytesB); }
        VertexDataType VNorm() { return make_type(VertexUsage::Normal, VertexDataFormatEnum::FourBytesC); }

        VertexDataType VCol(const std::uint32_t idx = 0)
        {
            return make_type(VertexUsage::Color, VertexDataFormatEnum::FourBytesC, idx);
        }

        VertexDataType VUV() { return make_type(VertexUsage::UV, VertexDataFormatEnum::TwoShorts); }

        // Append N bytes of Ignore entries (must be a multiple of 4).
        void append_ignore(std::vector<VertexDataType>& types, std::uint32_t bytes)
        {
            while (bytes >= 16)
            {
                types.push_back(make_type(VertexUsage::Ignore, VertexDataFormatEnum::FourFloats));
                bytes -= 16;
            }
            while (bytes >= 8)
            {
                types.push_back(make_type(VertexUsage::Ignore, VertexDataFormatEnum::FourShorts));
                bytes -= 8;
            }
            while (bytes >= 4)
            {
                types.push_back(make_type(VertexUsage::Ignore, VertexDataFormatEnum::FourBytesA));
                bytes -= 4;
            }
        }

        // Build a VertexArrayLayout, computing compressed/decompressed sizes.
        VertexArrayLayout build_layout(std::vector<VertexDataType> types)
        {
            VertexArrayLayout layout;
            std::uint32_t comp_offset = 0;
            std::uint32_t decomp_size = 0;
            for (auto& dt : types)
            {
                dt.data_offset = comp_offset;
                comp_offset += FormatEnumSize(dt.format);
                if (dt.usage != VertexUsage::Ignore)
                {
                    if (auto info = FindVertexFormatInfo(dt.usage, dt.format))
                        decomp_size += static_cast<std::uint32_t>(info->GetTotalDecompressedSize());
                }
            }
            layout.types = std::move(types);
            layout.compressed_vertex_size = comp_offset;
            layout.decompressed_vertex_size = decomp_size;
            return layout;
        }

        // =============================================================
        // Known corrected layouts
        // =============================================================

        // MTD names where QLOC incorrectly listed tangents in the layout.
        // Corrected layout: Position, BoneIndices, Normal, Color, UV (no tangents).
        const std::unordered_set<std::string> UNSHADED_MTDS = {
            "A10_lightshaft[Dn]_Add.mtd",
            "A10_BG_shaft[Dn]_Add_LS.mtd",
            "A10_mist_02[Dn]_Alp.mtd",
            "M_Sky[Dn].mtd",
            "M_Tree[D]_Edge.mtd",
            "S[NL].mtd",
        };

        VertexArrayLayout make_unshaded()
        {
            return build_layout({VPos(), VBoneI(), VNorm(), VCol(), VUV()});
        }

        // M[D].mtd / M[D]_Edge.mtd — 4 bytes useless tangent
        VertexArrayLayout make_ignore4()
        {
            std::vector<VertexDataType> t = {VPos(), VBoneI(), VNorm()};
            append_ignore(t, 4);
            t.push_back(VCol());
            t.push_back(VUV());
            return build_layout(std::move(t));
        }

        // A10_02_m9000_M[D].mtd — 8 bytes useless tangent + bitangent
        VertexArrayLayout make_ignore8()
        {
            std::vector<VertexDataType> t = {VPos(), VBoneI(), VNorm()};
            append_ignore(t, 8);
            t.push_back(VCol());
            t.push_back(VUV());
            return build_layout(std::move(t));
        }

        // A19_Division[D]_Alp.mtd — 32 bytes junk, color instance_index = 6
        VertexArrayLayout make_division()
        {
            std::vector<VertexDataType> t = {VPos(), VBoneI(), VNorm()};
            append_ignore(t, 32);
            t.push_back(VCol(6));
            t.push_back(VUV());
            return build_layout(std::move(t));
        }

        // A19_Fly[DSB]_edge.mtd — 32 bytes junk, color instance_index = 0
        VertexArrayLayout make_fly()
        {
            std::vector<VertexDataType> t = {VPos(), VBoneI(), VNorm()};
            append_ignore(t, 32);
            t.push_back(VCol());
            t.push_back(VUV());
            return build_layout(std::move(t));
        }

        using Builder = VertexArrayLayout(*)();
        const std::unordered_map<std::string, Builder> OTHER_MTDS = {
            {"M[D].mtd", make_ignore4},
            {"M[D]_Edge.mtd", make_ignore4},
            {"A10_02_m9000_M[D].mtd", make_ignore8},
            {"A19_Division[D]_Alp.mtd", make_division},
            {"A19_Fly[DSB]_edge.mtd", make_fly},
        };

        // =============================================================
        // MTD name extraction from raw mat_def_path bytes
        // =============================================================

        // The path is stored as raw bytes (UTF-16 LE for Unicode, Shift-JIS
        // otherwise). We only need the filename after the last separator.
        std::string extract_mtd_name(const std::string& raw)
        {
            if (raw.empty()) return {};

            // Detect UTF-16 LE: every odd byte is 0x00.
            bool utf16 = raw.size() >= 4;
            if (utf16)
            {
                for (std::size_t i = 1; i < raw.size(); i += 2)
                {
                    if (raw[i] != '\0')
                    {
                        utf16 = false;
                        break;
                    }
                }
            }

            if (utf16)
            {
                std::size_t last_sep = std::string::npos;
                for (std::size_t i = 0; i + 1 < raw.size(); i += 2)
                {
                    if (raw[i + 1] == '\0' && (raw[i] == '\\' || raw[i] == '/'))
                        last_sep = i;
                }
                std::size_t start = (last_sep == std::string::npos) ? 0 : last_sep + 2;
                std::string result;
                for (std::size_t i = start; i + 1 < raw.size(); i += 2)
                {
                    if (raw[i] == '\0' && raw[i + 1] == '\0') break;
                    result += (raw[i + 1] == '\0') ? raw[i] : '?';
                }
                return result;
            }

            // ASCII / Shift-JIS
            auto sep = raw.find_last_of("\\/");
            return (sep == std::string::npos) ? raw : raw.substr(sep + 1);
        }
    } // anonymous namespace

    int CheckDS1Layouts(
        std::vector<VertexArrayLayout>& layouts,
        std::vector<VertexArrayHeaderView>& va_headers,
        const std::vector<Material>& materials,
        const std::vector<std::vector<std::uint32_t>>& mesh_va_indices)
    {
        int repaired = 0;

        for (std::size_t i = 0; i < materials.size() && i < mesh_va_indices.size(); ++i)
        {
            std::string mtd_name = extract_mtd_name(materials[i].mat_def_path);
            if (mtd_name.size() < 4 || mtd_name.substr(mtd_name.size() - 4) != ".mtd")
                continue;

            for (auto ai : mesh_va_indices[i])
            {
                if (ai >= va_headers.size()) continue;
                auto& hdr = va_headers[ai];
                if (hdr.layout_index >= layouts.size()) continue;

                auto layout_size = layouts[hdr.layout_index].compressed_vertex_size;
                if (hdr.vertex_size == layout_size) continue;

                // Bad layout — attempt repair.
                Debug(
                    std::format(
                        "[layout_repair] Fixing mesh {} (MTD '{}', layout {} != vertex {}",
                        i, mtd_name, layout_size, hdr.vertex_size));

                std::optional<VertexArrayLayout> guess;
                if (auto it = OTHER_MTDS.find(mtd_name); it != OTHER_MTDS.end())
                    guess = it->second();
                else if (UNSHADED_MTDS.contains(mtd_name))
                    guess = make_unshaded();

                if (!guess)
                {
                    Warning(std::format("[layout_repair] No known layout for '{}'.", mtd_name));
                    continue;
                }

                if (guess->compressed_vertex_size != hdr.vertex_size)
                {
                    Error(
                        std::format(
                            "[layout_repair] Repaired size {} still != header {}.",
                            guess->compressed_vertex_size,
                            hdr.vertex_size));
                    continue;
                }

                Info(
                    std::format(
                        "[layout_repair] Repaired: {} -> {}.",
                        layout_size, guess->compressed_vertex_size));

                hdr.layout_index = static_cast<std::uint32_t>(layouts.size());
                layouts.push_back(std::move(*guess));
                ++repaired;
            }
        }
        return repaired;
    }
} // namespace Firelink
