#include <houio/NativeSceneWriter.h>

#include <houio/SceneArchive.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

struct HouIONativeSceneArchive
{
    std::unique_ptr<houio::SceneArchiveWriter> writer;
    bool finished = false;
};

namespace
{
    void writeError(const std::string& message, char* buffer, std::size_t capacity) noexcept
    {
        if (buffer == nullptr || capacity == 0)
            return;
        const std::size_t copied = std::min(message.size(), capacity - 1U);
        if (copied != 0)
            std::memcpy(buffer, message.data(), copied);
        buffer[copied] = '\0';
    }

    void clearError(char* buffer, std::size_t capacity) noexcept
    {
        if (buffer != nullptr && capacity != 0)
            buffer[0] = '\0';
    }

    [[nodiscard]] std::filesystem::path utf8Path(const char* value)
    {
        if (value == nullptr || value[0] == '\0')
            throw std::invalid_argument("Native scene destination cannot be empty");
        return std::filesystem::path(reinterpret_cast<const char8_t*>(value));
    }

    [[nodiscard]] houio::SceneGeometrySample adaptSample(
        const HouIONativePolygonWriteRequest& request)
    {
        if (request.point_count != 0 && request.positions_xyzw == nullptr)
            throw std::invalid_argument("Native scene positions cannot be null");
        if (request.vertex_count != 0 && request.topology == nullptr)
            throw std::invalid_argument("Native scene topology cannot be null");
        if (request.polygon_count != 0 && request.polygons == nullptr)
            throw std::invalid_argument("Native scene polygon records cannot be null");

        houio::SceneGeometrySample sample;
        sample.positionsXyzw.reserve(request.point_count * 4U);
        for (std::size_t point_index = 0; point_index < request.point_count; ++point_index)
        {
            const std::size_t offset = point_index * 4U;
            for (std::size_t component = 0; component < 4U; ++component)
            {
                const float value = request.positions_xyzw[offset + component];
                if (!std::isfinite(value))
                    throw std::invalid_argument("Native scene positions must be finite");
                sample.positionsXyzw.push_back(value);
            }
        }

        sample.topology.reserve(request.vertex_count);
        for (std::size_t vertex_index = 0; vertex_index < request.vertex_count; ++vertex_index)
        {
            const std::int32_t point_index = request.topology[vertex_index];
            if (point_index < 0
                || static_cast<std::size_t>(point_index) >= request.point_count)
            {
                throw std::out_of_range(
                    "Native scene topology references a point outside the detail");
            }
            sample.topology.push_back(point_index);
        }

        std::size_t expected_offset = 0;
        sample.polygons.reserve(request.polygon_count);
        for (std::size_t polygon_index = 0;
             polygon_index < request.polygon_count;
             ++polygon_index)
        {
            const HouIONativePolygon& polygon = request.polygons[polygon_index];
            if (polygon.vertex_offset != expected_offset)
            {
                throw std::invalid_argument(
                    "Native scene polygon ranges must form a contiguous partition");
            }
            if (polygon.vertex_offset > request.vertex_count
                || polygon.vertex_count > request.vertex_count - polygon.vertex_offset)
            {
                throw std::out_of_range(
                    "Native scene polygon range exceeds the topology domain");
            }
            const std::size_t minimum = polygon.closed != 0 ? 3U : 2U;
            if (polygon.vertex_count < minimum)
            {
                throw std::invalid_argument(
                    polygon.closed != 0
                        ? "Native scene closed polygon requires at least three vertices"
                        : "Native scene open polyline requires at least two vertices");
            }
            sample.polygons.push_back(houio::ScenePolygon{
                polygon.vertex_offset,
                polygon.vertex_count,
                polygon.closed != 0});
            expected_offset += polygon.vertex_count;
        }
        if (expected_offset != request.vertex_count)
        {
            throw std::invalid_argument(
                "Native scene polygon records do not cover the topology domain");
        }
        return sample;
    }
}

extern "C" HouIONativeSceneArchive* houio_create_native_scene_archive(
    const HouIONativeSceneArchiveOptions* options,
    char* error_buffer,
    std::size_t error_capacity)
{
    clearError(error_buffer, error_capacity);
    try
    {
        if (options == nullptr)
            throw std::invalid_argument("Native scene archive options cannot be null");
        houio::SceneArchiveOptions scene_options;
        scene_options.destination = utf8Path(options->destination_utf8);
        scene_options.framesPerSecond = options->frames_per_second;
        scene_options.startFrame = options->start_frame;
        scene_options.createParentDirectories = options->create_parent_directories != 0;
        scene_options.overwriteExisting = options->overwrite_existing != 0;
        scene_options.atomicReplace = options->atomic_replace != 0;

        auto archive = std::make_unique<HouIONativeSceneArchive>();
        archive->writer = houio::createSceneArchiveWriter(scene_options);
        return archive.release();
    }
    catch (const std::exception& exception)
    {
        writeError(exception.what(), error_buffer, error_capacity);
        return nullptr;
    }
    catch (...)
    {
        writeError(
            "HouIO native scene archive creation failed with an unknown error",
            error_buffer,
            error_capacity);
        return nullptr;
    }
}

extern "C" int houio_write_native_scene_sample(
    HouIONativeSceneArchive* archive,
    const HouIONativePolygonWriteRequest* sample,
    double frame,
    char* error_buffer,
    std::size_t error_capacity)
{
    clearError(error_buffer, error_capacity);
    try
    {
        if (archive == nullptr || !archive->writer)
            throw std::invalid_argument("Native scene archive handle is invalid");
        if (archive->finished)
            throw std::logic_error("Native scene archive is already finished");
        if (sample == nullptr)
            throw std::invalid_argument("Native scene sample cannot be null");
        if (!std::isfinite(frame))
            throw std::invalid_argument("Native scene frame must be finite");
        archive->writer->writeSample(adaptSample(*sample), frame);
        return 0;
    }
    catch (const std::exception& exception)
    {
        writeError(exception.what(), error_buffer, error_capacity);
        return 1;
    }
    catch (...)
    {
        writeError(
            "HouIO native scene sample write failed with an unknown error",
            error_buffer,
            error_capacity);
        return 2;
    }
}

extern "C" int houio_finish_native_scene_archive(
    HouIONativeSceneArchive* archive,
    char* error_buffer,
    std::size_t error_capacity)
{
    clearError(error_buffer, error_capacity);
    try
    {
        if (archive == nullptr || !archive->writer)
            throw std::invalid_argument("Native scene archive handle is invalid");
        if (archive->finished)
            throw std::logic_error("Native scene archive is already finished");
        archive->writer->finish();
        archive->finished = true;
        archive->writer.reset();
        return 0;
    }
    catch (const std::exception& exception)
    {
        writeError(exception.what(), error_buffer, error_capacity);
        return 1;
    }
    catch (...)
    {
        writeError(
            "HouIO native scene archive finalization failed with an unknown error",
            error_buffer,
            error_capacity);
        return 2;
    }
}

extern "C" void houio_destroy_native_scene_archive(HouIONativeSceneArchive* archive)
{
    const std::unique_ptr<HouIONativeSceneArchive> owner(archive);
}
