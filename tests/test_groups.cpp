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

using GroupNamesGetter = void (houio::HouGeoAdapter::*)(std::vector<std::string>&) const;
using GroupMembershipGetter = bool (houio::HouGeoAdapter::*)(const std::string&, std::vector<bool>&) const;

int verifyGroupDomain(const houio::HouGeo::Ptr& geometry, GroupNamesGetter namesGetter,
                      GroupMembershipGetter membershipGetter, const std::vector<std::string>& expectedNames,
                      const std::vector<std::vector<bool>>& expectedMemberships)
{
    std::vector<std::string> names;
    (geometry.get()->*namesGetter)(names);
    if (names != expectedNames)
    {
        return fail("group names were not preserved");
    }

    for (size_t groupIndex = 0; groupIndex < expectedNames.size(); ++groupIndex)
    {
        std::vector<bool> membership;
        if (!(geometry.get()->*membershipGetter)(expectedNames[groupIndex], membership))
        {
            return fail("group membership lookup failed for " + expectedNames[groupIndex]);
        }
        if (membership != expectedMemberships[groupIndex])
        {
            return fail("group membership changed for " + expectedNames[groupIndex]);
        }
    }
    return 0;
}

int verifyGroups(const houio::HouGeo::Ptr& geometry)
{
    if (!geometry || geometry->pointcount() != 4 || geometry->vertexcount() != 6
        || geometry->primitivecount() != 2)
    {
        return fail("group fixture counts are incorrect");
    }

    if (const int result = verifyGroupDomain(
            geometry, &houio::HouGeoAdapter::getPointGroupNames,
            &houio::HouGeoAdapter::getPointGroupMembership,
            {"left_points", "shared_points"}, {{true, false, false, true}, {true, false, true, false}});
        result != 0)
    {
        return result;
    }

    if (const int result = verifyGroupDomain(
            geometry, &houio::HouGeoAdapter::getVertexGroupNames,
            &houio::HouGeoAdapter::getVertexGroupMembership,
            {"first_face_vertices", "shared_point_vertices"},
            {{true, true, true, false, false, false}, {true, false, true, true, true, false}});
        result != 0)
    {
        return result;
    }

    return verifyGroupDomain(
        geometry, &houio::HouGeoAdapter::getPrimitiveGroupNames,
        &houio::HouGeoAdapter::getPrimitiveGroupMembership,
        {"left", "right"}, {{true, false}, {false, true}});
}
}

int main()
{
    std::istringstream source(groupedGeometry());
    houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(&source);
    if (const int result = verifyGroups(geometry); result != 0)
    {
        return result;
    }

    std::ostringstream binaryOutput(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::xport(&binaryOutput, geometry, true))
    {
        return fail("failed to export grouped geometry");
    }

    std::istringstream binaryInput(binaryOutput.str(), std::ios::in | std::ios::binary);
    return verifyGroups(houio::HouGeoIO::import(&binaryInput));
}
