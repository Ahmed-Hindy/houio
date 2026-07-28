#include "HoudiniGeometryAdapter.h"

#include <GA/GA_AttributeDict.h>
#include <GA/GA_Iterator.h>
#include <GA/GA_Primitive.h>
#include <GA/GA_PrimitiveTypes.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_Detail.h>
#include <UT/UT_Interrupt.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace houio::hdk
{
    namespace
    {
        template<typename Value>
        int checkedInt(Value value, const char* description)
        {
            if (value < 0
                || static_cast<unsigned long long>(value)
                    > static_cast<unsigned long long>(std::numeric_limits<int>::max()))
            {
                throw std::overflow_error(
                    std::string("Houdini ") + description + " exceeds HouIO's int range");
            }
            return static_cast<int>(value);
        }

        void checkInterrupted(UT_Interrupt* interrupt)
        {
            if (interrupt != nullptr && interrupt->opInterrupt())
                throw std::runtime_error("HouIO export was cancelled");
        }

        const char* ownerName(GA_AttributeOwner owner) noexcept
        {
            switch (owner)
            {
            case GA_ATTRIB_POINT:
                return "point";
            case GA_ATTRIB_VERTEX:
                return "vertex";
            case GA_ATTRIB_PRIMITIVE:
                return "primitive";
            case GA_ATTRIB_DETAIL:
                return "detail";
            default:
                return "unknown";
            }
        }

        void validatePublicAttributes(
            const GU_Detail& detail,
            GA_AttributeOwner owner,
            const char* allowed_name)
        {
            const GA_AttributeDict& attributes = detail.getAttributeDict(owner);
            for (GA_AttributeDict::iterator iterator = attributes.begin(GA_SCOPE_PUBLIC);
                 !iterator.atEnd();
                 ++iterator)
            {
                const char* name = iterator.name();
                if (allowed_name != nullptr && name != nullptr
                    && std::string(name) == allowed_name)
                {
                    continue;
                }
                throw std::runtime_error(
                    std::string("Native HouIO ROP does not yet support public ")
                    + ownerName(owner) + " attribute '"
                    + (name != nullptr ? name : "<unnamed>") + "'");
            }
        }

        void validateNoGroups(const GU_Detail& detail, GA_AttributeOwner owner)
        {
            const GA_ElementGroupTable& groups = detail.getElementGroupTable(owner);
            if (groups.entries() != 0)
            {
                throw std::runtime_error(
                    std::string("Native HouIO ROP does not yet support ")
                    + ownerName(owner) + " groups");
            }
        }
    }

    NativePolygonDetail adaptDetail(
        const GU_Detail& detail,
        UT_Interrupt* interrupt)
    {
        validatePublicAttributes(detail, GA_ATTRIB_POINT, "P");
        validatePublicAttributes(detail, GA_ATTRIB_VERTEX, nullptr);
        validatePublicAttributes(detail, GA_ATTRIB_PRIMITIVE, nullptr);
        validatePublicAttributes(detail, GA_ATTRIB_DETAIL, nullptr);
        validateNoGroups(detail, GA_ATTRIB_POINT);
        validateNoGroups(detail, GA_ATTRIB_VERTEX);
        validateNoGroups(detail, GA_ATTRIB_PRIMITIVE);

        const int point_count = checkedInt(detail.getNumPoints(), "point count");
        const int vertex_count = checkedInt(detail.getNumVertices(), "vertex count");
        const int primitive_count = checkedInt(
            detail.getNumPrimitives(),
            "primitive count");

        NativePolygonDetail output;
        output.positions_xyzw.resize(static_cast<std::size_t>(point_count) * 4);
        output.topology.reserve(static_cast<std::size_t>(vertex_count));
        output.polygons.reserve(static_cast<std::size_t>(primitive_count));

        std::size_t interrupt_counter = 0;
        std::size_t visited_point_count = 0;
        std::vector<std::uint8_t> visited_points(
            static_cast<std::size_t>(point_count),
            0U);
        for (GA_Iterator iterator(detail.getPointRange()); !iterator.atEnd(); ++iterator)
        {
            if ((interrupt_counter++ & 4095U) == 0U)
                checkInterrupted(interrupt);

            const int point_index = checkedInt(
                detail.pointIndex(*iterator),
                "point index");
            if (point_index >= point_count
                || visited_points[static_cast<std::size_t>(point_index)] != 0U)
            {
                throw std::runtime_error(
                    "Houdini point iteration returned an invalid or duplicate point index");
            }
            visited_points[static_cast<std::size_t>(point_index)] = 1U;
            ++visited_point_count;

            const UT_Vector3 position = detail.getPos3(*iterator);
            const std::size_t scalar_offset =
                static_cast<std::size_t>(point_index) * 4;
            output.positions_xyzw[scalar_offset] = static_cast<float>(position.x());
            output.positions_xyzw[scalar_offset + 1] = static_cast<float>(position.y());
            output.positions_xyzw[scalar_offset + 2] = static_cast<float>(position.z());
            output.positions_xyzw[scalar_offset + 3] = 1.0F;
        }
        if (visited_point_count != static_cast<std::size_t>(point_count))
        {
            throw std::runtime_error(
                "Houdini point iteration did not match its declared count");
        }

        for (GA_Iterator iterator(detail.getPrimitiveRange()); !iterator.atEnd(); ++iterator)
        {
            if ((interrupt_counter++ & 1023U) == 0U)
                checkInterrupted(interrupt);

            const GA_Offset primitive_offset = *iterator;
            if (detail.getPrimitiveTypeId(primitive_offset) != GA_PRIMPOLY)
            {
                throw std::runtime_error(
                    "Native HouIO ROP currently supports only polygon and polyline primitives");
            }

            const GA_Primitive* primitive = detail.getPrimitive(primitive_offset);
            if (primitive == nullptr)
                throw std::runtime_error("Houdini returned a null polygon primitive");
            const GEO_PrimPoly* polygon = static_cast<const GEO_PrimPoly*>(primitive);
            const int polygon_vertex_count = checkedInt(
                polygon->getVertexCount(),
                "polygon vertex count");
            if (polygon_vertex_count <= 0)
                throw std::runtime_error("Houdini polygon contains no vertices");

            HouIONativePolygon output_polygon;
            output_polygon.vertex_offset = output.topology.size();
            output_polygon.vertex_count =
                static_cast<std::size_t>(polygon_vertex_count);
            output_polygon.closed = polygon->isClosed() ? 1U : 0U;
            output.polygons.push_back(output_polygon);

            for (int vertex_index = 0;
                 vertex_index < polygon_vertex_count;
                 ++vertex_index)
            {
                const int point_index = checkedInt(
                    polygon->getPointIndex(static_cast<GA_Size>(vertex_index)),
                    "polygon point index");
                if (point_index >= point_count)
                {
                    throw std::runtime_error(
                        "Houdini polygon references a point outside the detail");
                }
                output.topology.push_back(static_cast<std::int32_t>(point_index));
            }
        }

        if (output.topology.size() != static_cast<std::size_t>(vertex_count))
        {
            throw std::runtime_error(
                "Houdini topology iteration did not match its declared count");
        }
        if (output.polygons.size() != static_cast<std::size_t>(primitive_count))
        {
            throw std::runtime_error(
                "Houdini primitive iteration did not match its declared count");
        }
        checkInterrupted(interrupt);
        return output;
    }
}
