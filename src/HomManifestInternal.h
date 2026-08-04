#pragma once

#include <houio/HomManifest.h>

#include <cstddef>
#include <exception>
#include <string>
#include <vector>

namespace houio::hom_manifest_detail
{
    [[noreturn]] void failManifest(
        DiagnosticCategory category,
        std::string message,
        std::string path);

    [[nodiscard]] json::ObjectPtr requireObject(
        const json::ObjectPtr& parent,
        const std::string& key,
        const std::string& path);

    [[nodiscard]] json::ArrayPtr requireArray(
        const json::ObjectPtr& parent,
        const std::string& key,
        const std::string& path);

    [[nodiscard]] int checkedCount(sint64 value, const std::string& path);
    [[nodiscard]] std::size_t checkedSize(sint64 value, const std::string& path);

    template<typename T>
    [[nodiscard]] std::vector<T> scalarArray(
        const json::ArrayPtr& array,
        const std::string& path)
    {
        if( !array )
            failManifest(DiagnosticCategory::schema,
                "Manifest scalar array is null", path);
        const std::size_t count = checkedSize(array->size(), path);
        std::vector<T> values;
        values.reserve(count);
        for( std::size_t index = 0; index < count; ++index )
        {
            try
            {
                values.push_back(array->get<T>(static_cast<int>(index)));
            }
            catch( const std::exception& exception )
            {
                failManifest(DiagnosticCategory::schema,
                    "Invalid scalar value: " + std::string(exception.what()),
                    path + "[" + std::to_string(index) + "]");
            }
        }
        return values;
    }

    [[nodiscard]] math::M44f parseMatrix44(
        const json::ArrayPtr& values,
        const std::string& path);

    [[nodiscard]] math::M33f parseMatrix33(
        const json::ArrayPtr& values,
        const std::string& path);

    [[nodiscard]] math::M33f parseFiniteMatrix33(
        const json::ArrayPtr& values,
        const std::string& path);

    [[nodiscard]] math::V3f parseVector3(
        const json::ArrayPtr& values,
        const std::string& path);

    [[nodiscard]] math::V3f parseFiniteVector3(
        const json::ArrayPtr& values,
        const std::string& path);

    [[nodiscard]] HouGeo::Ptr parseGeometryRoot(const json::ObjectPtr& root);

    void parseAttributeDomain(
        const json::ObjectPtr& attributes,
        const std::string& domain,
        int elementCount,
        const HouGeo::Ptr& geometry);

    void parseGroups(
        const json::ObjectPtr& root,
        int pointCount,
        int vertexCount,
        int primitiveCount,
        const HouGeo::Ptr& geometry);

    void parsePrimitives(
        const json::ObjectPtr& root,
        const std::vector<int>& topology,
        const HouGeo::Ptr& geometry);

    [[nodiscard]] bool parseSparseVdbPrimitive(
        const std::string& type,
        const json::ObjectPtr& definition,
        int vertexOffset,
        const HouGeo::Ptr& geometry,
        const std::string& path);
}
