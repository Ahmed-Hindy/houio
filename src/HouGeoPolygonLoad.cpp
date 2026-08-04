#include <houio/HouGeo.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace houio
{
    namespace
    {
        int checkedArrayCount(
            const json::ArrayPtr& array,
            const std::string& description)
        {
            if( !array )
                throw std::runtime_error(description + " must be an array");
            const sint64 count = array->size();
            if( count < 0 )
                throw std::runtime_error(description + " has a negative element count");
            if( count > static_cast<sint64>(std::numeric_limits<int>::max()) )
                throw std::length_error(description + " exceeds supported indexing");
            return static_cast<int>(count);
        }
    }

    void HouGeo::loadPolyPrimitive(json::ObjectPtr polygonObject)
    {
        if( !polygonObject )
            throw std::invalid_argument(
                "HouGeo::loadPolyPrimitive received invalid polygon data");
        if( !m_topology )
            throw std::runtime_error(
                "HouGeo::loadPolyPrimitive expects topology to be loaded already");
        if( !polygonObject->contains("vertex") )
            throw std::runtime_error(
                "HouGeo::loadPolyPrimitive is missing the vertex array");

        json::ArrayPtr topologyIndices = polygonObject->array("vertex");
        if( !topologyIndices )
            throw std::runtime_error(
                "HouGeo::loadPolyPrimitive vertex must be an array");
        const int vertexCount = checkedArrayCount(
            topologyIndices,
            "HouGeo::loadPolyPrimitive vertex array");
        if( vertexCount <= 0 )
            throw std::runtime_error(
                "HouGeo::loadPolyPrimitive polygon must contain vertices");

        HouPoly::Ptr polygonPrimitive = std::make_shared<HouPoly>();
        polygonPrimitive->m_closed = polygonObject->get<bool>("closed", true);
        polygonPrimitive->m_numPolys = 1;
        polygonPrimitive->m_perPolyVertexCount.push_back(vertexCount);
        polygonPrimitive->m_perPolyVertexListOffset.push_back(0);
        for( int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex )
        {
            const int topologyIndex =
                topologyIndices->get<sint32>(vertexIndex);
            if( topologyIndex < 0
                || static_cast<std::size_t>(topologyIndex)
                    >= m_topology->indexBuffer.size() )
            {
                throw std::runtime_error(
                    "HouGeo::loadPolyPrimitive topology index out of range");
            }
            polygonPrimitive->m_vertices.push_back(
                m_topology->indexBuffer[static_cast<std::size_t>(topologyIndex)]);
        }

        m_primitives.push_back(polygonPrimitive);
    }

    void HouGeo::loadPolyPrimitiveRun(
        json::ObjectPtr definition,
        json::ArrayPtr runEntries)
    {
        if( !m_topology )
            throw std::runtime_error(
                "HouGeo::loadPolyPrimitiveRun expects topology to be loaded already");
        if( !definition || !runEntries )
            throw std::runtime_error(
                "HouGeo::loadPolyPrimitiveRun received invalid run data");

        HouPoly::Ptr polygonRun = std::make_shared<HouPoly>();
        polygonRun->m_numPolys = checkedArrayCount(
            runEntries,
            "HouGeo::loadPolyPrimitiveRun entries");
        if( polygonRun->m_numPolys <= 0 )
            throw std::runtime_error(
                "HouGeo::loadPolyPrimitiveRun requires at least one entry");
        polygonRun->m_closed = true;
        std::size_t vertexOffset = 0;
        for( int primitiveIndex = 0;
            primitiveIndex < polygonRun->m_numPolys;
            ++primitiveIndex )
        {
            json::ArrayPtr runEntry = runEntries->array(primitiveIndex);
            if( !runEntry || runEntry->size() == 0 )
                throw std::runtime_error(
                    "HouGeo::loadPolyPrimitiveRun invalid polygon entry");
            json::ArrayPtr topologyIndices = runEntry->array(0);
            if( !topologyIndices )
                throw std::runtime_error(
                    "HouGeo::loadPolyPrimitiveRun missing polygon vertices");
            const int vertexCount = checkedArrayCount(
                topologyIndices,
                "HouGeo::loadPolyPrimitiveRun polygon vertices");
            if( vertexCount <= 0 )
                throw std::runtime_error(
                    "HouGeo::loadPolyPrimitiveRun polygon must contain vertices");
            if( vertexOffset
                > static_cast<std::size_t>(std::numeric_limits<int>::max()) )
            {
                throw std::overflow_error(
                    "HouGeo::loadPolyPrimitiveRun vertex offset exceeds int range");
            }
            polygonRun->m_perPolyVertexCount.push_back(vertexCount);
            polygonRun->m_perPolyVertexListOffset.push_back(
                static_cast<int>(vertexOffset));
            for( int vertexIndex = 0;
                vertexIndex < vertexCount;
                ++vertexIndex, ++vertexOffset )
            {
                const int topologyIndex =
                    topologyIndices->get<sint32>(vertexIndex);
                if( topologyIndex < 0
                    || static_cast<std::size_t>(topologyIndex)
                        >= m_topology->indexBuffer.size() )
                {
                    throw std::runtime_error(
                        "HouGeo::loadPolyPrimitiveRun topology index out of range");
                }
                polygonRun->m_vertices.push_back(
                    m_topology->indexBuffer[
                        static_cast<std::size_t>(topologyIndex)]);
            }
        }
        m_primitives.push_back(polygonRun);
    }

    void HouGeo::loadPolygonRun(json::ObjectPtr polygonRun, bool closed)
    {
        if( !polygonRun )
            throw std::invalid_argument(
                "HouGeo::loadPolygonRun received invalid polygon-run data");
        if( !m_topology )
            throw std::runtime_error(
                "HouGeo::loadPolygonRun expects topology to be loaded already");

        const std::string startVertexKey = polygonRun->contains("startvertex")
            ? "startvertex"
            : "s_v";
        const std::string primitiveCountKey = polygonRun->contains("nprimitives")
            ? "nprimitives"
            : "n_p";
        const std::string runLengthKey = polygonRun->contains("nvertices_rle")
            ? "nvertices_rle"
            : "r_v";
        const std::string vertexCountsKey = polygonRun->contains("nvertices")
            ? "nvertices"
            : "n_v";
        const bool hasRunLengthData = polygonRun->contains(runLengthKey);
        const bool hasVertexCounts = polygonRun->contains(vertexCountsKey);
        if( !polygonRun->contains(startVertexKey)
            || !polygonRun->contains(primitiveCountKey)
            || (!hasRunLengthData && !hasVertexCounts) )
        {
            throw std::runtime_error(
                "HouGeo::loadPolygonRun missing required fields");
        }

        const int startVertex = polygonRun->get<int>(startVertexKey, -1);
        const int expectedPrimitiveCount =
            polygonRun->get<int>(primitiveCountKey, -1);
        json::ArrayPtr vertexCountData = polygonRun->array(
            hasRunLengthData ? runLengthKey : vertexCountsKey);
        if( startVertex < 0 || expectedPrimitiveCount <= 0 || !vertexCountData )
            throw std::runtime_error(
                "HouGeo::loadPolygonRun invalid run metadata");
        if( static_cast<std::size_t>(startVertex)
            > m_topology->indexBuffer.size() )
        {
            throw std::runtime_error(
                "HouGeo::loadPolygonRun start vertex exceeds topology");
        }
        if( hasRunLengthData && (vertexCountData->size() % 2) != 0 )
            throw std::runtime_error(
                "HouGeo::loadPolygonRun invalid run-length data");
        if( !hasRunLengthData
            && hasVertexCounts
            && vertexCountData->size() != expectedPrimitiveCount )
        {
            throw std::runtime_error(
                "HouGeo::loadPolygonRun vertex-count array length mismatch");
        }

        HouPoly::Ptr polygonRunPrimitive = std::make_shared<HouPoly>();
        polygonRunPrimitive->m_closed =
            polygonRun->get<bool>("closed", closed);

        std::size_t topologyIndex = static_cast<std::size_t>(startVertex);
        int primitiveCount = 0;
        auto appendPolygons = [&](int verticesPerPrimitive, int repetitionCount)
        {
            if( verticesPerPrimitive <= 0 || repetitionCount <= 0 )
                throw std::runtime_error(
                    "HouGeo::loadPolygonRun invalid vertex-count data");
            if( primitiveCount > expectedPrimitiveCount
                || repetitionCount > expectedPrimitiveCount - primitiveCount )
            {
                throw std::runtime_error(
                    "HouGeo::loadPolygonRun primitive count exceeds nprimitives");
            }

            for( int repetition = 0;
                repetition < repetitionCount;
                ++repetition )
            {
                const std::size_t verticesToConsume =
                    static_cast<std::size_t>(verticesPerPrimitive);
                if( topologyIndex > m_topology->indexBuffer.size()
                    || verticesToConsume
                        > m_topology->indexBuffer.size() - topologyIndex )
                {
                    throw std::runtime_error(
                        "HouGeo::loadPolygonRun topology range exceeds index buffer");
                }
                const std::size_t nextTopologyIndex =
                    topologyIndex + verticesToConsume;
                if( polygonRunPrimitive->m_vertices.size()
                    > static_cast<std::size_t>(
                        std::numeric_limits<int>::max()) )
                {
                    throw std::overflow_error(
                        "HouGeo::loadPolygonRun vertex offset exceeds int range");
                }

                polygonRunPrimitive->m_perPolyVertexListOffset.push_back(
                    static_cast<int>(polygonRunPrimitive->m_vertices.size()));
                polygonRunPrimitive->m_perPolyVertexCount.push_back(
                    verticesPerPrimitive);
                for( ; topologyIndex < nextTopologyIndex; ++topologyIndex )
                {
                    polygonRunPrimitive->m_vertices.push_back(
                        m_topology->indexBuffer[topologyIndex]);
                }
                ++primitiveCount;
            }
        };

        if( hasRunLengthData )
        {
            for( sint64 index = 0;
                index < vertexCountData->size();
                index += 2 )
            {
                appendPolygons(
                    vertexCountData->get<int>(static_cast<int>(index)),
                    vertexCountData->get<int>(static_cast<int>(index + 1)));
            }
        }
        else
        {
            for( sint64 index = 0;
                index < vertexCountData->size();
                ++index )
            {
                appendPolygons(
                    vertexCountData->get<int>(static_cast<int>(index)),
                    1);
            }
        }

        if( primitiveCount != expectedPrimitiveCount )
            throw std::runtime_error(
                "HouGeo::loadPolygonRun primitive count mismatch");

        polygonRunPrimitive->m_numPolys = primitiveCount;
        m_primitives.push_back(polygonRunPrimitive);
    }
}
