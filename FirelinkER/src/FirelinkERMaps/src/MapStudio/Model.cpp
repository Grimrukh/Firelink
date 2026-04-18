#include "MSBBufferHelpers.h"
#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/BinaryValidation.h>
#include <FirelinkERMaps/MapStudio/Model.h>
#include <FirelinkERMaps/MapStudio/MSBFormatError.h>

#include <string>

using namespace Firelink::EldenRing::Maps::MapStudio;
using namespace Firelink::EldenRing::Maps::MapStudio::BufferHelpers;
using namespace Firelink::BinaryReadWrite;
using namespace Firelink::BinaryValidation;

struct ModelHeader
{
    int64_t nameOffset;
    ModelType modelDataType;
    int32_t subtypeIndex;
    int64_t sibPathOffset;
    int32_t instanceCount;
    int32_t unk1C;
    int64_t subtypeDataOffset;

    void Validate() const
    {
        if (nameOffset == 0)
            throw MSBFormatError("nameOffset must not be 0.");
        if (sibPathOffset == 0)
            throw MSBFormatError("sibOffset must not be 0.");
        if (subtypeDataOffset != 0)
            throw MSBFormatError("subtypeDataOffset must be 0 for models.");
    }
};

void Model::Deserialize(BufferReader& reader)
{
    const size_t start = reader.Position();

    const auto header = ReadValidatedStruct<ModelHeader>(reader);

    reader.Seek(start + header.nameOffset);
    m_name = BufferHelpers::ReadUTF16String(reader);

    if (header.modelDataType != GetType())
        throw MSBFormatError("Model header/class subtype mismatch.");
    m_instanceCount = header.instanceCount;

    reader.Seek(start + header.sibPathOffset);
    m_sibPath = BufferHelpers::ReadUTF16String(reader);
}

void Model::Serialize(BufferWriter& writer, int supertypeIndex, const int subtypeIndex) const
{
    const size_t start = writer.Position();
    const void* scope = this;

    ModelHeader header{
        .modelDataType = GetType(),
        .subtypeIndex = subtypeIndex,
        .instanceCount = m_instanceCount,
        .unk1C = hUnk1C,
    };
    writer.Reserve<ModelHeader>("ModelHeader", scope);

    header.nameOffset = static_cast<int64_t>(writer.Position() - start);
    BufferHelpers::WriteUTF16String(writer, m_name);

    header.sibPathOffset = static_cast<int64_t>(writer.Position() - start);
    BufferHelpers::WriteUTF16String(writer, m_sibPath);

    writer.PadAlign(8);

    header.Validate();
    writer.Fill<ModelHeader>("ModelHeader", header, scope);
}
