#include <houio/HouGeoIO.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

const char* mixedPolygonGeometry()
{
    return R"JSON([
        "pointcount", 4,
        "vertexcount", 6,
        "primitivecount", 2,
        "topology", [
            "pointref", [
                "indices", [0, 1, 2, 0, 2, 3]
            ]
        ],
        "attributes", [
            "pointattributes", [
                [
                    ["scope", "public", "type", "numeric", "name", "P"],
                    [
                        "size", 3,
                        "storage", "fpreal32",
                        "values", [
                            "size", 3,
                            "storage", "fpreal32",
                            "tuples", [
                                [0, 0, 0],
                                [1, 0, 0],
                                [1, 1, 0],
                                [0, 1, 0]
                            ]
                        ]
                    ]
                ]
            ],
            "globalattributes", [
                [
                    ["scope", "public", "type", "string", "name", "fixture"],
                    [
                        "size", 1,
                        "storage", "int32",
                        "strings", ["polygon_runs"],
                        "indices", ["size", 1, "storage", "int32", "arrays", [[0]]]
                    ]
                ],
                [
                    ["scope", "public", "type", "numeric", "name", "version"],
                    [
                        "size", 1,
                        "storage", "int32",
                        "values", ["size", 1, "storage", "int32", "arrays", [[22]]]
                    ]
                ],
                [
                    ["scope", "public", "type", "string", "name", "empty_value"],
                    [
                        "size", 1,
                        "storage", "int32",
                        "strings", [],
                        "indices", [
                            "size", 1,
                            "storage", "int32",
                            "pagesize", 1024,
                            "constantpageflags", [[true]],
                            "rawpagedata", [-1]
                        ]
                    ]
                ],
                [
                    ["scope", "public", "type", "dict", "name", "settings"],
                    [
                        "size", 1,
                        "storage", "int32",
                        "dicts", [
                            {
                                "empty": {"type": "string", "value": ""},
                                "count": {"type": "int", "value": 3},
                                "range": {"type": "vector2", "value": [0.0, 1.0]}
                            }
                        ]
                    ]
                ]
            ]
        ],
        "primitives", [
            [
                ["type", "p_r"],
                ["s_v", 0, "n_p", 2, "n_v", [3, 3]]
            ]
        ]
    ])JSON";
}

const char* openPolygonGeometry()
{
    return R"JSON([
        "pointcount", 4,
        "vertexcount", 4,
        "primitivecount", 1,
        "topology", [
            "pointref", [
                "indices", [0, 1, 2, 3]
            ]
        ],
        "attributes", [
            "pointattributes", [
                [
                    ["scope", "public", "type", "numeric", "name", "P"],
                    [
                        "size", 3,
                        "storage", "fpreal32",
                        "values", [
                            "size", 3,
                            "storage", "fpreal32",
                            "tuples", [
                                [0, 0, 0],
                                [1, 0, 0],
                                [2, 1, 0],
                                [3, 0, 0]
                            ]
                        ]
                    ]
                ]
            ]
        ],
        "primitives", [
            [
                ["type", "c_r"],
                ["s_v", 0, "n_p", 1, "n_v", [4]]
            ]
        ]
    ])JSON";
}

int verifyPolygon(const houio::HouGeoAdapter::PolyPrimitive::Ptr& polygon, bool expectedClosed,
                  const std::vector<std::vector<int>>& expectedPointIndices)
{
    if (!polygon)
    {
        return fail("polygon primitive is null");
    }
    if (polygon->closed() != expectedClosed)
    {
        return fail("polygon closed state was not preserved");
    }
    if (polygon->numPolys() != static_cast<int>(expectedPointIndices.size()))
    {
        return fail("unexpected polygon count");
    }

    for (int polygonIndex = 0; polygonIndex < polygon->numPolys(); ++polygonIndex)
    {
        const std::vector<int>& expected = expectedPointIndices[static_cast<size_t>(polygonIndex)];
        if (polygon->numVertices(polygonIndex) != static_cast<int>(expected.size()))
        {
            return fail("unexpected polygon vertex count");
        }

        const int* actual = polygon->vertices(polygonIndex);
        for (size_t vertexIndex = 0; vertexIndex < expected.size(); ++vertexIndex)
        {
            if (actual[vertexIndex] != expected[vertexIndex])
            {
                return fail(
                    "topology mismatch at polygon " + std::to_string(polygonIndex)
                    + ", vertex " + std::to_string(vertexIndex));
            }
        }
    }
    return 0;
}

