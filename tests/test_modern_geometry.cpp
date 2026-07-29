#include <houio/HouGeoIO.h>

#include "TestSupport.h"

#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::is_same_v<
    decltype(std::declval<const houio::HouGeoAdapter&>().pointAttribute(
        std::declval<const std::string&>())),
    houio::HouGeoAdapter::AttributeAdapter::ConstPtr>);
static_assert(std::is_same_v<
    decltype(std::declval<const houio::HouGeoAdapter&>().primitives()),
    std::vector<houio::HouGeoAdapter::Primitive::ConstPtr>>);
static_assert(std::is_same_v<
    decltype(std::declval<const houio::HouGeoAdapter&>().topology()),
    houio::HouGeoAdapter::Topology::ConstPtr>);
static_assert(std::is_same_v<
    decltype(std::declval<const houio::HouGeoAdapter::AttributeAdapter&>().tupleSize()),
    houio::HouGeoAdapter::AttributeAdapter::TupleSize>);
static_assert(!std::is_convertible_v<
    houio::HouGeoAdapter::AttributeAdapter::TupleSize,
    int>);

namespace
{
using houio::test::fail;

const char* modernQuadGeometry()
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
                    [
                        "scope", "public",
                        "type", "numeric",
                        "name", "P",
                        "options", {
                            "type": {"type": "string", "value": "point"},
                            "label": {"type": "string", "value": "position"}
                        }
                    ],
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
            "vertexattributes", [
                [
                    [
                        "scope", "public",
                        "type", "numeric",
                        "name", "N"
                    ],
                    [
                        "size", 3,
                        "storage", "fpreal32",
                        "values", [
                            "size", 3,
                            "storage", "fpreal32",
                            "tuples", [
                                [0, 0, 1],
                                [0, 0, 1],
                                [0, 0, 1],
                                [0, 0, 1]
                            ]
                        ]
                    ]
                ],
                [
                    [
                        "scope", "public",
                        "type", "numeric",
                        "name", "uv"
                    ],
                    [
                        "size", 2,
                        "storage", "fpreal32",
                        "values", [
                            "size", 2,
                            "storage", "fpreal32",
                            "tuples", [
                                [0, 0],
                                [1, 0],
                                [1, 1],
                                [0, 1]
                            ]
                        ]
                    ]
                ]
            ],
            "primitiveattributes", [
                [
                    [
                        "scope", "private",
                        "type", "string",
                        "name", "name"
                    ],
                    [
                        "size", 1,
                        "storage", "int32",
                        "strings", ["body", "prop"],
                        "indices", [
                            "size", 1,
                            "storage", "int32",
                            "arrays", [[1]]
                        ]
                    ]
                ],
                [
                    [
                        "scope", "public",
                        "type", "numeric",
                        "name", "piece"
                    ],
                    [
                        "size", 1,
                        "storage", "int32",
                        "values", [
                            "size", 1,
                            "storage", "int32",
                            "arrays", [[7]]
                        ]
                    ]
                ]
            ]
        ],
        "primitives", [
            [
                ["type", "Polygon_run"],
                [
                    "startvertex", 0,
                    "nprimitives", 1,
                    "nvertices_rle", [4, 1]
                ]
            ]
        ]
    ])JSON";
}

