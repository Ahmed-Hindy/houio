#pragma once

#include "HoudiniGeometryAdapter.h"

#include <memory>
#include <string>

namespace houio::hdk
{
    enum class NativeOutputFormat
    {
        bgeo,
        alembic,
        usd,
        unsupported,
    };

    struct SceneArchiveOptions
    {
        std::string destination;
        double frames_per_second = 24.0;
        double start_frame = 1.0;
        bool create_parent_directories = true;
        bool overwrite_existing = true;
        bool atomic_replace = true;
    };

    class SceneArchiveWriter
    {
    public:
        virtual ~SceneArchiveWriter() = default;

        virtual void writeSample(
            const NativePolygonDetail& geometry,
            double frame) = 0;
        virtual void finish() = 0;
    };

    [[nodiscard]] NativeOutputFormat detectNativeOutputFormat(
        const std::string& path);

    [[nodiscard]] std::unique_ptr<SceneArchiveWriter> createSceneArchiveWriter(
        NativeOutputFormat format,
        const SceneArchiveOptions& options);
}
