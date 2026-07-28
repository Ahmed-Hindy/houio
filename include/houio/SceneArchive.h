#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace houio
{
    enum class SceneArchiveFormat
    {
        automatic,
        alembic,
        usd,
        unsupported,
    };

    struct ScenePolygon
    {
        std::size_t vertexOffset = 0;
        std::size_t vertexCount = 0;
        bool closed = true;
    };

    /// Dependency-neutral polygon and polyline sample used by scene writers.
    ///
    /// Positions contain four Float32 components per point. Topology entries are
    /// zero-based point indices. Polygon ranges must form one contiguous
    /// partition of the topology array.
    struct SceneGeometrySample
    {
        std::vector<float> positionsXyzw;
        std::vector<std::int32_t> topology;
        std::vector<ScenePolygon> polygons;
    };

    struct SceneArchiveOptions
    {
        std::filesystem::path destination;
        SceneArchiveFormat format = SceneArchiveFormat::automatic;
        double framesPerSecond = 24.0;
        double startFrame = 1.0;
        bool createParentDirectories = true;
        bool overwriteExisting = true;
        bool atomicReplace = true;
    };

    /// Stateful writer for one animated Alembic or USD archive.
    class SceneArchiveWriter
    {
    public:
        virtual ~SceneArchiveWriter() = default;

        virtual void writeSample(const SceneGeometrySample& geometry, double frame) = 0;
        virtual void finish() = 0;
    };

    [[nodiscard]] SceneArchiveFormat detectSceneArchiveFormat(
        const std::filesystem::path& path);

    [[nodiscard]] bool sceneArchiveFormatAvailable(SceneArchiveFormat format) noexcept;

    [[nodiscard]] std::unique_ptr<SceneArchiveWriter> createSceneArchiveWriter(
        const SceneArchiveOptions& options);
}
