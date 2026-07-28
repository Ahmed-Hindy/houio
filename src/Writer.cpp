#include <houio/Writer.h>

#include <houio/SceneArchive.h>
#include <houio/SceneGeometryAdapter.h>

#include <exception>
#include <type_traits>
#include <utility>

namespace houio
{
    namespace
    {
        GeometryWriteResult failure(
            DiagnosticCategory category,
            std::string message,
            std::string path)
        {
            GeometryWriteResult result;
            result.diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::error,
                category,
                std::move(message),
                -1,
                std::move(path)});
            return result;
        }

        GeometryWriteResult invalidRequest(std::string message, std::string path)
        {
            return failure(
                DiagnosticCategory::malformed_input,
                std::move(message),
                std::move(path));
        }

        GeometryWriteResult writeSceneArchive(
            const std::filesystem::path& destination,
            const SceneGeometrySample& geometry,
            const GeometryWriteOptions& write_options)
        {
            const SceneArchiveFormat format = detectSceneArchiveFormat(destination);
            if (format == SceneArchiveFormat::unsupported)
            {
                return failure(
                    DiagnosticCategory::unsupported_input,
                    "Scene writer destination must use .abc, .usd, .usda, or .usdc",
                    "request.destination");
            }
            if (!sceneArchiveFormatAvailable(format))
            {
                return failure(
                    DiagnosticCategory::unsupported_input,
                    format == SceneArchiveFormat::alembic
                        ? "HouIO was built without Alembic writer support"
                        : "HouIO was built without USD writer support",
                    "request.destination");
            }

            try
            {
                SceneArchiveOptions options;
                options.destination = destination;
                options.format = format;
                options.createParentDirectories = write_options.createParentDirectories;
                options.overwriteExisting = write_options.overwriteExisting;
                options.atomicReplace = write_options.atomicReplace;

                std::unique_ptr<SceneArchiveWriter> writer =
                    createSceneArchiveWriter(options);
                writer->writeSample(geometry, 1.0);
                writer->finish();

                GeometryWriteResult result;
                result.succeeded = true;
                return result;
            }
            catch (const std::exception& exception)
            {
                return failure(
                    DiagnosticCategory::conversion,
                    exception.what(),
                    "scene_archive");
            }
            catch (...)
            {
                return failure(
                    DiagnosticCategory::conversion,
                    "Scene archive writer failed with an unknown error",
                    "scene_archive");
            }
        }
    }

    WriteResult Writer::write(const WriteRequest &request)
    {
        if( request.destination.empty() )
            return invalidRequest("Writer destination cannot be empty", "request.destination");

        const SceneArchiveFormat scene_format =
            detectSceneArchiveFormat(request.destination);
        return std::visit(
            [&request, scene_format](const auto &source) -> WriteResult
            {
                using Source = std::decay_t<decltype(source)>;
                if constexpr( std::is_same_v<Source, std::monostate> )
                {
                    return invalidRequest("Writer source cannot be empty", "request.source");
                }
                else if constexpr( std::is_same_v<Source, HouGeoAdapter::Ptr> )
                {
                    if (scene_format != SceneArchiveFormat::unsupported)
                    {
                        if (!source)
                            return invalidRequest(
                                "Scene writer geometry cannot be null",
                                "request.source");
                        try
                        {
                            return writeSceneArchive(
                                request.destination,
                                adaptSceneGeometry(*source),
                                request.options);
                        }
                        catch (const std::exception& exception)
                        {
                            return failure(
                                DiagnosticCategory::conversion,
                                exception.what(),
                                "scene_adapter");
                        }
                    }
                    return GeometryIO::writeHouGeo(
                        request.destination, source, request.options);
                }
                else if constexpr( std::is_same_v<Source, Geometry::Ptr> )
                {
                    if (scene_format != SceneArchiveFormat::unsupported)
                    {
                        if (!source)
                            return invalidRequest(
                                "Scene writer geometry cannot be null",
                                "request.source");
                        try
                        {
                            return writeSceneArchive(
                                request.destination,
                                adaptSceneGeometry(*source),
                                request.options);
                        }
                        catch (const std::exception& exception)
                        {
                            return failure(
                                DiagnosticCategory::conversion,
                                exception.what(),
                                "scene_adapter");
                        }
                    }
                    return GeometryIO::writeGeometry(
                        request.destination, source, request.options);
                }
                else
                {
                    if (scene_format != SceneArchiveFormat::unsupported)
                    {
                        return failure(
                            DiagnosticCategory::unsupported_input,
                            "Alembic and USD scene export does not yet support volume sources",
                            "request.source");
                    }
                    return GeometryIO::writeVolume(
                        request.destination, source, request.options);
                }
            },
            request.source);
    }

    WriteResult Writer::write(const std::filesystem::path &destination,
        const HouGeoAdapter::Ptr &geometry, const GeometryWriteOptions &options)
    {
        return write(WriteRequest{destination, geometry, options});
    }

    WriteResult Writer::write(const std::filesystem::path &destination,
        const Geometry::Ptr &geometry, const GeometryWriteOptions &options)
    {
        return write(WriteRequest{destination, geometry, options});
    }

    WriteResult Writer::write(const std::filesystem::path &destination,
        const ScalarField::Ptr &volume, const GeometryWriteOptions &options)
    {
        return write(WriteRequest{destination, volume, options});
    }

    const std::vector<WriterCapability> &Writer::capabilities() noexcept
    {
        static const std::vector<WriterCapability> values{
            {WriterDataType::houdini_geometry, WriterCapabilityLevel::supported,
                true, true, "houdini_geometry",
                "Faithful HouGeo/HouGeoAdapter geometry, attributes, groups, polygons, and dense volumes."},
            {WriterDataType::simplified_mesh, WriterCapabilityLevel::supported,
                true, true, "simplified_mesh",
                "Convenience mesh model adapted through HouIO's custom writer."},
            {WriterDataType::dense_scalar_volume, WriterCapabilityLevel::supported,
                true, true, "dense_scalar_volume",
                "Dense Float32 scalar Houdini Volume primitive."},
            {WriterDataType::packed_geometry, WriterCapabilityLevel::supported,
                true, true, "packed_geometry",
                "Embedded PackedGeometry records with shared HouGeo payload, transform, pivot, and flags."},
            {WriterDataType::packed_fragment, WriterCapabilityLevel::supported,
                true, true, "packed_fragment",
                "Embedded PackedFragment records with fragment identity, bounds, transform, and shared HouGeo payload."},
            {WriterDataType::packed_disk, WriterCapabilityLevel::supported,
                true, true, "packed_disk",
                "External PackedDisk references with authored filename, expansion policy, transform, pivot, and flags."},
            {WriterDataType::packed_disk_sequence, WriterCapabilityLevel::supported,
                true, true, "packed_disk_sequence",
                "Ordered PackedDiskSequence sample references with fractional index, wrap mode, transform, and viewport metadata."},
            {WriterDataType::curves, WriterCapabilityLevel::supported,
                true, true, "curves",
                "NURBS and Bezier curve records with topology, closure, order, knots, endpoint interpolation, and rational Pw attributes."},
            {WriterDataType::quadrics, WriterCapabilityLevel::supported,
                true, true, "quadrics",
                "Native Sphere and Tube records with topology, exact 3x3 transforms, tube caps, and taper."},
            {WriterDataType::sparse_openvdb, WriterCapabilityLevel::supported,
                true, true, "sparse_openvdb",
                "Lossless opaque Houdini VDB payload pass-through in every build; SparseFloatGrid, SparseInt32Grid, and SparseVec3fGrid voxel and active-tile construction are serialized as native Houdini VDB records when the optional OpenVDB backend is compiled."},
            {WriterDataType::alembic_scene,
                sceneArchiveFormatAvailable(SceneArchiveFormat::alembic)
                    ? WriterCapabilityLevel::supported
                    : WriterCapabilityLevel::unavailable,
                false,
                sceneArchiveFormatAvailable(SceneArchiveFormat::alembic),
                "alembic_scene",
                sceneArchiveFormatAvailable(SceneArchiveFormat::alembic)
                    ? "Animated polygon mesh and polygonal polyline archives written by HouIO's Alembic backend."
                    : "Alembic writer backend is not compiled into this HouIO build."},
            {WriterDataType::usd_scene,
                sceneArchiveFormatAvailable(SceneArchiveFormat::usd)
                    ? WriterCapabilityLevel::supported
                    : WriterCapabilityLevel::unavailable,
                false,
                sceneArchiveFormatAvailable(SceneArchiveFormat::usd),
                "usd_scene",
                sceneArchiveFormatAvailable(SceneArchiveFormat::usd)
                    ? "Animated polygon mesh and polygonal polyline stages written by HouIO's OpenUSD backend."
                    : "USD writer backend is not compiled into this HouIO build."}
        };
        return values;
    }

    std::optional<WriterCapability> Writer::capability(WriterDataType data_type)
    {
        for( const WriterCapability &value : capabilities() )
        {
            if( value.dataType == data_type )
                return value;
        }
        return std::nullopt;
    }
}
