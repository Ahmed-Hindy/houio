#include <houio/NativePolygonWriter.h>

#include <houio/Attribute.h>
#include <houio/GeometryIO.h>
#include <houio/HouGeo.h>
#include <houio/math/Math.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void writeError(const std::string& message, char* buffer, std::size_t capacity) noexcept
    {
        if (buffer == nullptr || capacity == 0)
            return;
        const std::size_t copied = std::min(message.size(), capacity - 1);
        if (copied != 0)
            std::memcpy(buffer, message.data(), copied);
        buffer[copied] = '\0';
    }

    [[nodiscard]] int checkedInt(std::size_t value, const char* description)
    {
        if (value > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::overflow_error(
                std::string(description) + " exceeds HouIO's int range");
        }
        return static_cast<int>(value);
    }

    [[nodiscard]] std::string diagnosticText(const houio::GeometryWriteResult& result)
    {
        std::ostringstream output;
        for (const houio::Diagnostic& diagnostic : result.diagnostics)
        {
            if (!diagnostic.path.empty())
                output << diagnostic.path << ": ";
            output << diagnostic.message << '\n';
        }
        std::string text = output.str();
        if (!text.empty())
            text.pop_back();
        return text.empty() ? "HouIO failed without a diagnostic" : text;
    }

    [[nodiscard]] houio::HouGeo::Ptr adaptRequest(
        const HouIONativePolygonWriteRequest& request)
    {
        if (request.destination_utf8 == nullptr || request.destination_utf8[0] == '\0')
            throw std::invalid_argument("Native polygon destination cannot be empty");
        if (request.point_count != 0 && request.positions_xyzw == nullptr)
            throw std::invalid_argument("Native polygon positions cannot be null");
        if (request.vertex_count != 0 && request.topology == nullptr)
            throw std::invalid_argument("Native polygon topology cannot be null");
        if (request.polygon_count != 0 && request.polygons == nullptr)
            throw std::invalid_argument("Native polygon records cannot be null");

        const int point_count = checkedInt(request.point_count, "Native point count");
        static_cast<void>(checkedInt(request.vertex_count, "Native vertex count"));
        static_cast<void>(checkedInt(request.polygon_count, "Native polygon count"));

        auto geometry = houio::HouGeo::create();
        auto positions = houio::Attribute::createV4f(point_count);
        for (std::size_t point_index = 0; point_index < request.point_count; ++point_index)
        {
            const std::size_t scalar_offset = point_index * 4;
            const float x = request.positions_xyzw[scalar_offset];
            const float y = request.positions_xyzw[scalar_offset + 1];
            const float z = request.positions_xyzw[scalar_offset + 2];
            const float w = request.positions_xyzw[scalar_offset + 3];
            if (!std::isfinite(x)
                || !std::isfinite(y)
                || !std::isfinite(z)
                || !std::isfinite(w))
            {
                throw std::invalid_argument(
                    "Native polygon positions must contain only finite values");
            }
            positions->set<houio::math::V4f>(
                static_cast<unsigned int>(point_index),
                houio::math::V4f(x, y, z, w));
        }
        geometry->setPointAttribute(
            std::make_shared<houio::HouGeo::HouAttribute>("P", std::move(positions)));

        auto topology = std::make_shared<houio::HouGeo::HouTopology>();
        topology->reserve(request.vertex_count);
        std::vector<int> topology_indices;
        topology_indices.reserve(request.vertex_count);
        for (std::size_t vertex_index = 0; vertex_index < request.vertex_count; ++vertex_index)
        {
            const std::int32_t point_index = request.topology[vertex_index];
            if (point_index < 0 || point_index >= point_count)
            {
                throw std::out_of_range(
                    "Native polygon topology references a point outside the detail");
            }
            topology_indices.push_back(static_cast<int>(point_index));
        }
        topology->appendIndices(topology_indices);
        geometry->setTopology(std::move(topology));

        std::size_t expected_vertex_offset = 0;
        for (std::size_t polygon_index = 0;
             polygon_index < request.polygon_count;
             ++polygon_index)
        {
            const HouIONativePolygon& input_polygon = request.polygons[polygon_index];
            if (input_polygon.vertex_offset != expected_vertex_offset)
            {
                throw std::invalid_argument(
                    "Native polygon ranges must form one contiguous topology partition");
            }
            if (input_polygon.vertex_count == 0
                || input_polygon.vertex_offset > request.vertex_count
                || input_polygon.vertex_count
                    > request.vertex_count - input_polygon.vertex_offset)
            {
                throw std::out_of_range(
                    "Native polygon range exceeds the topology domain");
            }

            const int polygon_vertex_count = checkedInt(
                input_polygon.vertex_count,
                "Native polygon vertex count");
            const auto begin = topology_indices.begin()
                + static_cast<std::ptrdiff_t>(input_polygon.vertex_offset);
            const auto end = begin + static_cast<std::ptrdiff_t>(input_polygon.vertex_count);
            std::vector<int> point_indices(begin, end);

            auto polygon = std::make_shared<houio::HouGeo::HouPoly>();
            polygon->setPolygonData(
                1,
                {polygon_vertex_count},
                {0},
                std::move(point_indices),
                input_polygon.closed != 0);
            geometry->addPrimitive(
                std::static_pointer_cast<houio::HouGeoAdapter::PolyPrimitive>(polygon));
            expected_vertex_offset += input_polygon.vertex_count;
        }
        if (expected_vertex_offset != request.vertex_count)
        {
            throw std::invalid_argument(
                "Native polygon records do not cover the complete topology domain");
        }
        return geometry;
    }
}

extern "C" int houio_write_native_polygons(
    const HouIONativePolygonWriteRequest* request,
    char* error_buffer,
    std::size_t error_capacity)
{
    if (error_buffer != nullptr && error_capacity != 0)
        error_buffer[0] = '\0';

    try
    {
        if (request == nullptr)
            throw std::invalid_argument("Native polygon write request cannot be null");

        const houio::HouGeo::Ptr geometry = adaptRequest(*request);
        houio::GeometryWriteOptions options;
        options.createParentDirectories = request->create_parent_directories != 0;
        options.overwriteExisting = request->overwrite_existing != 0;
        options.atomicReplace = request->atomic_replace != 0;
        if (request->blosc_library_utf8 != nullptr)
            options.bloscLibraryPath = request->blosc_library_utf8;

        const auto* destination_utf8 = reinterpret_cast<const char8_t*>(
            request->destination_utf8);
        const houio::GeometryWriteResult result = houio::GeometryIO::writeHouGeo(
            std::filesystem::path(destination_utf8),
            geometry,
            options);
        if (!result)
            throw std::runtime_error(diagnosticText(result));
        return 0;
    }
    catch (const std::exception& exception)
    {
        writeError(exception.what(), error_buffer, error_capacity);
        return 1;
    }
    catch (...)
    {
        writeError("HouIO native polygon writer failed with an unknown error",
            error_buffer,
            error_capacity);
        return 2;
    }
}
