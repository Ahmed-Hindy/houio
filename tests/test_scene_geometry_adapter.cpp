#include <houio/HouGeo.h>
#include <houio/SceneGeometryAdapter.h>

#include "TestSupport.h"

#include <array>
#include <memory>
#include <vector>

namespace
{
    using houio::test::fail;

    houio::HouGeo::Ptr createMixedGeometry()
    {
        auto geometry = houio::HouGeo::create();
        auto positions = houio::Attribute::createV4f();
        positions->appendElement(houio::math::V4f(0.0F, 0.0F, 0.0F, 1.0F));
        positions->appendElement(houio::math::V4f(1.0F, 0.0F, 0.0F, 1.0F));
        positions->appendElement(houio::math::V4f(1.0F, 1.0F, 0.0F, 1.0F));
        positions->appendElement(houio::math::V4f(0.0F, 1.0F, 0.0F, 1.0F));
        positions->appendElement(houio::math::V4f(2.0F, 0.0F, 0.0F, 1.0F));
        positions->appendElement(houio::math::V4f(3.0F, 1.0F, 0.0F, 1.0F));
        geometry->setPointAttribute(
            std::make_shared<houio::HouGeo::HouAttribute>("P", positions));

        auto topology = std::make_shared<houio::HouGeo::HouTopology>();
        topology->appendIndices(std::vector<int>{0, 1, 2, 3, 4, 5});
        geometry->setTopology(topology);

        auto quad = std::make_shared<houio::HouGeo::HouPoly>();
        quad->setPolygonData(1, {4}, {0}, {0, 1, 2, 3}, true);
        geometry->addPrimitive(
            std::static_pointer_cast<houio::HouGeoAdapter::PolyPrimitive>(quad));

        auto line = std::make_shared<houio::HouGeo::HouPoly>();
        line->setPolygonData(1, {2}, {0}, {4, 5}, false);
        geometry->addPrimitive(
            std::static_pointer_cast<houio::HouGeoAdapter::PolyPrimitive>(line));
        return geometry;
    }

    int verifyHouGeoAdapter()
    {
        const houio::SceneGeometrySample sample =
            houio::adaptSceneGeometry(*createMixedGeometry());
        if (sample.positionsXyzw.size() != 24U)
            return fail("Scene adapter changed the point-position count");
        if (sample.topology != std::vector<std::int32_t>{0, 1, 2, 3, 4, 5})
            return fail("Scene adapter changed polygon topology");
        if (sample.polygons.size() != 2U)
            return fail("Scene adapter changed primitive count");
        if (sample.polygons[0].vertexOffset != 0U
            || sample.polygons[0].vertexCount != 4U
            || !sample.polygons[0].closed)
        {
            return fail("Scene adapter changed the closed quad");
        }
        if (sample.polygons[1].vertexOffset != 4U
            || sample.polygons[1].vertexCount != 2U
            || sample.polygons[1].closed)
        {
            return fail("Scene adapter changed the open polyline");
        }
        return 0;
    }

    int verifySimplifiedGeometry()
    {
        const houio::Geometry::Ptr line = houio::Geometry::createLineGeometry();
        line->attribute("P")->appendElement(houio::math::V3f(0.0F, 0.0F, 0.0F));
        line->attribute("P")->appendElement(houio::math::V3f(1.0F, 2.0F, 3.0F));
        line->addLine(0, 1);

        const houio::SceneGeometrySample sample = houio::adaptSceneGeometry(*line);
        if (sample.positionsXyzw.size() != 8U
            || sample.topology != std::vector<std::int32_t>{0, 1}
            || sample.polygons.size() != 1U
            || sample.polygons.front().closed)
        {
            return fail("Simplified line did not adapt to one open scene polyline");
        }

        const houio::Geometry::Ptr polygons = houio::Geometry::createPolyGeometry();
        for (unsigned int point_index = 0; point_index < 8; ++point_index)
        {
            polygons->attribute("P")->appendElement(houio::math::V3f(
                static_cast<float>(point_index),
                0.0F,
                0.0F));
        }
        const std::array<houio::Geometry::Index, 5> pentagon = {0, 1, 2, 3, 4};
        const std::array<houio::Geometry::Index, 3> triangle = {5, 6, 7};
        polygons->addPolygon(pentagon);
        polygons->addPolygon(triangle);

        const houio::SceneGeometrySample polygon_sample =
            houio::adaptSceneGeometry(*polygons);
        if (polygon_sample.topology
                != std::vector<std::int32_t>{0, 1, 2, 3, 4, 5, 6, 7}
            || polygon_sample.polygons.size() != 2U
            || polygon_sample.polygons[0].vertexOffset != 0U
            || polygon_sample.polygons[0].vertexCount != 5U
            || !polygon_sample.polygons[0].closed
            || polygon_sample.polygons[1].vertexOffset != 5U
            || polygon_sample.polygons[1].vertexCount != 3U
            || !polygon_sample.polygons[1].closed)
        {
            return fail("Simplified variable polygons lost scene primitive boundaries");
        }
        return 0;
    }

    int verifyLossyInputRejected()
    {
        const houio::HouGeo::Ptr geometry = createMixedGeometry();
        geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>(
            "Cd",
            houio::Attribute::createV3f(6)));
        try
        {
            static_cast<void>(houio::adaptSceneGeometry(*geometry));
        }
        catch (const std::exception& exception)
        {
            if (std::string(exception.what()).find("only the public point attribute P")
                != std::string::npos)
            {
                return 0;
            }
            return fail("Scene adapter returned an unexpected lossy-input diagnostic");
        }
        return fail("Scene adapter silently discarded an unsupported point attribute");
    }
}

int main()
{
    if (const int result = verifyHouGeoAdapter(); result != 0)
        return result;
    if (const int result = verifySimplifiedGeometry(); result != 0)
        return result;
    return verifyLossyInputRejected();
}
