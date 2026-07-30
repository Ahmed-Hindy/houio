#include <houio/HouGeoIO.h>

#include "TestSupport.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using houio::test::fail;

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

    const houio::GeometryConversionResult detailed =
        houio::HouGeoIO::convertToGeometryResult(
            geometry, houio::HouGeoAdapter::Primitive::ConstPtr());
    if (detailed || detailed.report.sourcePointCount != 3
        || detailed.report.outputPointCount != 0
        || detailed.diagnostics.size() != 1
        || detailed.diagnostics.front().category != houio::DiagnosticCategory::schema
        || detailed.diagnostics.front().path != "attributes.pointattributes.P")
    {
        return fail("conversion result did not preserve failure diagnostics and partial report data");
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

int verifyPartialPrimitiveDomainIsSkipped()
{
    const houio::HouGeo::Ptr geometry = houio::HouGeo::create();
    houio::Attribute::Ptr positions = houio::Attribute::createV3f();
    positions->appendElement(houio::math::V3f(0.0f, 0.0f, 0.0f));
    positions->appendElement(houio::math::V3f(1.0f, 0.0f, 0.0f));
    positions->appendElement(houio::math::V3f(0.0f, 1.0f, 0.0f));
    geometry->setPointAttribute(
        std::make_shared<houio::HouGeo::HouAttribute>("P", positions));

    auto topology = std::make_shared<houio::HouGeo::HouTopology>();
    topology->setIndices({0, 1, 2});
    geometry->setTopology(topology);
    auto polygon = std::make_shared<houio::HouGeo::HouPoly>();
    polygon->setPolygonData(1, {3}, {0}, {0, 1, 2}, true);
    geometry->addPrimitive(polygon);
    auto second_polygon = std::make_shared<houio::HouGeo::HouPoly>();
    second_polygon->setPolygonData(1, {3}, {0}, {0, 1, 2}, true);
    geometry->addPrimitive(second_polygon);

    houio::Attribute::Ptr primitive_ids = houio::Attribute::createInt();
    primitive_ids->appendElement<houio::sint32>(7);
    primitive_ids->appendElement<houio::sint32>(9);
    geometry->setPrimitiveAttribute(
        "id", std::make_shared<houio::HouGeo::HouAttribute>("id", primitive_ids));

    const houio::GeometryConversionResult result =
        houio::HouGeoIO::convertToGeometryResult(geometry, polygon);
    const bool reported = std::find(
        result.report.skippedPrimitiveAttributes.begin(),
        result.report.skippedPrimitiveAttributes.end(),
        "id") != result.report.skippedPrimitiveAttributes.end();
    bool warned = false;
    for (const houio::Diagnostic& diagnostic : result.diagnostics)
        warned = warned || diagnostic.path == "attributes.primitiveattributes.id";
    if (!result || result.value->primitiveCount() != 1
        || result.value->primitiveAttribute("id") || !reported || !warned)
    {
        return fail("partial primitive-domain conversion was not reported and skipped safely");
    }
    return 0;
}

int verifyPolygonAccessorSafety()
{
    houio::HouGeo::HouPoly polygon;
    polygon.setPolygonData(1, {3}, {1}, {0, 1, 2}, true);

    try
    {
        static_cast<void>(polygon.polygonVertexCount(-1));
        return fail("negative polygon index was accepted");
    }
    catch (const std::out_of_range&)
    {
    }

    try
    {
        static_cast<void>(polygon.polygonVertexIndices(0));
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
        geometry->setAttribute("invalid", houio::Attribute::Ptr());
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
            ],
            "globalattributes", [
                [
                    ["type", "string", "name", "global_meta"],
                    [
                        "size", 1,
                        "storage", "int32",
                        "strings", ["scene"],
                        "indices", ["size", 1, "storage", "int32", "arrays", [[0]]]
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

    auto primitiveMetadata = std::make_shared<houio::HouGeo::HouAttribute>();
    primitiveMetadata->setName("primitive_meta");
    primitiveMetadata->setStringValues({"triangle"});
    geometry->setPrimitiveAttribute("primitive_meta", primitiveMetadata);

    geometry->setPointGroup("selected_points", {true, false, false});
    geometry->setVertexGroup("selected_vertices", {true, false, false});
    geometry->setPrimitiveGroup("selected_primitives", {true});

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

    const houio::GeometryConversionResult detailed =
        houio::HouGeoIO::convertToGeometryResult(geometry, primitives.front());
    const auto contains = [](const std::vector<std::string>& values, const std::string& expected)
    {
        return std::find(values.begin(), values.end(), expected) != values.end();
    };
    if (!detailed || detailed.report.sourcePointCount != 3
        || detailed.report.outputPointCount != 3
        || detailed.report.splitSourcePointCount != 0
        || detailed.report.duplicatedPointCount != 0
        || !detailed.report.windingReversed
        || !contains(detailed.report.skippedPointAttributes, "point_meta")
        || !contains(detailed.report.skippedVertexAttributes, "vertex_meta")
        || !contains(detailed.report.skippedPrimitiveAttributes, "primitive_meta")
        || !contains(detailed.report.skippedGlobalAttributes, "global_meta")
        || !contains(detailed.report.droppedPointGroups, "selected_points")
        || !contains(detailed.report.droppedVertexGroups, "selected_vertices")
        || !contains(detailed.report.droppedPrimitiveGroups, "selected_primitives"))
    {
        return fail("structured conversion result did not report successful losses");
    }
    return 0;
}

int verifyFaceVaryingPreservation()
{
    const std::string document = R"JSON([
        "pointcount", 4,
        "vertexcount", 6,
        "primitivecount", 2,
        "topology", ["pointref", ["indices", [0, 1, 2, 0, 2, 3]]],
        "attributes", [
            "pointattributes", [
                [
                    ["type", "numeric", "name", "P"],
                    ["size", 3, "storage", "fpreal32", "values", [
                        "size", 3,
                        "storage", "fpreal32",
                        "tuples", [[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]]
                    ]]
                ]
            ],
            "vertexattributes", [
                [
                    ["type", "numeric", "name", "uv"],
                    ["size", 2, "storage", "fpreal32", "values", [
                        "size", 2,
                        "storage", "fpreal32",
                        "tuples", [[0, 0], [1, 0], [1, 1], [0.5, 0.5], [1, 1], [0, 1]]
                    ]]
                ]
            ]
        ],
        "primitives", [[
            ["type", "Polygon_run"],
            ["startvertex", 0, "nprimitives", 2, "nvertices_rle", [3, 2]]
        ]]
    ])JSON";

    std::istringstream input(document);
    const houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(input);
    if (!geometry)
        return fail("point-split conversion fixture did not import");
    const std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives = geometry->primitives();
    if (primitives.size() != 1)
        return fail("point-split conversion fixture lost its polygon run");

    const houio::GeometryConversionResult result =
        houio::HouGeoIO::convertToGeometryResult(geometry, primitives.front());
    const houio::Attribute::Ptr vertexUv = result
        ? result.value->vertexAttribute("UV") : houio::Attribute::Ptr();
    const std::span<const houio::Geometry::Index> indices = result
        ? result.value->indexBuffer() : std::span<const houio::Geometry::Index>();
    if (!result || result.report.sourcePointCount != 4
        || result.report.outputPointCount != 4
        || result.report.splitSourcePointCount != 0
        || result.report.duplicatedPointCount != 0
        || !result.report.windingReversed
        || result.value->primitiveCount() != 2
        || indices.size() != 6
        || indices[0] != 2 || indices[1] != 1 || indices[2] != 0
        || indices[3] != 3 || indices[4] != 2 || indices[5] != 0
        || !vertexUv || vertexUv->numElements() != 6
        || result.value->hasPointAttribute("UV"))
    {
        return fail("conversion did not preserve lossless face-varying domains");
    }
    const std::array<houio::math::V2f, 6> expectedUv = {
        houio::math::V2f(1.0f, 1.0f),
        houio::math::V2f(1.0f, 0.0f),
        houio::math::V2f(0.0f, 0.0f),
        houio::math::V2f(0.0f, 1.0f),
        houio::math::V2f(1.0f, 1.0f),
        houio::math::V2f(0.5f, 0.5f),
    };
    for (std::size_t index = 0; index < expectedUv.size(); ++index)
    {
        if (vertexUv->get<houio::math::V2f>(index) != expectedUv[index])
            return fail("conversion changed face-varying UV ordering");
    }
    return 0;
}

int verifyOversizedIntegerPackingRejection()
{
    const std::string document = R"JSON([
        "pointcount", 1,
        "vertexcount", 0,
        "primitivecount", 0,
        "attributes", [
            "pointattributes", [
                [
                    ["type", "string", "name", "label"],
                    [
                        "size", 1,
                        "storage", "int32",
                        "strings", ["value"],
                        "indices", [
                            "size", 1,
                            "storage", "int32",
                            "pagesize", 1,
                            "packing", [2147483647, 2147483647, 3],
                            "constantpageflags", [[true], [true], [true]],
                            "rawpagedata", [0, 0, 0]
                        ]
                    ]
                ]
            ]
        ]
    ])JSON";

    std::istringstream input(document);
    houio::DiagnosticList diagnostics;
    if (houio::HouGeoIO::import(input, &diagnostics))
        return fail("oversized integer packing was accepted");
    if (diagnostics.empty() || diagnostics.back().category != houio::DiagnosticCategory::schema)
        return fail("oversized integer packing did not produce a schema diagnostic");
    return 0;
}

