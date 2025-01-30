#include <fstream>
#include <string>
#include <memory>
#include <vector>

#include "GrimHookER/Maps/MapStudio/Model.h"
#include "GrimHookER/Maps/MapStudio/MSBFormatError.h"
#include "GrimHook/BinaryReadWrite.h"
#include "GrimHook/BinaryValidation.h"

using namespace std;
using namespace GrimHookER::Maps::MapStudio;
using namespace GrimHook::BinaryReadWrite;
using namespace GrimHook::BinaryValidation;


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
        {
            throw MSBFormatError("nameOffset must not be 0.");
        }
        if (sibPathOffset == 0)
        {
            throw MSBFormatError("sibOffset must not be 0.");
        }
        if (subtypeDataOffset != 0)
        {
            throw MSBFormatError("subtypeDataOffset must be 0 for models.");
        }
    }
};


void Model::Deserialize(ifstream& stream)
{
    const streampos start = stream.tellg();

    const auto header = ReadValidatedStruct<ModelHeader>(stream);

    stream.seekg(start + header.nameOffset);
    u16string nameWide = ReadUTF16String(stream);
    m_name = string(nameWide.begin(), nameWide.end());

    if (header.modelDataType != GetType())
    {
        throw MSBFormatError("Model header/class subtype mismatch.");
    }
    instanceCount = header.instanceCount;

    stream.seekg(start + header.sibPathOffset);
    u16string sibPathWide = ReadUTF16String(stream);
    sibPath = string(sibPathWide.begin(), sibPathWide.end());

    // No subtype data for models.
}


void Model::Serialize(ofstream& stream, int supertypeIndex, const int subtypeIndex) const
{
    // `supertypeIndex` is not used.
    const streampos start = stream.tellp();

    Reserver reserver(stream, true);

    ModelHeader header
    {
        .modelDataType = GetType(),
        .subtypeIndex = subtypeIndex,
        .instanceCount = instanceCount,
        .unk1C = hUnk1C,
    };
    reserver.ReserveValidatedStruct("ModelHeader", sizeof(ModelHeader));

    header.nameOffset = stream.tellp() - start;
    WriteUTF16String(stream, m_name);

    header.sibPathOffset = stream.tellp() - start;
    WriteUTF16String(stream, sibPath);

    AlignStream(stream, 8);

    reserver.FillValidatedStruct("ModelHeader", header);

    reserver.Finish();
}
