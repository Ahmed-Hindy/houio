#include <houio/GeometryIO.h>
#include <houio/HomManifest.h>
#include <houio/Writer.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef HOUIO_VERSION_STRING
#define HOUIO_VERSION_STRING "development"
#endif

namespace
{
    enum class ExitCode : int
    {
        success = 0,
        operation_failed = 1,
        usage = 2,
        input = 3,
        output = 4,
        unsupported = 5,
        internal = 10
    };

    struct Arguments
    {
        std::vector<std::string> values;
        bool json = false;
        bool overwrite = true;
        bool createDirectories = true;
        bool atomic = true;
        std::optional<houio::GeometryFileFormat> outputFormat;
    };

    std::string jsonEscape(std::string_view value)
    {
        std::string result;
        result.reserve(value.size() + 8);
        for( const unsigned char character : value )
        {
            switch( character )
            {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if( character < 0x20U )
                    result += "?";
                else
                    result.push_back(static_cast<char>(character));
                break;
            }
        }
        return result;
    }

    std::string_view severityName(houio::DiagnosticSeverity severity) noexcept
    {
        return severity == houio::DiagnosticSeverity::error ? "error" : "warning";
    }

    std::string_view categoryName(houio::DiagnosticCategory category) noexcept
    {
        switch( category )
        {
        case houio::DiagnosticCategory::io: return "io";
        case houio::DiagnosticCategory::malformed_input: return "malformed_input";
        case houio::DiagnosticCategory::unsupported_input: return "unsupported_input";
        case houio::DiagnosticCategory::schema: return "schema";
        case houio::DiagnosticCategory::conversion: return "conversion";
        }
        return "unknown";
    }

    std::string_view formatName(houio::GeometryFileFormat format) noexcept
    {
        switch( format )
        {
        case houio::GeometryFileFormat::automatic: return "automatic";
        case houio::GeometryFileFormat::geo_ascii: return "geo_ascii";
        case houio::GeometryFileFormat::bgeo_binary: return "bgeo_binary";
        case houio::GeometryFileFormat::bgeo_scf: return "bgeo_scf";
        case houio::GeometryFileFormat::openvdb: return "openvdb";
        case houio::GeometryFileFormat::unknown: return "unknown";
        }
        return "unknown";
    }

    std::string_view capabilityLevelName(houio::WriterCapabilityLevel level) noexcept
    {
        switch( level )
        {
        case houio::WriterCapabilityLevel::supported: return "supported";
        case houio::WriterCapabilityLevel::recognized: return "recognized";
        case houio::WriterCapabilityLevel::unavailable: return "unavailable";
        }
        return "unavailable";
    }

    void printDiagnostics(const houio::DiagnosticList &diagnostics, bool json)
    {
        if( json )
        {
            std::cout << "[";
            for( std::size_t index = 0; index < diagnostics.size(); ++index )
            {
                const houio::Diagnostic &diagnostic = diagnostics[index];
                if( index != 0 )
                    std::cout << ',';
                std::cout << "{\"severity\":\"" << severityName(diagnostic.severity)
                    << "\",\"category\":\"" << categoryName(diagnostic.category)
                    << "\",\"message\":\"" << jsonEscape(diagnostic.message)
                    << "\",\"byte_offset\":" << diagnostic.byteOffset
                    << ",\"path\":\"" << jsonEscape(diagnostic.path) << "\"}";
            }
            std::cout << "]";
            return;
        }

        for( const houio::Diagnostic &diagnostic : diagnostics )
        {
            std::cerr << severityName(diagnostic.severity) << " ["
                << categoryName(diagnostic.category) << "]: " << diagnostic.message;
            if( diagnostic.byteOffset >= 0 )
                std::cerr << " at byte " << diagnostic.byteOffset;
            if( !diagnostic.path.empty() )
                std::cerr << " [" << diagnostic.path << ']';
            std::cerr << '\n';
        }
    }

    ExitCode classifyFailure(const houio::DiagnosticList &diagnostics, ExitCode fallback)
    {
        for( const houio::Diagnostic &diagnostic : diagnostics )
        {
            if( diagnostic.severity != houio::DiagnosticSeverity::error )
                continue;
            if( diagnostic.category == houio::DiagnosticCategory::unsupported_input )
                return ExitCode::unsupported;
            if( diagnostic.category == houio::DiagnosticCategory::io )
                return fallback;
            return ExitCode::operation_failed;
        }
        return fallback;
    }

    std::optional<houio::GeometryFileFormat> parseFormat(std::string_view value)
    {
        if( value == "bgeo" || value == "bgeo_binary" )
            return houio::GeometryFileFormat::bgeo_binary;
        if( value == "bgeo.sc" || value == "bgeo_scf" || value == "scf" )
            return houio::GeometryFileFormat::bgeo_scf;
        if( value == "geo" || value == "geo_ascii" )
            return houio::GeometryFileFormat::geo_ascii;
        if( value == "vdb" || value == "openvdb" )
            return houio::GeometryFileFormat::openvdb;
        if( value == "auto" || value == "automatic" )
            return houio::GeometryFileFormat::automatic;
        return std::nullopt;
    }

