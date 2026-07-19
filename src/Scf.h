#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include <houio/Diagnostic.h>

namespace houio::detail
{
    struct ScfReadOptions
    {
        std::size_t maxOutputBytes = 512ULL * 1024ULL * 1024ULL;
        std::string bloscLibraryPath;
    };

    struct ScfWriteOptions
    {
        std::size_t blockBytes = 1024ULL * 1024ULL;
        int compressionLevel = 9;
        bool shuffle = true;
        std::string compressor = "blosclz";
        std::string bloscLibraryPath;
    };

    [[nodiscard]] bool hasScfMagic(std::span<const char> input) noexcept;
    [[nodiscard]] bool decompressScf(std::span<const char> input, std::vector<char> &output,
        const ScfReadOptions &options, DiagnosticList *diagnostics);
    [[nodiscard]] bool compressScf(std::span<const char> input, std::vector<char> &output,
        const ScfWriteOptions &options, DiagnosticList *diagnostics);
}