int verifyStrongAttributeMetadata()
{
    using AttributeAdapter = houio::HouGeoAdapter::AttributeAdapter;
    static_assert(static_cast<int>(houio::Attribute::ComponentType::invalid) == 0);
    static_assert(static_cast<int>(houio::Attribute::ComponentType::int32) == 1);
    static_assert(static_cast<int>(houio::Attribute::ComponentType::float32) == 2);
    static_assert(static_cast<int>(houio::Attribute::ComponentType::float64) == 3);
    static_assert(static_cast<int>(houio::Attribute::ComponentType::int64) == 4);
    static_assert(static_cast<int>(houio::Attribute::ComponentType::float16) == 5);
    static_assert(static_cast<int>(houio::Attribute::ComponentType::uint8) == 6);
    static_assert(static_cast<int>(AttributeAdapter::Storage::invalid) == 0);
    static_assert(static_cast<int>(AttributeAdapter::Storage::float32) == 1);
    static_assert(static_cast<int>(AttributeAdapter::Storage::float64) == 2);
    static_assert(static_cast<int>(AttributeAdapter::Storage::int32) == 3);
    static_assert(static_cast<int>(AttributeAdapter::Storage::int64) == 4);
    static_assert(static_cast<int>(AttributeAdapter::Storage::float16) == 5);
    static_assert(static_cast<int>(AttributeAdapter::Storage::uint8) == 6);

    const AttributeAdapter::TupleSize tupleSize(3);
    if (tupleSize.value() != 3 || tupleSize.asSize() != 3u)
    {
        return fail("strong tuple-size metadata did not preserve its value");
    }

    for (const int invalidValue : {0, -1})
    {
        try
        {
            static_cast<void>(AttributeAdapter::TupleSize(invalidValue));
            return fail("strong tuple-size metadata accepted a non-positive value");
        }
        catch (const std::invalid_argument&)
        {
        }
    }

    struct TypeCase
    {
        AttributeAdapter::Type type;
        std::string_view name;
    };
    const std::array<TypeCase, 3> typeCases{{
        {AttributeAdapter::Type::numeric, "numeric"},
        {AttributeAdapter::Type::string, "string"},
        {AttributeAdapter::Type::dictionary, "dict"}}};
    for (const TypeCase& typeCase : typeCases)
    {
        const auto name = AttributeAdapter::typeName(typeCase.type);
        if (!name || *name != typeCase.name || AttributeAdapter::parseType(*name) != typeCase.type)
        {
            return fail("attribute type metadata did not round-trip through its canonical name");
        }
    }
    if (AttributeAdapter::typeName(AttributeAdapter::Type::invalid)
        || AttributeAdapter::parseType("unknown") != AttributeAdapter::Type::invalid)
    {
        return fail("invalid attribute type metadata was not rejected");
    }

    struct StorageCase
    {
        AttributeAdapter::Storage storage;
        std::string_view name;
        std::size_t byteWidth;
    };
    const std::array<StorageCase, 6> storageCases{{
        {AttributeAdapter::Storage::uint8, "uint8", sizeof(houio::ubyte)},
        {AttributeAdapter::Storage::float16, "fpreal16", sizeof(houio::uword)},
        {AttributeAdapter::Storage::float32, "fpreal32", sizeof(houio::real32)},
        {AttributeAdapter::Storage::float64, "fpreal64", sizeof(houio::real64)},
        {AttributeAdapter::Storage::int32, "int32", sizeof(houio::sint32)},
        {AttributeAdapter::Storage::int64, "int64", sizeof(houio::sint64)}}};
    for (const StorageCase& storageCase : storageCases)
    {
        const auto name = AttributeAdapter::storageName(storageCase.storage);
        const auto byteWidth = AttributeAdapter::storageByteWidth(storageCase.storage);
        if (!name || *name != storageCase.name || !byteWidth || *byteWidth != storageCase.byteWidth
            || AttributeAdapter::parseStorage(*name) != storageCase.storage)
        {
            return fail("attribute storage metadata did not round-trip through its canonical representation");
        }
    }
    if (AttributeAdapter::storageName(AttributeAdapter::Storage::invalid)
        || AttributeAdapter::storageByteWidth(AttributeAdapter::Storage::invalid)
        || AttributeAdapter::parseStorage("unknown") != AttributeAdapter::Storage::invalid)
    {
        return fail("invalid attribute storage metadata was not rejected");
    }

    houio::HouGeo::HouAttribute attribute;
    if (attribute.scope() != "public" || !attribute.options()
        || !attribute.options()->entries().empty())
    {
        return fail("default attribute definition metadata is invalid");
    }
    try
    {
        attribute.setScope("");
        return fail("attribute accepted an empty scope");
    }
    catch (const std::invalid_argument&)
    {
    }
    attribute.setOptions(nullptr);
    if (!attribute.options() || !attribute.options()->entries().empty())
        return fail("null options did not normalize to an empty object");
    return 0;
}

