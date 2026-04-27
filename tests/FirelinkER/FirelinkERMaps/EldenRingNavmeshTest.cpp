#include <doctest/doctest.h>

#include <FirelinkCore/HKX.h>
#include <FirelinkCore/Logging.h>
#include <FirelinkTestHelpers.h>
#include <FirelinkERMaps/Navmesh.h>

#include "FirelinkCore/Havok/HkaiTypes.h"
#include "FirelinkCore/Havok/HkcdTypes.h"

using namespace Firelink;
using namespace Firelink::Havok;

TEST_CASE("MSB: Load Chapel of Anticipation navmesh 001000 without error")
{
    HKX::Ptr hkx = HKX::FromPath(GetResourcePath("eldenring/n10_01_00_00_001000.hkx"));
    auto& namedVariants = hkx->root->namedVariants;
    Info("HKX named variants count: {}", namedVariants.size());

    for (const auto& v : namedVariants)
    {
        Info("Variant name / className: {}, {}", v.name, v.className);
    }

    // Get variants.
    hkaiNavMesh* navMesh = GetVariant<hkaiNavMesh>(*hkx->root);
    REQUIRE(navMesh != nullptr);
    hkaiStaticTreeNavMeshQueryMediator* mediator = GetVariant<hkaiStaticTreeNavMeshQueryMediator>(*hkx->root);
    REQUIRE(mediator != nullptr);

    // Mediator should reference the same navMesh (not a copy, not nullptr).
    REQUIRE(mediator->navMesh.get() == navMesh);

    // NAVMESH
    const auto faceCount = navMesh->faces.size();
    const auto edgeCount = navMesh->edges.size();
    const auto vertexCount = navMesh->vertices.size();
    REQUIRE(faceCount > 0);
    REQUIRE(edgeCount > 0);
    REQUIRE(vertexCount > 0);
    Info("Face count: {}", faceCount);
    Info("Edge count: {}", edgeCount);
    Info("Vertex count: {}", vertexCount);

    // Print first three and last three faces.
    for (int i = 0; i < 3; ++i)
        Info("Face {}: start edge {}", i, navMesh->faces[i].startEdgeIndex);
    for (int i = faceCount - 3; i < faceCount; ++i)
        Info("Face {}: start edge {}", i, navMesh->faces[i].startEdgeIndex);

    // Print first three and last three edges.
    for (int i = 0; i < 3; ++i)
        Info("Edge {}: a {} -> b {}", i, navMesh->edges[i].a, navMesh->edges[i].b);
    for (int i = edgeCount - 3; i < edgeCount; ++i)
        Info("Edge {}: a {} -> b {}", i, navMesh->edges[i].a, navMesh->edges[i].b);

    // Print first three and last three vertices.
    for (int i = 0; i < 3; ++i)
    {
        auto v = navMesh->vertices[i];
        Info("Vertex {}: {:.2f},{:.2f},{:.2f}", i, v.x, v.y, v.z);
    }
    for (int i = vertexCount - 3; i < vertexCount; ++i)
    {
        auto v = navMesh->vertices[i];
        Info("Vertex {}: {:.2f},{:.2f},{:.2f}", i, v.x, v.y, v.z);
    }

    Info("Streaming set count: {}", navMesh->streamingSets.size());
    Info("Face data size: {}", navMesh->faceData.size());
    Info("Edge data size: {}", navMesh->edgeData.size());
    Info("Face data striding: {}", navMesh->faceDataStriding);
    Info("Edge data striding: {}", navMesh->edgeDataStriding);
    Info("Flags: {}", navMesh->flags);
    Info("AABB min: {:.2f},{:.2f},{:.2f}", navMesh->aabb.min.x, navMesh->aabb.min.y, navMesh->aabb.min.z);
    Info("AABB max: {:.2f},{:.2f},{:.2f}", navMesh->aabb.max.x, navMesh->aabb.max.y, navMesh->aabb.max.z);
    Info("Erosion radius: {}", navMesh->erosionRadius);
    Info("User data: {}", navMesh->userData);
    Info("Has clearance cache: {}", navMesh->clearanceCacheSeedingDataSet != nullptr);
    if (navMesh->clearanceCacheSeedingDataSet)
    {
        Info("Clearance cache seeding data count: {}", navMesh->clearanceCacheSeedingDataSet->cacheDatas.size());
        for (const auto& cacheData : navMesh->clearanceCacheSeedingDataSet->cacheDatas)
        {
            Info("  Cache data: id {}, info {}, infoMask {}, has initial cache {}",
                cacheData.id, cacheData.info, cacheData.infoMask, cacheData.initialCache != nullptr);
            if (cacheData.initialCache)
            {
                auto& cache = *cacheData.initialCache;
                Info("    Initial cache clearanceCeiling = {}", cache.clearanceCeiling);
                Info("    Initial cache clearanceIntToRealMultiplier = {}", cache.clearanceIntToRealMultiplier);
                Info("    Initial cache clearanceRealToIntMultiplier = {}", cache.clearanceRealToIntMultiplier);
                Info("    Initial cache faceOffsets size = {}", cache.faceOffsets.size());
                // Print first and last three faceOffsets.
                if (cache.faceOffsets.size() >= 3)
                {
                    auto size = cache.faceOffsets.size();
                    for (int i = 0; i < 3; ++i)
                        Info("      faceOffsets[{}] = {}", i, cache.faceOffsets[i]);
                    for (int i = size - 3; i < size; ++i)
                        Info("      faceOffsets[{}] = {}", i, cache.faceOffsets[i]);
                }
                Info("    Initial cache edgePairClearances size = {}", cache.edgePairClearances.size());
                if (cache.edgePairClearances.size() >= 3)
                {
                    auto size = cache.edgePairClearances.size();
                    for (int i = 0; i < 3; ++i)
                        Info("      edgePairClearances[{}] = {}", i, cache.edgePairClearances[i]);
                    for (int i = size - 3; i < size; ++i)
                        Info("      edgePairClearances[{}] = {}", i, cache.edgePairClearances[i]);
                }
                Info("    Initial cache unusedEdgePairElements = {}", cache.unusedEdgePairElements);
                Info("    Initial cache mcpData size = {}", cache.mcpData.size());
                Info("    Initial cache vertexClearances size = {}", cache.vertexClearances.size());
                if (cache.vertexClearances.size() >= 3)
                {
                    auto size = cache.vertexClearances.size();
                    for (int i = 0; i < 3; ++i)
                        Info("      vertexClearances[{}] = {}", i, cache.vertexClearances[i]);
                    for (int i = size - 3; i < size; ++i)
                        Info("      vertexClearances[{}] = {}", i, cache.vertexClearances[i]);
                }
                uint8_t minVertexClearance = 255;
                uint8_t maxVertexClearance = 0;
                for (uint8_t c : cache.vertexClearances)
                {
                    minVertexClearance = std::min(minVertexClearance, c);
                    maxVertexClearance = std::max(maxVertexClearance, c);
                }
                Info("      vertexClearances min = {}", minVertexClearance);
                Info("      vertexClearances max = {}", maxVertexClearance);

                Info("    Initial cache uncalculatedFacesLowerBound = {}", cache.uncalculatedFacesLowerBound);
            }
        }
    }

    for (auto& ss : navMesh->streamingSets)
    {
        Info("Streaming set side: {}", ss.side);
        if (ss.streamingSet)
        {
            Info("  Streaming set has {} connections", ss.streamingSet->meshConnections.size());
            for (const auto& conn : ss.streamingSet->meshConnections)
            {
                Info("    Connection: aFace {} edge {}, bFace {} edge {}",
                    conn.aFaceEdgeIndex.faceIndex, conn.aFaceEdgeIndex.edgeIndex,
                    conn.bFaceEdgeIndex.faceIndex, conn.bFaceEdgeIndex.edgeIndex);
            }
        }
    }

    // MEDIATOR
    auto& tree = mediator->tree;

    if (tree)
    {
        auto& treeImpl = tree->treePtr->tree;
        Info("Static AABB tree node count: {}", treeImpl.nodes.size());
        auto& domain = treeImpl.domain;
        Info("Static AABB tree domain min: {:.2f},{:.2f},{:.2f}", domain.min.x, domain.min.y, domain.min.z);
        Info("Static AABB tree domain max: {:.2f},{:.2f},{:.2f}", domain.max.x, domain.max.y, domain.max.z);

        for (int i = 0; i < treeImpl.nodes.size(); ++i)
        {
            const auto& node = treeImpl.nodes[i];
            Info("  Node {}: xyz = {},{},{}, hiData = {}, loData = {}",
                i, node.xyz[0], node.xyz[1], node.xyz[2], node.hiData, node.loData);
        }
    }
}

