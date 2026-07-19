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
                        "name", "P"
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
                        "scope", "public",
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

int verifyGeometry(const houio::HouGeo::Ptr& geometry, int expectedPositionTupleSize)
{
    if (!geometry)
    {
        return fail("geometry is null");
    }
    if (geometry->pointcount() != 4 || geometry->vertexcount() != 4 || geometry->primitivecount() != 1)
    {
        return fail("unexpected geometry counts");
    }

    houio::HouGeoAdapter::AttributeAdapter::Ptr position = geometry->getPointAttribute("P");
    if (!position)
    {
        return fail("modern tuple-based P attribute is missing");
    }
    if (position->getTupleSize() != expectedPositionTupleSize || position->getNumElements() != 4)
    {
        return fail(
            "unexpected P metadata: tuple_size=" + std::to_string(position->getTupleSize())
            + ", elements=" + std::to_string(position->getNumElements()));
    }

    houio::HouGeoAdapter::AttributeAdapter::Ptr normals = geometry->getVertexAttribute("N");
    if (!normals || normals->getTupleSize() != 3 || normals->getNumElements() != 4)
    {
        return fail("vertex N attribute was not preserved");
    }

    houio::HouGeoAdapter::AttributeAdapter::Ptr uv = geometry->getVertexAttribute("uv");
    if (!uv || uv->getTupleSize() != 2 || uv->getNumElements() != 4)
    {
        return fail("vertex uv attribute was not preserved");
    }

    const auto* normalData = static_cast<const houio::real32*>(normals->getRawPointer()->ptr);
    const auto* uvData = static_cast<const houio::real32*>(uv->getRawPointer()->ptr);
    if (!normalData || normalData[2] != 1.0f || !uvData || uvData[4] != 1.0f || uvData[5] != 1.0f)
    {
        return fail("representative vertex attribute values were not preserved");
    }

    houio::HouGeoAdapter::AttributeAdapter::Ptr name = geometry->getPrimitiveAttribute("name");
    if (!name || name->getType() != houio::HouGeoAdapter::AttributeAdapter::ATTR_TYPE_STRING
        || name->getNumElements() != 1 || name->getString(0) != "prop")
    {
        return fail("indexed primitive string attribute was not preserved");
    }

    houio::HouGeoAdapter::AttributeAdapter::Ptr piece = geometry->getPrimitiveAttribute("piece");
    if (!piece || piece->getStorage() != houio::HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT32
        || piece->getTupleSize() != 1 || piece->getNumElements() != 1)
    {
        return fail("primitive integer attribute metadata was not preserved");
    }
    const auto* pieceData = static_cast<const houio::sint32*>(piece->getRawPointer()->ptr);
    if (!pieceData || pieceData[0] != 7)
    {
        return fail("primitive integer attribute value was not preserved");
    }

    std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives;
    geometry->getPrimitives(primitives);
    if (primitives.size() != 1)
    {
        return fail("unexpected primitive container count");
    }

    const auto polygon = std::dynamic_pointer_cast<houio::HouGeoAdapter::PolyPrimitive>(primitives.front());
    if (!polygon || polygon->numPolys() != 1 || polygon->numVertices(0) != 4)
    {
        return fail("Polygon_run was not expanded correctly");
    }

    houio::Geometry::Ptr converted = houio::HouGeoIO::convertToGeometry(geometry, primitives.front());
    if (!converted || converted->numPrimitives() != 1)
    {
        return fail("failed to convert modern geometry to the simplified mesh model");
    }

    houio::Attribute::Ptr convertedPositions = converted->getAttr("P");
    houio::Attribute::Ptr convertedUv = converted->getAttr("UV");
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
    std::istringstream source(modernQuadGeometry());
    houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(&source);
    if (const int result = verifyGeometry(geometry, 3); result != 0)
    {
        return result;
    }

    std::ostringstream binaryOutput(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::xport(&binaryOutput, geometry, true))
    {
        return fail("failed to export modern quad geometry");
    }

    std::istringstream binaryInput(binaryOutput.str(), std::ios::in | std::ios::binary);
    houio::HouGeo::Ptr roundtripGeometry = houio::HouGeoIO::import(&binaryInput);
    return verifyGeometry(roundtripGeometry, 4);
}
