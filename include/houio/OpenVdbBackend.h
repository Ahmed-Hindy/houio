#pragma once

#include <filesystem>
#include <string>

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
    };
}
