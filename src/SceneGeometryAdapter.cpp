#include <houio/SceneGeometryAdapter.h>

#include <houio/HalfFloat.h>
#include <houio/types.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace houio
{
    namespace
    {
        [[nodiscard]] std::size_t checkedSize(sint64 value, const char* description)
        {
            if (value < 0)
                throw std::runtime_error(std::string(description) + " cannot be negative");
            return static_cast<std::size_t>(value);
        }

        [[nodiscard]] std::int32_t checkedIndex(int value, std::size_t point_count)
        {
            if (value < 0 || static_cast<std::size_t>(value) >= point_count)
                throw std::out_of_range("Scene topology references a point outside the detail");
            return static_cast<std::int32_t>(value);
        }

        void validateAttributes(const HouGeoAdapter& geometry)
        {
            const std::vector<std::string> point_attributes = geometry.pointAttributeNames();
            if (point_attributes.size() != 1U || point_attributes.front() != "P")
            {
                throw std::runtime_error(
                    "Scene export currently supports only the public point attribute P");
            }
            if (!geometry.vertexAttributeNames().empty())
                throw std::runtime_error("Scene export does not yet support vertex attributes");
            if (!geometry.primitiveAttributeNames().empty())
                throw std::runtime_error("Scene export does not yet support primitive attributes");
            if (!geometry.globalAttributeNames().empty())
                throw std::runtime_error("Scene export does not yet support detail attributes");
            if (!geometry.pointGroupNames().empty()
                || !geometry.vertexGroupNames().empty()
                || !geometry.primitiveGroupNames().empty())
            {
                throw std::runtime_error("Scene export does not yet support geometry groups");
            }
        }

        void appendPositions(
            SceneGeometrySample& output,
            const HouGeoAdapter& geometry,
            std::size_t point_count)
        {
            const HouGeoAdapter::AttributeAdapter::ConstPtr positions =
                geometry.pointAttribute("P");
            if (!positions)
                throw std::runtime_error("Scene export requires point attribute P");
            if (positions->type() != HouGeoAdapter::AttributeAdapter::Type::numeric)
                throw std::runtime_error("Scene export requires numeric point attribute P");
            if (positions->tupleSize().value() < 3)
                throw std::runtime_error("Scene export requires at least three P components");
            if (positions->elementCount() < 0
                || static_cast<std::size_t>(positions->elementCount()) != point_count)
            {
                throw std::runtime_error("Scene export P count does not match point count");
            }

            const HouGeoAdapter::RawDataView raw = positions->rawData();
            if (!raw.available())
                throw std::runtime_error("Scene export requires contiguous raw P storage");

            const std::size_t tuple_size = positions->tupleSize().asSize();
            output.positionsXyzw.resize(point_count * 4U);
            for (std::size_t point_index = 0; point_index < point_count; ++point_index)
            {
                const std::size_t input_offset = point_index * tuple_size;
                const std::size_t output_offset = point_index * 4U;
                float x = 0.0F;
                float y = 0.0F;
                float z = 0.0F;
                switch (positions->storage())
                {
                case HouGeoAdapter::AttributeAdapter::Storage::float16:
                    x = halfBitsToFloat(raw.read<uword>(input_offset));
                    y = halfBitsToFloat(raw.read<uword>(input_offset + 1U));
                    z = halfBitsToFloat(raw.read<uword>(input_offset + 2U));
                    break;
                case HouGeoAdapter::AttributeAdapter::Storage::float32:
                    x = raw.read<real32>(input_offset);
                    y = raw.read<real32>(input_offset + 1U);
                    z = raw.read<real32>(input_offset + 2U);
                    break;
                case HouGeoAdapter::AttributeAdapter::Storage::float64:
                    x = static_cast<float>(raw.read<real64>(input_offset));
                    y = static_cast<float>(raw.read<real64>(input_offset + 1U));
                    z = static_cast<float>(raw.read<real64>(input_offset + 2U));
                    break;
                default:
                    throw std::runtime_error(
                        "Scene export supports Float16, Float32, or Float64 P storage");
                }
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                    throw std::runtime_error("Scene export P must contain finite values");
                output.positionsXyzw[output_offset] = x;
                output.positionsXyzw[output_offset + 1U] = y;
                output.positionsXyzw[output_offset + 2U] = z;
                output.positionsXyzw[output_offset + 3U] = 1.0F;
            }
        }
    }

    SceneGeometrySample adaptSceneGeometry(const HouGeoAdapter& geometry)
    {
        validateAttributes(geometry);
        const std::size_t point_count = checkedSize(geometry.pointCount(), "Point count");

        SceneGeometrySample output;
        appendPositions(output, geometry, point_count);

        const std::vector<HouGeoAdapter::Primitive::ConstPtr> primitives =
            geometry.primitives();
        for (const HouGeoAdapter::Primitive::ConstPtr& primitive : primitives)
        {
            if (!primitive)
                throw std::runtime_error("Scene export encountered a null primitive");
            const auto polygon =
                std::dynamic_pointer_cast<const HouGeoAdapter::PolyPrimitive>(primitive);
            if (!polygon)
            {
                if (std::dynamic_pointer_cast<const HouGeoAdapter::CurvePrimitive>(primitive))
                {
                    throw std::runtime_error(
                        "Scene export does not yet support NURBS or Bezier curves; use polygonal polylines");
                }
                throw std::runtime_error(
                    "Scene export currently supports only polygons and polygonal polylines");
            }

            const int polygon_count = polygon->polygonCount();
            if (polygon_count < 0)
                throw std::runtime_error("Scene polygon count cannot be negative");
            for (int polygon_index = 0; polygon_index < polygon_count; ++polygon_index)
            {
                const int vertex_count = polygon->polygonVertexCount(polygon_index);
                const bool closed = polygon->isClosed();
                const int minimum = closed ? 3 : 2;
                if (vertex_count < minimum)
                {
                    throw std::runtime_error(
                        closed
                            ? "Scene closed polygon requires at least three vertices"
                            : "Scene open polyline requires at least two vertices");
                }
                const std::span<const int> indices =
                    polygon->polygonVertexIndices(polygon_index);
                if (indices.size() != static_cast<std::size_t>(vertex_count))
                    throw std::runtime_error("Scene polygon vertex count is inconsistent");

                ScenePolygon output_polygon;
                output_polygon.vertexOffset = output.topology.size();
                output_polygon.vertexCount = indices.size();
                output_polygon.closed = closed;
                output.polygons.push_back(output_polygon);
                for (const int point_index : indices)
                    output.topology.push_back(checkedIndex(point_index, point_count));
            }
        }

        if (output.polygons.empty() && !output.topology.empty())
            throw std::runtime_error("Scene topology has no primitive records");
        return output;
    }

    SceneGeometrySample adaptSceneGeometry(const Geometry& geometry)
    {
        const Attribute::CPtr positions = geometry.attribute("P");
        if (!positions)
            throw std::runtime_error("Scene export requires simplified point attribute P");
        if (positions->numComponents() != 3
            || positions->elementComponentType() != Attribute::ComponentType::float32)
        {
            throw std::runtime_error(
                "Simplified scene export currently requires Float32 vec3 point attribute P");
        }
        if (positions->numElements() < 0)
            throw std::runtime_error("Simplified scene P count cannot be negative");

        SceneGeometrySample output;
        const std::size_t point_count =
            static_cast<std::size_t>(positions->numElements());
        output.positionsXyzw.reserve(point_count * 4U);
        for (std::size_t point_index = 0; point_index < point_count; ++point_index)
        {
            const math::V3f position = positions->get<math::V3f>(
                static_cast<unsigned int>(point_index));
            if (!std::isfinite(position.x)
                || !std::isfinite(position.y)
                || !std::isfinite(position.z))
            {
                throw std::runtime_error("Simplified scene P must contain finite values");
            }
            output.positionsXyzw.push_back(position.x);
            output.positionsXyzw.push_back(position.y);
            output.positionsXyzw.push_back(position.z);
            output.positionsXyzw.push_back(1.0F);
        }

        const std::span<const Geometry::Index> indices = geometry.indexBuffer();
        output.topology.reserve(indices.size());
        for (const Geometry::Index point_index : indices)
        {
            if (static_cast<std::size_t>(point_index) >= point_count)
            {
                throw std::out_of_range(
                    "Simplified scene topology references a point outside the geometry");
            }
            if (point_index > static_cast<Geometry::Index>(
                    std::numeric_limits<std::int32_t>::max()))
            {
                throw std::overflow_error(
                    "Simplified scene point index exceeds Int32 range");
            }
            output.topology.push_back(static_cast<std::int32_t>(point_index));
        }

        bool closed = true;
        std::size_t vertices_per_primitive = geometry.verticesPerPrimitive();
        switch (geometry.primitiveType())
        {
        case Geometry::PrimitiveType::line:
            closed = false;
            vertices_per_primitive = 2U;
            break;
        case Geometry::PrimitiveType::triangle:
            vertices_per_primitive = 3U;
            break;
        case Geometry::PrimitiveType::quad:
            vertices_per_primitive = 4U;
            break;
        case Geometry::PrimitiveType::polygon:
            vertices_per_primitive = indices.size();
            break;
        case Geometry::PrimitiveType::point:
            throw std::runtime_error(
                "Scene export does not yet support points-only simplified geometry");
        }

        if (vertices_per_primitive == 0U
            || indices.size() % vertices_per_primitive != 0U)
        {
            throw std::runtime_error(
                "Simplified scene topology is not divisible into primitives");
        }
        for (std::size_t offset = 0; offset < indices.size();
             offset += vertices_per_primitive)
        {
            output.polygons.push_back(ScenePolygon{
                offset,
                vertices_per_primitive,
                closed});
        }
        return output;
    }
}
