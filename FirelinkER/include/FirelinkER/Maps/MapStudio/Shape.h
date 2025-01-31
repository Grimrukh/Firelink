#pragma once

#include <fstream>
#include <map>
#include <memory>

#include "FirelinkER/Export.h"


namespace FirelinkER::Maps::MapStudio
{
    class Region;

    /// @brief Serialized enumeration for `Shape` subtypes.
    enum class ShapeType : uint32_t
    {
        NoShape = 0xFFFFFFFF,
        PointShape = 0,
        CircleShape = 1,
        SphereShape = 2,
        CylinderShape = 3,
        RectangleShape = 4,
        BoxShape = 5,
        CompositeShape = 6,
    };

    /// @brief Shape attached to an MSB `Region` entry.
    class FIRELINKER_API Shape
    {
    public:
        [[nodiscard]] static const std::map<ShapeType, std::string>& GetTypeNames()
        {
            static const std::map<ShapeType, std::string> data =
            {
                {ShapeType::PointShape, "Point"},
                {ShapeType::CircleShape, "Circle"},
                {ShapeType::SphereShape, "Sphere"},
                {ShapeType::CylinderShape, "Cylinder"},
                {ShapeType::RectangleShape, "Rectangle"},
                {ShapeType::BoxShape, "Box"},
                {ShapeType::CompositeShape, "Composite"},
            };
            return data;
        }

        static const inline std::map<ShapeType, std::string> TypeNames =
        {

        };

        virtual ~Shape() = default;

        [[nodiscard]] virtual ShapeType GetType() const = 0;
        [[nodiscard]] virtual bool HasShapeData() const = 0;

        [[nodiscard]] virtual std::unique_ptr<Shape> Clone() const = 0;

        virtual void DeserializeShapeData(std::ifstream& stream) = 0;
        virtual void SerializeShapeData(std::ofstream& stream) = 0;

        [[nodiscard]] std::string GetShapeTypeName() const { return TypeNames.at(GetType()); }
    };

    class FIRELINKER_API Point final : public Shape
    {
    public:
        static constexpr auto Type = ShapeType::PointShape;

        // No shape data.
        Point() = default;

        [[nodiscard]] ShapeType GetType() const override { return Type; }
        [[nodiscard]] bool HasShapeData() const override { return false; }

        [[nodiscard]] std::unique_ptr<Shape> Clone() const override { return std::make_unique<Point>(*this); }

        void DeserializeShapeData(std::ifstream &stream) override;
        void SerializeShapeData(std::ofstream &stream) override;
    };

    class FIRELINKER_API Circle final : public Shape
    {
    public:
        static constexpr auto Type = ShapeType::CircleShape;

        explicit Circle(const float radius) : m_radius(radius) {}
        Circle() : m_radius(1.0f) {}

        [[nodiscard]] ShapeType GetType() const override { return Type; }
        [[nodiscard]] bool HasShapeData() const override { return true; }

        [[nodiscard]] std::unique_ptr<Shape> Clone() const override { return std::make_unique<Circle>(*this); }

        [[nodiscard]] float GetRadius() const { return m_radius; }
        void SetRadius(const float radius) { m_radius = radius; }

        void DeserializeShapeData(std::ifstream &stream) override;
        void SerializeShapeData(std::ofstream &stream) override;

    protected:
        float m_radius;
    };

    class FIRELINKER_API Sphere final : public Shape
    {
    public:
        static constexpr auto Type = ShapeType::SphereShape;

        explicit Sphere(const float radius) : m_radius(radius) {}
        Sphere() : m_radius(1.0f) {}

        [[nodiscard]] ShapeType GetType() const override { return Type; }
        [[nodiscard]] bool HasShapeData() const override { return true; }

        [[nodiscard]] std::unique_ptr<Shape> Clone() const override { return std::make_unique<Sphere>(*this); }

        [[nodiscard]] float GetRadius() const { return m_radius; }
        void SetRadius(const float radius) { m_radius = radius; }

        void DeserializeShapeData(std::ifstream &stream) override;
        void SerializeShapeData(std::ofstream &stream) override;

    protected:
        float m_radius;
    };

