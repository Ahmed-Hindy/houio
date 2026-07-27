#pragma once

#include <filesystem>
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

        [[nodiscard]] static GeometryReadResult<std::vector<ubyte>> encodeFloatGrid(
            const SparseFloatGrid& grid);

        [[nodiscard]] static GeometryReadResult<SparseFloatGrid> decodeFloatGrid(
            std::span<const ubyte> openvdb_stream,
            const std::string& grid_name = {});
    };
}
