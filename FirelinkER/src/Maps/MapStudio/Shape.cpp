
#include "FirelinkER/Maps/MapStudio/MSBFormatError.h"
#include "FirelinkER/Maps/MapStudio/Shape.h"
#include "Firelink/BinaryReadWrite.h"

using namespace std;
using namespace Firelink::BinaryReadWrite;
using namespace FirelinkER::Maps::MapStudio;


void Point::DeserializeShapeData(ifstream &stream)
{
    throw MSBFormatError("Point shape should not have shape data to read.");
}

void Point::SerializeShapeData(ofstream &stream)
{
    throw MSBFormatError("Point shape should not have shape data to write.");
}

void Circle::DeserializeShapeData(ifstream &stream)
{
    m_radius = ReadValue<float>(stream);
}

void Circle::SerializeShapeData(ofstream &stream)
{
    WriteValue<float>(stream, m_radius);
}

void Sphere::DeserializeShapeData(ifstream &stream)
{
    m_radius = ReadValue<float>(stream);
}

void Sphere::SerializeShapeData(ofstream &stream)
{
    WriteValue<float>(stream, m_radius);
}

void Cylinder::DeserializeShapeData(ifstream &stream)
{
    m_radius = ReadValue<float>(stream);
    m_height = ReadValue<float>(stream);
}

void Cylinder::SerializeShapeData(ofstream &stream)
{
    WriteValue<float>(stream, m_radius);
    WriteValue<float>(stream, m_height);
}

void Rectangle::DeserializeShapeData(ifstream &stream)
{
    m_width = ReadValue<float>(stream);
    m_depth = ReadValue<float>(stream);
}

void Rectangle::SerializeShapeData(ofstream &stream)
{
    WriteValue<float>(stream, m_width);
    WriteValue<float>(stream, m_depth);
}

void Box::DeserializeShapeData(ifstream &stream)
{
    m_width = ReadValue<float>(stream);
    m_depth = ReadValue<float>(stream);
    m_height = ReadValue<float>(stream);
}

void Box::SerializeShapeData(ofstream &stream)
{
    WriteValue<float>(stream, m_width);
    WriteValue<float>(stream, m_depth);
    WriteValue<float>(stream, m_height);
}
