#include <FirelinkERMaps/Navmesh.h>

#include <FirelinkCore/Havok/HkcdTypes.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace Firelink
{

// ============================================================================
// Internal BVH helpers
// ============================================================================

namespace
{

using namespace Havok;

/// Per-face cached AABB and centroid, used during BVH construction.
struct FaceAABB
{
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
    float cx, cy, cz;   ///< centroid
    int   faceIndex;
};

/// Compute the axis-aligned bounding box and centroid for one navmesh face.
/// Vertex positions are gathered by iterating the face's edges and reading edge.a.
FaceAABB ComputeFaceAABB(const hkaiNavMesh& navMesh, int faceIdx)
{
    const auto& face = navMesh.faces[faceIdx];
    FaceAABB fa;
    fa.faceIndex = faceIdx;
    fa.minX = fa.minY = fa.minZ =  std::numeric_limits<float>::max();
    fa.maxX = fa.maxY = fa.maxZ = -std::numeric_limits<float>::max();
    fa.cx = fa.cy = fa.cz = 0.f;

    const int n = face.numEdges;
    for (int e = 0; e < n; ++e)
    {
        const auto& edge = navMesh.edges[face.startEdgeIndex + e];
        const auto& v    = navMesh.vertices[edge.a];
        fa.minX = std::min(fa.minX, v.x);  fa.maxX = std::max(fa.maxX, v.x);
        fa.minY = std::min(fa.minY, v.y);  fa.maxY = std::max(fa.maxY, v.y);
        fa.minZ = std::min(fa.minZ, v.z);  fa.maxZ = std::max(fa.maxZ, v.z);
        fa.cx += v.x;  fa.cy += v.y;  fa.cz += v.z;
    }

    if (n > 0)
    {
        const float inv = 1.f / static_cast<float>(n);
        fa.cx *= inv;  fa.cy *= inv;  fa.cz *= inv;
    }
    return fa;
}

/// Quantize a world-space value to [0, 255] relative to the domain.
uint8_t Quantize(float value, float domainMin, float domainMax)
{
    const float range = domainMax - domainMin;
    if (range <= 0.f) return 0;
    const float t = (value - domainMin) / range;
    const int   q = static_cast<int>(t * 255.f + 0.5f);
    return static_cast<uint8_t>(std::clamp(q, 0, 255));
}

/// Compress an AABB min-corner into the three quantized xyz bytes.
std::array<uint8_t, 3> QuantizeMin(
    float minX, float minY, float minZ, const hkAabb& domain)
{
    return {
        Quantize(minX, domain.min.x, domain.max.x),
        Quantize(minY, domain.min.y, domain.max.y),
        Quantize(minZ, domain.min.z, domain.max.z),
    };
}

/// Recursively build the BVH into `nodes`, in depth-first order.
/// Left child is always emitted at (current node index + 1); right child's
/// absolute index is stored in loData of the parent node.
///
/// @param nodes  Output flat node array (grows via emplace_back).
/// @param faces  Working slice of face AABBs; the range [start, start+count) is sorted.
/// @param start  First face in this subtree.
/// @param count  Number of faces in this subtree.
/// @param domain Quantization domain.
void BuildBVH(
    std::vector<hkcdCompressedAabbCodecsAabb6BytesCodec>& nodes,
    std::vector<FaceAABB>& faces,
    int start, int count,
    const hkAabb& domain)
{
    // Reserve a slot for this node now; fill it in after children are built.
    const int myIndex = static_cast<int>(nodes.size());
    nodes.emplace_back();

    if (count == 1)
    {
        // ---- Leaf node -------------------------------------------------------
        const FaceAABB& fa = faces[start];
        nodes[myIndex].xyz    = QuantizeMin(fa.minX, fa.minY, fa.minZ, domain);
        nodes[myIndex].hiData = 0x00;                           // leaf flag
        nodes[myIndex].loData = static_cast<uint16_t>(fa.faceIndex);
        return;
    }

    // ---- Internal node -------------------------------------------------------
    // Compute the union AABB for faces in [start, start+count).
    float ux0 = std::numeric_limits<float>::max(),
          uy0 = std::numeric_limits<float>::max(),
          uz0 = std::numeric_limits<float>::max();
    float ux1 = -std::numeric_limits<float>::max(),
          uy1 = -std::numeric_limits<float>::max(),
          uz1 = -std::numeric_limits<float>::max();

    for (int i = start; i < start + count; ++i)
    {
        const FaceAABB& fa = faces[i];
        ux0 = std::min(ux0, fa.minX);  ux1 = std::max(ux1, fa.maxX);
        uy0 = std::min(uy0, fa.minY);  uy1 = std::max(uy1, fa.maxY);
        uz0 = std::min(uz0, fa.minZ);  uz1 = std::max(uz1, fa.maxZ);
    }

    // Choose the longest axis to split along.
    const float dx = ux1 - ux0;
    const float dy = uy1 - uy0;
    const float dz = uz1 - uz0;
    const int axis = (dx >= dy && dx >= dz) ? 0 : (dy >= dz ? 1 : 2);

    // Sort by centroid along the chosen axis.
    std::sort(
        faces.begin() + start,
        faces.begin() + start + count,
        [axis](const FaceAABB& a, const FaceAABB& b)
        {
            if (axis == 0) return a.cx < b.cx;
            if (axis == 1) return a.cy < b.cy;
            return a.cz < b.cz;
        });

    // Split at the median.
    const int leftCount  = count / 2;
    const int rightCount = count - leftCount;

    // Left subtree is contiguous with this node (DFS: leftChild = myIndex + 1).
    BuildBVH(nodes, faces, start, leftCount, domain);

    // Right subtree starts after the left subtree has been fully emitted.
    const int rightIndex = static_cast<int>(nodes.size());
    BuildBVH(nodes, faces, start + leftCount, rightCount, domain);

    // Now fill in the internal node (vector may have reallocated, use index).
    nodes[myIndex].xyz    = QuantizeMin(ux0, uy0, uz0, domain);
    nodes[myIndex].hiData = 0x80;                               // internal flag
    nodes[myIndex].loData = static_cast<uint16_t>(rightIndex);
}

} // anonymous namespace

// ============================================================================
// Navmesh::FromHkaiNavMesh
// ============================================================================

GameFile<EldenRing::Maps::Navmesh>::Ptr EldenRing::Maps::Navmesh::FromHkaiNavMesh(
    const Havok::hkaiNavMesh& srcMesh)
{
    using namespace Havok;

    const int faceCount = static_cast<int>(srcMesh.faces.size());
    if (faceCount == 0)
        throw std::invalid_argument("Cannot build AABB tree for a navmesh with no faces.");

    // ------------------------------------------------------------------
    // 1. Compute per-face AABBs and centroids.
    // ------------------------------------------------------------------
    std::vector<FaceAABB> faceAABBs;
    faceAABBs.reserve(faceCount);
    for (int i = 0; i < faceCount; ++i)
        faceAABBs.push_back(ComputeFaceAABB(srcMesh, i));

    // ------------------------------------------------------------------
    // 2. Build the tree domain as the AABB of all face centroids.
    //    Using centroids (rather than full vertex extents) matches the
    //    tight domain seen in Elden Ring files.
    // ------------------------------------------------------------------
    hkAabb domain{};
    domain.min.x = domain.min.y = domain.min.z =  std::numeric_limits<float>::max();
    domain.max.x = domain.max.y = domain.max.z = -std::numeric_limits<float>::max();

    for (const auto& fa : faceAABBs)
    {
        domain.min.x = std::min(domain.min.x, fa.cx);
        domain.min.y = std::min(domain.min.y, fa.cy);
        domain.min.z = std::min(domain.min.z, fa.cz);
        domain.max.x = std::max(domain.max.x, fa.cx);
        domain.max.y = std::max(domain.max.y, fa.cy);
        domain.max.z = std::max(domain.max.z, fa.cz);
    }

    // Expand the domain slightly so face AABB min/max values that equal the
    // centroid boundary don't clamp hard to 0 or 255.
    const float pad = 0.01f;
    domain.min.x -= pad;  domain.min.y -= pad;  domain.min.z -= pad;
    domain.max.x += pad;  domain.max.y += pad;  domain.max.z += pad;

    // ------------------------------------------------------------------
    // 3. Build the BVH.  A binary tree over N leaves has exactly 2N-1 nodes.
    // ------------------------------------------------------------------
    std::vector<hkcdCompressedAabbCodecsAabb6BytesCodec> nodes;
    nodes.reserve(2 * faceCount - 1);

    BuildBVH(nodes, faceAABBs, 0, faceCount, domain);

    // ------------------------------------------------------------------
    // 4. Assemble hkcdStaticAabbTree.
    // ------------------------------------------------------------------
    auto treeImpl        = std::make_unique<hkcdStaticAabbTreeImpl>();
    treeImpl->tree.nodes = std::move(nodes);
    treeImpl->tree.domain = domain;

    auto staticTree         = std::make_unique<hkcdStaticAabbTree>();
    staticTree->treePtr     = std::move(treeImpl);

    // ------------------------------------------------------------------
    // 5. Copy the source navmesh (geometry only; no clearance cache).
    // ------------------------------------------------------------------
    auto navMeshCopy = std::make_unique<hkaiNavMesh>();
    navMeshCopy->faces         = srcMesh.faces;
    navMeshCopy->edges         = srcMesh.edges;
    navMeshCopy->vertices      = srcMesh.vertices;
    // hkaiAnnotatedStreamingSet contains a unique_ptr, so deep-copy manually.
    navMeshCopy->streamingSets.reserve(srcMesh.streamingSets.size());
    for (const auto& ss : srcMesh.streamingSets)
    {
        hkaiAnnotatedStreamingSet copy;
        copy.side = ss.side;
        if (ss.streamingSet)
        {
            copy.streamingSet = std::make_unique<hkaiStreamingSet>(*ss.streamingSet);
        }
        navMeshCopy->streamingSets.push_back(std::move(copy));
    }
    navMeshCopy->faceData      = srcMesh.faceData;
    navMeshCopy->edgeData      = srcMesh.edgeData;
    navMeshCopy->faceDataStriding = srcMesh.faceDataStriding;
    navMeshCopy->edgeDataStriding = srcMesh.edgeDataStriding;
    navMeshCopy->flags         = srcMesh.flags;
    navMeshCopy->aabb          = srcMesh.aabb;
    navMeshCopy->erosionRadius = srcMesh.erosionRadius;
    navMeshCopy->userData      = srcMesh.userData;
    // clearanceCacheSeedingDataSet intentionally left null for new meshes.

    // ------------------------------------------------------------------
    // 6. Assemble mediator and return.
    // ------------------------------------------------------------------
    auto mediator       = std::make_unique<hkaiStaticTreeNavMeshQueryMediator>();
    mediator->tree      = std::move(staticTree);
    mediator->navMesh   = std::move(navMeshCopy);

    auto result         = std::make_unique<EldenRing::Maps::Navmesh>();
    result->m_mediator  = std::move(mediator);
    return result;
}

// ============================================================================
// Deserialize / Serialize  (TODO: HKX round-trip)
// ============================================================================

void EldenRing::Maps::Navmesh::Deserialize(BinaryReadWrite::BufferReader& reader)
{
    // TODO: Convert from HKX with hkaiNavMesh and hkaiStaticTreeNavMeshQueryMediator objects.
}

void EldenRing::Maps::Navmesh::Serialize(BinaryReadWrite::BufferWriter& writer) const
{
    // TODO: Convert to HKX with hkaiNavMesh and hkaiStaticTreeNavMeshQueryMediator objects.
}

} // namespace Firelink
