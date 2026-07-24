#pragma once

#include <map>
#include <memory>
#include <span>
#include <string>
#include <utility>
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

        [[nodiscard]] math::BoundingBox3f bounds() const;

        [[nodiscard]] Attribute::Ptr attribute(const std::string& name);
        [[nodiscard]] Attribute::CPtr attribute(const std::string& name) const;
        void setAttribute(const std::string& name, Attribute::Ptr attribute);
        [[nodiscard]] bool hasAttribute(const std::string& name) const;
        [[nodiscard]] std::vector<std::string> attributeNames() const;
        void removeAttribute(const std::string& name);

        [[nodiscard]] const AttributeMap& attributes() const noexcept
        {
            return attributes_;
        }

        [[nodiscard]] PrimitiveType primitiveType() const noexcept
        {
            return primitive_type_;
        }

        [[nodiscard]] unsigned int primitiveCount() const noexcept
        {
            return primitive_count_;
        }

        [[nodiscard]] unsigned int verticesPerPrimitive() const noexcept
        {
            return vertices_per_primitive_;
        }

        [[nodiscard]] std::span<const Index> indexBuffer() const noexcept
        {
            return indices_;
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
            PrimitiveType primitive_type = PrimitiveType::point);
        [[nodiscard]] static Ptr createSphere(
            int u_subdivisions,
            int v_subdivisions,
            float radius,
            math::Vec3f center = math::V3f(0.0f),
            PrimitiveType primitive_type = PrimitiveType::triangle);
        [[nodiscard]] static Ptr createBox(
            const math::BoundingBox3f& bound,
            PrimitiveType primitive_type = PrimitiveType::triangle);
        [[nodiscard]] static Ptr createLine(
            const math::V3f& point0,
            const math::V3f& point1);
        [[nodiscard]] static Ptr merge(const std::vector<Ptr>& geometries);

    private:
        [[nodiscard]] static unsigned int fixedVertexCount(PrimitiveType primitive_type);
        void appendFixedPrimitive(std::span<const Index> point_indices);

        AttributeMap attributes_;
        PrimitiveType primitive_type_;
        std::vector<Index> indices_;
        unsigned int primitive_count_ = 0;
        unsigned int vertices_per_primitive_ = 0;
    };
}
