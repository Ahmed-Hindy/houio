#pragma once

#include <filesystem>
#include <ostream>
#include <span>
#include <string>
#include <vector>

#include <houio/GeometryIO.h>
#include <houio/SparseGrid.h>

namespace houio
{
    struct OpenVdbBackendInfo
    {
        bool compiled = false;
        std::string version;
        std::string detail;
    };

    class OpenVdbBackend final
    {
    public:
        OpenVdbBackend() = delete;

        [[nodiscard]] static OpenVdbBackendInfo info();

        [[nodiscard]] static GeometryReadResult<SparseFloatGrid> readFloatGrid(
            const std::filesystem::path& path,
            const std::string& grid_name = {});

        [[nodiscard]] static GeometryWriteResult writeFloatGrid(
            const std::filesystem::path& path,
            const SparseFloatGrid& grid,
            bool overwrite_existing = true,
            bool create_parent_directories = true);

        [[nodiscard]] static GeometryWriteResult encodeFloatGrid(
            std::ostream& output,
            const SparseFloatGrid& grid);

        [[nodiscard]] static GeometryReadResult<std::vector<ubyte>> encodeFloatGrid(
            const SparseFloatGrid& grid);

        [[nodiscard]] static GeometryReadResult<SparseFloatGrid> decodeFloatGrid(
            std::span<const ubyte> openvdb_stream,
            const std::string& grid_name = {});

        [[nodiscard]] static GeometryReadResult<SparseInt32Grid> readInt32Grid(
            const std::filesystem::path& path,
            const std::string& grid_name = {});

        [[nodiscard]] static GeometryWriteResult writeInt32Grid(
            const std::filesystem::path& path,
            const SparseInt32Grid& grid,
            bool overwrite_existing = true,
            bool create_parent_directories = true);

        [[nodiscard]] static GeometryWriteResult encodeInt32Grid(
            std::ostream& output,
            const SparseInt32Grid& grid);

        [[nodiscard]] static GeometryReadResult<std::vector<ubyte>> encodeInt32Grid(
            const SparseInt32Grid& grid);

        [[nodiscard]] static GeometryReadResult<SparseInt32Grid> decodeInt32Grid(
            std::span<const ubyte> openvdb_stream,
            const std::string& grid_name = {});
    };
}
