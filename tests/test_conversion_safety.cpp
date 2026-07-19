#include <houio/HouGeoIO.h>

#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

houio::HouGeo::Ptr importPointOnlyGeometry(int pointCount)
{
    std::ostringstream document;
    document << "[\"pointcount\"," << pointCount
             << ",\"vertexcount\",0,\"primitivecount\",0]";
    std::istringstream input(document.str());
    return houio::HouGeoIO::import(&input);
}

int verifyDeclaredPointCount()
{
    houio::HouGeo::Ptr geometry = importPointOnlyGeometry(3);
    if (!geometry || geometry->pointcount() != 3)
    {
        return fail("declared pointcount was lost when no point attributes existed");
    }

    houio::DiagnosticList diagnostics;
    houio::Geometry::Ptr converted = houio::HouGeoIO::convertToGeometry(
        geometry, houio::HouGeoAdapter::Primitive::Ptr(), &diagnostics);
    if (converted || diagnostics.size() != 1
        || diagnostics.front().category != houio::DiagnosticCategory::schema
        || diagnostics.front().path != "attributes.pointattributes.P")
    {
        return fail("point-only conversion without P did not produce a structured diagnostic");
    }
    return 0;
}

int verifyInvalidPolygonPointReference()
{
    houio::HouGeo::Ptr geometry = houio::HouGeo::create();
    houio::Attribute::Ptr positions = houio::Attribute::createV3f();
    positions->appendElement(houio::math::V3f(0.0f, 0.0f, 0.0f));
    positions->appendElement(houio::math::V3f(1.0f, 0.0f, 0.0f));
    geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>("P", positions));

    auto topology = std::make_shared<houio::HouGeo::HouTopology>();
    topology->indexBuffer = {0, 1, 2};
    geometry->setTopology(topology);

    auto polygon = std::make_shared<houio::HouGeo::HouPoly>();
    polygon->m_numPolys = 1;
    polygon->m_perPolyVertexCount = {3};
    polygon->m_perPolyVertexListOffset = {0};
    polygon->m_vertices = {0, 1, 2};
    geometry->addPrimitive(polygon);

    houio::DiagnosticList diagnostics;
    if (houio::HouGeoIO::convertToGeometry(geometry, polygon, &diagnostics)
        || diagnostics.size() != 1
        || diagnostics.front().category != houio::DiagnosticCategory::schema)
    {
        return fail("out-of-range polygon point reference was not rejected safely");
    }
    return 0;
}

int verifyPolygonAccessorSafety()
{
    houio::HouGeo::HouPoly polygon;
    polygon.m_numPolys = 1;
    polygon.m_perPolyVertexCount = {3};
    polygon.m_perPolyVertexListOffset = {1};
    polygon.m_vertices = {0, 1, 2};

    try
    {
        polygon.numVertices(-1);
        return fail("negative polygon index was accepted");
    }
    catch (const std::out_of_range&)
    {
    }

    try
    {
        polygon.vertices(0);
        return fail("polygon range beyond stored vertices was accepted");
    }
    catch (const std::runtime_error&)
    {
    }
    return 0;
}

int verifyNullAndCountGuards()
{
    houio::Geometry::Ptr geometry = houio::Geometry::createPointGeometry();
    try
    {
        geometry->setAttr("invalid", houio::Attribute::Ptr());
        return fail("Geometry accepted a null attribute");
    }
    catch (const std::invalid_argument&)
    {
    }

    try
    {
        geometry->duplicatePoint(0);
        return fail("Geometry duplicated a point from empty storage");
    }
    catch (const std::out_of_range&)
    {
    }

    houio::HouGeo::Ptr imported = importPointOnlyGeometry(2);
    houio::Attribute::Ptr onePosition = houio::Attribute::createV3f();
    onePosition->appendElement(houio::math::V3f(0.0f));
    try
    {
        imported->setPointAttribute(
            std::make_shared<houio::HouGeo::HouAttribute>("P", onePosition));
        return fail("HouGeo accepted a point attribute with the wrong element count");
    }
    catch (const std::invalid_argument&)
    {
    }

    try
    {
        imported->setPointAttribute(houio::HouGeo::HouAttribute::Ptr());
        return fail("HouGeo accepted a null point attribute");
    }
    catch (const std::invalid_argument&)
    {
    }

    try
    {
        imported->addPrimitive(houio::ScalarField::Ptr());
        return fail("HouGeo accepted a null field primitive");
    }
    catch (const std::invalid_argument&)
    {
    }
    return 0;
}

int verifyAttributeAndStringBounds()
{
    houio::Attribute oversized(4, houio::Attribute::FLOAT);
    try
    {
        oversized.resize(std::numeric_limits<size_t>::max());
        return fail("Attribute accepted an overflowing resize");
    }
    catch (const std::length_error&)
    {
    }

    houio::HouGeo::HouAttribute strings;
    strings.strings = {"first"};
    try
    {
        strings.getString(1);
        return fail("HouAttribute accepted an out-of-range string index");
    }
    catch (const std::out_of_range&)
    {
    }
    return 0;
}
}

int main()
{
    if (const int result = verifyDeclaredPointCount(); result != 0)
    {
        return result;
    }
    if (const int result = verifyInvalidPolygonPointReference(); result != 0)
    {
        return result;
    }
    if (const int result = verifyPolygonAccessorSafety(); result != 0)
    {
        return result;
    }
    if (const int result = verifyNullAndCountGuards(); result != 0)
    {
        return result;
    }
    return verifyAttributeAndStringBounds();
}
