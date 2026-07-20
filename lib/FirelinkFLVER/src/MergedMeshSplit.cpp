// MergedMeshSplit.cpp
//
// This is the export / split counterpart to MergedMesh.cpp (which is the
// import / merge side). It takes a merged mesh (typically built from Blender)
// and splits it back into FLVER submeshes: one per SplitMeshDef, and possibly
// several per def when a submesh would exceed the per-mesh bone limit.
//
// Design vs the Python source
// ---------------------------
// The Python version builds numpy structured arrays with two parallel dtypes
// (one using global UV layer names, one using local "uv_<i>" names) and renames
// at the end via a positional `astype`. We don't need any of that: we write each
// output vertex straight into the target layout's *decompressed* interleaved
// buffer, pulling each field from the right MergedMesh source array and placing
// it at the layout's local field offset. UV layers are resolved to local
// `uv_<i>` slots up front, so there is nothing to rename. Vertex dedup is then a
// plain byte-identity reduction over those decompressed vertices -- which is
// exactly what Python's `np.unique` over the full structured row computes.
//
// We derive `normal_w` from the vertex first bone index ONLY for non-dynamic
// submeshes whose layout stores the bone in normal_w (i.e. layout has normal_w
// but no bone_indices field). Dynamic submeshes that happen to carry a real
// normal_w byte take it from loop data.

#include <FirelinkFLVER/MergedMesh.h>