int verifyPolygonConversion()
{
    houio::HouGeo::Ptr geometry = houio::HouGeo::create();
    houio::Attribute::Ptr positions = houio::Attribute::createV3f();
    positions->appendElement(houio::math::V3f(0.0f, 0.0f, 0.0f));
    positions->appendElement(houio::math::V3f(1.0f, 0.0f, 0.0f));
    positions->appendElement(houio::math::V3f(1.5f, 1.0f, 0.0f));
    positions->appendElement(houio::math::V3f(0.5f, 2.0f, 0.0f));
    positions->appendElement(houio::math::V3f(-0.5f, 1.0f, 0.0f));
    geometry->setPointAttribute(
        std::make_shared<houio::HouGeo::HouAttribute>("P", positions));

    auto topology = std::make_shared<houio::HouGeo::HouTopology>();
    topology->setIndices({0, 1, 2, 3, 4});
    geometry->setTopology(topology);

    auto polygon = std::make_shared<houio::HouGeo::HouPoly>();
    polygon->setPolygonData(1, {5}, {0}, {0, 1, 2, 3, 4}, false);
    geometry->addPrimitive(polygon);

    houio::GeometryConversionResult conversion =
        houio::HouGeoIO::convertToGeometryResult(geometry, polygon);
    const std::span<const houio::Geometry::Index> indices = conversion.value
        ? conversion.value->indexBuffer() : std::span<const houio::Geometry::Index>();
    const houio::Attribute::CPtr convertedPositions = conversion.value
        ? conversion.value->attribute("P") : houio::Attribute::CPtr();
    bool closureWarning = false;
    for (const houio::Diagnostic& diagnostic : conversion.diagnostics)
    {
        closureWarning = closureWarning
            || (diagnostic.severity == houio::DiagnosticSeverity::warning
                && diagnostic.category == houio::DiagnosticCategory::conversion
                && diagnostic.path == "conversion.primitive.closed");
    }
    if (!conversion || conversion.value->primitiveType() != houio::Geometry::PrimitiveType::polygon
        || conversion.value->primitiveCount() != 1 || conversion.value->verticesPerPrimitive() != 5
        || indices.size() != 5 || indices[0] != 4 || indices[1] != 3 || indices[2] != 2
        || indices[3] != 1 || indices[4] != 0 || !convertedPositions
        || convertedPositions->numElements() != 5 || !conversion.report.windingReversed
        || !conversion.report.polygonClosureLost || !closureWarning)
    {
        return fail("single n-gon conversion did not preserve simplified polygon topology");
    }

    houio::HouGeo::Ptr multiple = houio::HouGeo::create();
    houio::Attribute::Ptr multiplePositions = houio::Attribute::createV3f();
    for (int pointIndex = 0; pointIndex < 8; ++pointIndex)
        multiplePositions->appendElement(houio::math::V3f(static_cast<float>(pointIndex), 0.0f, 0.0f));
    multiple->setPointAttribute(
        std::make_shared<houio::HouGeo::HouAttribute>("P", multiplePositions));
    auto multipleTopology = std::make_shared<houio::HouGeo::HouTopology>();
    multipleTopology->setIndices({0, 1, 2, 3, 4, 5, 6, 7});
    multiple->setTopology(multipleTopology);
    auto multiplePolygons = std::make_shared<houio::HouGeo::HouPoly>();
    multiplePolygons->setPolygonData(
        2, {5, 3}, {0, 5}, {0, 1, 2, 3, 4, 5, 6, 7}, true);
    multiple->addPrimitive(multiplePolygons);
    const houio::GeometryConversionResult converted =
        houio::HouGeoIO::convertToGeometryResult(multiple, multiplePolygons);
    const std::span<const unsigned int> polygonCounts = converted.value
        ? converted.value->primitiveVertexCounts() : std::span<const unsigned int>();
    const std::array<houio::Geometry::Index, 8> expectedIndices = {
        4, 3, 2, 1, 0, 7, 6, 5};
    if (!converted
        || converted.value->primitiveType() != houio::Geometry::PrimitiveType::polygon
        || converted.value->primitiveCount() != 2
        || converted.value->verticesPerPrimitive() != 0
        || polygonCounts.size() != 2 || polygonCounts[0] != 5 || polygonCounts[1] != 3
        || !std::equal(
            converted.value->indexBuffer().begin(),
            converted.value->indexBuffer().end(),
            expectedIndices.begin()))
    {
        return fail("variable polygon conversion did not preserve primitive boundaries");
    }
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
    houio::Attribute oversized(
        4, houio::Attribute::ComponentType::float32);
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
        static_cast<void>(strings.stringValue(1));
        return fail("HouAttribute accepted an out-of-range string index");
    }
    catch (const std::out_of_range&)
    {
    }

    using TupleSize = houio::HouGeoAdapter::AttributeAdapter::TupleSize;
    strings.setStringValues({"a0", "a1", "b0", "b1"}, TupleSize(2));
    if (strings.elementCount() != 2 || strings.tupleSize().value() != 2
        || strings.stringValue(0, 0) != "a0" || strings.stringValue(0, 1) != "a1"
        || strings.stringValue(1, 0) != "b0" || strings.stringValue(1, 1) != "b1")
    {
        return fail("HouAttribute did not preserve string tuple storage");
    }
    try
    {
        static_cast<void>(strings.stringValue(0, 2));
        return fail("HouAttribute accepted an out-of-range string component");
    }
    catch (const std::out_of_range&)
    {
    }
    try
    {
        strings.setStringValues({"incomplete", "tuple", "data"}, TupleSize(2));
        return fail("HouAttribute accepted incomplete string tuples");
    }
    catch (const std::invalid_argument&)
    {
    }
    if (strings.elementCount() != 2 || strings.stringValue(1, 1) != "b1")
        return fail("failed string tuple assignment partially mutated HouAttribute");
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
    if (const int result = verifyPartialPrimitiveDomainIsSkipped(); result != 0)
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
    if (const int result = verifyFaceVaryingPreservation(); result != 0)
    {
        return result;
    }
    if (const int result = verifyOversizedIntegerPackingRejection(); result != 0)
    {
        return result;
    }
    if (const int result = verifyPolygonConversion(); result != 0)
    {
        return result;
    }
    if (const int result = verifyRawDataViewBounds(); result != 0)
    {
        return result;
    }
    return verifyAttributeAndStringBounds();
}
