#include <houio/GeometryIO.h>
#include <houio/HouGeoIO.h>

#include "TestSupport.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using houio::test::fail;

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

int verifyVariablePolygonRoundtrip(const std::filesystem::path &directory)
{
    const houio::Geometry::Ptr geometry = houio::Geometry::createPolyGeometry();
    for (unsigned int pointIndex = 0; pointIndex < 8; ++pointIndex)
    {
        geometry->attribute("P")->appendElement(houio::math::V3f(
            static_cast<float>(pointIndex),
            0.0f,
            0.0f));
    }
    const std::array<houio::Geometry::Index, 5> pentagon = {0, 1, 2, 3, 4};
    const std::array<houio::Geometry::Index, 3> triangle = {5, 6, 7};
    geometry->addPolygon(pentagon);
    geometry->addPolygon(triangle);

    const std::filesystem::path path = directory / "variable_polygons.bgeo";
    const houio::GeometryWriteResult writeResult =
        houio::GeometryIO::writeGeometry(path, geometry);
    if (!writeResult)
        return fail("variable polygon write failed");

    const houio::GeometryReadResult<houio::HouGeo::Ptr> faithfulRead =
        houio::GeometryIO::readHouGeo(path);
    if (!faithfulRead || faithfulRead.value->primitiveCount() != 2)
        return fail("variable polygon faithful read returned incorrect primitive count");
    const houio::HouGeo& faithfulGeometry = *faithfulRead.value;
    const std::vector<houio::HouGeoAdapter::Primitive::ConstPtr> primitives =
        faithfulGeometry.primitives();
    const auto polygons = primitives.empty()
        ? houio::HouGeo::HouPoly::ConstPtr()
        : std::dynamic_pointer_cast<const houio::HouGeo::HouPoly>(primitives.front());
    if (primitives.size() != 1 || !polygons || polygons->polygonCount() != 2
        || polygons->polygonVertexCount(0) != 5
        || polygons->polygonVertexCount(1) != 3)
    {
        return fail("variable polygon write lost exact primitive boundaries");
    }

    const houio::GeometryReadResult<houio::Geometry::Ptr> simplifiedRead =
        houio::GeometryIO::readGeometry(path);
    const std::span<const unsigned int> counts = simplifiedRead.value
        ? simplifiedRead.value->primitiveVertexCounts() : std::span<const unsigned int>();
    if (!simplifiedRead
        || simplifiedRead.value->primitiveType() != houio::Geometry::PrimitiveType::polygon
        || simplifiedRead.value->primitiveCount() != 2
        || counts.size() != 2 || counts[0] != 5 || counts[1] != 3)
    {
        return fail("variable polygon simplified round-trip lost primitive boundaries");
    }
    return 0;
}

int verifyAllDomainRoundtrip(const std::filesystem::path &directory)
{
    const houio::Geometry::Ptr geometry = houio::Geometry::createTriangleGeometry();
    houio::Attribute::Ptr positions = geometry->pointAttribute("P");
    positions->appendElement(houio::math::V3f(0.0f, 0.0f, 0.0f));
    positions->appendElement(houio::math::V3f(1.0f, 0.0f, 0.0f));
    positions->appendElement(houio::math::V3f(0.0f, 1.0f, 0.0f));
    geometry->addTriangle(0, 1, 2);

    houio::Attribute::Ptr mask = std::make_shared<houio::Attribute>(
        1, houio::Attribute::ComponentType::uint8);
    mask->appendElement<houio::ubyte>(0);
    mask->appendElement<houio::ubyte>(128);
    mask->appendElement<houio::ubyte>(255);
    geometry->setPointAttribute("mask", mask);

    houio::Attribute::Ptr uv = houio::Attribute::createV2f();
    uv->appendElement(houio::math::V2f(0.0f, 0.0f));
    uv->appendElement(houio::math::V2f(1.0f, 0.0f));
    uv->appendElement(houio::math::V2f(0.0f, 1.0f));
    geometry->setVertexAttribute("UV", uv);

    houio::Attribute::Ptr primitive_id = houio::Attribute::createInt();
    primitive_id->appendElement<houio::sint32>(42);
    geometry->setPrimitiveAttribute("id", primitive_id);

    houio::Attribute::Ptr scale = houio::Attribute::createFloat();
    scale->appendElement(2.5f);
    geometry->setGlobalAttribute("scale", scale);

    const std::filesystem::path path = directory / "all_domains.bgeo";
    const houio::GeometryWriteResult write_result =
        houio::GeometryIO::writeGeometry(path, geometry);
    if (!write_result)
    {
        for (const houio::Diagnostic& diagnostic : write_result.diagnostics)
            std::cerr << diagnostic.message << " [" << diagnostic.path << "]\n";
        return fail("all-domain simplified geometry write failed");
    }

    const houio::GeometryReadResult<houio::HouGeo::Ptr> faithful =
        houio::GeometryIO::readHouGeo(path);
    if (!faithful
        || !faithful.value->pointAttribute("mask")
        || !faithful.value->vertexAttribute("UV")
        || !faithful.value->primitiveAttribute("id")
        || !faithful.value->globalAttribute("scale")
        || faithful.value->vertexAttribute("UV")->elementCount() != 3
        || faithful.value->primitiveAttribute("id")->rawData().read<houio::sint32>(0) != 42
        || faithful.value->globalAttribute("scale")->rawData().read<houio::real32>(0) != 2.5f)
    {
        return fail("all-domain BGEO write lost faithful attribute domains");
    }

    const houio::GeometryReadResult<houio::Geometry::Ptr> simplified =
        houio::GeometryIO::readGeometry(path);
    const houio::Geometry::Ptr result = simplified.value;
    const houio::Attribute::Ptr result_mask = result ? result->pointAttribute("mask") : nullptr;
    const houio::Attribute::Ptr result_uv = result ? result->vertexAttribute("UV") : nullptr;
    const houio::Attribute::Ptr result_id = result ? result->primitiveAttribute("id") : nullptr;
    const houio::Attribute::Ptr result_scale = result ? result->globalAttribute("scale") : nullptr;
    if (!simplified || !result || result->primitiveCount() != 1
        || result->indexBuffer().size() != 3
        || result->indexBuffer()[0] != 0 || result->indexBuffer()[1] != 1
        || result->indexBuffer()[2] != 2
        || !result_mask || result_mask->get<houio::ubyte>(1) != 128
        || !result_uv || result_uv->numElements() != 3
        || result_uv->get<houio::math::V2f>(0) != houio::math::V2f(0.0f, 0.0f)
        || result_uv->get<houio::math::V2f>(1) != houio::math::V2f(1.0f, 0.0f)
        || result_uv->get<houio::math::V2f>(2) != houio::math::V2f(0.0f, 1.0f)
        || !result_id || result_id->get<houio::sint32>(0) != 42
        || !result_scale || result_scale->get<houio::real32>(0) != 2.5f)
    {
        return fail("all-domain simplified BGEO round-trip changed data or winding");
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
    if( const int result = verifyVariablePolygonRoundtrip(testDirectory); result != 0 )
        return result;
    if( const int result = verifyAllDomainRoundtrip(testDirectory); result != 0 )
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
