#include <houio/HouGeoIO.h>

#include <array>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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
    return houio::HouGeoIO::import(input);
}

int verifyDeclaredPointCount()
{
    houio::HouGeo::Ptr geometry = importPointOnlyGeometry(3);
    if (!geometry || geometry->pointCount() != 3)
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
    topology->setIndices({0, 1, 2});
    geometry->setTopology(topology);

    auto polygon = std::make_shared<houio::HouGeo::HouPoly>();
    polygon->setPolygonData(1, {3}, {0}, {0, 1, 2}, true);
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
    polygon.setPolygonData(1, {3}, {1}, {0, 1, 2}, true);

    try
    {
        static_cast<void>(polygon.numVertices(-1));
        return fail("negative polygon index was accepted");
    }
    catch (const std::out_of_range&)
    {
    }

    try
    {
        static_cast<void>(polygon.vertices(0));
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
        static_cast<void>(geometry->duplicatePoint(0));
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

int verifyNonNumericAttributesAreSkipped()
{
    const std::string document = R"JSON([
        "pointcount", 3,
        "vertexcount", 3,
        "primitivecount", 1,
        "topology", ["pointref", ["indices", [0, 1, 2]]],
        "attributes", [
            "pointattributes", [
                [
                    ["type", "numeric", "name", "P"],
                    ["size", 3, "storage", "fpreal32", "values", [
                        "size", 3,
                        "storage", "fpreal32",
                        "tuples", [[0, 0, 0], [1, 0, 0], [0, 1, 0]]
                    ]]
                ],
                [
                    ["type", "dict", "name", "point_meta"],
                    [
                        "size", 1,
                        "storage", "int32",
                        "dicts", [{"label": {"type": "string", "value": "point"}}],
                        "indices", ["size", 1, "storage", "int32", "arrays", [[0, 0, 0]]]
                    ]
                ]
            ],
            "vertexattributes", [
                [
                    ["type", "dict", "name", "vertex_meta"],
                    [
                        "size", 1,
                        "storage", "int32",
                        "dicts", [{"label": {"type": "string", "value": "vertex"}}],
                        "indices", ["size", 1, "storage", "int32", "arrays", [[0, 0, 0]]]
                    ]
                ]
            ]
        ],
        "primitives", [[["type", "Poly"], ["vertex", [0, 1, 2], "closed", true]]]
    ])JSON";

    std::istringstream input(document);
    houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(input);
    if (!geometry)
        return fail("dictionary-attribute conversion fixture did not import");

    const std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives = geometry->primitives();
    if (primitives.size() != 1)
        return fail("dictionary-attribute conversion fixture lost its polygon");

    houio::DiagnosticList diagnostics;
    houio::Geometry::Ptr converted = houio::HouGeoIO::convertToGeometry(
        geometry, primitives.front(), &diagnostics);
    if (!converted)
        return fail("non-numeric attributes prevented geometry conversion");

    bool pointWarning = false;
    bool vertexWarning = false;
    for (const houio::Diagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.severity != houio::DiagnosticSeverity::warning
            || diagnostic.category != houio::DiagnosticCategory::conversion)
        {
            continue;
        }
        pointWarning = pointWarning
            || diagnostic.path == "attributes.pointattributes.point_meta";
        vertexWarning = vertexWarning
            || diagnostic.path == "attributes.vertexattributes.vertex_meta";
    }
    if (!pointWarning || !vertexWarning)
        return fail("non-numeric attributes were not reported as skipped");
    return 0;
}

int verifyRawDataViewBounds()
{
    const houio::HouGeoAdapter::RawDataView unavailable;
    if (unavailable.available())
        return fail("default RawDataView unexpectedly reports available data");
    try
    {
        static_cast<void>(unavailable.read<houio::sint32>(0));
        return fail("unavailable RawDataView allowed a scalar read");
    }
    catch (const std::logic_error&)
    {
    }

    const std::array<houio::sint32, 2> values = {7, 11};
    const auto view = houio::HouGeoAdapter::RawDataView::from<houio::sint32>(values);
    if (!view.available() || view.sizeBytes() != sizeof(values)
        || view.read<houio::sint32>(0) != 7 || view.read<houio::sint32>(1) != 11)
    {
        return fail("RawDataView did not preserve bounded scalar data");
    }
    try
    {
        static_cast<void>(view.read<houio::sint32>(2));
        return fail("RawDataView allowed an out-of-range scalar read");
    }
    catch (const std::out_of_range&)
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
    strings.setStringValues({"first"});
    try
    {
        static_cast<void>(strings.getString(1));
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
    if (const int result = verifyNonNumericAttributesAreSkipped(); result != 0)
    {
        return result;
    }
    if (const int result = verifyRawDataViewBounds(); result != 0)
    {
        return result;
    }
    return verifyAttributeAndStringBounds();
}
