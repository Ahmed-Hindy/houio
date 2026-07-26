#include <houio/FieldIO.h>
#include <houio/HouGeoIO.h>

#include "TestSupport.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using houio::test::expectThrows;
using houio::test::fail;

std::string positionAttribute(const std::string& storage = "fpreal32")
{
    const bool integerStorage = storage == "int32";
    const std::string values = integerStorage ? "[[10, 20, 30]]" : "[[10.0, 20.0, 30.0]]";
    return "[[\"scope\", \"public\", \"type\", \"numeric\", \"name\", \"P\"],"
           "[\"size\", 3, \"storage\", \"" + storage
        + "\", \"values\", [\"size\", 3, \"storage\", \"" + storage
        + "\", \"tuples\", " + values + "]]]";
}

std::string makeVolumeDocument(const std::string& primitiveData, bool includePosition = true,
                               const std::string& positionStorage = "fpreal32")
{
    const std::string pointAttributes = includePosition ? "[" + positionAttribute(positionStorage) + "]" : "[]";
    return "["
           "\"pointcount\", 1,"
           "\"vertexcount\", 1,"
           "\"primitivecount\", 1,"
           "\"topology\", [\"pointref\", [\"indices\", [0]]],"
           "\"attributes\", [\"pointattributes\", " + pointAttributes + "],"
           "\"primitives\", [[[\"type\", \"Volume\"], [" + primitiveData + "]]]"
           "]";
}

std::string identityTransform()
{
    return "[1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]";
}

houio::HouGeo::HouVolume::Ptr importVolume(const std::string& source, houio::DiagnosticList* diagnostics)
{
    std::istringstream input(source);
    houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(input, diagnostics);
    if (!geometry)
    {
        return houio::HouGeo::HouVolume::Ptr();
    }

    const std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives = geometry->primitives();
    if (primitives.size() != 1)
    {
        return houio::HouGeo::HouVolume::Ptr();
    }
    return std::dynamic_pointer_cast<houio::HouGeo::HouVolume>(primitives.front());
}

int verifyConstantArray()
{
    const std::string primitiveData =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [2, 2, 2], \"voxels\", [\"constantarray\", 3.25]";
    houio::DiagnosticList diagnostics;
    houio::HouGeo::HouVolume::Ptr volume = importVolume(makeVolumeDocument(primitiveData), &diagnostics);
    if (!volume || !diagnostics.empty())
    {
        return fail("constant volume did not import cleanly");
    }
    const houio::math::V3i resolution = volume->resolution();
    if (resolution.x != 2 || resolution.y != 2 || resolution.z != 2)
    {
        return fail("constant volume resolution was not preserved");
    }
    if (volume->topologyVertex() != 0)
    {
        return fail("constant volume topology vertex was not preserved");
    }
    for (int z = 0; z < 2; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                if (std::abs(volume->voxelValue(x, y, z) - 3.25f) > 1.0e-6f)
                {
                    return fail("constant volume value mismatch");
                }
            }
        }
    }
    return 0;
}

std::string firstTileValues()
{
    std::ostringstream values;
    values << '[';
    for (int index = 0; index < 16; ++index)
    {
        if (index != 0)
        {
            values << ", ";
        }
        values << static_cast<float>(index) << ".0";
    }
    values << ']';
    return values.str();
}

int verifyBoundaryTiles()
{
    const std::string voxelData =
        "[\"tiledarray\", [\"tiles\", [[\"compression\", 0, \"data\", " + firstTileValues()
        + "], [\"compression\", 2, \"data\", 42.0]]]]";
    const std::string primitiveData =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [17, 1, 1], \"voxels\", " + voxelData;

    houio::DiagnosticList diagnostics;
    houio::HouGeo::HouVolume::Ptr volume = importVolume(makeVolumeDocument(primitiveData), &diagnostics);
    if (!volume || !diagnostics.empty())
    {
        return fail("tiled boundary volume did not import cleanly");
    }
    for (int x = 0; x < 16; ++x)
    {
        if (std::abs(volume->voxelValue(x, 0, 0) - static_cast<float>(x)) > 1.0e-6f)
        {
            return fail("raw boundary tile value mismatch");
        }
    }
    if (std::abs(volume->voxelValue(16, 0, 0) - 42.0f) > 1.0e-6f)
    {
        return fail("constant boundary tile value mismatch");
    }
    return 0;
}

