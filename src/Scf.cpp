#include "Scf.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace houio::detail
{
    namespace
    {
        constexpr std::string_view scfHeaderMagic = "scf1";
        constexpr std::string_view scfFooterMagic = "1fcs";
        constexpr std::size_t scfPrefixBytes = 12;
        constexpr std::size_t scfSuffixBytes = 12;
        constexpr std::size_t bloscHeaderBytes = 16;
        constexpr std::size_t bloscMaximumOverhead = 16;

        using BloscCompressFunction = int (*)(int, int, std::size_t, std::size_t, const void*, void*,
            std::size_t, const char*, std::size_t, int);
        using BloscDecompressFunction = int (*)(const void*, void*, std::size_t, int);
        using BloscBufferSizesFunction = void (*)(const void*, std::size_t*, std::size_t*, std::size_t*);

        void appendError(DiagnosticList *diagnostics, DiagnosticCategory category, std::string message,
            sint64 byteOffset = -1)
        {
            appendDiagnostic(diagnostics, Diagnostic{
                DiagnosticSeverity::error, category, std::move(message), byteOffset, "wrapper.scf"});
        }

        std::uint32_t readLittleUInt32(std::span<const char> input, std::size_t offset)
        {
            const auto byte = [&](std::size_t index)
            {
                return static_cast<std::uint32_t>(
                    static_cast<unsigned char>(input[offset + index]));
            };
            return byte(0)
                | (byte(1) << 8U)
                | (byte(2) << 16U)
                | (byte(3) << 24U);
        }

        std::uint64_t readBigUInt64(std::span<const char> input, std::size_t offset)
        {
            std::uint64_t value = 0;
            for (std::size_t byte_index = 0; byte_index < 8; ++byte_index)
            {
                const auto byte = static_cast<unsigned char>(input[offset + byte_index]);
                value = (value << 8U) | static_cast<std::uint64_t>(byte);
            }
            return value;
        }

        void appendBigUInt64(std::vector<char> &output, std::uint64_t value)
        {
            for( int shift = 56; shift >= 0; shift -= 8 )
                output.push_back(static_cast<char>((value >> static_cast<unsigned int>(shift)) & 0xffU));
        }

        bool checkedAdd(std::size_t left, std::size_t right, std::size_t &result)
        {
            if( left > std::numeric_limits<std::size_t>::max() - right )
                return false;
            result = left + right;
            return true;
        }

        bool checkedMultiply(std::size_t left, std::size_t right, std::size_t &result)
        {
            if( left != 0 && right > std::numeric_limits<std::size_t>::max() / left )
                return false;
            result = left * right;
            return true;
        }

        std::string environmentValue(const char* name)
        {
#if defined(_WIN32)
            std::size_t required_bytes = 0;
            if (getenv_s(&required_bytes, nullptr, 0, name) != 0 || required_bytes == 0)
                return {};
            std::string value(required_bytes - 1, '\0');
            if (getenv_s(&required_bytes, value.data(), value.size() + 1, name) != 0)
                return {};
            return value;
#else
            const char* value = std::getenv(name);
            return value ? std::string(value) : std::string();
#endif
        }

#if defined(_WIN32)
        std::string windowsErrorMessage(DWORD error_code)
        {
            if (error_code == ERROR_SUCCESS)
                return "unknown Windows loader error";

            std::array<char, 1024> message_buffer{};
            const DWORD message_length = FormatMessageA(
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                error_code,
                0,
                message_buffer.data(),
                static_cast<DWORD>(message_buffer.size()),
                nullptr);
            std::string message = message_length > 0
                ? std::string(message_buffer.data(), message_length)
                : "Windows loader error " + std::to_string(error_code);
            while (!message.empty() && (message.back() == '\r' || message.back() == '\n'))
                message.pop_back();
            return message;
        }
#endif

        class BloscLibrary
        {
        public:
            BloscLibrary() = default;
            BloscLibrary(const BloscLibrary&) = delete;
            BloscLibrary& operator=(const BloscLibrary&) = delete;

            ~BloscLibrary()
            {
#if defined(_WIN32)
                if( handle )
                    FreeLibrary(handle);
#else
                if( handle )
                    dlclose(handle);
#endif
            }

            bool load(const std::string &explicitPath, DiagnosticList *diagnostics)
            {
                std::vector<std::filesystem::path> candidates;
                if( !explicitPath.empty() )
                    candidates.emplace_back(explicitPath);

                const std::string environmentPath = environmentValue("HOUIO_BLOSC_LIBRARY");
                if( !environmentPath.empty() )
                    candidates.emplace_back(environmentPath);

#if defined(_WIN32)
                const std::string houdiniRoot = environmentValue("HFS");
                if( !houdiniRoot.empty() )
                    candidates.emplace_back(std::filesystem::path(houdiniRoot) / "bin" / "blosc.dll");
                candidates.emplace_back("blosc.dll");
#else
#if defined(__APPLE__)
                candidates.emplace_back("libblosc.dylib");
#else
                candidates.emplace_back("libblosc.so.1");
                candidates.emplace_back("libblosc.so");
#endif
#endif

                std::string attemptedLibraries;
                for( const std::filesystem::path &candidate : candidates )
                {
                    if( open(candidate) && resolveFunctions() )
                        return true;
                    if( !attemptedLibraries.empty() )
                        attemptedLibraries += "; ";
                    attemptedLibraries += candidate.string() + " (" + lastError + ")";
                    close();
                }

                std::string message =
                    "SCF support requires a C-Blosc runtime. Set GeometryReadOptions::bloscLibraryPath, "
                    "GeometryWriteOptions::bloscLibraryPath, or HOUIO_BLOSC_LIBRARY.";
                if( !attemptedLibraries.empty() )
                    message += " Attempts: " + attemptedLibraries;
                appendError(diagnostics, DiagnosticCategory::unsupported_input, std::move(message));
                return false;
            }

            [[nodiscard]] int compress(int level, bool shuffle, std::size_t typeSize, std::size_t inputBytes,
                const void *input, void *output, std::size_t outputBytes, const std::string &compressor,
                std::size_t blockBytes) const
            {
                return compressFunction(level, shuffle ? 1 : 0, typeSize, inputBytes, input, output,
                    outputBytes, compressor.c_str(), blockBytes, 1);
            }

            [[nodiscard]] int decompress(const void *input, void *output, std::size_t outputBytes) const
            {
                return decompressFunction(input, output, outputBytes, 1);
            }

            void bufferSizes(const void *input, std::size_t &uncompressedBytes,
                std::size_t &compressedBytes, std::size_t &blockBytes) const
            {
                bufferSizesFunction(input, &uncompressedBytes, &compressedBytes, &blockBytes);
            }

        private:
            bool open(const std::filesystem::path &path)
            {
#if defined(_WIN32)
                std::filesystem::path loadPath = path;
                std::error_code pathError;
                const bool pathExists = std::filesystem::exists(path, pathError);
                if( path.is_relative() && (path.has_parent_path() || (!pathError && pathExists)) )
                {
                    const std::filesystem::path absolutePath = std::filesystem::absolute(path, pathError);
                    if( !pathError )
                        loadPath = absolutePath;
                }

                DLL_DIRECTORY_COOKIE directoryCookie = nullptr;
                const std::filesystem::path parentDirectory = loadPath.parent_path();
                DWORD searchFlags = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
                if( !parentDirectory.empty() )
                {
                    directoryCookie = AddDllDirectory(parentDirectory.wstring().c_str());
                    searchFlags |= LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR;
                }
                if( directoryCookie )
                    searchFlags |= LOAD_LIBRARY_SEARCH_USER_DIRS;
                handle = LoadLibraryExW(loadPath.wstring().c_str(), nullptr, searchFlags);
                const DWORD loadError = handle ? ERROR_SUCCESS : GetLastError();
                if( directoryCookie )
                    RemoveDllDirectory(directoryCookie);
                if( !handle )
                    lastError = windowsErrorMessage(loadError);
#else
                handle = dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
                if( !handle )
                {
                    const char *loaderError = dlerror();
                    lastError = loaderError ? loaderError : "unknown dynamic loader error";
                }
#endif
                return handle != nullptr;
            }

            void close()
            {
#if defined(_WIN32)
                if( handle )
                    FreeLibrary(handle);
#else
                if( handle )
                    dlclose(handle);
#endif
                handle = nullptr;
                compressFunction = nullptr;
                decompressFunction = nullptr;
                bufferSizesFunction = nullptr;
            }

            template<typename Function>
            Function resolve(const char *name) const
            {
                Function function = nullptr;
#if defined(_WIN32)
                const FARPROC symbol = GetProcAddress(handle, name);
#else
                void *const symbol = dlsym(handle, name);
#endif
                static_assert(sizeof(function) == sizeof(symbol));
                std::memcpy(&function, &symbol, sizeof(function));
                return function;
            }

            bool resolveFunctions()
            {
                compressFunction = resolve<BloscCompressFunction>("blosc_compress_ctx");
                decompressFunction = resolve<BloscDecompressFunction>("blosc_decompress_ctx");
                bufferSizesFunction = resolve<BloscBufferSizesFunction>("blosc_cbuffer_sizes");
                if( compressFunction && decompressFunction && bufferSizesFunction )
                    return true;
                lastError = "required C-Blosc symbols are missing";
                return false;
            }

#if defined(_WIN32)
            HMODULE handle = nullptr;
#else
            void *handle = nullptr;
#endif
            BloscCompressFunction compressFunction = nullptr;
            BloscDecompressFunction decompressFunction = nullptr;
            BloscBufferSizesFunction bufferSizesFunction = nullptr;
            std::string lastError = "not attempted";
        };
    }

    bool hasScfMagic(std::span<const char> input) noexcept
    {
        return input.size() >= scfHeaderMagic.size()
            && std::memcmp(input.data(), scfHeaderMagic.data(), scfHeaderMagic.size()) == 0;
    }

    bool decompressScf(std::span<const char> input, std::vector<char> &output,
        const ScfReadOptions &options, DiagnosticList *diagnostics)
    {
        output.clear();
        if( input.size() < scfPrefixBytes + scfSuffixBytes + 16 )
        {
            appendError(diagnostics, DiagnosticCategory::malformed_input, "SCF file is too small");
            return false;
        }
        if( !hasScfMagic(input) )
        {
            appendError(diagnostics, DiagnosticCategory::malformed_input, "SCF header magic is missing", 0);
            return false;
        }
        if( std::memcmp(input.data() + input.size() - scfFooterMagic.size(), scfFooterMagic.data(),
                scfFooterMagic.size()) != 0 )
        {
            appendError(diagnostics, DiagnosticCategory::malformed_input, "SCF footer magic is missing",
                static_cast<sint64>(input.size() - scfFooterMagic.size()));
            return false;
        }

        const std::uint64_t metadataBytes64 = readBigUInt64(input, 4);
        const std::uint64_t indexBytes64 = readBigUInt64(input, input.size() - scfSuffixBytes);
        if( metadataBytes64 > std::numeric_limits<std::size_t>::max()
            || indexBytes64 > std::numeric_limits<std::size_t>::max() )
        {
            appendError(diagnostics, DiagnosticCategory::malformed_input, "SCF section length exceeds this platform");
            return false;
        }

        const std::size_t metadataBytes = static_cast<std::size_t>(metadataBytes64);
        const std::size_t indexBytes = static_cast<std::size_t>(indexBytes64);
        std::size_t compressedStart = 0;
        if( !checkedAdd(scfPrefixBytes, metadataBytes, compressedStart)
            || compressedStart > input.size() - scfSuffixBytes )
        {
            appendError(diagnostics, DiagnosticCategory::malformed_input, "SCF metadata length is invalid", 4);
            return false;
        }
        if( indexBytes < 16 || indexBytes % 8 != 0
            || indexBytes > input.size() - scfSuffixBytes - compressedStart )
        {
            appendError(diagnostics, DiagnosticCategory::malformed_input, "SCF index length is invalid",
                static_cast<sint64>(input.size() - scfSuffixBytes));
            return false;
        }

        const std::size_t indexStart = input.size() - scfSuffixBytes - indexBytes;
        if( indexStart <= compressedStart )
        {
            appendError(diagnostics, DiagnosticCategory::malformed_input, "SCF contains no compressed blocks");
            return false;
        }

        const std::size_t indexValueCount = indexBytes / 8;
        const std::size_t blockCount = indexValueCount - 1;
        const std::size_t blockOffsetCount = blockCount - 1;
        const std::uint64_t blockBytes64 = readBigUInt64(input, indexStart + blockOffsetCount * 8);
        const std::uint64_t lastBlockBytes64 = readBigUInt64(input, indexStart + (blockOffsetCount + 1) * 8);
        if( blockBytes64 == 0 || lastBlockBytes64 == 0 || lastBlockBytes64 > blockBytes64
            || blockBytes64 > std::numeric_limits<std::size_t>::max() )
        {
            appendError(diagnostics, DiagnosticCategory::malformed_input, "SCF block-size metadata is invalid",
                static_cast<sint64>(indexStart));
            return false;
        }

        const std::size_t blockBytes = static_cast<std::size_t>(blockBytes64);
        const std::size_t lastBlockBytes = static_cast<std::size_t>(lastBlockBytes64);
        std::size_t fullBlockBytes = 0;
        std::size_t outputBytes = 0;
        if( !checkedMultiply(blockCount - 1, blockBytes, fullBlockBytes)
            || !checkedAdd(fullBlockBytes, lastBlockBytes, outputBytes)
            || outputBytes > options.maxOutputBytes )
        {
            appendError(diagnostics, DiagnosticCategory::malformed_input,
                "SCF decompressed size exceeds the configured limit", static_cast<sint64>(indexStart));
            return false;
        }

        BloscLibrary blosc;
        if( !blosc.load(options.bloscLibraryPath, diagnostics) )
            return false;

        output.resize(outputBytes);
        std::size_t compressedPosition = compressedStart;
        for( std::size_t blockIndex = 0; blockIndex < blockCount; ++blockIndex )
        {
            if( compressedPosition > indexStart || indexStart - compressedPosition < bloscHeaderBytes )
            {
                appendError(diagnostics, DiagnosticCategory::malformed_input, "SCF block header is truncated",
                    static_cast<sint64>(compressedPosition));
                output.clear();
                return false;
            }

            const std::uint32_t declaredUncompressedBytes = readLittleUInt32(input, compressedPosition + 4);
            const std::uint32_t declaredCompressedBytes = readLittleUInt32(input, compressedPosition + 12);
            if( declaredCompressedBytes < bloscHeaderBytes
                || declaredCompressedBytes > indexStart - compressedPosition )
            {
                appendError(diagnostics, DiagnosticCategory::malformed_input, "SCF Blosc block length is invalid",
                    static_cast<sint64>(compressedPosition + 12));
                output.clear();
                return false;
            }

            const std::size_t expectedUncompressedBytes = blockIndex + 1 == blockCount
                ? lastBlockBytes : blockBytes;
            if( declaredUncompressedBytes != expectedUncompressedBytes )
            {
                appendError(diagnostics, DiagnosticCategory::malformed_input,
                    "SCF Blosc block size does not match the seek index",
                    static_cast<sint64>(compressedPosition + 4));
                output.clear();
                return false;
            }

            std::size_t expectedCompressedEnd = indexStart;
            if( blockIndex < blockOffsetCount )
            {
                const std::uint64_t relativeOffset64 =
                    readBigUInt64(input, indexStart + blockIndex * 8);
                if( relativeOffset64 > std::numeric_limits<std::size_t>::max()
                    || static_cast<std::size_t>(relativeOffset64) > indexStart - compressedStart )
                {
                    appendError(diagnostics, DiagnosticCategory::malformed_input,
                        "SCF seek offset is outside the compressed payload",
                        static_cast<sint64>(indexStart + blockIndex * 8));
                    output.clear();
                    return false;
                }
                expectedCompressedEnd = compressedStart + static_cast<std::size_t>(relativeOffset64);
            }
            const std::size_t actualCompressedEnd = compressedPosition + declaredCompressedBytes;
            if( expectedCompressedEnd != actualCompressedEnd )
            {
                appendError(diagnostics, DiagnosticCategory::malformed_input,
                    "SCF seek index does not match its compressed blocks", static_cast<sint64>(indexStart));
                output.clear();
                return false;
            }

            std::size_t libraryUncompressedBytes = 0;
            std::size_t libraryCompressedBytes = 0;
            std::size_t libraryBlockBytes = 0;
            blosc.bufferSizes(input.data() + compressedPosition, libraryUncompressedBytes,
                libraryCompressedBytes, libraryBlockBytes);
            if( libraryUncompressedBytes != expectedUncompressedBytes
                || libraryCompressedBytes != declaredCompressedBytes
                || libraryBlockBytes == 0 )
            {
                appendError(diagnostics, DiagnosticCategory::malformed_input,
                    "C-Blosc rejected the SCF block header", static_cast<sint64>(compressedPosition));
                output.clear();
                return false;
            }

            const std::size_t outputOffset = blockIndex * blockBytes;
            const int decompressedBytes = blosc.decompress(input.data() + compressedPosition,
                output.data() + outputOffset, expectedUncompressedBytes);
            if( decompressedBytes <= 0 || static_cast<std::size_t>(decompressedBytes) != expectedUncompressedBytes )
            {
                appendError(diagnostics, DiagnosticCategory::malformed_input,
                    "C-Blosc failed to decompress an SCF block", static_cast<sint64>(compressedPosition));
                output.clear();
                return false;
            }
            compressedPosition = actualCompressedEnd;
        }

        if( compressedPosition != indexStart )
        {
            appendError(diagnostics, DiagnosticCategory::malformed_input,
                "SCF compressed payload does not end at the seek index",
                static_cast<sint64>(compressedPosition));
            output.clear();
            return false;
        }
        return true;
    }

    bool compressScf(std::span<const char> input, std::vector<char> &output,
        const ScfWriteOptions &options, DiagnosticList *diagnostics)
    {
        output.clear();
        if( input.empty() )
        {
            appendError(diagnostics, DiagnosticCategory::io, "Cannot create an SCF wrapper for empty input");
            return false;
        }
        if( options.blockBytes == 0
            || options.blockBytes > static_cast<std::size_t>(INT_MAX - static_cast<int>(bloscMaximumOverhead)) )
        {
            appendError(diagnostics, DiagnosticCategory::io, "SCF block size is outside the C-Blosc range");
            return false;
        }
        if( options.compressionLevel < 0 || options.compressionLevel > 9 )
        {
            appendError(diagnostics, DiagnosticCategory::io, "SCF compression level must be between 0 and 9");
            return false;
        }
        if( options.compressor.empty() )
        {
            appendError(diagnostics, DiagnosticCategory::io, "SCF compressor name cannot be empty");
            return false;
        }

        BloscLibrary blosc;
        if( !blosc.load(options.bloscLibraryPath, diagnostics) )
            return false;

        output.insert(output.end(), scfHeaderMagic.begin(), scfHeaderMagic.end());
        appendBigUInt64(output, 0);
        const std::size_t compressedStart = output.size();
        const std::size_t blockCount = input.size() / options.blockBytes
            + static_cast<std::size_t>(input.size() % options.blockBytes != 0);
        std::vector<std::uint64_t> compressedOffsets;
        compressedOffsets.reserve(blockCount > 0 ? blockCount - 1 : 0);

        std::size_t inputOffset = 0;
        for( std::size_t blockIndex = 0; blockIndex < blockCount; ++blockIndex )
        {
            const std::size_t sourceBytes = std::min(options.blockBytes, input.size() - inputOffset);
            std::vector<char> compressedBlock(sourceBytes + bloscMaximumOverhead);
            const int compressedBytes = blosc.compress(options.compressionLevel, options.shuffle, 4,
                sourceBytes, input.data() + inputOffset, compressedBlock.data(), compressedBlock.size(),
                options.compressor, sourceBytes);
            if( compressedBytes <= 0 )
            {
                appendError(diagnostics, DiagnosticCategory::io,
                    "C-Blosc failed to compress an SCF block");
                output.clear();
                return false;
            }

            output.insert(output.end(), compressedBlock.begin(), compressedBlock.begin() + compressedBytes);
            inputOffset += sourceBytes;
            if( blockIndex + 1 < blockCount )
                compressedOffsets.push_back(static_cast<std::uint64_t>(output.size() - compressedStart));
        }

        const std::size_t indexStart = output.size();
        for( const std::uint64_t offset : compressedOffsets )
            appendBigUInt64(output, offset);
        appendBigUInt64(output, static_cast<std::uint64_t>(options.blockBytes));
        const std::size_t lastBlockBytes = input.size() - (blockCount - 1) * options.blockBytes;
        appendBigUInt64(output, static_cast<std::uint64_t>(lastBlockBytes));
        appendBigUInt64(output, static_cast<std::uint64_t>(output.size() - indexStart));
        output.insert(output.end(), scfFooterMagic.begin(), scfFooterMagic.end());
        return true;
    }
}
