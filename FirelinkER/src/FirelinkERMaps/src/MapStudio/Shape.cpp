#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkERMaps/MapStudio/MSBFormatError.h>
#include <FirelinkERMaps/MapStudio/Shape.h>

using namespace Firelink::BinaryReadWrite;
using namespace Firelink::EldenRing::Maps::MapStudio;

void Point::DeserializeShapeData(BufferReader& reader)
{
    throw MSBFormatError("Point shape should not have shape data to read.");
}

void Point::SerializeShapeData(BufferWriter& writer)
{
    throw MSBFormatError("Point shape should not have shape data to write.");
}

void Circle::DeserializeShapeData(BufferReader& reader)
{
    m_radius = reader.Read<float>();
}

void Circle::SerializeShapeData(BufferWriter& writer)
{
    writer.Write<float>(m_radius);
}

void Sphere::DeserializeShapeData(BufferReader& reader)
{
    m_radius = reader.Read<float>();
}

void Sphere::SerializeShapeData(BufferWriter& writer)
{
    writer.Write<float>(m_radius);
}

void Cylinder::DeserializeShapeData(BufferReader& reader)
{
    m_radius = reader.Read<float>();
    m_height = reader.Read<float>();
}

void Cylinder::SerializeShapeData(BufferWriter& writer)
{
    writer.Write<float>(m_radius);
    writer.Write<float>(m_height);
}

void Rectangle::DeserializeShapeData(BufferReader& reader)
{
    m_width = reader.Read<float>();
    m_depth = reader.Read<float>();
}

void Rectangle::SerializeShapeData(BufferWriter& writer)
{
    writer.Write<float>(m_width);
    writer.Write<float>(m_depth);
}

void Box::DeserializeShapeData(BufferReader& reader)
{
    m_width = reader.Read<float>();
    m_depth = reader.Read<float>();
    m_height = reader.Read<float>();
}

void Box::SerializeShapeData(BufferWriter& writer)
{
    writer.Write<float>(m_width);
    writer.Write<float>(m_depth);
    writer.Write<float>(m_height);
}