int expectFailure(const std::string& source, houio::DiagnosticCategory expectedCategory,
                  const std::string& expectedPath, const std::string& description)
{
    houio::DiagnosticList diagnostics;
    if (importVolume(source, &diagnostics))
    {
        return fail(description + " was accepted");
    }
    if (diagnostics.empty())
    {
        return fail(description + " did not produce a diagnostic");
    }
    const houio::Diagnostic& diagnostic = diagnostics.back();
    if (diagnostic.category != expectedCategory)
    {
        return fail(description + " produced the wrong diagnostic category");
    }
    if (diagnostic.path != expectedPath)
    {
        return fail(description + " path mismatch: " + diagnostic.path);
    }
    return 0;
}

int verifyBinaryRoundTrip()
{
    const houio::math::V3i resolution(17, 2, 1);
    houio::ScalarField::Ptr sourceField = houio::ScalarField::create(resolution);
    for (int z = 0; z < resolution.z; ++z)
    {
        for (int y = 0; y < resolution.y; ++y)
        {
            for (int x = 0; x < resolution.x; ++x)
            {
                sourceField->voxel(x, y, z) = static_cast<float>(x + y * 100 + z * 1000);
            }
        }
    }

    const houio::math::M44f sourceTransform = houio::math::M44f::scaleMatrix(2.0f, 3.0f, 4.0f)
        * houio::math::M44f::translationMatrix(5.0f, 6.0f, 7.0f);
    sourceField->setLocalToWorld(sourceTransform);

    houio::HouGeo::Ptr sourceGeometry = houio::HouGeo::create();
    sourceGeometry->addPrimitive(sourceField);
    const std::vector<houio::HouGeoAdapter::Primitive::Ptr> sourcePrimitives =
        sourceGeometry->primitives();
    houio::HouGeo::HouVolume::Ptr sourceVolume = sourcePrimitives.size() == 1
        ? std::dynamic_pointer_cast<houio::HouGeo::HouVolume>(sourcePrimitives.front())
        : houio::HouGeo::HouVolume::Ptr();
    if (!sourceVolume)
    {
        return fail("volume source primitive was not created");
    }
    sourceVolume->setVisualization("iso", 0.125f, 0.75f);

    std::ostringstream output(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::exportGeometry(output, sourceGeometry))
    {
        return fail("volume binary export failed");
    }
    if (output.str().find("compressiontypes") == std::string::npos)
    {
        return fail("volume binary export used an invalid tiled-array compression key");
    }
    if (output.str().find("iso") == std::string::npos)
    {
        return fail("volume binary export lost the visualization mode");
    }

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::DiagnosticList diagnostics;
    houio::HouGeo::Ptr importedGeometry = houio::HouGeoIO::import(input, &diagnostics);
    if (!importedGeometry || !diagnostics.empty())
    {
        return fail("volume binary re-import failed");
    }

    const std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives =
        importedGeometry->primitives();
    houio::HouGeo::HouVolume::Ptr importedVolume = primitives.size() == 1
        ? std::dynamic_pointer_cast<houio::HouGeo::HouVolume>(primitives.front())
        : houio::HouGeo::HouVolume::Ptr();
    if (!importedVolume || importedVolume->topologyVertex() != 0)
    {
        return fail("volume binary round-trip lost primitive metadata");
    }
    if (importedVolume->visualizationMode() != "iso"
        || std::abs(importedVolume->visualizationIso() - 0.125f) > 1.0e-6f
        || std::abs(importedVolume->visualizationDensity() - 0.75f) > 1.0e-6f)
    {
        return fail("volume binary round-trip lost visualization metadata");
    }

    const houio::math::V3i importedResolution = importedVolume->resolution();
    if (importedResolution.x != resolution.x || importedResolution.y != resolution.y
        || importedResolution.z != resolution.z)
    {
        return fail("volume binary round-trip lost resolution");
    }
    for (int z = 0; z < resolution.z; ++z)
    {
        for (int y = 0; y < resolution.y; ++y)
        {
            for (int x = 0; x < resolution.x; ++x)
            {
                if (std::abs(importedVolume->voxelValue(x, y, z) - sourceField->voxel(x, y, z)) > 1.0e-6f)
                {
                    return fail("volume binary round-trip lost voxel values");
                }
            }
        }
    }

    const houio::math::M44f importedTransform = importedVolume->transform();
    for (int component = 0; component < 16; ++component)
    {
        if (std::abs(importedTransform.ma[component] - sourceTransform.ma[component]) > 1.0e-5f)
        {
            return fail("volume binary round-trip lost the local-to-world transform");
        }
    }
    return 0;
}