    class FIRELINKER_API Cylinder final : public Shape
    {
    public:
        static constexpr auto Type = ShapeType::CylinderShape;

        Cylinder(const float radius, const float height) : m_radius(radius), m_height(height) {}
        Cylinder() : m_radius(1.0f), m_height(1.0f) {}

        [[nodiscard]] ShapeType GetType() const override { return Type; }
        [[nodiscard]] bool HasShapeData() const override { return true; }

        [[nodiscard]] std::unique_ptr<Shape> Clone() const override { return std::make_unique<Cylinder>(*this); }
        
        [[nodiscard]] float GetRadius() const { return m_radius; }
        void SetRadius(const float radius) { m_radius = radius; }

        [[nodiscard]] float GetHeight() const { return m_height; }
        void SetHeight(const float height) { m_height = height; }

        void DeserializeShapeData(std::ifstream &stream) override;
        void SerializeShapeData(std::ofstream &stream) override;

    protected:
        float m_radius;
        float m_height;
    };

    class FIRELINKER_API Rectangle final : public Shape
    {
    public:
        static constexpr auto Type = ShapeType::RectangleShape;

        Rectangle(const float width, const float depth) : m_width(width), m_depth(depth) {}
        Rectangle() : m_width(1.0f), m_depth(1.0f) {}

        [[nodiscard]] ShapeType GetType() const override { return Type; }
        [[nodiscard]] bool HasShapeData() const override { return true; }

        [[nodiscard]] std::unique_ptr<Shape> Clone() const override { return std::make_unique<Rectangle>(*this); }

        [[nodiscard]] float GetWidth() const { return m_width; }
        void SetWidth(const float width) { m_width = width; }

        [[nodiscard]] float GetDepth() const { return m_depth; }
        void SetDepth(const float depth) { m_depth = depth; }

        void DeserializeShapeData(std::ifstream &stream) override;
        void SerializeShapeData(std::ofstream &stream) override;

    protected:
        float m_width;  // X axis
        float m_depth;  // Z axis
    };

    class FIRELINKER_API Box final : public Shape
    {
    public:
        static constexpr auto Type = ShapeType::BoxShape;

        Box(const float width, const float depth, const float height) : m_width(width), m_depth(depth), m_height(height) {}
        Box() : m_width(1.0f), m_depth(1.0f), m_height(1.0f) {}

        [[nodiscard]] ShapeType GetType() const override { return Type; }
        [[nodiscard]] bool HasShapeData() const override { return true; }

        [[nodiscard]] std::unique_ptr<Shape> Clone() const override { return std::make_unique<Box>(*this); }

        [[nodiscard]] float GetWidth() const { return m_width; }
        void SetWidth(const float width) { m_width = width; }

        [[nodiscard]] float GetDepth() const { return m_depth; }
        void SetDepth(const float depth) { m_depth = depth; }

        [[nodiscard]] float GetHeight() const { return m_height; }
        void SetHeight(const float height) { m_height = height; }

        void DeserializeShapeData(std::ifstream &stream) override;
        void SerializeShapeData(std::ofstream &stream) override;

    protected:
        float m_width;  // X axis
        float m_depth;  // Z axis
        float m_height; // Y axis
    };

    /// @brief Region composed of up to 8 other referenced 'child' Regions.
    /// Child references are stored in `Region` entries to make use of safe entry referencing system. This class stores
    /// nothing; the sixteen integers (child region indices and unknown values) de/serialized are handled as a special
    /// case by the caller.
    class FIRELINKER_API Composite final : public Shape
    {
    public:
        static constexpr auto Type = ShapeType::CompositeShape;

        /// @brief Composite constructor. (No data.)
        Composite() = default;

        [[nodiscard]] ShapeType GetType() const override { return Type; }
        [[nodiscard]] bool HasShapeData() const override { return true; }

        [[nodiscard]] std::unique_ptr<Shape> Clone() const override { return std::make_unique<Composite>(*this); }

        // Dummy implementations. Handled by caller as special case so child references can be assigned to `Region`.
        void DeserializeShapeData(std::ifstream &stream) override {}
        void SerializeShapeData(std::ofstream &stream) override {}
    };
}
