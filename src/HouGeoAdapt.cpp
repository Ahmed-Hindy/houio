#include <houio/HouGeoIO.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace houio
{
namespace
{
std::vector<size_t> houdiniVertexOrder(const Geometry& geometry)
{
    const std::span<const Geometry::Index> indices = geometry.indexBuffer();
    std::vector<size_t> order;
    order.reserve(indices.size());
    size_t primitive_offset = 0;
    for (unsigned int primitive_index = 0; primitive_index < geometry.primitiveCount();
         ++primitive_index)
    {
        const size_t vertex_count = geometry.primitiveVertexCount(primitive_index);
        if (vertex_count == 0 || vertex_count > indices.size() - primitive_offset)
            throw std::runtime_error("Geometry primitive boundaries do not match topology");
        for (size_t local_index = 0; local_index < vertex_count; ++local_index)
            order.push_back(primitive_offset + vertex_count - local_index - 1U);
        primitive_offset += vertex_count;
    }
    if (primitive_offset != indices.size())
        throw std::runtime_error("Geometry topology contains trailing vertices");
    return order;
}

Attribute::Ptr reorderAttribute(const Attribute& source, std::span<const size_t> source_order)
{
    if (source.numElements() < 0 ||
        static_cast<size_t>(source.numElements()) != source_order.size())
    {
        throw std::runtime_error("Vertex attribute count does not match topology order");
    }
    Attribute::Ptr reordered =
        std::make_shared<Attribute>(source.numComponents(), source.elementComponentType());
    reordered->resize(source_order.size());
    for (size_t destination_index = 0; destination_index < source_order.size(); ++destination_index)
    {
        const std::span<const std::byte> source_value =
            source.elementBytes(source_order[destination_index]);
        std::span<std::byte> destination_value = reordered->mutableElementBytes(destination_index);
        std::copy(source_value.begin(), source_value.end(), destination_value.begin());
    }
    return reordered;
}
} // namespace

HouGeo::Ptr HouGeoIO::adaptVolume(ScalarField::Ptr volume)
{
    if (!volume)
        return HouGeo::Ptr();
    HouGeo::Ptr houGeo = std::make_shared<HouGeo>();
    houGeo->addPrimitive(volume);
    return houGeo;
}

HouGeo::Ptr HouGeoIO::adaptGeometry(Geometry::Ptr geometry)
{
    if (!geometry)
        return HouGeo::Ptr();
    HouGeo::Ptr houdiniGeometry = std::make_shared<HouGeo>();

    const std::vector<std::string> pointAttributeNames = geometry->pointAttributeNames();
    for (const std::string& name : pointAttributeNames)
    {
        Attribute::Ptr sourceAttribute = geometry->pointAttribute(name);

        // Houdini stores P as a four-component point position in this representation.
        if (name == "P" && sourceAttribute->numComponents() == 3)
        {
            const int elementCount = sourceAttribute->numElements();
            Attribute::Ptr promotedAttribute = Attribute::createV4f(elementCount);
            for (int elementIndex = 0; elementIndex < elementCount; ++elementIndex)
            {
                const math::V3f position = sourceAttribute->get<math::V3f>(elementIndex);
                promotedAttribute->set<math::V4f>(
                    static_cast<unsigned int>(elementIndex),
                    math::V4f(position.x, position.y, position.z, 1.0f));
            }
            sourceAttribute = promotedAttribute;
        }

        HouGeo::HouAttribute::Ptr houdiniAttribute =
            std::make_shared<HouGeo::HouAttribute>(name, sourceAttribute);
        houdiniGeometry->setPointAttribute(houdiniAttribute);
    }

    const std::vector<std::string> vertexAttributeNames = geometry->vertexAttributeNames();
    const std::span<const Geometry::Index> geometry_indices = geometry->indexBuffer();
    const std::vector<size_t> vertex_order = houdiniVertexOrder(*geometry);
    for (const std::string& name : geometry->globalAttributeNames())
    {
        Attribute::Ptr sourceAttribute = geometry->globalAttribute(name);
        if (!sourceAttribute || sourceAttribute->numElements() != 1)
            throw std::runtime_error(
                "HouGeoIO::adaptGeometry global attributes require one element");
        houdiniGeometry->setGlobalAttribute(
            std::make_shared<HouGeo::HouAttribute>(name, sourceAttribute));
    }

    if (geometry->primitiveCount() > 0)
    {
        HouGeo::HouTopology::Ptr topology = std::make_shared<HouGeo::HouTopology>();
        topology->reserve(geometry_indices.size());
        for (const size_t source_vertex_index : vertex_order)
        {
            if (source_vertex_index >= geometry_indices.size())
                throw std::out_of_range("HouGeoIO::adaptGeometry vertex order is out of range");
            const unsigned int point_index = geometry_indices[source_vertex_index];
            if (point_index > static_cast<unsigned int>(std::numeric_limits<int>::max()))
                throw std::overflow_error("HouGeoIO::adaptGeometry point index exceeds int range");
            topology->appendIndex(static_cast<int>(point_index));
        }
        houdiniGeometry->setTopology(topology);

        HouGeo::HouPoly::Ptr polygon_run = std::make_shared<HouGeo::HouPoly>();
        if (geometry->primitiveCount() > static_cast<unsigned int>(std::numeric_limits<int>::max()))
        {
            throw std::overflow_error("HouGeoIO::adaptGeometry primitive count exceeds int range");
        }
        const int primitive_count = static_cast<int>(geometry->primitiveCount());
        std::vector<int> vertex_counts;
        vertex_counts.reserve(static_cast<size_t>(primitive_count));
        if (geometry->primitiveType() == Geometry::PrimitiveType::polygon)
        {
            const std::span<const unsigned int> polygon_counts = geometry->primitiveVertexCounts();
            if (polygon_counts.size() != static_cast<size_t>(primitive_count))
            {
                throw std::runtime_error(
                    "HouGeoIO::adaptGeometry polygon counts do not match primitive count");
            }
            for (const unsigned int vertex_count : polygon_counts)
            {
                if (vertex_count == 0 ||
                    vertex_count > static_cast<unsigned int>(std::numeric_limits<int>::max()))
                {
                    throw std::runtime_error(
                        "HouGeoIO::adaptGeometry polygon vertex count is invalid");
                }
                vertex_counts.push_back(static_cast<int>(vertex_count));
            }
        }
        else
        {
            const unsigned int vertices_per_primitive = geometry->verticesPerPrimitive();
            if (vertices_per_primitive == 0 ||
                vertices_per_primitive > static_cast<unsigned int>(std::numeric_limits<int>::max()))
            {
                throw std::runtime_error(
                    "HouGeoIO::adaptGeometry fixed primitive vertex count is invalid");
            }
            vertex_counts.assign(static_cast<size_t>(primitive_count),
                                 static_cast<int>(vertices_per_primitive));
        }

        std::vector<int> vertex_offsets;
        vertex_offsets.reserve(static_cast<size_t>(primitive_count));
        size_t observed_vertex_count = 0;
        for (const int vertex_count : vertex_counts)
        {
            if (observed_vertex_count > static_cast<size_t>(std::numeric_limits<int>::max()))
            {
                throw std::overflow_error(
                    "HouGeoIO::adaptGeometry polygon offset exceeds int range");
            }
            if (static_cast<size_t>(vertex_count) > geometry_indices.size() - observed_vertex_count)
            {
                throw std::runtime_error(
                    "HouGeoIO::adaptGeometry topology does not match primitive counts");
            }
            vertex_offsets.push_back(static_cast<int>(observed_vertex_count));
            observed_vertex_count += static_cast<size_t>(vertex_count);
        }
        if (observed_vertex_count != geometry_indices.size())
            throw std::runtime_error("HouGeoIO::adaptGeometry topology contains trailing vertices");

        if (geometry_indices.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
            throw std::overflow_error("HouGeoIO::adaptGeometry index buffer exceeds int range");
        std::vector<int> point_indices(geometry_indices.size());
        for (size_t vertex_index = 0; vertex_index < geometry_indices.size(); ++vertex_index)
            point_indices[vertex_index] = static_cast<int>(vertex_index);

        polygon_run->setPolygonData(primitive_count, std::move(vertex_counts),
                                    std::move(vertex_offsets), std::move(point_indices),
                                    geometry->primitiveType() != Geometry::LINE);
        houdiniGeometry->addPrimitive(polygon_run);
    }

    for (const std::string& name : vertexAttributeNames)
    {
        Attribute::Ptr sourceAttribute = geometry->vertexAttribute(name);
        if (!sourceAttribute)
            throw std::runtime_error("HouGeoIO::adaptGeometry encountered a null vertex attribute");
        if (sourceAttribute->numElements() < 0 ||
            static_cast<size_t>(sourceAttribute->numElements()) != geometry_indices.size())
        {
            throw std::runtime_error(
                "HouGeoIO::adaptGeometry vertex attribute count does not match topology");
        }
        houdiniGeometry->setVertexAttribute(std::make_shared<HouGeo::HouAttribute>(
            name, reorderAttribute(*sourceAttribute, vertex_order)));
    }

    for (const std::string& name : geometry->primitiveAttributeNames())
    {
        Attribute::Ptr sourceAttribute = geometry->primitiveAttribute(name);
        if (!sourceAttribute || sourceAttribute->numElements() < 0 ||
            static_cast<unsigned int>(sourceAttribute->numElements()) != geometry->primitiveCount())
        {
            throw std::runtime_error(
                "HouGeoIO::adaptGeometry primitive attribute count does not match topology");
        }
        houdiniGeometry->setPrimitiveAttribute(
            name, std::make_shared<HouGeo::HouAttribute>(name, sourceAttribute));
    }

    return houdiniGeometry;
}
} // namespace houio
