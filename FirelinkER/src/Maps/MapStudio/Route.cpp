#include <fstream>

#include "FirelinkER/Maps/MapStudio/Route.h"
#include "Firelink/BinaryReadWrite.h"
#include "Firelink/BinaryValidation.h"
#include "Firelink/MemoryUtils.h"

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
    int32_t subtypeIndex;  // preserved for `OtherRoute`
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

    unk08 = header.unk08;
    unk0C = header.unk0C;
    subtypeIndexOverride = header.subtypeIndex;

    stream.seekg(start + header.nameOffset);
    const u16string nameWide = ReadUTF16String(stream);
    m_name = Firelink::UTF16ToUTF8(nameWide);
}


void Route::Serialize(std::ofstream& stream, const int supertypeIndex, const int subtypeIndex) const
{
    const streampos start = stream.tellp();

    // Supertype index is not used.
    auto header = RouteHeader
    {
        .unk08 = unk08,
        .unk0C = unk0C,
        .routeType = GetType(),
        .subtypeIndex = GetType() == RouteType::Other && subtypeIndexOverride >= 0 ? subtypeIndexOverride : subtypeIndex,
    };

    header.nameOffset = stream.tellp() - start;
    WriteUTF16String(stream, m_name);
}
