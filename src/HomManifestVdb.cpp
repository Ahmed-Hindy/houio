#include "HomManifestInternal.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace houio::hom_manifest_detail
{
    namespace
    {
        SparseGridClass parseGridClass(
            const std::string& value,
            bool allowStaggered,
            const std::string& label,
            const std::string& path)
        {
            if( value == "unknown" )
                return SparseGridClass::unknown;
            if( value == "fog_volume" )
                return SparseGridClass::fog_volume;
            if( value == "level_set" )
                return SparseGridClass::level_set;
            if( allowStaggered && value == "staggered" )
                return SparseGridClass::staggered;
            failManifest(DiagnosticCategory::schema,
                label + " grid class is invalid", path);
        }

        SparseVectorType parseSparseVectorType(
            const std::string& value,
            const std::string& path)
        {
            if( value == "invariant" )
                return SparseVectorType::invariant;
            if( value == "covariant" )
                return SparseVectorType::covariant;
            if( value == "covariant_normalize" )
                return SparseVectorType::covariant_normalize;
            if( value == "contravariant_relative" )
                return SparseVectorType::contravariant_relative;
            if( value == "contravariant_absolute" )
                return SparseVectorType::contravariant_absolute;
            failManifest(DiagnosticCategory::schema,
                "Sparse Vec3f VDB vector type is invalid", path);
        }

        SparseIndexBounds parseTileBounds(
            const json::ObjectPtr& tile,
            const std::string& label,
            const std::string& tilePath)
        {
            const std::vector<sint32> minimum = scalarArray<sint32>(
                requireArray(tile, "minimum", tilePath),
                tilePath + ".minimum");
            const std::vector<sint32> maximum = scalarArray<sint32>(
                requireArray(tile, "maximum", tilePath),
                tilePath + ".maximum");
            if( minimum.size() != 3 || maximum.size() != 3 )
            {
                failManifest(DiagnosticCategory::schema,
                    label + " active tile bounds must contain three integers",
                    tilePath);
            }
            const SparseIndexBounds bounds{
                math::V3i(minimum[0], minimum[1], minimum[2]),
                math::V3i(maximum[0], maximum[1], maximum[2])};
            if( bounds.minimum.x > bounds.maximum.x
                || bounds.minimum.y > bounds.maximum.y
                || bounds.minimum.z > bounds.maximum.z )
            {
                failManifest(DiagnosticCategory::schema,
                    label + " active tile bounds must be ordered", tilePath);
            }
            return bounds;
        }

        void rejectDuplicateBounds(
            const std::vector<SparseIndexBounds>& parsedBounds,
            const SparseIndexBounds& bounds,
            const std::string& label,
            const std::string& tilePath)
        {
            const auto duplicate = std::find_if(
                parsedBounds.begin(), parsedBounds.end(),
                [&](const SparseIndexBounds& existing)
                {
                    return existing.minimum == bounds.minimum
                        && existing.maximum == bounds.maximum;
                });
            if( duplicate != parsedBounds.end() )
                failManifest(DiagnosticCategory::schema,
                    label + " active tiles contain duplicate bounds", tilePath);
        }

        template<typename Grid>
        void parseMetadata(
            const json::ObjectPtr& definition,
            Grid& grid,
            const std::string& label,
            const std::string& path)
        {
            if( const json::ObjectPtr metadata = definition->object("metadata") )
            {
                for( const std::string& key : metadata->keys() )
                {
                    if( key.empty() )
                        failManifest(DiagnosticCategory::schema,
                            label + " metadata key cannot be empty",
                            path + ".metadata");
                    grid.setMetadata(key, metadata->get<std::string>(key));
                }
            }
        }

        template<typename Grid, typename Value>
        void parseScalarActiveVoxels(
            const json::ObjectPtr& definition,
            Grid& grid,
            const std::string& label,
            const std::string& path)
        {
            const bool hasActiveIndices = definition->contains("active_indices");
            const bool hasActiveValues = definition->contains("active_values");
            if( hasActiveIndices != hasActiveValues )
            {
                failManifest(DiagnosticCategory::schema,
                    label + " active_indices and active_values must be provided together",
                    path);
            }
            if( !hasActiveIndices )
                return;

            const std::vector<sint32> activeIndices = scalarArray<sint32>(
                requireArray(definition, "active_indices", path),
                path + ".active_indices");
            const std::vector<Value> activeValues = scalarArray<Value>(
                requireArray(definition, "active_values", path),
                path + ".active_values");
            if( activeIndices.size() % 3 != 0
                || activeValues.size() != activeIndices.size() / 3 )
            {
                failManifest(DiagnosticCategory::schema,
                    label + " active indices and values have inconsistent lengths",
                    path + ".active_indices");
            }
            for( std::size_t valueIndex = 0; valueIndex < activeValues.size(); ++valueIndex )
            {
                const std::size_t coordinateOffset = valueIndex * 3;
                grid.setVoxel(
                    math::V3i(
                        activeIndices[coordinateOffset],
                        activeIndices[coordinateOffset + 1],
                        activeIndices[coordinateOffset + 2]),
                    activeValues[valueIndex]);
            }
            if( grid.activeVoxelCount() != activeValues.size() )
            {
                failManifest(DiagnosticCategory::schema,
                    label + " active indices contain duplicate coordinates",
                    path + ".active_indices");
            }
        }

        template<typename Grid, typename ValueParser>
        void parseActiveTiles(
            const json::ObjectPtr& definition,
            Grid& grid,
            const std::string& label,
            const std::string& path,
            ValueParser parseValue)
        {
            if( !definition->contains("active_tiles") )
                return;

            const json::ArrayPtr activeTiles = requireArray(definition, "active_tiles", path);
            const std::size_t tileCount = checkedSize(
                activeTiles->size(), path + ".active_tiles");
            std::vector<SparseIndexBounds> parsedBounds;
            parsedBounds.reserve(tileCount);
            for( std::size_t tileIndex = 0; tileIndex < tileCount; ++tileIndex )
            {
                const std::string tilePath = path + ".active_tiles["
                    + std::to_string(tileIndex) + "]";
                const json::ObjectPtr tile = activeTiles->object(static_cast<int>(tileIndex));
                if( !tile )
                    failManifest(DiagnosticCategory::schema,
                        label + " active tile must be an object", tilePath);

                const SparseIndexBounds bounds = parseTileBounds(tile, label, tilePath);
                rejectDuplicateBounds(parsedBounds, bounds, label, tilePath);
                parsedBounds.push_back(bounds);
                grid.addActiveTile(bounds, parseValue(tile, tilePath));
            }
        }

        void parseFloatGrid(
            const json::ObjectPtr& definition,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            SparseFloatGrid grid(definition->get<real32>("background", 0.0f));
            grid.setName(definition->get<std::string>("name", ""));
            grid.setGridClass(parseGridClass(
                definition->get<std::string>("grid_class", "unknown"),
                false, "Sparse VDB", path + ".grid_class"));
            grid.setIndexToWorld(parseMatrix44(
                requireArray(definition, "index_to_world", path),
                path + ".index_to_world"));

            parseScalarActiveVoxels<SparseFloatGrid, real32>(
                definition, grid, "Sparse VDB", path);
            parseActiveTiles(
                definition, grid, "Sparse VDB", path,
                [&](const json::ObjectPtr& tile, const std::string& tilePath)
                {
                    if( !tile->contains("value") )
                        failManifest(DiagnosticCategory::schema,
                            "Sparse VDB active tile requires a value",
                            tilePath + ".value");
                    real32 value = 0.0f;
                    try
                    {
                        value = tile->get<real32>("value");
                    }
                    catch( const std::exception& exception )
                    {
                        failManifest(DiagnosticCategory::schema,
                            "Invalid sparse VDB active tile value: "
                                + std::string(exception.what()),
                            tilePath + ".value");
                    }
                    if( !std::isfinite(value) )
                        failManifest(DiagnosticCategory::schema,
                            "Sparse VDB active tile value must be finite",
                            tilePath + ".value");
                    return value;
                });
            parseMetadata(definition, grid, "Sparse VDB", path);

            auto sparseVdb = std::make_shared<HouGeo::HouSparseVdb>();
            sparseVdb->setTopologyVertex(vertexOffset);
            sparseVdb->setSparseGrid(std::move(grid));
            geometry->addPrimitive(
                std::static_pointer_cast<HouGeoAdapter::SparseVdbPrimitive>(sparseVdb));
        }

        void parseInt32Grid(
            const json::ObjectPtr& definition,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            SparseInt32Grid grid(definition->get<sint32>("background", 0));
            grid.setName(definition->get<std::string>("name", ""));
            grid.setGridClass(parseGridClass(
                definition->get<std::string>("grid_class", "unknown"),
                false, "Sparse Int32 VDB", path + ".grid_class"));
            grid.setIndexToWorld(parseMatrix44(
                requireArray(definition, "index_to_world", path),
                path + ".index_to_world"));

            parseScalarActiveVoxels<SparseInt32Grid, sint32>(
                definition, grid, "Sparse Int32 VDB", path);
            parseActiveTiles(
                definition, grid, "Sparse Int32 VDB", path,
                [&](const json::ObjectPtr& tile, const std::string& tilePath)
                {
                    if( !tile->contains("value") )
                        failManifest(DiagnosticCategory::schema,
                            "Sparse Int32 VDB active tile requires a value",
                            tilePath + ".value");
                    try
                    {
                        return tile->get<sint32>("value");
                    }
                    catch( const std::exception& exception )
                    {
                        failManifest(DiagnosticCategory::schema,
                            "Invalid sparse Int32 VDB active tile value: "
                                + std::string(exception.what()),
                            tilePath + ".value");
                    }
                });
            parseMetadata(definition, grid, "Sparse Int32 VDB", path);

            auto sparseVdb = std::make_shared<HouGeo::HouSparseInt32Vdb>();
            sparseVdb->setTopologyVertex(vertexOffset);
            sparseVdb->setSparseGrid(std::move(grid));
            geometry->addPrimitive(
                std::static_pointer_cast<HouGeoAdapter::SparseInt32VdbPrimitive>(sparseVdb));
        }

        void parseVec3fGrid(
            const json::ObjectPtr& definition,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            math::V3f background(0.0f);
            if( definition->contains("background") )
            {
                background = parseFiniteVector3(
                    requireArray(definition, "background", path),
                    path + ".background");
            }
            SparseVec3fGrid grid(background);
            grid.setName(definition->get<std::string>("name", ""));
            grid.setGridClass(parseGridClass(
                definition->get<std::string>("grid_class", "unknown"),
                true, "Sparse Vec3f VDB", path + ".grid_class"));
            grid.setVectorType(parseSparseVectorType(
                definition->get<std::string>("vector_type", "invariant"),
                path + ".vector_type"));
            grid.setIndexToWorld(parseMatrix44(
                requireArray(definition, "index_to_world", path),
                path + ".index_to_world"));

            const bool hasActiveIndices = definition->contains("active_indices");
            const bool hasActiveValues = definition->contains("active_values");
            if( hasActiveIndices != hasActiveValues )
            {
                failManifest(DiagnosticCategory::schema,
                    "Sparse Vec3f VDB active_indices and active_values must be provided together",
                    path);
            }
            if( hasActiveIndices )
            {
                const std::vector<sint32> activeIndices = scalarArray<sint32>(
                    requireArray(definition, "active_indices", path),
                    path + ".active_indices");
                const json::ArrayPtr activeValues = requireArray(
                    definition, "active_values", path);
                const std::size_t valueCount = checkedSize(
                    activeValues->size(), path + ".active_values");
                if( activeIndices.size() % 3 != 0
                    || valueCount != activeIndices.size() / 3 )
                {
                    failManifest(DiagnosticCategory::schema,
                        "Sparse Vec3f VDB active indices and values have inconsistent lengths",
                        path + ".active_indices");
                }
                for( std::size_t valueIndex = 0; valueIndex < valueCount; ++valueIndex )
                {
                    const std::size_t coordinateOffset = valueIndex * 3;
                    const std::string valuePath = path + ".active_values["
                        + std::to_string(valueIndex) + "]";
                    const json::ArrayPtr tuple = activeValues->array(
                        static_cast<int>(valueIndex));
                    if( !tuple )
                        failManifest(DiagnosticCategory::schema,
                            "Sparse Vec3f VDB active value must be a three-component array",
                            valuePath);
                    grid.setVoxel(
                        math::V3i(
                            activeIndices[coordinateOffset],
                            activeIndices[coordinateOffset + 1],
                            activeIndices[coordinateOffset + 2]),
                        parseFiniteVector3(tuple, valuePath));
                }
                if( grid.activeVoxelCount() != valueCount )
                {
                    failManifest(DiagnosticCategory::schema,
                        "Sparse Vec3f VDB active indices contain duplicate coordinates",
                        path + ".active_indices");
                }
            }

            parseActiveTiles(
                definition, grid, "Sparse Vec3f VDB", path,
                [&](const json::ObjectPtr& tile, const std::string& tilePath)
                {
                    return parseFiniteVector3(
                        requireArray(tile, "value", tilePath),
                        tilePath + ".value");
                });
            parseMetadata(definition, grid, "Sparse Vec3f VDB", path);

            auto sparseVdb = std::make_shared<HouGeo::HouSparseVec3fVdb>();
            sparseVdb->setTopologyVertex(vertexOffset);
            sparseVdb->setSparseGrid(std::move(grid));
            geometry->addPrimitive(
                std::static_pointer_cast<HouGeoAdapter::SparseVec3fVdbPrimitive>(sparseVdb));
        }
    }

    bool parseSparseVdbPrimitive(
        const std::string& type,
        const json::ObjectPtr& definition,
        int vertexOffset,
        const HouGeo::Ptr& geometry,
        const std::string& path)
    {
        if( type == "sparse_float_vdb" )
            parseFloatGrid(definition, vertexOffset, geometry, path);
        else if( type == "sparse_int32_vdb" )
            parseInt32Grid(definition, vertexOffset, geometry, path);
        else if( type == "sparse_vec3f_vdb" )
            parseVec3fGrid(definition, vertexOffset, geometry, path);
        else
            return false;
        return true;
    }
}
