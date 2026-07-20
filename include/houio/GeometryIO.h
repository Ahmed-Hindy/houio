#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include <houio/Diagnostic.h>
#include <houio/Geometry.h>
#include <houio/HouGeo.h>
#include <houio/json.h>

namespace houio
{
    /// File containers recognized by the path-based geometry API.
    ///
    /// OpenVDB is detected so callers receive an actionable diagnostic. Native
    /// sparse-grid loading is not part of the standalone library; use the HOM
    /// bridge when running inside Houdini.
    enum class GeometryFileFormat
    {
        automatic,
        geo_ascii,
        bgeo_binary,
        bgeo_scf,
        openvdb,
        unknown
    };

    /// Limits and backend selection for a single synchronous read operation.
    ///
    /// The options object is copied by the caller as needed. HouIO never stores
    /// references to it or to the strings it contains.
    struct GeometryReadOptions
    {
        GeometryFileFormat format = GeometryFileFormat::automatic;
        json::ParserLimits parserLimits;
        /// Maximum bytes in the on-disk container before optional SCF decompression.
        std::size_t maxFileBytes = 512ULL * 1024ULL * 1024ULL;
        std::string bloscLibraryPath;
    };

    /// Format and SCF compression settings for a synchronous write operation.
    ///
    /// SCF compression dynamically loads C-Blosc. An empty library path uses
    /// HOUIO_BLOSC_LIBRARY, the active Houdini installation, or platform names.
    struct GeometryWriteOptions
    {
        GeometryFileFormat format = GeometryFileFormat::automatic;
        std::size_t scfBlockBytes = 1024ULL * 1024ULL;
        int scfCompressionLevel = 9;
        bool scfShuffle = true;
        std::string scfCompressor = "blosclz";
        std::string bloscLibraryPath;
    };

    /// Owned value and diagnostics returned by a path-based read.
    ///
    /// No member refers to the input stream, compressed file buffer, or parser
    /// tree. Shared-pointer values own the parsed objects after this result is
    /// moved or destroyed. `succeeded` distinguishes an empty-but-valid generic
    /// value from a failed operation.
    template<typename T>
    struct GeometryReadResult
    {
        T value{};
        DiagnosticList diagnostics;
        bool succeeded = false;

        [[nodiscard]] bool hasErrors() const noexcept
        {
            for( const Diagnostic &diagnostic : diagnostics )
            {
                if( diagnostic.severity == DiagnosticSeverity::error )
                    return true;
            }
            return false;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return succeeded && !hasErrors();
        }
    };

    /// Owned status and diagnostics returned by a path-based write.
    struct GeometryWriteResult
    {
        bool succeeded = false;
        DiagnosticList diagnostics;

        [[nodiscard]] bool hasErrors() const noexcept
        {
            for( const Diagnostic &diagnostic : diagnostics )
            {
                if( diagnostic.severity == DiagnosticSeverity::error )
                    return true;
            }
            return false;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return succeeded && !hasErrors();
        }
    };

    /// Preferred path-based facade for Houdini geometry files.
    ///
    /// All operations are synchronous. Input files may be closed or replaced
    /// after a read returns. Adapter-backed writes consume raw pointers only
    /// during the call; the caller must keep the adapter and its exposed memory
    /// valid until the function returns. Independent calls do not share writer
    /// state and may operate concurrently on different files.
    class GeometryIO final
    {
    public:
        GeometryIO() = delete;

        /// Detect a format from file magic when readable, otherwise extension.
        [[nodiscard]] static GeometryFileFormat detectFormat(const std::filesystem::path &path);

        /// Read the supported Houdini-oriented representation without lossy
        /// conversion. Supports ASCII .geo, binary .bgeo, and SCF .bgeo.sc.
        /// Invalid options and file failures are returned as diagnostics.
        [[nodiscard]] static GeometryReadResult<HouGeo::Ptr> readHouGeo(
            const std::filesystem::path &path, const GeometryReadOptions &options = {});
        /// Read the first supported polygon representation into the lossy
        /// render-oriented Geometry model.
        [[nodiscard]] static GeometryReadResult<Geometry::Ptr> readGeometry(
            const std::filesystem::path &path, const GeometryReadOptions &options = {});
        /// Return the first dense scalar volume. A warning is included when
        /// additional volumes are present.
        [[nodiscard]] static GeometryReadResult<ScalarField::Ptr> readVolume(
            const std::filesystem::path &path, const GeometryReadOptions &options = {});
        /// Return every primitive when all primitives are supported dense
        /// scalar volumes; mixed primitive sets fail rather than dropping data.
        [[nodiscard]] static GeometryReadResult<std::vector<ScalarField::Ptr>> readVolumes(
            const std::filesystem::path &path, const GeometryReadOptions &options = {});

        /// Write a HouGeoAdapter synchronously. Raw adapter memory must remain
        /// valid for the duration of this call.
        [[nodiscard]] static GeometryWriteResult writeHouGeo(
            const std::filesystem::path &path, const HouGeoAdapter::Ptr &geometry,
            const GeometryWriteOptions &options = {});
        /// Adapt and write the simplified Geometry model.
        [[nodiscard]] static GeometryWriteResult writeGeometry(
            const std::filesystem::path &path, const Geometry::Ptr &geometry,
            const GeometryWriteOptions &options = {});
        /// Adapt and write one dense scalar volume.
        [[nodiscard]] static GeometryWriteResult writeVolume(
            const std::filesystem::path &path, const ScalarField::Ptr &volume,
            const GeometryWriteOptions &options = {});
    };
}
