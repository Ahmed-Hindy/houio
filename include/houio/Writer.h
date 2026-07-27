#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <houio/GeometryIO.h>

namespace houio
{
    enum class WriterDataType
    {
        houdini_geometry = 0,
        simplified_mesh = 1,
        dense_scalar_volume = 2,
        packed_geometry = 3,
        packed_fragment = 4,
        packed_disk = 5,
        sparse_openvdb = 6,
        packed_disk_sequence = 7
    };

    enum class WriterCapabilityLevel
    {
        supported,
        recognized,
        unavailable
    };

    struct WriterCapability
    {
        WriterDataType dataType = WriterDataType::houdini_geometry;
        WriterCapabilityLevel level = WriterCapabilityLevel::unavailable;
        bool readable = false;
        bool writable = false;
        std::string name;
        std::string detail;
    };

    using WriterSource = std::variant<
        std::monostate,
        HouGeoAdapter::Ptr,
        Geometry::Ptr,
        ScalarField::Ptr>;

    struct WriteRequest
    {
        std::filesystem::path destination;
        WriterSource source;
        GeometryWriteOptions options;
    };

    using WriteResult = GeometryWriteResult;

    /// Primary synchronous facade for custom HouIO serialization.
    ///
    /// The request owns its path and smart pointers. Adapter-exposed raw storage
    /// must remain valid until write() returns. No Houdini native file writer is
    /// invoked by this API.
    class Writer final
    {
    public:
        Writer() = delete;

        [[nodiscard]] static WriteResult write(const WriteRequest &request);

        [[nodiscard]] static WriteResult write(
            const std::filesystem::path &destination,
            const HouGeoAdapter::Ptr &geometry,
            const GeometryWriteOptions &options = {});
        [[nodiscard]] static WriteResult write(
            const std::filesystem::path &destination,
            const Geometry::Ptr &geometry,
            const GeometryWriteOptions &options = {});
        [[nodiscard]] static WriteResult write(
            const std::filesystem::path &destination,
            const ScalarField::Ptr &volume,
            const GeometryWriteOptions &options = {});

        [[nodiscard]] static const std::vector<WriterCapability> &capabilities() noexcept;
        [[nodiscard]] static std::optional<WriterCapability> capability(
            WriterDataType data_type);
    };
}