    std::optional<Arguments> parseArguments(int argc, char *argv[])
    {
        Arguments result;
        for( int index = 1; index < argc; ++index )
        {
            const std::string argument = argv[index];
            if( argument == "--json" )
                result.json = true;
            else if( argument == "--no-overwrite" )
                result.overwrite = false;
            else if( argument == "--no-create-directories" )
                result.createDirectories = false;
            else if( argument == "--no-atomic" )
                result.atomic = false;
            else if( argument == "--format" )
            {
                if( index + 1 >= argc )
                    return std::nullopt;
                result.outputFormat = parseFormat(argv[++index]);
                if( !result.outputFormat )
                    return std::nullopt;
            }
            else
                result.values.push_back(argument);
        }
        return result;
    }

    void printUsage()
    {
        std::cout
            << "HouIO " << HOUIO_VERSION_STRING << "\n\n"
            << "Usage:\n"
            << "  houio write <input> <output> [options]\n"
            << "  houio write-manifest <manifest.json> <output> [options]\n"
            << "  houio convert <input> <output> [options]\n"
            << "  houio inspect <input> [--json]\n"
            << "  houio validate <input> [--json]\n"
            << "  houio capabilities [--json]\n"
            << "  houio diagnose [--json]\n\n"
            << "Write options:\n"
            << "  --format <bgeo|bgeo.sc|geo|vdb>\n"
            << "    ASCII GEO and standalone VDB output may be unavailable;\n"
            << "    run 'houio capabilities' to verify write support.\n"
            << "  --no-overwrite\n"
            << "  --no-create-directories\n"
            << "  --no-atomic\n"
            << "  --json\n";
    }

    void printStringArray(const std::vector<std::string> &values)
    {
        std::cout << '[';
        for( std::size_t index = 0; index < values.size(); ++index )
        {
            if( index != 0 )
                std::cout << ',';
            std::cout << '"' << jsonEscape(values[index]) << '"';
        }
        std::cout << ']';
    }

    int inspectFile(const std::filesystem::path &path, bool json)
    {
        const houio::GeometryReadResult<houio::HouGeo::Ptr> result =
            houio::GeometryIO::readHouGeo(path);
        if( !result )
        {
            if( json )
            {
                std::cout << "{\"success\":false,\"diagnostics\":";
                printDiagnostics(result.diagnostics, true);
                std::cout << "}\n";
            }
            else
                printDiagnostics(result.diagnostics, false);
            return static_cast<int>(classifyFailure(result.diagnostics, ExitCode::input));
        }

        std::size_t polygonRecords = 0;
        std::size_t denseVolumes = 0;
        std::size_t packedGeometryRecords = 0;
        std::size_t packedFragmentRecords = 0;
        std::size_t packedDiskRecords = 0;
        std::size_t nativeVdbRecords = 0;
        for( const houio::HouGeoAdapter::Primitive::Ptr &primitive : result.value->primitives() )
        {
            if( std::dynamic_pointer_cast<houio::HouGeoAdapter::PolyPrimitive>(primitive) )
                ++polygonRecords;
            else if( std::dynamic_pointer_cast<houio::HouGeoAdapter::VolumePrimitive>(primitive) )
                ++denseVolumes;
            else if( std::dynamic_pointer_cast<houio::HouGeoAdapter::PackedFragmentPrimitive>(primitive) )
                ++packedFragmentRecords;
            else if( std::dynamic_pointer_cast<houio::HouGeoAdapter::PackedGeometryPrimitive>(primitive) )
                ++packedGeometryRecords;
            else if( std::dynamic_pointer_cast<houio::HouGeoAdapter::PackedDiskPrimitive>(primitive) )
                ++packedDiskRecords;
            else if( std::dynamic_pointer_cast<houio::HouGeoAdapter::NativeVdbPrimitive>(primitive) )
                ++nativeVdbRecords;
        }

        if( json )
        {
            std::cout << "{\"success\":true,\"path\":\"" << jsonEscape(path.string())
                << "\",\"format\":\"" << formatName(houio::GeometryIO::detectFormat(path))
                << "\",\"points\":" << result.value->pointCount()
                << ",\"vertices\":" << result.value->vertexCount()
                << ",\"primitives\":" << result.value->primitiveCount()
                << ",\"polygon_records\":" << polygonRecords
                << ",\"dense_volumes\":" << denseVolumes
                << ",\"packed_geometry_records\":" << packedGeometryRecords
                << ",\"packed_fragment_records\":" << packedFragmentRecords
                << ",\"packed_disk_records\":" << packedDiskRecords
                << ",\"native_vdb_records\":" << nativeVdbRecords
                << ",\"point_attributes\":";
            printStringArray(result.value->pointAttributeNames());
            std::cout << ",\"vertex_attributes\":";
            printStringArray(result.value->vertexAttributeNames());
            std::cout << ",\"primitive_attributes\":";
            printStringArray(result.value->primitiveAttributeNames());
            std::cout << ",\"global_attributes\":";
            printStringArray(result.value->globalAttributeNames());
            std::cout << ",\"point_groups\":";
            printStringArray(result.value->pointGroupNames());
            std::cout << ",\"vertex_groups\":";
            printStringArray(result.value->vertexGroupNames());
            std::cout << ",\"primitive_groups\":";
            printStringArray(result.value->primitiveGroupNames());
            std::cout << ",\"diagnostics\":";
            printDiagnostics(result.diagnostics, true);
            std::cout << "}\n";
        }
        else
        {
            std::cout << "path=" << path.string() << '\n'
                << "format=" << formatName(houio::GeometryIO::detectFormat(path)) << '\n'
                << "points=" << result.value->pointCount() << '\n'
                << "vertices=" << result.value->vertexCount() << '\n'
                << "primitives=" << result.value->primitiveCount() << '\n'
                << "polygon_records=" << polygonRecords << '\n'
                << "dense_volumes=" << denseVolumes << '\n'
                << "packed_geometry_records=" << packedGeometryRecords << '\n'
                << "packed_fragment_records=" << packedFragmentRecords << '\n'
                << "packed_disk_records=" << packedDiskRecords << '\n'
                << "native_vdb_records=" << nativeVdbRecords << '\n';
            printDiagnostics(result.diagnostics, false);
        }
        return static_cast<int>(ExitCode::success);
    }

