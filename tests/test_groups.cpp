#include <houio/HouGeoIO.h>

#include "TestSupport.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using houio::test::fail;

const char* groupedGeometry()
{
    return R"JSON([
        "pointcount", 4,
        "vertexcount", 6,
        "primitivecount", 2,
        "topology", [
            "pointref", ["indices", [0, 1, 2, 0, 2, 3]]
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
                            "tuples", [[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]]
                        ]
                    ]
                ]
            ]
        ],
        "primitives", [
            [["type", "p_r"], ["s_v", 0, "n_p", 2, "n_v", [3, 3]]]
        ],
        "pointgroups", [
            [["name", "left_points"], ["selection", ["unordered", ["i8", [1, 0, 0, 1]]]]],
            [["name", "shared_points"], ["selection", ["unordered", ["i8", [1, 0, 1, 0]]]]]
        ],
        "vertexgroups", [
            [["name", "first_face_vertices"], ["selection", ["unordered", ["i8", [1, 1, 1, 0, 0, 0]]]]],
            [["name", "shared_point_vertices"], ["selection", ["unordered", ["i8", [1, 0, 1, 1, 1, 0]]]]]
        ],
        "primitivegroups", [
            [["name", "left"], ["selection", ["unordered", ["i8", [1, 0]]]]],
            [["name", "right"], ["selection", ["unordered", ["i8", [0, 1]]]]]
        ]
    ])JSON";
}

using GroupNamesGetter = std::vector<std::string> (houio::HouGeoAdapter::*)() const;
using GroupMembershipGetter = std::optional<std::vector<bool>>
    (houio::HouGeoAdapter::*)(const std::string&) const;

int verifyGroupDomain(const houio::HouGeo::Ptr& geometry, GroupNamesGetter namesGetter,
                      GroupMembershipGetter membershipGetter, const std::vector<std::string>& expectedNames,
                      const std::vector<std::vector<bool>>& expectedMemberships)
{
    const std::vector<std::string> names = (geometry.get()->*namesGetter)();
    if (names != expectedNames)
    {
        return fail("group names were not preserved");
    }

    for (size_t groupIndex = 0; groupIndex < expectedNames.size(); ++groupIndex)
    {
        const auto membership =
            (geometry.get()->*membershipGetter)(expectedNames[groupIndex]);
        if (!membership)
            return fail("group membership lookup failed for " + expectedNames[groupIndex]);
        if (*membership != expectedMemberships[groupIndex])
        {
            return fail("group membership changed for " + expectedNames[groupIndex]);
        }
    }
    return 0;
}

int verifyGroups(const houio::HouGeo::Ptr& geometry)
{
    if (!geometry || geometry->pointCount() != 4 || geometry->vertexCount() != 6
        || geometry->primitiveCount() != 2)
    {
        return fail("group fixture counts are incorrect");
    }

    if (const int result = verifyGroupDomain(
            geometry, &houio::HouGeoAdapter::pointGroupNames,
            &houio::HouGeoAdapter::pointGroupMembership,
            {"left_points", "shared_points"}, {{true, false, false, true}, {true, false, true, false}});
        result != 0)
    {
        return result;
    }

    if (const int result = verifyGroupDomain(
            geometry, &houio::HouGeoAdapter::vertexGroupNames,
            &houio::HouGeoAdapter::vertexGroupMembership,
            {"first_face_vertices", "shared_point_vertices"},
            {{true, true, true, false, false, false}, {true, false, true, true, true, false}});
        result != 0)
    {
        return result;
    }

    return verifyGroupDomain(
        geometry, &houio::HouGeoAdapter::primitiveGroupNames,
        &houio::HouGeoAdapter::primitiveGroupMembership,
        {"left", "right"}, {{true, false}, {false, true}});
}