int verifyMalformedVolumes()
{
    const std::string validPrefix = "\"vertex\", 0, \"transform\", " + identityTransform();
    const std::string constantVoxels = "\"voxels\", [\"constantarray\", 1.0]";

    if (const int result = expectFailure(
            makeVolumeDocument(validPrefix + ", \"res\", [0, 2, 2], " + constantVoxels),
            houio::DiagnosticCategory::schema, "primitives[0].data.res", "zero volume resolution");
        result != 0)
    {
        return result;
    }
    if (const int result = expectFailure(
            makeVolumeDocument("\"vertex\", 0, \"transform\", [1.0], \"res\", [2, 2, 2], "
                               + constantVoxels),
            houio::DiagnosticCategory::schema, "primitives[0].data.transform", "short volume transform");
        result != 0)
    {
        return result;
    }
    if (const int result = expectFailure(
            makeVolumeDocument("\"vertex\", 1, \"transform\", " + identityTransform()
                               + ", \"res\", [2, 2, 2], " + constantVoxels),
            houio::DiagnosticCategory::schema, "primitives[0].data.vertex", "volume topology vertex overflow");
        result != 0)
    {
        return result;
    }
    if (const int result = expectFailure(
            makeVolumeDocument(validPrefix + ", \"res\", [2, 2, 2], " + constantVoxels, false),
            houio::DiagnosticCategory::schema, "primitives[0].data.vertex", "volume without P");
        result != 0)
    {
        return result;
    }
    if (const int result = expectFailure(
            makeVolumeDocument(validPrefix + ", \"res\", [2, 2, 2], " + constantVoxels, true, "int32"),
            houio::DiagnosticCategory::unsupported_input, "primitives[0].data.vertex.P.storage",
            "volume with integer P");
        result != 0)
    {
        return result;
    }

    const std::string missingTile =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [17, 1, 1], \"voxels\", [\"tiledarray\", [\"tiles\", "
          "[[\"compression\", 2, \"data\", 1.0]]]]";
    if (const int result = expectFailure(makeVolumeDocument(missingTile), houio::DiagnosticCategory::schema,
                                         "primitives[0].data.voxels", "volume tile count mismatch");
        result != 0)
    {
        return result;
    }

    const std::string shortTile =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [2, 2, 2], \"voxels\", [\"tiledarray\", [\"tiles\", "
          "[[\"compression\", 0, \"data\", [1.0]]]]]";
    if (const int result = expectFailure(
            makeVolumeDocument(shortTile), houio::DiagnosticCategory::schema,
            "primitives[0].data.voxels.tiledarray.tiles[0]", "volume tile payload mismatch");
        result != 0)
    {
        return result;
    }

    const std::string extraTile =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [1, 1, 1], \"voxels\", [\"tiledarray\", [\"tiles\", "
          "[[\"compression\", 2, \"data\", 1.0], [\"compression\", 2, \"data\", 2.0]]]]";
    if (const int result = expectFailure(makeVolumeDocument(extraTile), houio::DiagnosticCategory::schema,
            "primitives[0].data.voxels", "extra volume tile"); result != 0)
    {
        return result;
    }

    const std::string nonObjectTile =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [1, 1, 1], \"voxels\", [\"tiledarray\", [\"tiles\", [1.0]]]";
    if (const int result = expectFailure(makeVolumeDocument(nonObjectTile), houio::DiagnosticCategory::schema,
            "primitives[0].data.voxels.tiledarray.tiles[0]", "non-object volume tile"); result != 0)
    {
        return result;
    }

    const std::string missingTileData =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [1, 1, 1], \"voxels\", [\"tiledarray\", [\"tiles\", "
          "[[\"compression\", 2]]]]";
    if (const int result = expectFailure(makeVolumeDocument(missingTileData), houio::DiagnosticCategory::schema,
            "primitives[0].data.voxels.tiledarray.tiles[0]", "volume tile without data"); result != 0)
    {
        return result;
    }

    const std::string scalarRawTile =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [1, 1, 1], \"voxels\", [\"tiledarray\", [\"tiles\", "
          "[[\"compression\", 0, \"data\", 1.0]]]]";
    if (const int result = expectFailure(makeVolumeDocument(scalarRawTile), houio::DiagnosticCategory::schema,
            "primitives[0].data.voxels.tiledarray.tiles[0]", "scalar raw volume tile"); result != 0)
    {
        return result;
    }

    const std::string arrayConstantTile =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [1, 1, 1], \"voxels\", [\"tiledarray\", [\"tiles\", "
          "[[\"compression\", 2, \"data\", [1.0]]]]]";
    if (const int result = expectFailure(makeVolumeDocument(arrayConstantTile), houio::DiagnosticCategory::schema,
            "primitives[0].data.voxels.tiledarray.tiles[0]", "array constant volume tile"); result != 0)
    {
        return result;
    }

    const std::string invalidCompressionType =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [1, 1, 1], \"voxels\", [\"tiledarray\", [\"tiles\", "
          "[[\"compression\", \"two\", \"data\", 1.0]]]]";
    if (const int result = expectFailure(makeVolumeDocument(invalidCompressionType),
            houio::DiagnosticCategory::malformed_input, "primitives[0].data.voxels.tiledarray.tiles[0]",
            "non-integer volume compression"); result != 0)
    {
        return result;
    }

    const int maximum = std::numeric_limits<int>::max();
    const std::string oversizedTileGrid =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [" + std::to_string(maximum) + ", " + std::to_string(maximum)
        + ", 1], \"voxels\", [\"tiledarray\", [\"tiles\", []]]";
    if (const int result = expectFailure(makeVolumeDocument(oversizedTileGrid),
            houio::DiagnosticCategory::schema, "primitives[0].data.voxels",
            "oversized volume tile grid"); result != 0)
    {
        return result;
    }

    const std::string unsupportedCompression =
        "\"vertex\", 0, \"transform\", " + identityTransform()
        + ", \"res\", [1, 1, 1], \"voxels\", [\"tiledarray\", [\"tiles\", "
          "[[\"compression\", 4, \"data\", 1.0]]]]";
    return expectFailure(makeVolumeDocument(unsupportedCompression), houio::DiagnosticCategory::unsupported_input,
                         "primitives[0].data.voxels.tiledarray.tiles[0].compression",
                         "unsupported volume compression");
}

int verifyFieldStorage()
{
    const std::filesystem::path storagePath = std::filesystem::temp_directory_path() / "houio_field_storage.bin";
    const std::filesystem::path truncatedPath = std::filesystem::temp_directory_path() / "houio_field_truncated.bin";
    const std::filesystem::path compactPath = std::filesystem::temp_directory_path() / "houio_field_compact.bin";
    const std::filesystem::path missingPayloadPath =
        std::filesystem::temp_directory_path() / "houio_field_missing_payload.bin";

    houio::ScalarField source;
    source.resize(2, 2, 1);
    source.voxel(0, 0, 0) = 1.0f;
    source.voxel(1, 0, 0) = 2.0f;
    source.voxel(0, 1, 0) = 3.0f;
    source.voxel(1, 1, 0) = 4.0f;
    if (!houio::storeField(source, storagePath.string()))
    {
        return fail("field storage write failed");
    }

    houio::ScalarField::Ptr loaded = houio::loadField<float>(storagePath.string());
    std::filesystem::remove(storagePath);
    if (!loaded || std::abs(loaded->voxel(1, 1, 0) - 4.0f) > 1.0e-6f)
    {
        return fail("field storage round-trip failed");
    }

    {
        std::ofstream truncatedOutput(truncatedPath, std::ios::binary | std::ios::trunc);
        truncatedOutput.write("bad", 3);
    }
    loaded = houio::loadField<float>(truncatedPath.string());
    std::filesystem::remove(truncatedPath);
    if (loaded)
    {
        return fail("truncated field storage was accepted");
    }

    {
        std::ofstream missingPayloadOutput(missingPayloadPath, std::ios::binary | std::ios::trunc);
        const auto writeValue = [&missingPayloadOutput](const auto& value)
        {
            missingPayloadOutput.write(
                reinterpret_cast<const char*>(&value),
                static_cast<std::streamsize>(sizeof(value)));
        };
        const int resolutionX = 50000;
        const int resolutionY = 50000;
        const int resolutionZ = 1;
        const float minimum = 0.0f;
        const float maximum = 1.0f;
        const int dataType = 1;
        writeValue(resolutionX);
        writeValue(resolutionY);
        writeValue(resolutionZ);
        writeValue(minimum);
        writeValue(minimum);
        writeValue(minimum);
        writeValue(maximum);
        writeValue(maximum);
        writeValue(maximum);
        writeValue(dataType);
    }
    loaded = houio::loadField<float>(missingPayloadPath.string());
    std::filesystem::remove(missingPayloadPath);
    if (loaded)
    {
        return fail("field storage allocated before validating its payload size");
    }

    if (!houio::storeFieldWithoutBoundingBox(source, compactPath.string()))
    {
        return fail("compact field storage write failed");
    }
    const std::uintmax_t compactSize = std::filesystem::file_size(compactPath);
    std::filesystem::remove(compactPath);
    const std::uintmax_t expectedCompactSize = sizeof(int) * 4u + sizeof(float) * 4u;
    if (compactSize != expectedCompactSize)
    {
        return fail("compact field storage layout changed");
    }

    houio::ScalarField empty;
    empty.resize(0, 2, 2);
    if (!houio::storeField(empty, storagePath.string()))
    {
        return fail("empty field storage write failed");
    }
    std::filesystem::remove(storagePath);
    if (const int result = expectThrows<std::invalid_argument>(
            [&empty] { static_cast<void>(houio::field_maximum(empty)); },
            "field_maximum accepted an empty field"); result != 0)
    {
        return result;
    }

    float minimum = 0.0f;
    float maximum = 0.0f;
    if (const int result = expectThrows<std::invalid_argument>(
            [&empty, &minimum, &maximum] { houio::field_range(empty, minimum, maximum); },
            "field_range accepted an empty field"); result != 0)
    {
        return result;
    }
    return 0;
}

bool nearlyEqual(float left, float right, float tolerance = 1.0e-6f)
{
    return std::abs(left - right) <= tolerance;
}

bool nearlyEqual(
    const houio::math::V3f& left,
    const houio::math::V3f& right,
    float tolerance = 1.0e-6f)
{
    return nearlyEqual(left.x, right.x, tolerance)
        && nearlyEqual(left.y, right.y, tolerance)
        && nearlyEqual(left.z, right.z, tolerance);
}

bool nearlyEqual(
    const houio::math::M44f& left,
    const houio::math::M44f& right,
    float tolerance = 1.0e-6f)
{
    for (std::size_t component = 0; component < left.ma.size(); ++component)
    {
        if (!nearlyEqual(left.ma[component], right.ma[component], tolerance))
            return false;
    }
    return true;
}

int verifyConstFieldConversion()
{
    houio::ScalarField source;
    source.resize(2, 2, 1);
    source.setBound(houio::math::Box3f(-2.0f, -1.0f, 3.0f, 2.0f, 5.0f, 7.0f));
    source.voxel(0, 0, 0) = 1.25f;
    source.voxel(1, 0, 0) = -2.5f;
    source.voxel(0, 1, 0) = 3.75f;
    source.voxel(1, 1, 0) = 8.5f;

    const houio::ScalarField& immutable_source = source;
    const houio::Fieldd::Ptr converted = houio::Fieldd::create(immutable_source);
    if (!converted || converted->resolution() != source.resolution()
        || !nearlyEqual(converted->bound().minPoint, source.bound().minPoint)
        || !nearlyEqual(converted->bound().maxPoint, source.bound().maxPoint)
        || std::abs(converted->voxel(0, 0, 0) - 1.25) > 1.0e-12
        || std::abs(converted->voxel(1, 0, 0) + 2.5) > 1.0e-12
        || std::abs(converted->voxel(0, 1, 0) - 3.75) > 1.0e-12
        || std::abs(converted->voxel(1, 1, 0) - 8.5) > 1.0e-12)
    {
        return fail("const field conversion did not preserve metadata and values");
    }
    return 0;
}

int verifyScalarFieldSampling()
{
    houio::ScalarField field;
    field.resize(3, 2, 2);
    for (int z = 0; z < 2; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 3; ++x)
            {
                field.voxel(x, y, z) = static_cast<float>(x + y * 10 + z * 100);
            }
        }
    }

    const houio::ScalarField& sampledField = field;
    for (int z = 0; z < 2; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 3; ++x)
            {
                const float sampled = sampledField.evaluate(houio::math::V3f(
                    static_cast<float>(x) + 0.5f,
                    static_cast<float>(y) + 0.5f,
                    static_cast<float>(z) + 0.5f));
                if (!nearlyEqual(sampled, field.voxel(x, y, z)))
                {
                    return fail("field sampling did not reproduce an exact voxel center");
                }
            }
        }
    }

    if (!nearlyEqual(sampledField.evaluate(houio::math::V3f(1.0f)), 55.5f))
    {
        return fail("field trilinear center interpolation is incorrect");
    }
    if (!nearlyEqual(sampledField.evaluate(houio::math::V3f(1.25f, 0.75f, 1.1f)), 63.25f))
    {
        return fail("field arbitrary trilinear interpolation is incorrect");
    }
    if (!nearlyEqual(sampledField.evaluate(houio::math::V3f(-100.0f, 1.0f, 1.0f)), 55.0f)
        || !nearlyEqual(sampledField.evaluate(houio::math::V3f(100.0f, 1.0f, 1.0f)), 57.0f)
        || !nearlyEqual(sampledField.evaluate(houio::math::V3f(-100.0f)), 0.0f)
        || !nearlyEqual(sampledField.evaluate(houio::math::V3f(100.0f)), 112.0f))
    {
        return fail("field sampling did not clamp coordinates to boundary voxels");
    }

    houio::ScalarField singleAxisField;
    singleAxisField.resize(1, 2, 2);
    singleAxisField.voxel(0, 0, 0) = 0.0f;
    singleAxisField.voxel(0, 1, 0) = 10.0f;
    singleAxisField.voxel(0, 0, 1) = 100.0f;
    singleAxisField.voxel(0, 1, 1) = 110.0f;
    if (!nearlyEqual(singleAxisField.evaluate(houio::math::V3f(-50.0f, 1.0f, 1.0f)), 55.0f)
        || !nearlyEqual(singleAxisField.evaluate(houio::math::V3f(50.0f, 1.0f, 1.0f)), 55.0f))
    {
        return fail("field sampling failed for a single-voxel axis");
    }

    houio::ScalarField tileBoundaryField;
    tileBoundaryField.resize(18, 1, 1);
    for (int x = 0; x < 18; ++x)
    {
        tileBoundaryField.voxel(x, 0, 0) = static_cast<float>(x);
    }
    if (!nearlyEqual(tileBoundaryField.evaluate(houio::math::V3f(15.5f, 0.5f, 0.5f)), 15.0f)
        || !nearlyEqual(tileBoundaryField.evaluate(houio::math::V3f(16.0f, 0.5f, 0.5f)), 15.5f)
        || !nearlyEqual(tileBoundaryField.evaluate(houio::math::V3f(16.5f, 0.5f, 0.5f)), 16.0f)
        || !nearlyEqual(tileBoundaryField.evaluate(houio::math::V3f(17.0f, 0.5f, 0.5f)), 16.5f))
    {
        return fail("field interpolation failed across a 16-voxel tile boundary");
    }
    return 0;
}