#include <FirelinkFLVER/Exceptions.h>
#include <FirelinkFLVER/Material.h>
#include <FirelinkFLVER/Mesh.h>
#include <FirelinkFLVER/VertexFormats.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Firelink
{
    namespace
    {
        // ====================================================================
        // Decompressed field map for the TARGET layout (the write-side twin of
        // build_field_map in MergedMesh.cpp). Returns each field's name plus its
        // byte offset / element count / scalar within one decompressed vertex.
        // Field order and offsets MUST match what VertexArray::Compress expects,
        // i.e. the same sequential walk over layout.types the read side uses.
        // ====================================================================

        struct FieldInfo
        {
            std::string name; // "position", "normal", "normal_w", "uv_0", ...
            std::uint32_t offset = 0; // byte offset within decompressed vertex
            std::uint32_t count = 0; // decompressed element count
            VertexScalarType scalar = VertexScalarType::F32; // decompressed scalar type

            [[nodiscard]] std::uint32_t Size() const
            {
                return static_cast<std::uint32_t>(GetScalarSize(scalar)) * count;
            }
        };

        std::vector<FieldInfo> BuildFieldMap(const VertexArrayLayout& layout)
        {
            std::vector<FieldInfo> fields;
            int uv_index = 0, tangent_index = 0, color_index = 0;
            std::uint32_t decomp_offset = 0;

            for (const auto& dt : layout.types)
            {
                if (dt.usage == VertexUsage::Ignore) continue;
                const auto info = FindVertexFormatInfo(dt.usage, dt.format);
                if (!info) continue;

                for (std::uint8_t f = 0; f < info->field_count; ++f)
                {
                    const auto& fs = info->fields[f];

                    if (dt.usage == VertexUsage::UV)
                    {
                        const bool has_two = (dt.format == VertexDataFormatEnum::FourFloats ||
                            dt.format == VertexDataFormatEnum::FourShorts);
                        if (has_two && fs.decompressed_count == 4)
                        {
                            const auto scalar_size =
                                static_cast<std::uint32_t>(GetScalarSize(fs.decompressed_scalar));
                            fields.push_back(
                                {
                                    "uv_" + std::to_string(uv_index),
                                    decomp_offset, 2, fs.decompressed_scalar
                                });
                            fields.push_back(
                                {
                                    "uv_" + std::to_string(uv_index + 1),
                                    decomp_offset + 2 * scalar_size, 2, fs.decompressed_scalar
                                });
                            decomp_offset += static_cast<std::uint32_t>(fs.GetDecompressedSize());
                            uv_index += 2;
                            continue;
                        }
                        fields.push_back(
                            {
                                "uv_" + std::to_string(uv_index),
                                decomp_offset, fs.decompressed_count, fs.decompressed_scalar
                            });
                        uv_index++;
                    }
                    else if (dt.usage == VertexUsage::Tangent)
                    {
                        fields.push_back(
                            {
                                "tangent_" + std::to_string(tangent_index),
                                decomp_offset, fs.decompressed_count, fs.decompressed_scalar
                            });
                        if (f == info->field_count - 1) tangent_index++;
                    }
                    else if (dt.usage == VertexUsage::Color)
                    {
                        fields.push_back(
                            {
                                "color_" + std::to_string(color_index),
                                decomp_offset, fs.decompressed_count, fs.decompressed_scalar
                            });
                        if (f == info->field_count - 1) color_index++;
                    }
                    else
                    {
                        fields.push_back({fs.name, decomp_offset, fs.decompressed_count, fs.decompressed_scalar});
                    }

                    decomp_offset += static_cast<std::uint32_t>(fs.GetDecompressedSize());
                }
            }
            return fields;
        }

        const FieldInfo* FindField(const std::vector<FieldInfo>& fields, const std::string_view name)
        {
            for (const auto& f : fields)
                if (f.name == name) return &f;
            return nullptr;
        }

        bool StartsWith(const std::string_view s, const std::string_view prefix)
        {
            return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
        }

        // ====================================================================
        // Source resolution: map each layout field to the MergedMesh array it
        // is filled from, resolved once per submesh.
        // ====================================================================

        enum class SourceKind
        {
            Position, BoneWeights, BoneIndices, Normal, NormalW,
            Tangent, Bitangent, Color, UV, Zero
        };

        struct ResolvedField
        {
            std::uint32_t offset = 0;
            std::uint32_t count = 0;
            SourceKind kind = SourceKind::Zero;
            std::uint32_t instance = 0; // tangent_/color_ slot
            const std::vector<float>* uv_data = nullptr; // for UV
            std::uint32_t uv_dim = 0; // columns in that UV layer
        };

        // Find a UV layer in the merged mesh by name; nullptr if absent.
        const MergedMesh::UVLayer* FindUVLayer(const MergedMesh& m, const std::string& name)
        {
            for (const auto& layer : m.loop_uvs)
                if (layer.name == name) return &layer;
            return nullptr;
        }

        std::vector<ResolvedField> ResolveFields(
            const std::vector<FieldInfo>& fmap, const SplitMeshDef& def, const MergedMesh& m)
        {
            std::vector<ResolvedField> resolved;
            resolved.reserve(fmap.size());

            for (const auto& fi : fmap)
            {
                ResolvedField r;
                r.offset = fi.offset;
                r.count = fi.count;

                if (fi.name == "position") r.kind = SourceKind::Position;
                else if (fi.name == "bone_weights") r.kind = SourceKind::BoneWeights;
                else if (fi.name == "bone_indices") r.kind = SourceKind::BoneIndices;
                else if (fi.name == "normal")
                    r.kind = m.loop_normals.empty() ? SourceKind::Zero : SourceKind::Normal;
                else if (fi.name == "normal_w") r.kind = SourceKind::NormalW; // derive/loop handled at write
                else if (fi.name == "bitangent")
                    r.kind = m.loop_bitangents.empty() ? SourceKind::Zero : SourceKind::Bitangent;
                else if (StartsWith(fi.name, "tangent_"))
                {
                    const auto t = static_cast<std::uint32_t>(std::stoi(fi.name.substr(8)));
                    if (t < m.loop_tangents.size())
                    {
                        r.kind = SourceKind::Tangent;
                        r.instance = t;
                    }
                    else r.kind = SourceKind::Zero;
                }
                else if (StartsWith(fi.name, "color_"))
                {
                    const auto c = static_cast<std::uint32_t>(std::stoi(fi.name.substr(6)));
                    if (c < m.loop_vertex_colors.size())
                    {
                        r.kind = SourceKind::Color;
                        r.instance = c;
                    }
                    else r.kind = SourceKind::Zero;
                }
                else if (StartsWith(fi.name, "uv_"))
                {
                    const auto u = static_cast<std::uint32_t>(std::stoi(fi.name.substr(3)));
                    std::string layer_name = "UVMap" + std::to_string(u);
                    if (u < def.uv_layer_names.size() && !def.uv_layer_names[u].empty())
                        layer_name = def.uv_layer_names[u];
                    if (const auto* layer = FindUVLayer(m, layer_name))
                    {
                        r.kind = SourceKind::UV;
                        r.uv_data = &layer->data;
                        r.uv_dim = layer->dim;
                    }
                    else r.kind = SourceKind::Zero; // unused UV slot -> zeros
                }
                else
                {
                    // cloth_tangent / cloth_bitangent and anything else are not
                    // FLVER output vertex fields; leave as zeros.
                    r.kind = SourceKind::Zero;
                }

                resolved.push_back(r);
            }
            return resolved;
        }

        // ====================================================================
        // Vertex writer: scatter one loop's data into a decompressed vertex.
        // ====================================================================

        void PutFloats(std::byte* dst, const float* src, const std::uint32_t n)
        {
            std::memcpy(dst, src, n * sizeof(float));
        }

        // Write the decompressed vertex for loop `L` (vertex `V`) into `dst`.
        // `boneLut` (non-null only for local-bone submeshes) maps global bone ->
        // local index; `dynamicUnusedMarking` forces zero-weight slots to local 0.
        // `deriveNormalWBone` derives normal_w from bone_indices[0].
        void WriteVertex(
            std::byte* dst, const std::uint32_t L, const std::uint32_t V,
            const std::vector<ResolvedField>& fields, const MergedMesh& m,
            const std::unordered_map<std::int32_t, std::int32_t>* boneLut,
            const bool dynamicUnusedMarking, const bool deriveNormalWBone)
        {
            for (const auto& f : fields)
            {
                std::byte* p = dst + f.offset;
                switch (f.kind)
                {
                case SourceKind::Position:
                    PutFloats(p, &m.positions[V * 3], std::min<std::uint32_t>(f.count, 3));
                    break;
                case SourceKind::BoneWeights:
                    PutFloats(p, &m.bone_weights[V * 4], std::min<std::uint32_t>(f.count, 4));
                    break;
                case SourceKind::BoneIndices:
                    {
                        std::int32_t out[4] = {0, 0, 0, 0};
                        const std::uint32_t n = std::min<std::uint32_t>(f.count, 4);
                        for (std::uint32_t k = 0; k < n; ++k)
                        {
                            const std::int32_t g = m.bone_indices[V * 4 + k];
                            if (boneLut)
                            {
                                if (dynamicUnusedMarking && m.bone_weights[V * 4 + k] == 0.0f)
                                {
                                    out[k] = 0; // unused slot -> local 0
                                }
                                else
                                {
                                    const auto it = boneLut->find(g);
                                    out[k] = it != boneLut->end() ? it->second : 0;
                                }
                            }
                            else
                            {
                                out[k] = g; // global bone index
                            }
                        }
                        std::memcpy(p, out, n * sizeof(std::int32_t));
                        break;
                    }
                case SourceKind::Normal:
                    PutFloats(p, &m.loop_normals[L * 3], std::min<std::uint32_t>(f.count, 3));
                    break;
                case SourceKind::NormalW:
                    {
                        std::uint8_t nw;
                        if (deriveNormalWBone)
                        {
                            const std::int32_t g = m.bone_indices[V * 4 + 0];
                            if (g < 0 || g > 255)
                                throw FLVERError(
                                    "normal_w bone index out of uint8 range (0-255): non-dynamic meshes "
                                    "cannot reference more than 256 distinct bones via normal_w.");
                            nw = static_cast<std::uint8_t>(g);
                        }
                        else
                        {
                            nw = m.loop_normals_w.empty() ? std::uint8_t{127} : m.loop_normals_w[L];
                        }
                        std::memcpy(p, &nw, 1);
                        break;
                    }
                case SourceKind::Tangent:
                    PutFloats(p, &m.loop_tangents[f.instance][L * 4], std::min<std::uint32_t>(f.count, 4));
                    break;
                case SourceKind::Bitangent:
                    PutFloats(p, &m.loop_bitangents[L * 4], std::min<std::uint32_t>(f.count, 4));
                    break;
                case SourceKind::Color:
                    PutFloats(p, &m.loop_vertex_colors[f.instance][L * 4], std::min<std::uint32_t>(f.count, 4));
                    break;
                case SourceKind::UV:
                    PutFloats(p, &(*f.uv_data)[L * f.uv_dim], std::min<std::uint32_t>(f.count, f.uv_dim));
                    break;
                case SourceKind::Zero:
                    break; // buffer is pre-zeroed
                }
            }
        }

        // ====================================================================
        // Bone-count sub-splitting
        // ====================================================================

        struct SubSplit
        {
            std::uint32_t face_start = 0;
            std::uint32_t face_end = 0;
            std::vector<std::int32_t> bones; // sorted global bone list (empty for non-local)
        };

        std::vector<SubSplit> ComputeSubSplits(
            const std::vector<std::uint32_t>& loopSeq, const std::uint32_t numFaces,
            const bool isDynamic, const int maxBonesPerMesh, const bool unusedMinusOne, const MergedMesh& m)
        {
            auto face_real_bones = [&](const std::uint32_t fi, std::set<std::int32_t>& outSet)
            {
                outSet.clear();
                for (std::uint32_t c = 0; c < 3; ++c)
                {
                    const std::uint32_t L = loopSeq[3 * fi + c];
                    const std::uint32_t V = m.loop_vertex_indices[L];
                    for (std::uint32_t k = 0; k < 4; ++k)
                    {
                        const std::int32_t g = m.bone_indices[V * 4 + k];
                        bool unused;
                        if (unusedMinusOne) unused = (g == -1);
                        else if (isDynamic) unused = (m.bone_weights[V * 4 + k] == 0.0f);
                        else unused = false; // non-dynamic: padding zeros are kept (matches Python)
                        if (!unused) outSet.insert(g);
                    }
                }
            };

            std::vector<std::uint32_t> boundaries{0};
            std::vector<std::vector<std::int32_t>> boneSets;
            std::set<std::int32_t> running;
            std::set<std::int32_t> faceSet;

            for (std::uint32_t fi = 0; fi < numFaces; ++fi)
            {
                face_real_bones(fi, faceSet);

                // A single triangle cannot be split across meshes; if it alone
                // exceeds the cap, fail loudly instead of emitting an empty sub-split.
                if (static_cast<int>(faceSet.size()) > maxBonesPerMesh)
                    throw FLVERError(
                        "A single face is influenced by " + std::to_string(faceSet.size()) +
                        " distinct bones, exceeding max_bones_per_mesh=" + std::to_string(maxBonesPerMesh) +
                        ". Reduce its bone influences or raise max_bones_per_mesh.");

                std::set<std::int32_t> merged = running;
                merged.insert(faceSet.begin(), faceSet.end());

                if (static_cast<int>(merged.size()) > maxBonesPerMesh)
                {
                    // Adding this face overflows: close the current sub-split here,
                    // and start a new one beginning at this face.
                    boundaries.push_back(fi);
                    boneSets.emplace_back(running.begin(), running.end()); // std::set -> sorted
                    running = faceSet;
                }
                else
                {
                    running = std::move(merged);
                }
            }

            boundaries.push_back(numFaces);
            boneSets.emplace_back(running.begin(), running.end());

            std::vector<SubSplit> result;
            result.reserve(boneSets.size());
            for (std::size_t i = 0; i < boneSets.size(); ++i)
                result.push_back({boundaries[i], boundaries[i + 1], std::move(boneSets[i])});
            return result;
        }

        // ====================================================================
        // Dedup: loops -> unique FLVER vertices (+ face vertex indices).
        // Both paths preserve first-appearance order, matching Python's de-sort.
        // ====================================================================

        void DedupExact(
            const std::vector<std::byte>& buffers, const std::uint32_t stride, const std::uint32_t nLoops,
            std::vector<std::byte>& outData, std::vector<std::uint32_t>& faceVerts, std::uint32_t& vertCount)
        {
            std::unordered_map<std::string, std::uint32_t> seen;
            seen.reserve(nLoops);
            outData.clear();
            faceVerts.resize(nLoops);
            vertCount = 0;

            for (std::uint32_t i = 0; i < nLoops; ++i)
            {
                const std::byte* span = &buffers[static_cast<std::size_t>(i) * stride];
                // Byte-identity key (consistent hash + equality).
                std::string key(reinterpret_cast<const char*>(span), stride);
                if (const auto it = seen.find(key); it != seen.end())
                {
                    faceVerts[i] = it->second;
                }
                else
                {
                    seen.emplace(std::move(key), vertCount);
                    outData.insert(outData.end(), span, span + stride);
                    faceVerts[i] = vertCount++;
                }
            }
        }

        void DedupApprox(
            const std::vector<std::byte>& buffers, std::uint32_t stride, std::uint32_t nLoops,
            const std::vector<FieldInfo>& fmap, float threshold,
            std::vector<std::byte>& outData, std::vector<std::uint32_t>& faceVerts, std::uint32_t& vertCount)
        {
            const FieldInfo* posField = FindField(fmap, "position");
            const std::uint32_t posOff = posField ? posField->offset : 0;
            const std::uint32_t posBytes = posField ? posField->Size() : 0;

            struct VecField
            {
                std::uint32_t off;
                std::uint32_t n;
            };
            struct ExactField
            {
                std::uint32_t off;
                std::uint32_t bytes;
            };
            std::vector<VecField> vecFields;
            std::vector<ExactField> exactFields; // non-vector, non-position

            for (const auto& fi : fmap)
            {
                if (fi.name == "position") continue;
                // NOTE: normal_w is intentionally NOT a vector field — it can be a
                // bone index, and dot-comparing bone indices merges distinct bones.
                const bool isVec = (fi.name == "normal") ||
                    StartsWith(fi.name, "tangent_") ||
                    StartsWith(fi.name, "bitangent");
                if (isVec) vecFields.push_back({fi.offset, fi.count});
                else exactFields.push_back({fi.offset, fi.Size()});
            }

            std::unordered_map<std::string, std::uint32_t> exactSeen; // full bytes -> vertex
            std::unordered_map<std::string, std::vector<std::uint32_t>> byPos; // pos bytes -> vertex list
            std::vector<std::uint32_t> repLoop; // vertex -> representative loop index in `buffers`

            outData.clear();
            faceVerts.resize(nLoops);
            vertCount = 0;

            for (std::uint32_t i = 0; i < nLoops; ++i)
            {
                const std::byte* span = &buffers[static_cast<std::size_t>(i) * stride];
                std::string full(reinterpret_cast<const char*>(span), stride);
                if (const auto eit = exactSeen.find(full); eit != exactSeen.end())
                {
                    faceVerts[i] = eit->second;
                    continue;
                }

                std::string posKey(reinterpret_cast<const char*>(span + posOff), posBytes);
                auto& bucket = byPos[posKey];

                bool found = false;
                for (const std::uint32_t vi : bucket)
                {
                    const std::byte* cand = &buffers[static_cast<std::size_t>(repLoop[vi]) * stride];

                    bool exactEq = true;
                    for (const auto& [off, bytes] : exactFields)
                        if (std::memcmp(span + off, cand + off, bytes) != 0)
                        {
                            exactEq = false;
                            break;
                        }
                    if (!exactEq) continue; // try next candidate (fix: was `break` in Python)

                    bool vecEq = true;
                    for (const auto& [off, n] : vecFields)
                    {
                        const auto* a = reinterpret_cast<const float*>(span + off);
                        const auto* b = reinterpret_cast<const float*>(cand + off);
                        float dot = 0.f;
                        for (std::uint32_t k = 0; k < n; ++k) dot += a[k] * b[k];
                        if (!(dot > threshold))
                        {
                            vecEq = false;
                            break;
                        }
                    }
                    if (!vecEq) continue;

                    faceVerts[i] = vi;
                    exactSeen.emplace(std::move(full), vi);
                    found = true;
                    break;
                }

                if (!found)
                {
                    const std::uint32_t vi = vertCount++;
                    outData.insert(outData.end(), span, span + stride);
                    repLoop.push_back(i);
                    faceVerts[i] = vi;
                    exactSeen.emplace(std::move(full), vi);
                    bucket.push_back(vi);
                }
            }
        }
    } // namespace

    std::vector<Mesh> MergedMesh::SplitMesh(
        const std::vector<SplitMeshDef>& splitMeshDefs,
        const SplitMeshParams& params) const
    {
        if (params.useMeshBoneIndices && params.maxBonesPerMesh < 3)
            throw FLVERError("max_bones_per_mesh must be >= 3 (and realistically much higher).");

        std::vector<Mesh> outMeshes;

        const auto defCount = static_cast<std::uint32_t>(splitMeshDefs.size());
        for (std::uint32_t materialIndex = 0; materialIndex < defCount; ++materialIndex)
        {
            const SplitMeshDef& def = splitMeshDefs[materialIndex];

            // --- Gather this material's faces (as loop-index triplets). --------
            std::vector<std::uint32_t> loopSeq; // face-corner loop indices (3 per face)
            for (std::uint32_t f = 0; f < face_count; ++f)
            {
                if (faces[f * 4 + 3] != materialIndex) continue;
                loopSeq.push_back(faces[f * 4 + 0]);
                loopSeq.push_back(faces[f * 4 + 1]);
                loopSeq.push_back(faces[f * 4 + 2]);
            }
            if (loopSeq.empty()) continue; // unused material index
            const auto numFaces = static_cast<std::uint32_t>(loopSeq.size() / 3);

            // --- Target layout field map / stride. ----------------------------
            const std::vector<FieldInfo> fmap = BuildFieldMap(def.layout);
            std::uint32_t stride = 0;
            for (const auto& fi : fmap) stride = std::max(stride, fi.offset + fi.Size());

            const bool layoutHasBoneIndices = FindField(fmap, "bone_indices") != nullptr;
            const bool layoutHasNormalW = FindField(fmap, "normal_w") != nullptr;
            const bool useLocalBones = layoutHasBoneIndices && params.useMeshBoneIndices;
            // Derive normal_w from bone[0] only for rigid meshes that store the
            // single bone there (normal_w present, no bone_indices field).
            const bool deriveNormalWBone = layoutHasNormalW && !layoutHasBoneIndices && !def.is_dynamic;
            const bool dynamicUnusedMarking = def.is_dynamic && !params.unusedBoneIndicesAreMinusOne;

            const std::vector<ResolvedField> resolved = ResolveFields(fmap, def, *this);

            // --- Determine sub-splits (bone-count splitting) or a single split.
            std::vector<SubSplit> sub_splits;
            if (useLocalBones)
                sub_splits = ComputeSubSplits(
                    loopSeq, numFaces, def.is_dynamic,
                    params.maxBonesPerMesh, params.unusedBoneIndicesAreMinusOne, *this);
            else
                sub_splits.push_back({0, numFaces, {}});

            for (auto& [face_start, face_end, bones] : sub_splits)
            {
                // Local bone LUT (global -> rank) for this sub-split.
                std::unordered_map<std::int32_t, std::int32_t> boneLut;
                if (useLocalBones)
                {
                    boneLut.reserve(bones.size() * 2);
                    for (std::size_t i = 0; i < bones.size(); ++i)
                        boneLut[bones[i]] = static_cast<std::int32_t>(i);
                }

                // Build decompressed vertices for every loop in this sub-split.
                const std::uint32_t loStart = 3 * face_start;
                const std::uint32_t loEnd = 3 * face_end;
                const std::uint32_t nLoops = loEnd - loStart;

                std::vector buffers(static_cast<std::size_t>(nLoops) * stride, std::byte{0});
                for (std::uint32_t i = 0; i < nLoops; ++i)
                {
                    const std::uint32_t L = loopSeq[loStart + i];
                    const std::uint32_t V = loop_vertex_indices[L];
                    WriteVertex(
                        &buffers[static_cast<std::size_t>(i) * stride], L, V, resolved, *this,
                        useLocalBones ? &boneLut : nullptr, dynamicUnusedMarking, deriveNormalWBone);
                }

                // Reduce loops to unique vertices.
                std::vector<std::byte> vData;
                std::vector<std::uint32_t> faceVerts;
                std::uint32_t vertCount = 0;
                if (params.normalTangentDotThreshold >= 1.0f)
                    DedupExact(buffers, stride, nLoops, vData, faceVerts, vertCount);
                else
                    DedupApprox(
                        buffers, stride, nLoops, fmap, params.normalTangentDotThreshold,
                        vData, faceVerts, vertCount);

                if (params.maxVerticesPerMesh > 0 && vertCount > params.maxVerticesPerMesh)
                    throw FLVERError(
                        "Submesh has " + std::to_string(vertCount) + " vertices, exceeding the maximum of " +
                        std::to_string(params.maxVerticesPerMesh) + ". Simplify the mesh, split its material slot, "
                        "or lower `normalTangentDotThreshold` to merge more aggressively.");

                // --- Assemble the output Mesh. --------------------------------
                VertexArray va;
                va.layout = def.layout;
                va.layout.decompressed_vertex_size = stride; // ensure Compress sees the right stride
                va.vertex_count = vertCount;
                va.decompressed_data = std::move(vData);

                FaceSet faceSet;
                faceSet.flags = 0;
                faceSet.is_triangle_strip = false;
                faceSet.use_backface_culling = def.use_backface_culling;
                faceSet.vertex_indices.assign(faceVerts.begin(), faceVerts.end());

                Mesh mesh;
                mesh.is_dynamic = def.is_dynamic;
                mesh.default_bone_index = def.default_bone_index;
                mesh.material = def.material;
                mesh.uses_bounding_boxes = def.uses_bounding_boxes;
                if (useLocalBones) mesh.bone_indices = bones; // global bone list for this submesh
                mesh.vertex_arrays.push_back(std::move(va));
                mesh.face_sets.push_back(std::move(faceSet));

                // Duplicate the base face set to LOD levels 1/2 if requested.
                for (int i = 1; i < def.face_set_count; ++i)
                {
                    FaceSet lod = mesh.face_sets[0];
                    lod.flags = i; // 1 or 2
                    mesh.face_sets.push_back(std::move(lod));
                }

                // TODO: Refresh FLVER2 bounding boxes. Something like:
                //   if (!isFlver0 && mesh.uses_bounding_boxes)
                //       mesh.bounding_box = ComputeAABB(va.decompressed_data, stride,
                //                                       find_field(fmap, "position")->offset, vertCount);
                (void)params.isFlver0;

                outMeshes.push_back(std::move(mesh));
            }
        }

        return outMeshes;
    }
} // namespace Firelink
