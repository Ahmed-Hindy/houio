#include <houio/HomManifest.h>
#include <houio/Writer.h>

#include "TestSupport.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace
{
    using houio::test::fail;

    void writeText(const std::filesystem::path &path, const std::string &text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << text;
    }

    bool containsCategory(
        const houio::DiagnosticList &diagnostics,
        houio::DiagnosticCategory category)
    {
        for( const houio::Diagnostic &diagnostic : diagnostics )
        {
            if( diagnostic.category == category )
                return true;
        }
        return false;
    }

    int verifyMixedManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path manifestPath = directory / "mixed.json";
        writeText(manifestPath, R"JSON({
            "schema":"houio.hom/1",
            "point_count":4,
            "vertex_count":4,
            "primitive_count":2,
            "topology":[0,1,2,3],
            "primitives":[
                {"type":"polygon","vertex_offset":0,"vertex_count":3,"closed":true},
                {
                    "type":"dense_volume",
                    "vertex_offset":3,
                    "resolution":[1,1,1],
                    "local_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                    "voxels":[4.5],
                    "visualization":{"mode":"smoke","iso":0.25,"density":2.0}
                }
            ],
            "attributes":{
                "point":[
                    {
                        "name":"P","kind":"numeric","storage":"float32",
                        "tuple_size":4,"element_count":4,
                        "values":[0,0,0,1,1,0,0,1,0,1,0,1,0.5,0.5,0.5,1]
                    },
                    {
                        "name":"label","kind":"string","tuple_size":1,
                        "element_count":4,"values":["a","b","c","volume"]
                    }
                ],
                "vertex":[
                    {
                        "name":"uv","kind":"numeric","storage":"float32",
                        "tuple_size":2,"element_count":4,
                        "values":[0,0,1,0,0,1,0.5,0.5]
                    }
                ],
                "primitive":[
                    {
                        "name":"kind","kind":"string","tuple_size":1,
                        "element_count":2,"values":["polygon","volume"]
                    }
                ],
                "global":[
                    {
                        "name":"asset","kind":"string","tuple_size":1,
                        "element_count":1,"values":["manifest_test"]
                    }
                ]
            },
            "groups":{
                "point":{"selected":[0,3]},
                "vertex":{"corner":[1]},
                "primitive":{"volumes":[1]}
            }
        })JSON");

        const houio::GeometryReadResult<houio::HouGeo::Ptr> result =
            houio::HomManifest::read(manifestPath);
        if( !result )
        {
            for( const houio::Diagnostic &diagnostic : result.diagnostics )
                std::cerr << diagnostic.message << " [" << diagnostic.path << "]\n";
            return fail("mixed HOM manifest failed to load");
        }
        if( result.value->pointCount() != 4
            || result.value->vertexCount() != 4
            || result.value->primitiveCount() != 2 )
        {
            return fail("mixed HOM manifest domain counts are incorrect");
        }
        if( !result.value->pointAttribute("label")
            || !result.value->vertexAttribute("uv")
            || !result.value->primitiveAttribute("kind")
            || !result.value->globalAttribute("asset") )
        {
            return fail("mixed HOM manifest lost attributes");
        }
        const auto volumeGroup = result.value->primitiveGroupMembership("volumes");
        if( !volumeGroup || volumeGroup->size() != 2 || !(*volumeGroup)[1] )
            return fail("mixed HOM manifest lost primitive group membership");

        const auto primitives = result.value->primitives();
        if( primitives.size() != 2 )
            return fail("mixed HOM manifest did not retain two records");
        const auto volume = std::dynamic_pointer_cast<houio::HouGeo::HouVolume>(primitives[1]);
        if( !volume || volume->voxelValue(0, 0, 0) != 4.5f
            || volume->visualizationDensity() != 2.0f )
        {
            return fail("mixed HOM manifest did not retain dense volume data");
        }

        const std::filesystem::path outputPath = directory / "mixed.bgeo";
        const houio::WriteResult writeResult = houio::Writer::write(
            outputPath,
            std::static_pointer_cast<houio::HouGeoAdapter>(result.value));
        if( !writeResult )
            return fail("mixed HOM manifest failed custom serialization");
        const auto roundtrip = houio::GeometryIO::readHouGeo(outputPath);
        if( !roundtrip || roundtrip.value->primitiveCount() != 2
            || !roundtrip.value->vertexAttribute("uv") )
        {
            return fail("mixed HOM manifest output did not round-trip");
        }
        return 0;
    }

    int verifyInvalidTopologyOffsets(const std::filesystem::path &directory)
    {
        const std::filesystem::path densePath = directory / "invalid_dense_offset.json";
        writeText(densePath, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,
            "vertex_count":1,
            "primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"dense_volume","vertex_offset":1,"resolution":[1,1,1],
                "local_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "voxels":[1]
            }],
            "attributes":{
                "point":[{
                    "name":"P","kind":"numeric","storage":"float32",
                    "tuple_size":4,"element_count":1,"values":[0,0,0,1]
                }],
                "vertex":[],"primitive":[],"global":[]
            }
        })JSON");
        const auto denseResult = houio::HomManifest::read(densePath);
        if( denseResult
            || !containsCategory(denseResult.diagnostics, houio::DiagnosticCategory::schema) )
        {
            return fail("dense-volume manifest accepted an out-of-range topology vertex");
        }

        const std::filesystem::path packedPath = directory / "invalid_packed_offset.json";
        writeText(packedPath, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,
            "vertex_count":1,
            "primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"packed_geometry","vertex_offset":1,
                "pivot":[0,0,0],
                "transform":[1,0,0,0,1,0,0,0,1],
                "embedded_manifest":{
                    "schema":"houio.hom/1",
                    "point_count":0,"vertex_count":0,"primitive_count":0,
                    "topology":[],"primitives":[],
                    "attributes":{"point":[],"vertex":[],"primitive":[],"global":[]}
                }
            }],
            "attributes":{
                "point":[{
                    "name":"P","kind":"numeric","storage":"float32",
                    "tuple_size":4,"element_count":1,"values":[0,0,0,1]
                }],
                "vertex":[],"primitive":[],"global":[]
            }
        })JSON");
        const auto packedResult = houio::HomManifest::read(packedPath);
        if( packedResult
            || !containsCategory(packedResult.diagnostics, houio::DiagnosticCategory::schema) )
        {
            return fail("packed manifest accepted an out-of-range topology vertex");
        }
        return 0;
    }

    int verifyUnsupportedRecord(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "packed_fragment.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,
            "vertex_count":1,
            "primitive_count":1,
            "topology":[0],
            "primitives":[{"type":"packed_fragment","vertex_offset":0}],
            "attributes":{
                "point":[{
                    "name":"P","kind":"numeric","storage":"float32",
                    "tuple_size":4,"element_count":1,"values":[0,0,0,1]
                }],
                "vertex":[],"primitive":[],"global":[]
            }
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( result
            || !containsCategory(result.diagnostics, houio::DiagnosticCategory::unsupported_input) )
        {
            return fail("unsupported manifest record did not return an explicit diagnostic");
        }
        return 0;
    }
}

int main()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "houio_hom_manifest_test";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory, error);
    if( error )
        return fail("could not create HOM manifest test directory");

    if( const int result = verifyMixedManifest(directory); result != 0 )
        return result;
    if( const int result = verifyInvalidTopologyOffsets(directory); result != 0 )
        return result;
    if( const int result = verifyUnsupportedRecord(directory); result != 0 )
        return result;

    std::filesystem::remove_all(directory, error);
    return 0;
}
