#include <fstream>

#include "FirelinkER/Maps/MapStudio/Route.h"
#include "Firelink/BinaryReadWrite.h"
#include "Firelink/BinaryValidation.h"

using namespace std;
using namespace Firelink::BinaryReadWrite;
using namespace Firelink::BinaryValidation;
using namespace FirelinkER::Maps::MapStudio;


struct RouteHeader
{
    int64_t nameOffset;
    int32_t unk08;
    int32_t unk0C;
    RouteType routeType;
    int32_t subtypeIndex;  // preserved for bizarre `OtherRoute` usage
    padding<0x68> _pad1;

    void Validate() const
    {
        AssertNotValue("RouteHeader.nameOffset", 0, nameOffset);
        AssertPadding("RouteHeader", _pad1);
    }
};


void Route::Deserialize(ifstream& stream)
{
    const streampos start = stream.tellg();
    const auto header = ReadValidatedStruct<RouteHeader>(stream);

    m_hUnk08 = header.unk08;
    m_hUnk0C = header.unk0C;
    subtypeIndexOverride = header.subtypeIndex;

    stream.seekg(start + header.nameOffset);
    m_name = ReadUTF16String(stream);
}


void Route::Serialize(std::ofstream& stream, const int supertypeIndex, const int subtypeIndex) const
{
    const streampos start = stream.tellp();

    Reserver reserver(stream, true);
    reserver.ReserveValidatedStruct("RouteHeader", sizeof(RouteHeader));
    auto header = RouteHeader
    {
        // Supertype index is not used.
        .unk08 = m_hUnk08,
        .unk0C = m_hUnk0C,
        .routeType = GetType(),
        .subtypeIndex = GetType() == RouteType::Other && subtypeIndexOverride >= 0 ? subtypeIndexOverride : subtypeIndex,
    };

    header.nameOffset = stream.tellp() - start;
    WriteUTF16String(stream, m_name);

    reserver.FillValidatedStruct("RouteHeader", header);

    AlignStream(stream, 8);

    reserver.Finish();
}
