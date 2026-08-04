#include "HomManifestInternal.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <utility>

namespace houio::hom_manifest_detail
{
    [[noreturn]] void failManifest(
        DiagnosticCategory category,
        std::string message,
        std::string path)
    {
        throw DiagnosticException(Diagnostic{
            DiagnosticSeverity::error,
            category,
            std::move(message),
            -1,
            std::move(path)});
    }

    json::ObjectPtr requireObject(
        const json::ObjectPtr& parent,
        const std::string& key,
        const std::string& path)
    {
        if( !parent )
            failManifest(DiagnosticCategory::schema, "Manifest object is null", path);
        json::ObjectPtr value = parent->object(key);
        if( !value )
            failManifest(DiagnosticCategory::schema,
                "Manifest requires object member " + key, path + "." + key);
        return value;
    }

    json::ArrayPtr requireArray(
        const json::ObjectPtr& parent,
        const std::string& key,
        const std::string& path)
    {
        if( !parent )
            failManifest(DiagnosticCategory::schema, "Manifest object is null", path);
        json::ArrayPtr value = parent->array(key);
        if( !value )
            failManifest(DiagnosticCategory::schema,
                "Manifest requires array member " + key, path + "." + key);
        return value;
    }

    int checkedCount(sint64 value, const std::string& path)
    {
        if( value < 0 || value > static_cast<sint64>(std::numeric_limits<int>::max()) )
            failManifest(DiagnosticCategory::schema,
                "Manifest count is outside the supported int range", path);
        return static_cast<int>(value);
    }

    std::size_t checkedSize(sint64 value, const std::string& path)
    {
        if( value < 0 )
            failManifest(DiagnosticCategory::schema,
                "Manifest count cannot be negative", path);
        if( value > static_cast<sint64>(std::numeric_limits<int>::max()) )
            failManifest(DiagnosticCategory::schema,
                "Manifest count is outside the supported int range", path);
        return static_cast<std::size_t>(value);
    }

    math::M44f parseMatrix44(
        const json::ArrayPtr& values,
        const std::string& path)
    {
        const std::vector<real32> components = scalarArray<real32>(values, path);
        if( components.size() != 16 )
            failManifest(DiagnosticCategory::schema,
                "Matrix44 requires exactly 16 row-major values", path);
        return math::M44f(
            components[0], components[1], components[2], components[3],
            components[4], components[5], components[6], components[7],
            components[8], components[9], components[10], components[11],
            components[12], components[13], components[14], components[15]);
    }

    math::M33f parseMatrix33(
        const json::ArrayPtr& values,
        const std::string& path)
    {
        const std::vector<real32> components = scalarArray<real32>(values, path);
        if( components.size() != 9 )
            failManifest(DiagnosticCategory::schema,
                "Matrix33 requires exactly nine row-major values", path);
        return math::M33f(
            components[0], components[1], components[2],
            components[3], components[4], components[5],
            components[6], components[7], components[8]);
    }

    math::M33f parseFiniteMatrix33(
        const json::ArrayPtr& values,
        const std::string& path)
    {
        const math::M33f result = parseMatrix33(values, path);
        if( std::any_of(std::begin(result.ma), std::end(result.ma),
                [](real32 value) { return !std::isfinite(value); }) )
        {
            failManifest(DiagnosticCategory::schema,
                "Matrix33 components must be finite", path);
        }
        return result;
    }

    math::V3f parseVector3(
        const json::ArrayPtr& values,
        const std::string& path)
    {
        const std::vector<real32> components = scalarArray<real32>(values, path);
        if( components.size() != 3 )
            failManifest(DiagnosticCategory::schema,
                "Vector3 requires exactly three values", path);
        return math::V3f(components[0], components[1], components[2]);
    }

    math::V3f parseFiniteVector3(
        const json::ArrayPtr& values,
        const std::string& path)
    {
        const math::V3f result = parseVector3(values, path);
        if( !std::isfinite(result.x)
            || !std::isfinite(result.y)
            || !std::isfinite(result.z) )
        {
            failManifest(DiagnosticCategory::schema,
                "Vector3 components must be finite", path);
        }
        return result;
    }

    HouGeo::Ptr parseGeometryRoot(const json::ObjectPtr& root)
    {
        if( !root )
            failManifest(DiagnosticCategory::schema,
                "HOM manifest root must be an object", "root");
        if( root->get<std::string>("schema") != "houio.hom/1" )
            failManifest(DiagnosticCategory::unsupported_input,
                "Unsupported HOM manifest schema", "schema");

        const int pointCount = checkedCount(
            root->get<sint64>("point_count"), "point_count");
        const int vertexCount = checkedCount(
            root->get<sint64>("vertex_count"), "vertex_count");
        const int primitiveCount = checkedCount(
            root->get<sint64>("primitive_count"), "primitive_count");

        const std::vector<int> topology = scalarArray<int>(
            requireArray(root, "topology", "root"), "topology");
        if( topology.size() != static_cast<std::size_t>(vertexCount) )
            failManifest(DiagnosticCategory::schema,
                "Topology count does not match vertex_count", "topology");
        for( std::size_t index = 0; index < topology.size(); ++index )
        {
            if( topology[index] < 0 || topology[index] >= pointCount )
                failManifest(DiagnosticCategory::schema,
                    "Topology point index exceeds point_count",
                    "topology[" + std::to_string(index) + "]");
        }

        HouGeo::Ptr geometry = HouGeo::create();
        auto houTopology = std::make_shared<HouGeo::HouTopology>();
        houTopology->setIndices(topology);
        geometry->setTopology(std::move(houTopology));

        json::ObjectPtr attributes = requireObject(root, "attributes", "root");
        parseAttributeDomain(attributes, "point", pointCount, geometry);
        if( geometry->pointCount() != pointCount )
            failManifest(DiagnosticCategory::schema,
                "Point attributes do not establish the declared point_count",
                "attributes.point");
        parsePrimitives(root, topology, geometry);
        if( geometry->primitiveCount() != primitiveCount )
            failManifest(DiagnosticCategory::schema,
                "Primitive records do not match primitive_count", "primitives");
        parseAttributeDomain(attributes, "vertex", vertexCount, geometry);
        parseAttributeDomain(attributes, "primitive", primitiveCount, geometry);
        parseAttributeDomain(attributes, "global", 1, geometry);
        parseGroups(root, pointCount, vertexCount, primitiveCount, geometry);
        return geometry;
    }
}

namespace houio
{
    GeometryReadResult<HouGeo::Ptr> HomManifest::read(
        const std::filesystem::path& path,
        const json::ParserLimits& limits)
    {
        GeometryReadResult<HouGeo::Ptr> result;
        std::ifstream input(path, std::ios::binary);
        if( !input )
        {
            result.diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::io,
                "Could not open HOM manifest: " + path.string(),
                -1,
                "file"});
            return result;
        }

        try
        {
            json::JSONReader reader;
            json::Parser parser(limits);
            if( !parser.parse(input, reader, result.diagnostics) )
                return result;
            result.value = hom_manifest_detail::parseGeometryRoot(reader.root().asObject());
            result.succeeded = true;
        }
        catch( const DiagnosticException& exception )
        {
            result.diagnostics.push_back(exception.diagnostic());
        }
        catch( const std::exception& exception )
        {
            result.diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                exception.what(),
                -1,
                "manifest"});
        }
        return result;
    }
}