int verifyGeometry(const houio::HouGeo::ConstPtr& geometry, int expectedPositionTupleSize)
{
    if (!geometry)
    {
        return fail("geometry is null");
    }
    if (geometry->pointCount() != 4 || geometry->vertexCount() != 4 || geometry->primitiveCount() != 1)
    {
        return fail("unexpected geometry counts");
    }

    houio::HouGeoAdapter::AttributeAdapter::ConstPtr position = geometry->pointAttribute("P");
    if (!position)
    {
        return fail("modern tuple-based P attribute is missing");
    }
    if (position->tupleSize().value() != expectedPositionTupleSize || position->elementCount() != 4)
    {
        return fail(
            "unexpected P metadata: tuple_size=" + std::to_string(position->tupleSize().value())
            + ", elements=" + std::to_string(position->elementCount()));
    }
    const houio::json::ObjectPtr positionOptions = position->options();
    const houio::json::ObjectPtr positionType = positionOptions ? positionOptions->object("type") : nullptr;
    const houio::json::ObjectPtr positionLabel = positionOptions ? positionOptions->object("label") : nullptr;
    if (position->scope() != "public" || !positionType || !positionLabel
        || positionType->get<std::string>("value") != "point"
        || positionLabel->get<std::string>("value") != "position")
    {
        return fail("P scope or semantic options were not preserved");
    }

    houio::HouGeoAdapter::AttributeAdapter::ConstPtr normals = geometry->vertexAttribute("N");
    if (!normals || normals->tupleSize().value() != 3 || normals->elementCount() != 4)
    {
        return fail("vertex N attribute was not preserved");
    }

    houio::HouGeoAdapter::AttributeAdapter::ConstPtr uv = geometry->vertexAttribute("uv");
    if (!uv || uv->tupleSize().value() != 2 || uv->elementCount() != 4)
    {
        return fail("vertex uv attribute was not preserved");
    }

    const auto normal_data = normals->rawData();
    const auto uv_data = uv->rawData();
    if (!normal_data.available() || normal_data.read<houio::real32>(2) != 1.0f
        || !uv_data.available() || uv_data.read<houio::real32>(4) != 1.0f
        || uv_data.read<houio::real32>(5) != 1.0f)
    {
        return fail("representative vertex attribute values were not preserved");
    }

    houio::HouGeoAdapter::AttributeAdapter::ConstPtr name =
        geometry->primitiveAttribute("name");
    if (!name || name->type() != houio::HouGeoAdapter::AttributeAdapter::Type::string
        || name->scope() != "private" || name->elementCount() != 1
        || name->stringValue(0) != "prop")
    {
        return fail("indexed primitive string attribute was not preserved");
    }

    houio::HouGeoAdapter::AttributeAdapter::ConstPtr piece =
        geometry->primitiveAttribute("piece");
    if (!piece || piece->storage() != houio::HouGeoAdapter::AttributeAdapter::Storage::int32
        || piece->tupleSize().value() != 1 || piece->elementCount() != 1)
    {
        return fail("primitive integer attribute metadata was not preserved");
    }
    const auto piece_data = piece->rawData();
    if (!piece_data.available() || piece_data.read<houio::sint32>(0) != 7)
    {
        return fail("primitive integer attribute value was not preserved");
    }

    const std::vector<houio::HouGeoAdapter::Primitive::ConstPtr> primitives =
        geometry->primitives();
    if (primitives.size() != 1)
    {
        return fail("unexpected primitive container count");
    }

    const auto polygon =
        std::dynamic_pointer_cast<const houio::HouGeoAdapter::PolyPrimitive>(primitives.front());
    if (!polygon || polygon->polygonCount() != 1 || polygon->polygonVertexCount(0) != 4)
    {
        return fail("Polygon_run was not expanded correctly");
    }

    houio::Geometry::Ptr converted = houio::HouGeoIO::convertToGeometry(geometry, primitives.front());
    if (!converted || converted->primitiveCount() != 1)
    {
        return fail("failed to convert modern geometry to the simplified mesh model");
    }

    houio::Attribute::Ptr convertedPositions = converted->attribute("P");
    houio::Attribute::Ptr convertedUv = converted->attribute("UV");
    if (!convertedPositions || convertedPositions->numElements() != 4 || !convertedUv
        || convertedUv->numElements() != 4)
    {
        return fail("simplified mesh conversion lost P or UV elements");
    }

    const houio::math::V3f convertedPosition = convertedPositions->get<houio::math::V3f>(2);
    const houio::math::V2f convertedUvValue = convertedUv->get<houio::math::V2f>(2);
    if (convertedPosition.x != 1.0f || convertedPosition.y != 1.0f
        || convertedUvValue.x != 1.0f || convertedUvValue.y != 1.0f)
    {
        return fail("simplified mesh conversion used an incorrect tuple stride");
    }

    return 0;
}
}

int main()
{
    if (const int result = verifyStrongAttributeMetadata(); result != 0)
    {
        return result;
    }

    std::istringstream source(modernQuadGeometry());
    houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(source);
    if (const int result = verifyGeometry(geometry, 3); result != 0)
    {
        return result;
    }

    std::ostringstream binaryOutput(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::exportGeometry(binaryOutput, geometry, true))
    {
        return fail("failed to export modern quad geometry");
    }

    std::istringstream binaryInput(binaryOutput.str(), std::ios::in | std::ios::binary);
    houio::HouGeo::Ptr roundtripGeometry = houio::HouGeoIO::import(binaryInput);
    return verifyGeometry(roundtripGeometry, 4);
}
