#pragma once

#include <initializer_list>
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

        /// Compatibility alias for pointAttribute().
        [[nodiscard]] Attribute::Ptr attribute(const std::string& name);
        /// Compatibility alias for pointAttribute().
        [[nodiscard]] Attribute::CPtr attribute(const std::string& name) const;
        /// Compatibility alias for setPointAttribute().
        void setAttribute(const std::string& name, Attribute::Ptr attribute);
        /// Compatibility alias for hasPointAttribute().
        [[nodiscard]] bool hasAttribute(const std::string& name) const;
        /// Compatibility alias for pointAttributeNames().
        [[nodiscard]] std::vector<std::string> attributeNames() const;
        /// Compatibility alias for removePointAttribute().
        void removeAttribute(const std::string& name);

        [[nodiscard]] Attribute::Ptr pointAttribute(const std::string& name);
        [[nodiscard]] Attribute::CPtr pointAttribute(const std::string& name) const;
        void setPointAttribute(const std::string& name, Attribute::Ptr attribute);
        [[nodiscard]] bool hasPointAttribute(const std::string& name) const;
        [[nodiscard]] std::vector<std::string> pointAttributeNames() const;
        void removePointAttribute(const std::string& name);

        [[nodiscard]] Attribute::Ptr vertexAttribute(const std::string& name);
        [[nodiscard]] Attribute::CPtr vertexAttribute(const std::string& name) const;
        void setVertexAttribute(const std::string& name, Attribute::Ptr attribute);
        [[nodiscard]] bool hasVertexAttribute(const std::string& name) const;
        [[nodiscard]] std::vector<std::string> vertexAttributeNames() const;
        void removeVertexAttribute(const std::string& name);

        [[nodiscard]] Attribute::Ptr primitiveAttribute(const std::string& name);
        [[nodiscard]] Attribute::CPtr primitiveAttribute(const std::string& name) const;
        void setPrimitiveAttribute(const std::string& name, Attribute::Ptr attribute);
        [[nodiscard]] bool hasPrimitiveAttribute(const std::string& name) const;
        [[nodiscard]] std::vector<std::string> primitiveAttributeNames() const;
        void removePrimitiveAttribute(const std::string& name);

        [[nodiscard]] Attribute::Ptr globalAttribute(const std::string& name);
        [[nodiscard]] Attribute::CPtr globalAttribute(const std::string& name) const;
        void setGlobalAttribute(const std::string& name, Attribute::Ptr attribute);
        [[nodiscard]] bool hasGlobalAttribute(const std::string& name) const;
        [[nodiscard]] std::vector<std::string> globalAttributeNames() const;
        void removeGlobalAttribute(const std::string& name);

        /// Compatibility view of point attributes.
        [[nodiscard]] const AttributeMap& attributes() const noexcept
        {
            return point_attributes_;
        }

        [[nodiscard]] const AttributeMap& pointAttributes() const noexcept
        {
            return point_attributes_;
        }

        [[nodiscard]] const AttributeMap& vertexAttributes() const noexcept
        {
            return vertex_attributes_;
        }

        [[nodiscard]] const AttributeMap& primitiveAttributes() const noexcept
        {
            return primitive_attributes_;
        }

        [[nodiscard]] const AttributeMap& globalAttributes() const noexcept
        {
            return global_attributes_;
        }

        [[nodiscard]] PrimitiveType primitiveType() const noexcept
        {
            return primitive_type_;
        }

        [[nodiscard]] unsigned int primitiveCount() const noexcept
        {
            return primitive_count_;
        }

        /// Return the fixed/common vertex count, or zero for variable-size polygons.
        [[nodiscard]] unsigned int verticesPerPrimitive() const noexcept
        {
            return vertices_per_primitive_;
        }

        /// Return the exact vertex count for one primitive.
        [[nodiscard]] unsigned int primitiveVertexCount(
            unsigned int primitive_index) const;

        /// Exact polygon boundaries. Fixed-size primitive types return an empty span.
        [[nodiscard]] std::span<const unsigned int> primitiveVertexCounts() const noexcept
        {
            return primitive_vertex_counts_;
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
        /// Append one complete polygon and return its primitive index.
        unsigned int addPolygon(std::span<const Index> point_indices);
        /// Compatibility builder that appends a vertex to the last polygon.
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
        [[nodiscard]] static Ptr merge(std::span<const Ptr> geometries);
        [[nodiscard]] static Ptr merge(std::span<const ConstPtr> geometries);
        [[nodiscard]] static Ptr merge(std::initializer_list<Ptr> geometries);

    private:
        template<typename Pointer>
        [[nodiscard]] static Ptr mergeRange(std::span<const Pointer> geometries);
        [[nodiscard]] static unsigned int fixedVertexCount(PrimitiveType primitive_type);
        [[nodiscard]] unsigned int commonPolygonVertexCount() const noexcept;
        void appendFixedPrimitive(std::span<const Index> point_indices);

        AttributeMap point_attributes_;
        AttributeMap vertex_attributes_;
        AttributeMap primitive_attributes_;
        AttributeMap global_attributes_;
        PrimitiveType primitive_type_;
        std::vector<Index> indices_;
        std::vector<unsigned int> primitive_vertex_counts_;
        unsigned int primitive_count_ = 0;
        unsigned int vertices_per_primitive_ = 0;
    };
}
