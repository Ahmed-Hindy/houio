#include <houio/GeometryIO.h>
#include <houio/NativePolygonWriter.h>

#include "TestSupport.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>

namespace
{
using houio::test::fail;

HouIONativePolygonWriteRequest makeRequest(
    const std::array<float, 16>& positions,
    const std::array<std::int32_t, 6>& topology,
    const std::array<HouIONativePolygon, 2>& polygons,
    const std::string& destination)
{
    HouIONativePolygonWriteRequest request = {};
    request.positions_xyzw = positions.data();
    request.point_count = 4;
    request.topology = topology.data();
    request.vertex_count = topology.size();
    request.polygons = polygons.data();
    request.polygon_count = polygons.size();
    request.destination_utf8 = destination.c_str();
    request.create_parent_directories = 1;
    request.overwrite_existing = 1;
    request.atomic_replace = 1;
    return request;
}
}

int main()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "houio_native_polygon_writer";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const std::filesystem::path output_path = directory / "two_triangles.bgeo";
    const std::string output_text = output_path.string();

    const std::array<float, 16> positions = {
        0.0F, 0.0F, 0.0F, 1.0F,
        1.0F, 0.0F, 0.0F, 1.0F,
        1.0F, 1.0F, 0.0F, 1.0F,
        0.0F, 1.0F, 0.0F, 1.0F,
    };
    const std::array<std::int32_t, 6> topology = {0, 1, 2, 0, 2, 3};
    const std::array<HouIONativePolygon, 2> polygons = {
        HouIONativePolygon{0, 3, 1},
        HouIONativePolygon{3, 3, 1},
    };

    HouIONativePolygonWriteRequest request = makeRequest(
        positions,
        topology,
        polygons,
        output_text);
    std::array<char, 512> error = {};
    if (houio_write_native_polygons(&request, error.data(), error.size()) != 0)
        return fail(std::string("native polygon write failed: ") + error.data());

    const auto read_result = houio::GeometryIO::readHouGeo(output_path);
    if (!read_result
        || read_result.value->pointCount() != 4
        || read_result.value->vertexCount() != 6
        || read_result.value->primitiveCount() != 2)
    {
        return fail("native polygon write changed geometry counts");
    }
    const auto read_topology = read_result.value->topology();
    const std::span<const int> read_indices = read_topology
        ? read_topology->indexView()
        : std::span<const int>();
    if (read_indices.size() != topology.size())
        return fail("native polygon write did not preserve topology");
    for (std::size_t index = 0; index < topology.size(); ++index)
    {
        if (read_indices[index] != topology[index])
            return fail("native polygon write changed a topology index");
    }

    std::array<std::int32_t, 6> invalid_topology = topology;
    invalid_topology[5] = 9;
    request.topology = invalid_topology.data();
    error.fill('\0');
    if (houio_write_native_polygons(&request, error.data(), error.size()) == 0
        || std::string(error.data()).find("outside the detail") == std::string::npos)
    {
        return fail("native polygon writer did not reject invalid topology");
    }

    request.topology = topology.data();
    std::array<float, 16> invalid_positions = positions;
    invalid_positions[2] = std::numeric_limits<float>::infinity();
    request.positions_xyzw = invalid_positions.data();
    error.fill('\0');
    if (houio_write_native_polygons(&request, error.data(), error.size()) == 0
        || std::string(error.data()).find("finite values") == std::string::npos)
    {
        return fail("native polygon writer accepted a non-finite position");
    }

    request.positions_xyzw = positions.data();
    request.polygon_count = 1;
    error.fill('\0');
    if (houio_write_native_polygons(&request, error.data(), error.size()) == 0
        || std::string(error.data()).find("complete topology") == std::string::npos)
    {
        return fail("native polygon writer accepted incomplete polygon ranges");
    }

    std::filesystem::remove_all(directory);
    return 0;
}
