#pragma once

#include <FirelinkCore/GameFile.h>
#include <FirelinkCore/Havok/HkaiTypes.h>

namespace Firelink::EldenRing::Maps
{
    class Navmesh : public GameFile<Navmesh>
    {
    public:
        [[nodiscard]] static BinaryReadWrite::Endian GetEndian() noexcept
        {
            return BinaryReadWrite::Endian::Little;
        }

        /// @brief Construct full NavMesh with static AABB tree from just the mesh component.
        /// Builds a median-split BVH over face AABBs and wraps everything in a
        /// hkaiStaticTreeNavMeshQueryMediator.
        static Ptr FromHkaiNavMesh(const Havok::hkaiNavMesh& navMesh);

        /// @brief Access the built mediator (tree + mesh).
        [[nodiscard]] const Havok::hkaiStaticTreeNavMeshQueryMediator* GetMediator() const
        {
            return m_mediator.get();
        }

        void Deserialize(BinaryReadWrite::BufferReader& reader);
        void Serialize(BinaryReadWrite::BufferWriter& writer) const;

    private:
        std::unique_ptr<Havok::hkaiStaticTreeNavMeshQueryMediator> m_mediator;
    };
}
