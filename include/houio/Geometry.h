#pragma once

#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <houio/Attribute.h>
#include <houio/math/Math.h>

namespace houio
{
    class Geometry final
    {
    public:
        using Ptr = std::shared_ptr<Geometry>;
        using ConstPtr = std::shared_ptr<const Geometry>;
        using AttributeMap = std::map<std::string, Attribute::Ptr>;
        using Index = unsigned int;

        enum class PrimitiveType
        {
            point,
            line,
            triangle,
            quad,
            polygon,
        };

        static constexpr PrimitiveType POINT = PrimitiveType::point;
        static constexpr PrimitiveType LINE = PrimitiveType::line;
        static constexpr PrimitiveType TRIANGLE = PrimitiveType::triangle;
        static constexpr PrimitiveType QUAD = PrimitiveType::quad;
        static constexpr PrimitiveType POLYGON = PrimitiveType::polygon;

        explicit Geometry(PrimitiveType primitive_type = PrimitiveType::triangle);

        void clear();
        void reverse();
        [[nodiscard]] Index duplicatePoint(Index point_index);
        void transform(const math::M44f& transform_matrix);
        void addNormals();

        [[nodiscard]] math::BoundingBox3f getBound();

        [[nodiscard]] Attribute::Ptr getAttr(const std::string& name);
        void setAttr(const std::string& name, Attribute::Ptr attribute);
        [[nodiscard]] bool hasAttr(const std::string& name);
        void getAttrNames(std::vector<std::string>& names) const;
        void removeAttr(const std::string& name);

        [[nodiscard]] const AttributeMap& attributes() const noexcept
        {
            return m_attributes;
        }

        [[nodiscard]] PrimitiveType primitiveType();
        [[nodiscard]] unsigned int numPrimitives();
        [[nodiscard]] unsigned int numPrimitiveVertices();
        [[nodiscard]] std::span<const Index> indexBuffer() const noexcept
        {
            return m_indexBuffer;
        }

        unsigned int addPoint(Index point_index);
        unsigned int addLine(Index point_index0, Index point_index1);
        unsigned int addTriangle(
            Index point_index0,
            Index point_index1,
            Index point_index2);
        unsigned int addQuad(
            Index point_index0,
            Index point_index1,
            Index point_index2,
            Index point_index3);
        unsigned int addPolygonVertex(Index point_index);

        [[nodiscard]] static Ptr createPointGeometry();
        [[nodiscard]] static Ptr createLineGeometry();
        [[nodiscard]] static Ptr createTriangleGeometry();
        [[nodiscard]] static Ptr createQuadGeometry();
        [[nodiscard]] static Ptr createPolyGeometry();
        [[nodiscard]] static Ptr createQuad(
            PrimitiveType primitive_type = PrimitiveType::triangle);
        [[nodiscard]] static Ptr createGrid(
            int x_resolution,
            int z_resolution,
            PrimitiveType primitive_type = PrimitiveType::triangle);
        [[nodiscard]] static Ptr createGrid(
            int x_resolution,
            int y_resolution,
            int z_resolution,
            PrimitiveType primitive_type = PrimitiveType::triangle);
        [[nodiscard]] static Ptr createSphere(
            int u_subdivisions,
            int v_subdivisions,
            float radius,
            math::Vec3f center = math::V3f(0.0f),
            PrimitiveType primitive_type = PrimitiveType::triangle);
        [[nodiscard]] static Ptr createBox(
            const math::BoundingBox3f& bound,
            PrimitiveType primitive_type = PrimitiveType::triangle);
        [[nodiscard]] static Ptr createLine(const math::V3f& point0, const math::V3f& point1);
        [[nodiscard]] static Ptr merge(const std::vector<Ptr>& geometries);

    private:
        AttributeMap m_attributes;
        PrimitiveType m_primitiveType;
        std::vector<Index> m_indexBuffer;
        bool m_indexBufferIsDirty = true;
        unsigned int m_numPrimitives = 0;
        unsigned int m_numPrimitiveVertices = 0;
    };
}
