#include <houio/HouGeoIO.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

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
    houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(&input, diagnostics);
    if (!geometry)
    {
        return houio::HouGeo::HouVolume::Ptr();
    }

    std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives;
    geometry->getPrimitives(primitives);
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
    const houio::math::V3i resolution = volume->getResolution();
    if (resolution.x != 2 || resolution.y != 2 || resolution.z != 2)
    {
        return fail("constant volume resolution was not preserved");
    }
    if (volume->getVertex() != 0)
    {
        return fail("constant volume topology vertex was not preserved");
    }
    for (int z = 0; z < 2; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                if (std::abs(volume->getVoxel(x, y, z) - 3.25f) > 1.0e-6f)
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
        if (std::abs(volume->getVoxel(x, 0, 0) - static_cast<float>(x)) > 1.0e-6f)
        {
            return fail("raw boundary tile value mismatch");
        }
    }
    if (std::abs(volume->getVoxel(16, 0, 0) - 42.0f) > 1.0e-6f)
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
                sourceField->lvalue(x, y, z) = static_cast<float>(x + y * 100 + z * 1000);
            }
        }
    }

    const houio::math::M44f sourceTransform = houio::math::M44f::ScaleMatrix(2.0f, 3.0f, 4.0f)
        * houio::math::M44f::TranslationMatrix(5.0f, 6.0f, 7.0f);
    sourceField->setLocalToWorld(sourceTransform);

    houio::HouGeo::Ptr sourceGeometry = houio::HouGeo::create();
    sourceGeometry->addPrimitive(sourceField);
    std::vector<houio::HouGeoAdapter::Primitive::Ptr> sourcePrimitives;
    sourceGeometry->getPrimitives(sourcePrimitives);
    houio::HouGeo::HouVolume::Ptr sourceVolume = sourcePrimitives.size() == 1
        ? std::dynamic_pointer_cast<houio::HouGeo::HouVolume>(sourcePrimitives.front())
        : houio::HouGeo::HouVolume::Ptr();
    if (!sourceVolume)
    {
        return fail("volume source primitive was not created");
    }
    sourceVolume->visualizationMode = "iso";
    sourceVolume->visualizationIso = 0.125f;
    sourceVolume->visualizationDensity = 0.75f;

    std::ostringstream output(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::xport(&output, sourceGeometry))
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
    houio::HouGeo::Ptr importedGeometry = houio::HouGeoIO::import(&input, &diagnostics);
    if (!importedGeometry || !diagnostics.empty())
    {
        return fail("volume binary re-import failed");
    }

    std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives;
    importedGeometry->getPrimitives(primitives);
    houio::HouGeo::HouVolume::Ptr importedVolume = primitives.size() == 1
        ? std::dynamic_pointer_cast<houio::HouGeo::HouVolume>(primitives.front())
        : houio::HouGeo::HouVolume::Ptr();
    if (!importedVolume || importedVolume->getVertex() != 0)
    {
        return fail("volume binary round-trip lost primitive metadata");
    }
    if (importedVolume->getVisualizationMode() != "iso"
        || std::abs(importedVolume->getVisualizationIso() - 0.125f) > 1.0e-6f
        || std::abs(importedVolume->getVisualizationDensity() - 0.75f) > 1.0e-6f)
    {
        return fail("volume binary round-trip lost visualization metadata");
    }

    const houio::math::V3i importedResolution = importedVolume->getResolution();
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
                if (std::abs(importedVolume->getVoxel(x, y, z) - sourceField->sample(x, y, z)) > 1.0e-6f)
                {
                    return fail("volume binary round-trip lost voxel values");
                }
            }
        }
    }

    const houio::math::M44f importedTransform = importedVolume->getTransform();
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

    houio::ScalarField source;
    source.resize(2, 2, 1);
    source.lvalue(0, 0, 0) = 1.0f;
    source.lvalue(1, 0, 0) = 2.0f;
    source.lvalue(0, 1, 0) = 3.0f;
    source.lvalue(1, 1, 0) = 4.0f;
    source.store(storagePath.string());

    houio::ScalarField::Ptr loaded = houio::ScalarField::load(storagePath.string());
    std::filesystem::remove(storagePath);
    if (!loaded || std::abs(loaded->sample(1, 1, 0) - 4.0f) > 1.0e-6f)
    {
        return fail("field storage round-trip failed");
    }

    {
        std::ofstream truncatedOutput(truncatedPath, std::ios::binary | std::ios::trunc);
        truncatedOutput.write("bad", 3);
    }
    loaded = houio::ScalarField::load(truncatedPath.string());
    std::filesystem::remove(truncatedPath);
    if (loaded)
    {
        return fail("truncated field storage was accepted");
    }

    houio::ScalarField empty;
    empty.resize(0, 2, 2);
    empty.store(storagePath.string());
    std::filesystem::remove(storagePath);
    try
    {
        static_cast<void>(houio::field_maximum(empty));
        return fail("field_maximum accepted an empty field");
    }
    catch (const std::invalid_argument&)
    {
    }

    float minimum = 0.0f;
    float maximum = 0.0f;
    try
    {
        houio::field_range(empty, minimum, maximum);
        return fail("field_range accepted an empty field");
    }
    catch (const std::invalid_argument&)
    {
    }
    return 0;
}

int verifyFieldResizeSafety()
{
    houio::ScalarField field;
    field.resize(0, 2, 2);
    const houio::math::V3i emptyResolution = field.getResolution();
    if (emptyResolution.x != 0 || emptyResolution.y != 2 || emptyResolution.z != 2)
    {
        return fail("empty field resolution was not retained");
    }

    try
    {
        field.resize(-1, 2, 2);
        return fail("negative field resolution was accepted");
    }
    catch (const std::invalid_argument&)
    {
    }

    try
    {
        const int maximum = std::numeric_limits<int>::max();
        field.resize(maximum, maximum, maximum);
        return fail("overflowing field resolution was accepted");
    }
    catch (const std::length_error&)
    {
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
    return verifyFieldResizeSafety();
}