int verifyVectorFieldSampling()
{
    houio::VectorField field;
    field.resize(2, 2, 2);
    for (int z = 0; z < 2; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                field.voxel(x, y, z) = houio::math::V3f(
                    static_cast<float>(x),
                    static_cast<float>(y * 2),
                    static_cast<float>(z * 4));
            }
        }
    }

    const houio::math::V3f interpolated = field.evaluate(houio::math::V3f(1.0f));
    if (!nearlyEqual(interpolated, houio::math::V3f(0.5f, 1.0f, 2.0f)))
    {
        return fail("vector field trilinear interpolation is incorrect");
    }
    if (!nearlyEqual(
            field.evaluate(houio::math::V3f(-10.0f, 1.0f, 10.0f)),
            houio::math::V3f(0.0f, 1.0f, 4.0f)))
    {
        return fail("vector field boundary clamping is incorrect");
    }
    return 0;
}

int verifyFieldCoordinateTransforms()
{
    houio::ScalarField field;
    field.resize(2, 3, 4);
    field.setBound(houio::math::Box3f(10.0f, 20.0f, 30.0f, 14.0f, 26.0f, 38.0f));

    const houio::math::V3f firstCenterWorld = field.voxelToWorld(houio::math::V3f(0.5f));
    if (!nearlyEqual(firstCenterWorld, houio::math::V3f(11.0f, 21.0f, 31.0f), 1.0e-5f))
    {
        return fail("voxel-to-world transform does not map the first voxel center correctly");
    }
    if (!nearlyEqual(field.worldToVoxel(firstCenterWorld), houio::math::V3f(0.5f), 1.0e-5f))
    {
        return fail("world-to-voxel transform did not invert voxel-to-world");
    }

    const houio::math::V3f lastCenterWorld = field.voxelToWorld(houio::math::V3f(1.5f, 2.5f, 3.5f));
    if (!nearlyEqual(lastCenterWorld, houio::math::V3f(13.0f, 25.0f, 37.0f), 1.0e-5f))
    {
        return fail("voxel-to-world transform does not map the last voxel center correctly");
    }
    if (!nearlyEqual(field.voxelSize(), houio::math::V3f(2.0f), 1.0e-5f))
    {
        return fail("axis-aligned field voxel spacing is incorrect");
    }

    const houio::math::V3f localPosition(0.25f, 0.5f, 0.75f);
    const houio::math::V3f voxelPosition = field.localToVoxel(localPosition);
    const houio::math::V3f worldPosition = field.localToWorld(localPosition);
    if (!nearlyEqual(voxelPosition, houio::math::V3f(0.5f, 1.5f, 3.0f), 1.0e-5f)
        || !nearlyEqual(field.voxelToLocal(voxelPosition), localPosition, 1.0e-5f)
        || !nearlyEqual(field.worldToLocal(worldPosition), localPosition, 1.0e-5f)
        || !nearlyEqual(field.worldToVoxel(worldPosition), voxelPosition, 1.0e-5f)
        || !nearlyEqual(field.voxelToWorld(voxelPosition), worldPosition, 1.0e-5f))
    {
        return fail("field local, voxel, and world transforms do not compose consistently");
    }

    houio::math::M44f rotatedTransform = houio::math::M44f::identity();
    rotatedTransform.scale(4.0f, 6.0f, 8.0f)
        .rotateZ(std::numbers::pi_v<float> * 0.5f)
        .translate(10.0f, 20.0f, 30.0f);
    field.setLocalToWorld(rotatedTransform);
    if (!nearlyEqual(field.voxelSize(), houio::math::V3f(2.0f), 1.0e-5f))
    {
        return fail("rotated field voxel spacing must use transformed basis lengths");
    }
    const houio::math::V3f rotatedWorldPosition = field.localToWorld(localPosition);
    if (!nearlyEqual(field.worldToLocal(rotatedWorldPosition), localPosition, 1.0e-5f)
        || !nearlyEqual(
            field.worldToVoxel(rotatedWorldPosition), field.localToVoxel(localPosition), 1.0e-5f))
    {
        return fail("rotated field coordinate transforms are inconsistent");
    }
    return 0;
}