    int validateFile(const std::filesystem::path &path, bool json)
    {
        const houio::GeometryReadResult<houio::HouGeo::Ptr> result =
            houio::GeometryIO::readHouGeo(path);
        if( json )
        {
            std::cout << "{\"success\":" << (result ? "true" : "false")
                << ",\"path\":\"" << jsonEscape(path.string())
                << "\",\"format\":\"" << formatName(houio::GeometryIO::detectFormat(path))
                << "\",\"diagnostics\":";
            printDiagnostics(result.diagnostics, true);
            std::cout << "}\n";
        }
        else
        {
            printDiagnostics(result.diagnostics, false);
            std::cout << (result ? "valid" : "invalid") << ": " << path.string() << '\n';
        }
        return result
            ? static_cast<int>(ExitCode::success)
            : static_cast<int>(classifyFailure(result.diagnostics, ExitCode::input));
    }

    int writeReadResult(
        houio::GeometryReadResult<houio::HouGeo::Ptr> readResult,
        const std::filesystem::path &sourcePath,
        const std::filesystem::path &outputPath,
        const Arguments &arguments,
        std::string_view readStage,
        std::string_view sourceField)
    {
        if( !readResult )
        {
            if( arguments.json )
            {
                std::cout << "{\"success\":false,\"stage\":\""
                    << jsonEscape(readStage) << "\",\"diagnostics\":";
                printDiagnostics(readResult.diagnostics, true);
                std::cout << "}\n";
            }
            else
                printDiagnostics(readResult.diagnostics, false);
            return static_cast<int>(classifyFailure(readResult.diagnostics, ExitCode::input));
        }

        houio::GeometryWriteOptions options;
        options.overwriteExisting = arguments.overwrite;
        options.createParentDirectories = arguments.createDirectories;
        options.atomicReplace = arguments.atomic;
        if( arguments.outputFormat )
            options.format = *arguments.outputFormat;

        const houio::WriteResult writeResult = houio::Writer::write(
            outputPath,
            std::static_pointer_cast<houio::HouGeoAdapter>(readResult.value),
            options);
        if( arguments.json )
        {
            std::cout << "{\"success\":" << (writeResult ? "true" : "false")
                << ",\"" << jsonEscape(sourceField) << "\":\""
                << jsonEscape(sourcePath.string())
                << "\",\"output\":\"" << jsonEscape(outputPath.string())
                << "\",\"points\":" << readResult.value->pointCount()
                << ",\"vertices\":" << readResult.value->vertexCount()
                << ",\"primitives\":" << readResult.value->primitiveCount()
                << ",\"diagnostics\":";
            houio::DiagnosticList diagnostics = std::move(readResult.diagnostics);
            diagnostics.insert(
                diagnostics.end(),
                writeResult.diagnostics.begin(),
                writeResult.diagnostics.end());
            printDiagnostics(diagnostics, true);
            std::cout << "}\n";
        }
        else
        {
            printDiagnostics(readResult.diagnostics, false);
            printDiagnostics(writeResult.diagnostics, false);
            if( writeResult )
            {
                std::cout << "wrote=" << outputPath.string() << '\n'
                    << "points=" << readResult.value->pointCount() << '\n'
                    << "vertices=" << readResult.value->vertexCount() << '\n'
                    << "primitives=" << readResult.value->primitiveCount() << '\n';
            }
        }
        return writeResult
            ? static_cast<int>(ExitCode::success)
            : static_cast<int>(classifyFailure(writeResult.diagnostics, ExitCode::output));
    }

