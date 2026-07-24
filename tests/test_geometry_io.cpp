#include <houio/GeometryIO.h>
#include <houio/HouGeoIO.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int fail(const std::string &message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

houio::HouGeo::Ptr createPointGeometry()
{
    houio::Attribute::Ptr positions = houio::Attribute::createV4f();
    positions->appendElement(houio::math::V4f(0.0f, 0.0f, 0.0f, 1.0f));
    positions->appendElement(houio::math::V4f(1.0f, 2.0f, 3.0f, 1.0f));

    houio::HouGeo::Ptr geometry = houio::HouGeo::create();
    geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>("P", positions));
    return geometry;
}

std::uint64_t readBigUInt64(const std::vector<char> &bytes, std::size_t offset)
{
    std::uint64_t value = 0;
    for( std::size_t byteIndex = 0; byteIndex < 8; ++byteIndex )
        value = (value << 8U) | static_cast<unsigned char>(bytes[offset + byteIndex]);
    return value;
}

std::vector<char> readBytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const std::streamsize byteCount = input.tellg();
    input.seekg(0, std::ios::beg);
    std::vector<char> bytes(static_cast<std::size_t>(byteCount));
    input.read(bytes.data(), byteCount);
    return bytes;
}

void writeBytes(const std::filesystem::path &path, const std::vector<char> &bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

bool containsCategory(const houio::DiagnosticList &diagnostics, houio::DiagnosticCategory category)
{
    for( const houio::Diagnostic &diagnostic : diagnostics )
    {
        if( diagnostic.category == category )
            return true;
    }
    return false;
}

int verifyRawRoundtrip(const std::filesystem::path &directory)
{
    const std::filesystem::path path = directory / "points.bgeo";
    const houio::GeometryWriteResult writeResult = houio::GeometryIO::writeHouGeo(path, createPointGeometry());
    if( !writeResult )
        return fail("raw bgeo write failed");
    if( houio::GeometryIO::detectFormat(path) != houio::GeometryFileFormat::bgeo_binary )
        return fail("raw bgeo format detection failed");

    const houio::GeometryReadResult<houio::HouGeo::Ptr> readResult = houio::GeometryIO::readHouGeo(path);
    if( !readResult || readResult.value->pointCount() != 2 || readResult.value->primitiveCount() != 0 )
        return fail("raw bgeo round-trip failed");
    return 0;
}

int verifyScfRoundtrip(const std::filesystem::path &directory)
{
    const std::filesystem::path path = directory / "points.bgeo.sc";
    houio::GeometryWriteOptions writeOptions;
    writeOptions.scfBlockBytes = 128;
    const houio::GeometryWriteResult writeResult =
        houio::GeometryIO::writeHouGeo(path, createPointGeometry(), writeOptions);
    if( !writeResult )
    {
        if( containsCategory(writeResult.diagnostics, houio::DiagnosticCategory::unsupported_input)
            && std::getenv("HOUIO_BLOSC_LIBRARY") == nullptr )
        {
            std::cout << "SCF test skipped because no C-Blosc runtime was found\n";
            return 0;
        }
        return fail("SCF write failed");
    }
    if( houio::GeometryIO::detectFormat(path) != houio::GeometryFileFormat::bgeo_scf )
        return fail("SCF format detection failed");

    const houio::GeometryReadResult<houio::HouGeo::Ptr> readResult = houio::GeometryIO::readHouGeo(path);
    if( !readResult || readResult.value->pointCount() != 2 || readResult.value->primitiveCount() != 0 )
        return fail("SCF round-trip failed");

    std::vector<char> scfBytes = readBytes(path);
    const std::uint64_t indexBytes = readBigUInt64(scfBytes, scfBytes.size() - 12);
    if( indexBytes <= 16 )
        return fail("SCF test did not produce multiple compressed blocks");

    const std::size_t indexStart = scfBytes.size() - 12 - static_cast<std::size_t>(indexBytes);
    scfBytes[indexStart + 7] ^= 1;
    writeBytes(path, scfBytes);
    const houio::GeometryReadResult<houio::HouGeo::Ptr> corruptIndexResult =
        houio::GeometryIO::readHouGeo(path);
    if( corruptIndexResult
        || !containsCategory(corruptIndexResult.diagnostics, houio::DiagnosticCategory::malformed_input) )
    {
        return fail("corrupted SCF index was not rejected");
    }

    const houio::GeometryWriteResult rewriteResult =
        houio::GeometryIO::writeHouGeo(path, createPointGeometry(), writeOptions);
    if( !rewriteResult )
        return fail("SCF rewrite failed");
    scfBytes = readBytes(path);
    scfBytes.back() = 'x';
    writeBytes(path, scfBytes);
    const houio::GeometryReadResult<houio::HouGeo::Ptr> corruptFooterResult =
        houio::GeometryIO::readHouGeo(path);
    if( corruptFooterResult
        || !containsCategory(corruptFooterResult.diagnostics, houio::DiagnosticCategory::malformed_input) )
    {
        return fail("corrupted SCF footer was not rejected");
    }
    return 0;
}

int verifyMultipleVolumes(const std::filesystem::path &directory)
{
    houio::ScalarField::Ptr first = houio::ScalarField::create(houio::math::V3i(2, 1, 1));
    houio::ScalarField::Ptr second = houio::ScalarField::create(houio::math::V3i(2, 1, 1));
    first->voxel(0, 0, 0) = 1.0f;
    first->voxel(1, 0, 0) = 2.0f;
    second->voxel(0, 0, 0) = 3.0f;
    second->voxel(1, 0, 0) = 4.0f;

    houio::HouGeo::Ptr geometry = houio::HouGeo::create();
    geometry->addPrimitive(first);
    geometry->addPrimitive(second);
    const std::filesystem::path path = directory / "volumes.bgeo";
    if( !houio::GeometryIO::writeHouGeo(path, geometry) )
        return fail("multiple-volume write failed");

    const auto volumesResult = houio::GeometryIO::readVolumes(path);
    if( !volumesResult || volumesResult.value.size() != 2
        || volumesResult.value[0]->voxel(1, 0, 0) != 2.0f
        || volumesResult.value[1]->voxel(0, 0, 0) != 3.0f )
    {
        return fail("readVolumes did not preserve both fields");
    }

    const auto firstVolumeResult = houio::GeometryIO::readVolume(path);
    if( !firstVolumeResult || firstVolumeResult.value->voxel(1, 0, 0) != 2.0f
        || !containsCategory(firstVolumeResult.diagnostics, houio::DiagnosticCategory::conversion) )
    {
        return fail("readVolume did not report first-volume conversion loss");
    }
    return 0;
}

int verifyInvalidReadOptions(const std::filesystem::path &directory)
{
    houio::GeometryReadOptions options;
    options.parserLimits.maxInputBytes = -1;
    const auto invalidLimitResult =
        houio::GeometryIO::readHouGeo(directory / "points.bgeo", options);
    if( invalidLimitResult
        || !containsCategory(invalidLimitResult.diagnostics, houio::DiagnosticCategory::malformed_input) )
    {
        return fail("invalid parser limits were not captured by GeometryIO");
    }

    options = houio::GeometryReadOptions();
    options.parserLimits.maxNestingDepth = 0;
    const auto invalidDepthResult =
        houio::GeometryIO::readHouGeo(directory / "points.bgeo", options);
    if( invalidDepthResult
        || !containsCategory(invalidDepthResult.diagnostics, houio::DiagnosticCategory::malformed_input) )
    {
        return fail("invalid parser nesting depth was not captured by GeometryIO");
    }
    return 0;
}

int verifyLegacyFailureContract(const std::filesystem::path &directory)
{
    const std::filesystem::path missingPath = directory / "missing.bgeo";
    bool threwDiagnostic = false;
    try
    {
        static_cast<void>(houio::HouGeoIO::importGeometry(missingPath.string()));
    }
    catch( const houio::DiagnosticException &exception )
    {
        threwDiagnostic = exception.diagnostic().category == houio::DiagnosticCategory::io;
    }
    if( !threwDiagnostic )
        return fail("compatibility importGeometry did not throw without a diagnostic list");

    houio::DiagnosticList diagnostics;
    const houio::Geometry::Ptr geometry =
        houio::HouGeoIO::importGeometry(missingPath.string(), &diagnostics);
    if( geometry || !containsCategory(diagnostics, houio::DiagnosticCategory::io) )
        return fail("compatibility diagnostics overload did not return an I/O diagnostic");
    return 0;
}

int verifyVdbContract(const std::filesystem::path &directory)
{
    const std::filesystem::path path = directory / "density.cache";
    std::ofstream output(path, std::ios::binary);
    output.write(" BDV", 4);
    output << "not-a-complete-vdb";
    output.close();

    if( houio::GeometryIO::detectFormat(path) != houio::GeometryFileFormat::openvdb )
        return fail("VDB signature detection failed");
    const houio::GeometryReadResult<houio::HouGeo::Ptr> result = houio::GeometryIO::readHouGeo(path);
    if( result || !containsCategory(result.diagnostics, houio::DiagnosticCategory::unsupported_input) )
        return fail("native VDB fallback contract failed");
    return 0;
}
}

int main()
{
    const std::filesystem::path testDirectory =
        std::filesystem::temp_directory_path() / "houio_geometry_io_test";
    std::error_code cleanupError;
    std::filesystem::remove_all(testDirectory, cleanupError);
    std::filesystem::create_directories(testDirectory);

    if( const int result = verifyRawRoundtrip(testDirectory); result != 0 )
        return result;
    if( const int result = verifyScfRoundtrip(testDirectory); result != 0 )
        return result;
    if( const int result = verifyMultipleVolumes(testDirectory); result != 0 )
        return result;
    if( const int result = verifyInvalidReadOptions(testDirectory); result != 0 )
        return result;
    if( const int result = verifyLegacyFailureContract(testDirectory); result != 0 )
        return result;
    if( const int result = verifyVdbContract(testDirectory); result != 0 )
        return result;

    std::filesystem::remove_all(testDirectory, cleanupError);
    return 0;
}