int verifyMixedGeometry(const houio::HouGeo::Ptr& geometry)
{
    if (!geometry || geometry->pointCount() != 4 || geometry->vertexCount() != 6
        || geometry->primitiveCount() != 2)
    {
        return fail("mixed polygon counts are incorrect");
    }

    const std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives = geometry->primitives();
    if (primitives.size() != 1)
    {
        return fail("mixed polygon run was not grouped into one primitive adapter");
    }

    const auto polygon = std::dynamic_pointer_cast<houio::HouGeoAdapter::PolyPrimitive>(primitives.front());
    if (const int result = verifyPolygon(polygon, true, {{0, 1, 2}, {0, 2, 3}}); result != 0)
    {
        return result;
    }

    const auto fixture = geometry->globalAttribute("fixture");
    if (!fixture || fixture->stringValue(0) != "polygon_runs")
    {
        return fail("global string attribute was not preserved");
    }

    const auto version = geometry->globalAttribute("version");
    if (!version || version->storage() != houio::HouGeoAdapter::AttributeAdapter::Storage::int32)
    {
        return fail("global integer attribute metadata was not preserved");
    }
    const houio::HouGeoAdapter::RawDataView version_data = version->rawData();
    if (!version_data.available())
    {
        return fail("global integer attribute data is unavailable");
    }
    if (version_data.read<houio::sint32>(0) != 22)
    {
        return fail("global integer attribute value was not preserved");
    }

    const auto emptyValue = geometry->globalAttribute("empty_value");
    if (!emptyValue || emptyValue->stringValue(0) != "")
    {
        return fail("empty global string attribute was not preserved");
    }

    const auto settings = std::dynamic_pointer_cast<houio::HouGeo::HouAttribute>(
        geometry->globalAttribute("settings"));
    if (!settings || settings->type() != houio::HouGeoAdapter::AttributeAdapter::Type::dictionary
        || settings->dictionaryValues().size() != 1)
    {
        return fail("global dictionary attribute metadata was not preserved");
    }
    const auto dictionary = settings->dictionaryValues().front();
    const auto emptySetting = dictionary->getObject("empty");
    const auto countSetting = dictionary->getObject("count");
    const auto rangeSetting = dictionary->getObject("range");
    const auto rangeValues = rangeSetting ? rangeSetting->getArray("value") : houio::json::ArrayPtr();
    if (!emptySetting || emptySetting->get<std::string>("value") != ""
        || !countSetting || countSetting->get<int>("value") != 3
        || !rangeValues || rangeValues->size() != 2
        || rangeValues->get<double>(0) != 0.0 || rangeValues->get<double>(1) != 1.0)
    {
        return fail("global dictionary attribute values were not preserved");
    }
    return 0;
}

int verifyOpenGeometry(const houio::HouGeo::Ptr& geometry)
{
    if (!geometry || geometry->pointCount() != 4 || geometry->vertexCount() != 4
        || geometry->primitiveCount() != 1)
    {
        return fail("open polygon counts are incorrect");
    }

    const std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives = geometry->primitives();
    if (primitives.size() != 1)
    {
        return fail("open polygon run was not imported");
    }

    const auto polygon = std::dynamic_pointer_cast<houio::HouGeoAdapter::PolyPrimitive>(primitives.front());
    return verifyPolygon(polygon, false, {{0, 1, 2, 3}});
}

template <typename Verifier>
int verifyRoundtrip(const char* sourceText, Verifier verifier)
{
    std::istringstream source(sourceText);
    houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(source);
    if (const int result = verifier(geometry); result != 0)
    {
        return result;
    }

    std::ostringstream binaryOutput(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::exportGeometry(binaryOutput, geometry, true))
    {
        return fail("failed to export polygon-run geometry");
    }

    std::istringstream binaryInput(binaryOutput.str(), std::ios::in | std::ios::binary);
    return verifier(houio::HouGeoIO::import(binaryInput));
}
}

int main()
{
    if (const int result = verifyRoundtrip(mixedPolygonGeometry(), verifyMixedGeometry); result != 0)
    {
        return result;
    }
    return verifyRoundtrip(openPolygonGeometry(), verifyOpenGeometry);
}