int verifyFieldTransformMutationSafety()
{
    houio::ScalarField field;
    field.resize(2, 2, 2);
    field.setBound(houio::math::Box3f(1.0f, 2.0f, 3.0f, 5.0f, 8.0f, 11.0f));
    const houio::math::M44f originalTransform = field.localToWorldMatrix();
    const houio::math::Box3f originalBound = field.bound();

    if (const int result = expectThrows<std::domain_error>(
            [&field]
            {
                field.setLocalToWorld(houio::math::M44f::scaleMatrix(1.0f, 0.0f, 1.0f));
            },
            "singular field transform was accepted"); result != 0)
    {
        return result;
    }
    if (!nearlyEqual(field.localToWorldMatrix(), originalTransform)
        || !nearlyEqual(field.bound().minPoint, originalBound.minPoint)
        || !nearlyEqual(field.bound().maxPoint, originalBound.maxPoint))
    {
        return fail("singular field transform partially mutated field state");
    }

    if (const int result = expectThrows<std::invalid_argument>(
            [&field]
            {
                field.setBound(houio::math::Box3f(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
            },
            "empty field bound was accepted"); result != 0)
    {
        return result;
    }
    if (!nearlyEqual(field.localToWorldMatrix(), originalTransform)
        || !nearlyEqual(field.bound().minPoint, originalBound.minPoint)
        || !nearlyEqual(field.bound().maxPoint, originalBound.maxPoint))
    {
        return fail("invalid field bound partially mutated field state");
    }

    houio::math::M44f nonFiniteTransform = originalTransform;
    nonFiniteTransform.ma[0] = std::numeric_limits<float>::infinity();
    if (const int result = expectThrows<std::invalid_argument>(
            [&field, &nonFiniteTransform] { field.setLocalToWorld(nonFiniteTransform); },
            "non-finite field transform was accepted"); result != 0)
    {
        return result;
    }
    return 0;
}

int verifyFieldSamplingRejectsNonFiniteCoordinates()
{
    houio::ScalarField field;
    field.resize(1, 1, 1);
    field.voxel(0, 0, 0) = 1.0f;

    const float infinity = std::numeric_limits<float>::infinity();
    const float quietNaN = std::numeric_limits<float>::quiet_NaN();
    const houio::math::V3f invalidCoordinates[] = {
        houio::math::V3f(infinity, 0.5f, 0.5f),
        houio::math::V3f(0.5f, -infinity, 0.5f),
        houio::math::V3f(0.5f, 0.5f, quietNaN)};
    for (const houio::math::V3f& coordinates : invalidCoordinates)
    {
        if (const int result = expectThrows<std::invalid_argument>(
                [&field, coordinates] { static_cast<void>(field.evaluate(coordinates)); },
                "field sampling accepted non-finite coordinates"); result != 0)
        {
            return result;
        }
    }
    return 0;
}

int verifyFieldResizeSafety()
{
    houio::ScalarField field;
    field.resize(0, 2, 2);
    const houio::math::V3i emptyResolution = field.resolution();
    if (emptyResolution.x != 0 || emptyResolution.y != 2 || emptyResolution.z != 2)
    {
        return fail("empty field resolution was not retained");
    }

    if (const int result = expectThrows<std::invalid_argument>(
            [&field] { field.resize(-1, 2, 2); },
            "negative field resolution was accepted"); result != 0)
    {
        return result;
    }

    if (const int result = expectThrows<std::length_error>(
            [&field]
            {
                const int maximum = std::numeric_limits<int>::max();
                field.resize(maximum, maximum, maximum);
            },
            "overflowing field resolution was accepted"); result != 0)
    {
        return result;
    }
    return 0;
}
}

int main()
{
    if (const int result = verifyConstantArray(); result != 0)
    {
        return result;
    }
    if (const int result = verifyBoundaryTiles(); result != 0)
    {
        return result;
    }
    if (const int result = verifyBinaryRoundTrip(); result != 0)
    {
        return result;
    }
    if (const int result = verifyMalformedVolumes(); result != 0)
    {
        return result;
    }
    if (const int result = verifyFieldStorage(); result != 0)
    {
        return result;
    }
    if (const int result = verifyConstFieldConversion(); result != 0)
    {
        return result;
    }
    if (const int result = verifyScalarFieldSampling(); result != 0)
    {
        return result;
    }
    if (const int result = verifyVectorFieldSampling(); result != 0)
    {
        return result;
    }
    if (const int result = verifyFieldCoordinateTransforms(); result != 0)
    {
        return result;
    }
    if (const int result = verifyFieldTransformMutationSafety(); result != 0)
    {
        return result;
    }
    if (const int result = verifyFieldSamplingRejectsNonFiniteCoordinates(); result != 0)
    {
        return result;
    }
    return verifyFieldResizeSafety();
}
