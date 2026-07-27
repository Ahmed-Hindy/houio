#include <houio/Writer.h>

#include <type_traits>
#include <utility>

namespace houio
{
    namespace
    {
        GeometryWriteResult invalidRequest(std::string message, std::string path)
        {
            GeometryWriteResult result;
            result.diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::malformed_input,
                std::move(message),
                -1,
                std::move(path)});
            return result;
        }
    }

    WriteResult Writer::write(const WriteRequest &request)
    {
        if( request.destination.empty() )
            return invalidRequest("Writer destination cannot be empty", "request.destination");

        return std::visit(
            [&request](const auto &source) -> WriteResult
            {
                using Source = std::decay_t<decltype(source)>;
                if constexpr( std::is_same_v<Source, std::monostate> )
                {
                    return invalidRequest("Writer source cannot be empty", "request.source");
                }
                else if constexpr( std::is_same_v<Source, HouGeoAdapter::Ptr> )
                {
                    return GeometryIO::writeHouGeo(
                        request.destination, source, request.options);
                }
                else if constexpr( std::is_same_v<Source, Geometry::Ptr> )
                {
                    return GeometryIO::writeGeometry(
                        request.destination, source, request.options);
                }
                else
                {
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
            {WriterDataType::sparse_openvdb, WriterCapabilityLevel::supported,
                true, true, "sparse_openvdb",
                "Lossless opaque Houdini VDB payload pass-through in every build; SparseFloatGrid and SparseInt32Grid voxel and active-tile construction are serialized as native Houdini VDB records when the optional OpenVDB backend is compiled."}
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