TEST_CASE("Navmesh: FromHkaiNavMesh generates a valid static AABB tree")
{
    HKX::Ptr hkx = HKX::FromPath(GetResourcePath("eldenring/n10_01_00_00_001000.hkx"));
    hkaiNavMesh* srcMesh = GetVariant<hkaiNavMesh>(*hkx->root);
    REQUIRE(srcMesh != nullptr);

    const auto faceCount = static_cast<int>(srcMesh->faces.size());

    // Build a new navmesh with a generated AABB tree.
    EldenRing::Maps::Navmesh::Ptr navmesh = EldenRing::Maps::Navmesh::FromHkaiNavMesh(*srcMesh);
    REQUIRE(navmesh != nullptr);

    const auto* mediator = navmesh->GetMediator();
    REQUIRE(mediator != nullptr);
    REQUIRE(mediator->tree != nullptr);
    REQUIRE(mediator->tree->treePtr != nullptr);
    REQUIRE(mediator->navMesh != nullptr);

    const auto& treeImpl = mediator->tree->treePtr->tree;
    const auto& nodes    = treeImpl.nodes;
    const auto& domain   = treeImpl.domain;
    const int   nodeCount = static_cast<int>(nodes.size());

    // --- Structural checks ---------------------------------------------------

    // A binary tree over N leaves has exactly 2N-1 nodes.
    REQUIRE(nodeCount == 2 * faceCount - 1);
    Info("Generated node count: {} (expected {})", nodeCount, 2 * faceCount - 1);

    // Domain must be non-degenerate on every axis.
    CHECK(domain.min.x < domain.max.x);
    CHECK(domain.min.y < domain.max.y);
    CHECK(domain.min.z < domain.max.z);
    Info("Domain min: {:.4f},{:.4f},{:.4f}", domain.min.x, domain.min.y, domain.min.z);
    Info("Domain max: {:.4f},{:.4f},{:.4f}", domain.max.x, domain.max.y, domain.max.z);

    // Root must be an internal node (the tree has more than one face).
    CHECK((nodes[0].hiData & 0x80) == 0x80);

    // --- Per-node validity ---------------------------------------------------
    int leafCount     = 0;
    int internalCount = 0;

    // Track which face indices are referenced by leaves (each should appear exactly once).
    std::vector<int> faceRefCount(faceCount, 0);

    for (int i = 0; i < nodeCount; ++i)
    {
        const auto& node = nodes[i];
        const bool isLeaf = (node.hiData & 0x80) == 0x00;

        if (isLeaf)
        {
            ++leafCount;
            const int faceIdx = static_cast<int>(node.loData);
            // Face index must be in range.
            CHECK(faceIdx >= 0);
            CHECK(faceIdx < faceCount);
            if (faceIdx >= 0 && faceIdx < faceCount)
                ++faceRefCount[faceIdx];
        }
        else
        {
            ++internalCount;
            const int rightChild = static_cast<int>(node.loData);
            // Right child must be a valid index that comes after this node.
            CHECK(rightChild > i);
            CHECK(rightChild < nodeCount);
            // Left child is implicitly at i+1.
            CHECK(i + 1 < nodeCount);
        }
    }

    // Leaf / internal split must match a full binary tree.
    CHECK(leafCount == faceCount);
    CHECK(internalCount == faceCount - 1);
    Info("Leaf nodes: {}, internal nodes: {}", leafCount, internalCount);

    // Every face index must be referenced by exactly one leaf.
    bool allFacesReferenced = true;
    for (int i = 0; i < faceCount; ++i)
    {
        if (faceRefCount[i] != 1)
        {
            allFacesReferenced = false;
            Info("Face {} referenced {} times (expected 1)", i, faceRefCount[i]);
        }
    }
    CHECK(allFacesReferenced);

    // --- Mesh copy checks ----------------------------------------------------
    CHECK(mediator->navMesh->faces.size()    == srcMesh->faces.size());
    CHECK(mediator->navMesh->edges.size()    == srcMesh->edges.size());
    CHECK(mediator->navMesh->vertices.size() == srcMesh->vertices.size());
    CHECK(mediator->navMesh->clearanceCacheSeedingDataSet == nullptr);
}

