#include "MSBBufferHelpers.h"
#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/BinaryValidation.h>
#include <FirelinkERMaps/MapStudio/Route.h>

using namespace Firelink::BinaryReadWrite;
using namespace Firelink::BinaryValidation;
using namespace Firelink::EldenRing::Maps::MapStudio;
using namespace Firelink::EldenRing::Maps::MapStudio::BufferHelpers;

struct RouteHeader
{
    int64_t nameOffset;
    int32_t unk08;
    int32_t unk0C;
    RouteType routeType;
    int32_t subtypeIndex;
    padding<0x68> _pad1;

    void Validate() const
    {
        AssertNotValue("RouteHeader.nameOffset", 0, nameOffset);
        AssertPadding("RouteHeader", _pad1);
    }
};

void Route::Deserialize(BufferReader& reader)
{
    const size_t start = reader.Position();
    const auto header = ReadValidatedStruct<RouteHeader>(reader);

    m_hUnk08 = header.unk08;
    m_hUnk0C = header.unk0C;
    subtypeIndexOverride = header.subtypeIndex;

    reader.Seek(start + header.nameOffset);
    m_name = BufferHelpers::ReadUTF16String(reader);
}

void Route::Serialize(BufferWriter& writer, const int supertypeIndex, const int subtypeIndex) const
{
    const size_t start = writer.Position();
    const void* scope = this;

    writer.Reserve<RouteHeader>("RouteHeader", scope);
    auto header = RouteHeader{
        .unk08 = m_hUnk08,
        .unk0C = m_hUnk0C,
        .routeType = GetType(),
        .subtypeIndex = GetType() == RouteType::Other && subtypeIndexOverride >= 0 ? subtypeIndexOverride : subtypeIndex,
    };

    header.nameOffset = static_cast<int64_t>(writer.Position() - start);
    BufferHelpers::WriteUTF16String(writer, m_name);

    header.Validate();
    writer.Fill<RouteHeader>("RouteHeader", header, scope);

    writer.PadAlign(8);
}