houio::HouGeo::Ptr createMixedPrimitiveGeometry()
{
    houio::HouGeo::Ptr geometry = houio::HouGeo::create();
    houio::Attribute::Ptr positions = houio::Attribute::createV4f();
    positions->appendElement(houio::math::V4f(0.0f, 0.0f, 0.0f, 1.0f));
    positions->appendElement(houio::math::V4f(1.0f, 0.0f, 0.0f, 1.0f));
    positions->appendElement(houio::math::V4f(0.0f, 1.0f, 0.0f, 1.0f));
    geometry->setPointAttribute(
        std::make_shared<houio::HouGeo::HouAttribute>("P", positions));

    auto topology = std::make_shared<houio::HouGeo::HouTopology>();
    topology->setIndices({0, 1, 2});
    geometry->setTopology(topology);

    auto polygon = std::make_shared<houio::HouGeo::HouPoly>();
    polygon->setPolygonData(1, {3}, {0}, {0, 1, 2}, true);
    geometry->addPrimitive(polygon);

    houio::ScalarField::Ptr field = houio::ScalarField::create(
        houio::math::V3i(1),
        houio::math::Box3f(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f),
        2.0f);
    geometry->addPrimitive(field);
    geometry->setPrimitiveGroup("surface", {true, false});
    geometry->setPrimitiveGroup("density", {false, true});
    return geometry;
}

int verifyMixedPrimitiveGroups(const houio::HouGeo::Ptr& geometry)
{
    if (!geometry || geometry->pointCount() != 4 || geometry->vertexCount() != 4
        || geometry->primitiveCount() != 2)
    {
        return fail("mixed primitive geometry counts are incorrect");
    }
    const std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives = geometry->primitives();
    if (primitives.size() != 2
        || !std::dynamic_pointer_cast<houio::HouGeoAdapter::PolyPrimitive>(primitives[0])
        || !std::dynamic_pointer_cast<houio::HouGeoAdapter::VolumePrimitive>(primitives[1]))
    {
        return fail("mixed primitive records were not preserved");
    }
    return verifyGroupDomain(
        geometry, &houio::HouGeoAdapter::primitiveGroupNames,
        &houio::HouGeoAdapter::primitiveGroupMembership,
        {"density", "surface"}, {{false, true}, {true, false}});
}

int verifyProgrammaticGroupValidation(const houio::HouGeo::Ptr& geometry)
{
    try
    {
        geometry->setPrimitiveGroup("", {true, false});
        return fail("primitive group accepted an empty name");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        geometry->setPrimitiveGroup("invalid", {true});
        return fail("primitive group accepted the wrong membership count");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        geometry->setPointGroup("invalid", {true});
        return fail("point group accepted the wrong membership count");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        geometry->setVertexGroup("invalid", {true});
        return fail("vertex group accepted the wrong membership count");
    }
    catch (const std::invalid_argument&)
    {
    }
    return 0;
}
}

int main()
{
    std::istringstream source(groupedGeometry());
    houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(source);
    if (const int result = verifyGroups(geometry); result != 0)
    {
        return result;
    }

    std::ostringstream binaryOutput(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::exportGeometry(binaryOutput, geometry, true))
    {
        return fail("failed to export grouped geometry");
    }

    std::istringstream binaryInput(binaryOutput.str(), std::ios::in | std::ios::binary);
    if (const int result = verifyGroups(houio::HouGeoIO::import(binaryInput)); result != 0)
        return result;

    houio::HouGeo::Ptr mixedGeometry = createMixedPrimitiveGeometry();
    if (const int result = verifyProgrammaticGroupValidation(mixedGeometry); result != 0)
        return result;
    if (const int result = verifyMixedPrimitiveGroups(mixedGeometry); result != 0)
        return result;

    std::ostringstream mixedOutput(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::exportGeometry(mixedOutput, mixedGeometry, true))
        return fail("failed to export mixed primitive groups");
    std::istringstream mixedInput(mixedOutput.str(), std::ios::in | std::ios::binary);
    return verifyMixedPrimitiveGroups(houio::HouGeoIO::import(mixedInput));
}