    int writeFile(const std::filesystem::path &inputPath,
        const std::filesystem::path &outputPath, const Arguments &arguments)
    {
        return writeReadResult(
            houio::GeometryIO::readHouGeo(inputPath),
            inputPath,
            outputPath,
            arguments,
            "read",
            "input");
    }

    int writeManifest(const std::filesystem::path &manifestPath,
        const std::filesystem::path &outputPath, const Arguments &arguments)
    {
        return writeReadResult(
            houio::HomManifest::read(manifestPath),
            manifestPath,
            outputPath,
            arguments,
            "manifest",
            "manifest");
    }

    int printCapabilities(bool json)
    {
        const std::vector<houio::WriterCapability> &capabilities = houio::Writer::capabilities();
        if( json )
        {
            std::cout << "{\"capabilities\":[";
            for( std::size_t index = 0; index < capabilities.size(); ++index )
            {
                const houio::WriterCapability &capability = capabilities[index];
                if( index != 0 )
                    std::cout << ',';
                std::cout << "{\"name\":\"" << jsonEscape(capability.name)
                    << "\",\"level\":\"" << capabilityLevelName(capability.level)
                    << "\",\"readable\":" << (capability.readable ? "true" : "false")
                    << ",\"writable\":" << (capability.writable ? "true" : "false")
                    << ",\"detail\":\"" << jsonEscape(capability.detail) << "\"}";
            }
            std::cout << "]}\n";
        }
        else
        {
            for( const houio::WriterCapability &capability : capabilities )
            {
                std::cout << capability.name << '\t' << capabilityLevelName(capability.level)
                    << "\tread=" << (capability.readable ? "yes" : "no")
                    << "\twrite=" << (capability.writable ? "yes" : "no")
                    << '\t' << capability.detail << '\n';
            }
        }
        return static_cast<int>(ExitCode::success);
    }

    int diagnose(bool json)
    {
        if( json )
        {
            std::cout << "{\"version\":\"" << HOUIO_VERSION_STRING
                << "\",\"platform\":\""
#if defined(_WIN32)
                << "windows"
#elif defined(__APPLE__)
                << "macos"
#else
                << "linux"
#endif
                << "\",\"default_write_format\":\"bgeo_binary\",\"atomic_replace\":true,"
                << "\"capability_count\":" << houio::Writer::capabilities().size() << "}\n";
        }
        else
        {
            std::cout << "version=" << HOUIO_VERSION_STRING << '\n'
                << "default_write_format=bgeo_binary\n"
                << "atomic_replace=yes\n"
                << "capability_count=" << houio::Writer::capabilities().size() << '\n';
        }
        return static_cast<int>(ExitCode::success);
    }
}

int main(int argc, char *argv[])
{
    try
    {
        const std::optional<Arguments> parsed = parseArguments(argc, argv);
        if( !parsed || parsed->values.empty() )
        {
            printUsage();
            return static_cast<int>(ExitCode::usage);
        }

        const std::string &command = parsed->values.front();
        if( command == "help" || command == "--help" || command == "-h" )
        {
            printUsage();
            return static_cast<int>(ExitCode::success);
        }
        if( command == "version" || command == "--version" )
        {
            std::cout << HOUIO_VERSION_STRING << '\n';
            return static_cast<int>(ExitCode::success);
        }
        if( command == "capabilities" && parsed->values.size() == 1 )
            return printCapabilities(parsed->json);
        if( command == "diagnose" && parsed->values.size() == 1 )
            return diagnose(parsed->json);
        if( command == "inspect" && parsed->values.size() == 2 )
            return inspectFile(parsed->values[1], parsed->json);
        if( command == "validate" && parsed->values.size() == 2 )
            return validateFile(parsed->values[1], parsed->json);
        if( command == "write-manifest" && parsed->values.size() == 3 )
            return writeManifest(parsed->values[1], parsed->values[2], *parsed);
        if( (command == "write" || command == "convert") && parsed->values.size() == 3 )
            return writeFile(parsed->values[1], parsed->values[2], *parsed);

        printUsage();
        return static_cast<int>(ExitCode::usage);
    }
    catch( const std::exception &exception )
    {
        std::cerr << "error [internal]: " << exception.what() << '\n';
        return static_cast<int>(ExitCode::internal);
    }
}
