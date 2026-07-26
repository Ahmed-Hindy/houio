#include <houio/GeometryIO.h>

#include "Scf.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <houio/HouGeoIO.h>

namespace houio
{
    namespace
    {
        std::string lowercase(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
            return value;
        }

        bool endsWith(std::string_view value, std::string_view suffix)
        {
            return value.size() >= suffix.size()
                && value.substr(value.size() - suffix.size()) == suffix;
        }

        void appendResultError(DiagnosticList &diagnostics, DiagnosticCategory category,
            std::string message, std::string path = {})
        {
            diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::error, category, std::move(message), -1, std::move(path)});
        }

        bool validateReadOptions(const GeometryReadOptions &options, DiagnosticList &diagnostics)
        {
            const json::ParserLimits &limits = options.parserLimits;
            if( limits.maxInputBytes < 0 || limits.maxStringBytes < 0
                || limits.maxUniformArrayElements < 0 )
            {
                appendResultError(diagnostics, DiagnosticCategory::malformed_input,
                    "Parser limits cannot be negative", "options.parserLimits");
                return false;
            }
            if( limits.maxNestingDepth == 0 )
            {
                appendResultError(diagnostics, DiagnosticCategory::malformed_input,
                    "Parser nesting depth must be greater than zero", "options.parserLimits.maxNestingDepth");
                return false;
            }
            if( static_cast<unsigned long long>(limits.maxInputBytes)
                > std::numeric_limits<std::size_t>::max() )
            {
                appendResultError(diagnostics, DiagnosticCategory::unsupported_input,
                    "Parser input limit exceeds this platform's addressable size",
                    "options.parserLimits.maxInputBytes");
                return false;
            }
            return true;
        }

        GeometryFileFormat detectFormatFromName(const std::filesystem::path &path)
        {
            const std::string filename = lowercase(path.filename().string());
            if( endsWith(filename, ".bgeo.sc") || endsWith(filename, ".geo.sc") )
                return GeometryFileFormat::bgeo_scf;
            if( endsWith(filename, ".bgeo") )
                return GeometryFileFormat::bgeo_binary;
            if( endsWith(filename, ".geo") )
                return GeometryFileFormat::geo_ascii;
            if( endsWith(filename, ".vdb") )
                return GeometryFileFormat::openvdb;
            return GeometryFileFormat::unknown;
        }

        GeometryFileFormat detectFormatFromBytes(std::span<const char> bytes)
        {
            if( detail::hasScfMagic(bytes) )
                return GeometryFileFormat::bgeo_scf;
            if( bytes.size() >= 4 && bytes[0] == ' ' && bytes[1] == 'B'
                && bytes[2] == 'D' && bytes[3] == 'V' )
            {
                return GeometryFileFormat::openvdb;
            }
            if( bytes.size() >= 5 && static_cast<unsigned char>(bytes[0]) == 0x7f
                && bytes[1] == 'N' && bytes[2] == 'S' && bytes[3] == 'J' && bytes[4] == 'b' )
            {
                return GeometryFileFormat::bgeo_binary;
            }
            for( const char byte : bytes )
            {
                if( std::isspace(static_cast<unsigned char>(byte)) != 0 )
                    continue;
                if( byte == '[' || byte == '{' )
                    return GeometryFileFormat::geo_ascii;
                break;
            }
            return GeometryFileFormat::unknown;
        }

        bool readFileBytes(const std::filesystem::path &path, std::size_t maxBytes,
            std::vector<char> &bytes, DiagnosticList &diagnostics)
        {
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if( !input )
            {
                appendResultError(diagnostics, DiagnosticCategory::io,
                    "Could not open geometry file: " + path.string());
                return false;
            }

            const std::streampos endPosition = input.tellg();
            if( endPosition < 0 )
            {
                appendResultError(diagnostics, DiagnosticCategory::io,
                    "Could not determine geometry file size: " + path.string());
                return false;
            }
            const auto fileBytes64 = static_cast<unsigned long long>(endPosition);
            if( fileBytes64 > maxBytes || fileBytes64 > std::numeric_limits<std::size_t>::max() )
            {
                appendResultError(diagnostics, DiagnosticCategory::malformed_input,
                    "Geometry file exceeds the configured file-size limit: " + path.string());
                return false;
            }

            bytes.resize(static_cast<std::size_t>(fileBytes64));
            input.seekg(0, std::ios::beg);
            if( !bytes.empty() )
                input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if( !input && !bytes.empty() )
            {
                appendResultError(diagnostics, DiagnosticCategory::io,
                    "Could not read complete geometry file: " + path.string());
                bytes.clear();
                return false;
            }
            return true;
        }

        unsigned long long currentProcessId() noexcept
        {
#if defined(_WIN32)
            return static_cast<unsigned long long>(GetCurrentProcessId());
#else
            return static_cast<unsigned long long>(getpid());
#endif
        }

        std::filesystem::path temporaryOutputPath(const std::filesystem::path &path)
        {
            static std::atomic<unsigned long long> sequence{0};
            const unsigned long long timestamp = static_cast<unsigned long long>(
                std::chrono::steady_clock::now().time_since_epoch().count());
            std::filesystem::path temporary = path;
            temporary += ".houio.tmp." + std::to_string(currentProcessId()) + "."
                + std::to_string(timestamp) + "."
                + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
            return temporary;
        }

        bool writeExclusiveTemporaryFile(
            const std::filesystem::path &path,
            std::span<const char> bytes,
            DiagnosticList &diagnostics)
        {
#if defined(_WIN32)
            const HANDLE handle = CreateFileW(
                path.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
                nullptr);
            if( handle == INVALID_HANDLE_VALUE )
            {
                appendResultError(diagnostics, DiagnosticCategory::io,
                    "Could not exclusively create temporary geometry output: "
                        + path.string() + " (Windows error "
                        + std::to_string(GetLastError()) + ")");
                return false;
            }

            std::size_t offset = 0;
            DWORD writeError = ERROR_SUCCESS;
            while( offset < bytes.size() )
            {
                const std::size_t remaining = bytes.size() - offset;
                const DWORD chunkSize = static_cast<DWORD>(std::min<std::size_t>(
                    remaining, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
                DWORD written = 0;
                if( WriteFile(handle, bytes.data() + offset, chunkSize, &written, nullptr) == 0 )
                {
                    writeError = GetLastError();
                    break;
                }
                if( written == 0 )
                {
                    writeError = ERROR_WRITE_FAULT;
                    break;
                }
                offset += static_cast<std::size_t>(written);
            }

            const bool synchronized = writeError == ERROR_SUCCESS
                && FlushFileBuffers(handle) != 0;
            const DWORD syncError = synchronized ? ERROR_SUCCESS : GetLastError();
            const bool closed = CloseHandle(handle) != 0;
            const DWORD closeError = closed ? ERROR_SUCCESS : GetLastError();
            if( writeError == ERROR_SUCCESS && synchronized && closed )
                return true;

            std::error_code removeError;
            std::filesystem::remove(path, removeError);
            const DWORD errorCode = writeError != ERROR_SUCCESS
                ? writeError : syncError != ERROR_SUCCESS ? syncError : closeError;
            appendResultError(diagnostics, DiagnosticCategory::io,
                "Could not write and synchronize temporary geometry output: "
                    + path.string() + " (Windows error "
                    + std::to_string(errorCode) + ")");
            return false;
#else
            int openFlags = O_WRONLY | O_CREAT | O_EXCL;
#ifdef O_NOFOLLOW
            openFlags |= O_NOFOLLOW;
#endif
            const int descriptor = open(path.c_str(), openFlags, 0600);
            if( descriptor < 0 )
            {
                appendResultError(diagnostics, DiagnosticCategory::io,
                    "Could not exclusively create temporary geometry output: "
                        + path.string() + " (" + std::strerror(errno) + ")");
                return false;
            }

            std::size_t offset = 0;
            int writeError = 0;
            while( offset < bytes.size() )
            {
                const std::size_t chunkSize = std::min<std::size_t>(
                    bytes.size() - offset, 1024ULL * 1024ULL * 1024ULL);
                const ssize_t written = write(
                    descriptor, bytes.data() + offset, chunkSize);
                if( written < 0 )
                {
                    if( errno == EINTR )
                        continue;
                    writeError = errno;
                    break;
                }
                if( written == 0 )
                {
                    writeError = EIO;
                    break;
                }
                offset += static_cast<std::size_t>(written);
            }

            int syncError = 0;
            if( writeError == 0 && fsync(descriptor) != 0 )
                syncError = errno;
            int closeError = 0;
            if( close(descriptor) != 0 )
                closeError = errno;
            if( writeError == 0 && syncError == 0 && closeError == 0 )
                return true;

            std::error_code removeError;
            std::filesystem::remove(path, removeError);
            const int errorCode = writeError != 0
                ? writeError : syncError != 0 ? syncError : closeError;
            appendResultError(diagnostics, DiagnosticCategory::io,
                "Could not write and synchronize temporary geometry output: "
                    + path.string() + " (" + std::strerror(errorCode) + ")");
            return false;
#endif
        }

#if !defined(_WIN32)
        bool synchronizeDirectory(const std::filesystem::path &path, DiagnosticList &diagnostics)
        {
            const std::filesystem::path directory = path.empty()
                ? std::filesystem::path(".") : path;
#ifdef O_DIRECTORY
            const int descriptor = open(directory.c_str(), O_RDONLY | O_DIRECTORY);
#else
            const int descriptor = open(directory.c_str(), O_RDONLY);
#endif
            if( descriptor < 0 )
            {
                appendResultError(diagnostics, DiagnosticCategory::io,
                    "Could not open geometry output directory for synchronization: "
                        + directory.string() + " (" + std::strerror(errno) + ")");
                return false;
            }
            const int syncResult = fsync(descriptor);
            const int syncError = errno;
            close(descriptor);
            if( syncResult == 0 )
                return true;
            appendResultError(diagnostics, DiagnosticCategory::io,
                "Could not synchronize geometry output directory: " + directory.string()
                    + " (" + std::strerror(syncError) + ")");
            return false;
        }
#endif

        bool replaceOutputFile(const std::filesystem::path &temporary,
            const std::filesystem::path &destination, bool overwriteExisting,
            DiagnosticList &diagnostics)
        {
#if defined(_WIN32)
            DWORD flags = MOVEFILE_WRITE_THROUGH;
            if( overwriteExisting )
                flags |= MOVEFILE_REPLACE_EXISTING;
            DWORD errorCode = ERROR_SUCCESS;
            constexpr int maxAttempts = 40;
            for( int attempt = 0; attempt < maxAttempts; ++attempt )
            {
                if( MoveFileExW(temporary.c_str(), destination.c_str(), flags) != 0 )
                    return true;
                errorCode = GetLastError();
                if( errorCode != ERROR_SHARING_VIOLATION
                    && errorCode != ERROR_ACCESS_DENIED )
                {
                    break;
                }
                Sleep(25);
            }
            appendResultError(diagnostics, DiagnosticCategory::io,
                "Could not replace geometry output file: " + destination.string()
                    + " (Windows error " + std::to_string(errorCode) + ")");
#else
            if( !overwriteExisting )
            {
                std::error_code linkError;
                std::filesystem::create_hard_link(temporary, destination, linkError);
                if( linkError )
                {
                    const std::string message = std::filesystem::exists(destination)
                        ? "Geometry output file already exists: " + destination.string()
                        : "Could not create geometry output file without replacement: "
                            + destination.string() + " (" + linkError.message() + ")";
                    appendResultError(diagnostics, DiagnosticCategory::io, message);
                    return false;
                }
                std::error_code removeError;
                std::filesystem::remove(temporary, removeError);
                if( removeError )
                {
                    appendResultError(diagnostics, DiagnosticCategory::io,
                        "Could not remove temporary geometry output file: "
                            + temporary.string() + " (" + removeError.message() + ")");
                    return false;
                }
                return synchronizeDirectory(destination.parent_path(), diagnostics);
            }
            std::error_code renameError;
            std::filesystem::rename(temporary, destination, renameError);
            if( !renameError )
                return synchronizeDirectory(destination.parent_path(), diagnostics);
            appendResultError(diagnostics, DiagnosticCategory::io,
                "Could not replace geometry output file: " + destination.string()
                    + " (" + renameError.message() + ")");
#endif
            return false;
        }

        bool writeFileBytes(const std::filesystem::path &path, std::span<const char> bytes,
            const GeometryWriteOptions &options, DiagnosticList &diagnostics)
        {
            const std::filesystem::path parent = path.parent_path();
            if( !parent.empty() )
            {
                if( options.createParentDirectories )
                {
                    std::error_code directoryError;
                    std::filesystem::create_directories(parent, directoryError);
                    if( directoryError )
                    {
                        appendResultError(diagnostics, DiagnosticCategory::io,
                            "Could not create output directory: " + parent.string());
                        return false;
                    }
                }
                else if( !std::filesystem::is_directory(parent) )
                {
                    appendResultError(diagnostics, DiagnosticCategory::io,
                        "Geometry output directory does not exist: " + parent.string());
                    return false;
                }
            }

            if( !options.overwriteExisting && std::filesystem::exists(path) )
            {
                appendResultError(diagnostics, DiagnosticCategory::io,
                    "Geometry output file already exists: " + path.string());
                return false;
            }

            const std::filesystem::path outputPath = options.atomicReplace
                ? temporaryOutputPath(path)
                : path;
            if( options.atomicReplace )
            {
                if( !writeExclusiveTemporaryFile(outputPath, bytes, diagnostics) )
                    return false;
            }
            else
            {
                std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
                if( !output )
                {
                    appendResultError(diagnostics, DiagnosticCategory::io,
                        "Could not open geometry output file: " + outputPath.string());
                    return false;
                }
                if( !bytes.empty() )
                    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                output.close();
                if( !output )
                {
                    std::error_code removeError;
                    std::filesystem::remove(outputPath, removeError);
                    appendResultError(diagnostics, DiagnosticCategory::io,
                        "Could not write complete geometry output file: " + outputPath.string());
                    return false;
                }
                return true;
            }

            if( replaceOutputFile(outputPath, path, options.overwriteExisting, diagnostics) )
                return true;
            std::error_code removeError;
            std::filesystem::remove(outputPath, removeError);
            return false;
        }

        GeometryFileFormat resolveReadFormat(const std::filesystem::path &path,
            const GeometryReadOptions &options, std::span<const char> bytes)
        {
            if( options.format != GeometryFileFormat::automatic )
                return options.format;
            const GeometryFileFormat byteFormat = detectFormatFromBytes(bytes);
            if( byteFormat != GeometryFileFormat::unknown )
                return byteFormat;
            return detectFormatFromName(path);
        }

        GeometryFileFormat resolveWriteFormat(const std::filesystem::path &path,
            const GeometryWriteOptions &options)
        {
            if( options.format != GeometryFileFormat::automatic )
                return options.format;
            const GeometryFileFormat namedFormat = detectFormatFromName(path);
            return namedFormat == GeometryFileFormat::unknown
                ? GeometryFileFormat::bgeo_binary : namedFormat;
        }
    }

    GeometryFileFormat GeometryIO::detectFormat(const std::filesystem::path &path)
    {
        std::ifstream input(path, std::ios::binary);
        if( input )
        {
            std::array<char, 16> prefix{};
            input.read(prefix.data(), static_cast<std::streamsize>(prefix.size()));
            const std::size_t bytesRead = static_cast<std::size_t>(input.gcount());
            const GeometryFileFormat byteFormat = detectFormatFromBytes(
                std::span<const char>(prefix.data(), bytesRead));
            if( byteFormat != GeometryFileFormat::unknown )
                return byteFormat;
        }
        return detectFormatFromName(path);
    }

    GeometryReadResult<HouGeo::Ptr> GeometryIO::readHouGeo(
        const std::filesystem::path &path, const GeometryReadOptions &options)
    {
        GeometryReadResult<HouGeo::Ptr> result;
        if( !validateReadOptions(options, result.diagnostics) )
            return result;

        std::vector<char> fileBytes;
        if( !readFileBytes(path, options.maxFileBytes, fileBytes, result.diagnostics) )
            return result;

        const GeometryFileFormat format = resolveReadFormat(path, options, fileBytes);
        std::vector<char> decodedBytes;
        std::span<const char> parserBytes(fileBytes.data(), fileBytes.size());
        if( format == GeometryFileFormat::bgeo_scf )
        {
            detail::ScfReadOptions scfOptions;
            scfOptions.maxOutputBytes = static_cast<std::size_t>(options.parserLimits.maxInputBytes);
            scfOptions.bloscLibraryPath = options.bloscLibraryPath;
            if( !detail::decompressScf(parserBytes, decodedBytes, scfOptions, &result.diagnostics) )
                return result;
            parserBytes = std::span<const char>(decodedBytes.data(), decodedBytes.size());
        }
        else if( format == GeometryFileFormat::openvdb )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::unsupported_input,
                "Native OpenVDB file loading is not enabled. Use the HouIO HOM bridge to convert the VDB "
                "to a dense Houdini volume, or build a dedicated OpenVDB adapter.", "file");
            return result;
        }
        else if( format == GeometryFileFormat::unknown )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::unsupported_input,
                "Could not identify the geometry file format: " + path.string(), "file");
            return result;
        }

        std::string parserStorage;
        parserStorage.assign(parserBytes.begin(), parserBytes.end());
        std::istringstream input(parserStorage, std::ios::in | std::ios::binary);
        try
        {
            result.value = HouGeoIO::import(input, options.parserLimits, &result.diagnostics);
            result.succeeded = static_cast<bool>(result.value);
        }
        catch( const DiagnosticException &exception )
        {
            result.diagnostics.push_back(exception.diagnostic());
        }
        catch( const std::exception &exception )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::malformed_input,
                exception.what(), "file");
        }
        return result;
    }

    GeometryReadResult<Geometry::Ptr> GeometryIO::readGeometry(
        const std::filesystem::path &path, const GeometryReadOptions &options)
    {
        GeometryReadResult<Geometry::Ptr> result;
        GeometryReadResult<HouGeo::Ptr> houGeoResult = readHouGeo(path, options);
        result.diagnostics = std::move(houGeoResult.diagnostics);
        if( !houGeoResult.value )
            return result;

        const std::vector<HouGeoAdapter::Primitive::Ptr> primitives =
            houGeoResult.value->primitives();
        if( primitives.empty() )
        {
            result.value = HouGeoIO::convertToGeometry(
                houGeoResult.value, HouGeoAdapter::Primitive::Ptr(), &result.diagnostics);
            result.succeeded = static_cast<bool>(result.value);
            return result;
        }
        if( !std::dynamic_pointer_cast<HouGeoAdapter::PolyPrimitive>(primitives.front()) )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::unsupported_input,
                "The first primitive is not polygon geometry", "primitives[0]");
            return result;
        }
        result.value = HouGeoIO::convertToGeometry(
            houGeoResult.value, primitives.front(), &result.diagnostics);
        result.succeeded = static_cast<bool>(result.value);
        return result;
    }

    GeometryReadResult<ScalarField::Ptr> GeometryIO::readVolume(
        const std::filesystem::path &path, const GeometryReadOptions &options)
    {
        GeometryReadResult<ScalarField::Ptr> result;
        GeometryReadResult<std::vector<ScalarField::Ptr>> volumesResult = readVolumes(path, options);
        result.diagnostics = std::move(volumesResult.diagnostics);
        if( !volumesResult || volumesResult.value.empty() )
            return result;
        result.value = volumesResult.value.front();
        result.succeeded = true;
        if( volumesResult.value.size() > 1 )
        {
            appendDiagnostic(&result.diagnostics, Diagnostic{DiagnosticSeverity::warning,
                DiagnosticCategory::conversion,
                "The file contains multiple dense volumes; readVolume returned only the first one",
                -1, "primitives"});
        }
        return result;
    }

    GeometryReadResult<std::vector<ScalarField::Ptr>> GeometryIO::readVolumes(
        const std::filesystem::path &path, const GeometryReadOptions &options)
    {
        GeometryReadResult<std::vector<ScalarField::Ptr>> result;
        GeometryReadResult<HouGeo::Ptr> houGeoResult = readHouGeo(path, options);
        result.diagnostics = std::move(houGeoResult.diagnostics);
        if( !houGeoResult )
            return result;

        const std::vector<HouGeoAdapter::Primitive::Ptr> primitives =
            houGeoResult.value->primitives();
        if( primitives.empty() )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::schema,
                "The geometry file contains no primitives", "primitives");
            return result;
        }

        result.value.reserve(primitives.size());
        for( std::size_t primitiveIndex = 0; primitiveIndex < primitives.size(); ++primitiveIndex )
        {
            const HouGeo::HouVolume::Ptr volume =
                std::dynamic_pointer_cast<HouGeo::HouVolume>(primitives[primitiveIndex]);
            if( !volume )
            {
                appendResultError(result.diagnostics, DiagnosticCategory::unsupported_input,
                    "The geometry contains a primitive that is not a supported dense scalar volume",
                    "primitives[" + std::to_string(primitiveIndex) + "]");
                result.value.clear();
                return result;
            }
            result.value.push_back(volume->scalarField());
        }
        result.succeeded = true;
        return result;
    }

    GeometryWriteResult GeometryIO::writeHouGeo(const std::filesystem::path &path,
        const HouGeoAdapter::Ptr &geometry, const GeometryWriteOptions &options)
    {
        GeometryWriteResult result;
        if( !geometry )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::io,
                "Cannot write a null geometry object");
            return result;
        }

        const GeometryFileFormat format = resolveWriteFormat(path, options);
        if( format == GeometryFileFormat::geo_ascii )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::unsupported_input,
                "ASCII .geo export is not implemented", "file");
            return result;
        }
        if( format == GeometryFileFormat::openvdb )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::unsupported_input,
                "Native OpenVDB export is not enabled. Use the HouIO HOM bridge for .vdb output.", "file");
            return result;
        }
        if( format != GeometryFileFormat::bgeo_binary && format != GeometryFileFormat::bgeo_scf )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::unsupported_input,
                "Unsupported geometry output format", "file");
            return result;
        }

        try
        {
            std::ostringstream rawOutput(std::ios::out | std::ios::binary);
            if( !HouGeoIO::exportGeometry(rawOutput, geometry, true) )
            {
                appendResultError(result.diagnostics, DiagnosticCategory::io,
                    "HouIO failed to serialize geometry");
                return result;
            }
            const std::string rawString = rawOutput.str();
            std::span<const char> outputBytes(rawString.data(), rawString.size());
            std::vector<char> scfBytes;
            if( format == GeometryFileFormat::bgeo_scf )
            {
                detail::ScfWriteOptions scfOptions;
                scfOptions.blockBytes = options.scfBlockBytes;
                scfOptions.compressionLevel = options.scfCompressionLevel;
                scfOptions.shuffle = options.scfShuffle;
                scfOptions.compressor = options.scfCompressor;
                scfOptions.bloscLibraryPath = options.bloscLibraryPath;
                if( !detail::compressScf(outputBytes, scfBytes, scfOptions, &result.diagnostics) )
                    return result;
                outputBytes = std::span<const char>(scfBytes.data(), scfBytes.size());
            }
            result.succeeded = writeFileBytes(path, outputBytes, options, result.diagnostics);
        }
        catch( const DiagnosticException &exception )
        {
            result.diagnostics.push_back(exception.diagnostic());
        }
        catch( const std::exception &exception )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::io, exception.what());
        }
        return result;
    }

    GeometryWriteResult GeometryIO::writeGeometry(const std::filesystem::path &path,
        const Geometry::Ptr &geometry, const GeometryWriteOptions &options)
    {
        GeometryWriteResult result;
        try
        {
            const HouGeo::Ptr adaptedGeometry = HouGeoIO::adaptGeometry(geometry);
            if( !adaptedGeometry )
            {
                appendResultError(result.diagnostics, DiagnosticCategory::io,
                    "Cannot write a null simplified geometry object");
                return result;
            }
            return writeHouGeo(path, adaptedGeometry, options);
        }
        catch( const DiagnosticException &exception )
        {
            result.diagnostics.push_back(exception.diagnostic());
            return result;
        }
        catch( const std::exception &exception )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::conversion, exception.what());
            return result;
        }
    }

    GeometryWriteResult GeometryIO::writeVolume(const std::filesystem::path &path,
        const ScalarField::Ptr &volume, const GeometryWriteOptions &options)
    {
        GeometryWriteResult result;
        try
        {
            const HouGeo::Ptr adaptedVolume = HouGeoIO::adaptVolume(volume);
            if( !adaptedVolume )
            {
                appendResultError(result.diagnostics, DiagnosticCategory::io,
                    "Cannot write a null scalar volume");
                return result;
            }
            return writeHouGeo(path, adaptedVolume, options);
        }
        catch( const DiagnosticException &exception )
        {
            result.diagnostics.push_back(exception.diagnostic());
            return result;
        }
        catch( const std::exception &exception )
        {
            appendResultError(result.diagnostics, DiagnosticCategory::conversion, exception.what());
            return result;
        }
    }
}
