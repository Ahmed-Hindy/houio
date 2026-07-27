#include <houio/OpenVdbBackend.h>

#include <array>
#include <exception>
#include <system_error>
#include <utility>

#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
#include <openvdb/openvdb.h>
#include <openvdb/io/File.h>
#include <openvdb/version.h>
#endif

namespace houio
{
    namespace
    {
        Diagnostic unavailableDiagnostic()
        {
            return Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::unsupported_input,
                "HouIO was built without the optional OpenVDB backend. Configure with "
                "HOUIO_ENABLE_OPENVDB=ON and provide an OpenVDB package.",
                -1,
                "openvdb_backend"};
        }

#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        openvdb::math::Mat4d toOpenVdbMatrix(const math::M44f& source)
        {
            openvdb::math::Mat4d result = openvdb::math::Mat4d::identity();
            for( int row = 0; row < 4; ++row )
            {
                for( int column = 0; column < 4; ++column )
                {
                    const std::size_t offset =
                        static_cast<std::size_t>(row * 4 + column);
                    result(row, column) = static_cast<double>(source.ma[offset]);
                }
            }
            return result;
        }

        math::M44f fromOpenVdbMatrix(const openvdb::math::Mat4d& source)
        {
            math::M44f result = math::M44f::zero();
            for( int row = 0; row < 4; ++row )
            {
                for( int column = 0; column < 4; ++column )
                {
                    const std::size_t offset =
                        static_cast<std::size_t>(row * 4 + column);
                    result.ma[offset] = static_cast<float>(source(row, column));
                }
            }
            return result;
        }

        SparseGridClass fromOpenVdbClass(openvdb::GridClass value) noexcept
        {
            switch( value )
            {
            case openvdb::GRID_FOG_VOLUME: return SparseGridClass::fog_volume;
            case openvdb::GRID_LEVEL_SET: return SparseGridClass::level_set;
            default: return SparseGridClass::unknown;
            }
        }

        openvdb::GridClass toOpenVdbClass(SparseGridClass value) noexcept
        {
            switch( value )
            {
            case SparseGridClass::fog_volume: return openvdb::GRID_FOG_VOLUME;
            case SparseGridClass::level_set: return openvdb::GRID_LEVEL_SET;
            case SparseGridClass::unknown: return openvdb::GRID_UNKNOWN;
            }
            return openvdb::GRID_UNKNOWN;
        }

        bool isReservedMetadataName(const std::string& name) noexcept
        {
            static constexpr std::array<const char*, 5> reserved{
                "name", "class", "creator", "vector_type", "is_saved_as_half"};
            for( const char* value : reserved )
            {
                if( name == value )
                    return true;
            }
            return false;
        }
#endif
    }

    OpenVdbBackendInfo OpenVdbBackend::info()
    {
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        return OpenVdbBackendInfo{
            true,
            OPENVDB_LIBRARY_VERSION_STRING,
            "Optional OpenVDB backend is compiled; FloatGrid read and write are available."};
#else
        return OpenVdbBackendInfo{
            false,
            {},
            "Optional OpenVDB backend is not compiled; dependency-neutral sparse-grid editing remains available."};
#endif
    }

    GeometryReadResult<SparseFloatGrid> OpenVdbBackend::readFloatGrid(
        const std::filesystem::path& path,
        const std::string& gridName)
    {
        GeometryReadResult<SparseFloatGrid> result;
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        try
        {
            openvdb::initialize();
            openvdb::io::File file(path.string());
            file.open();

            openvdb::GridBase::Ptr baseGrid;
            if( !gridName.empty() )
            {
                baseGrid = file.readGrid(gridName);
            }
            else
            {
                for( auto iterator = file.beginName(); iterator != file.endName(); ++iterator )
                {
                    openvdb::GridBase::Ptr probe =
                        file.readGridMetadata(iterator.gridName());
                    if( probe && probe->isType<openvdb::FloatGrid>() )
                    {
                        baseGrid = file.readGrid(iterator.gridName());
                        break;
                    }
                }
            }
            file.close();

            if( !baseGrid )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::unsupported_input,
                    gridName.empty()
                        ? "OpenVDB file contains no FloatGrid"
                        : "OpenVDB file contains no requested FloatGrid named '" + gridName + "'",
                    -1,
                    "grid"});
                return result;
            }

            openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);
            if( !grid )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::unsupported_input,
                    "Requested OpenVDB grid is not a FloatGrid",
                    -1,
                    "grid"});
                return result;
            }
            if( !grid->transform().isLinear() )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::unsupported_input,
                    "HouIO SparseFloatGrid currently supports only linear OpenVDB transforms",
                    -1,
                    "transform"});
                return result;
            }

            SparseFloatGrid sparse(grid->background());
            sparse.setName(grid->getName());
            sparse.setGridClass(fromOpenVdbClass(grid->getGridClass()));
            sparse.setIndexToWorld(fromOpenVdbMatrix(
                grid->transform().baseMap()->getAffineMap()->getMat4()));

            for( auto iterator = grid->beginMeta(); iterator != grid->endMeta(); ++iterator )
            {
                if( isReservedMetadataName(iterator->first)
                    || !iterator->second
                    || iterator->second->typeName() != openvdb::StringMetadata::staticTypeName() )
                {
                    continue;
                }
                const auto& metadata =
                    static_cast<const openvdb::StringMetadata&>(*iterator->second);
                sparse.setMetadata(iterator->first, metadata.value());
            }

            for( auto iterator = grid->cbeginValueOn(); iterator; ++iterator )
            {
                if( !iterator.isVoxelValue() )
                {
                    result.diagnostics.push_back(Diagnostic{
                        DiagnosticSeverity::error,
                        DiagnosticCategory::unsupported_input,
                        "HouIO SparseFloatGrid does not yet represent active OpenVDB tiles",
                        -1,
                        "tree"});
                    return result;
                }
                const openvdb::Coord coordinate = iterator.getCoord();
                sparse.setVoxel(
                    math::V3i(coordinate.x(), coordinate.y(), coordinate.z()),
                    *iterator);
            }

            result.value = std::move(sparse);
            result.succeeded = true;
        }
        catch( const std::exception& exception )
        {
            result.diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::io,
                std::string("OpenVDB read failed: ") + exception.what(),
                -1,
                "file"});
        }
#else
        static_cast<void>(path);
        static_cast<void>(gridName);
        result.diagnostics.push_back(unavailableDiagnostic());
#endif
        return result;
    }

    GeometryWriteResult OpenVdbBackend::writeFloatGrid(
        const std::filesystem::path& path,
        const SparseFloatGrid& grid,
        bool overwriteExisting,
        bool createParentDirectories)
    {
        GeometryWriteResult result;
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        try
        {
            if( path.empty() )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::io,
                    "OpenVDB output path cannot be empty",
                    -1,
                    "file"});
                return result;
            }
            if( !overwriteExisting && std::filesystem::exists(path) )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::io,
                    "OpenVDB output already exists and overwrite is disabled",
                    -1,
                    "file"});
                return result;
            }
            if( createParentDirectories && path.has_parent_path() )
            {
                std::error_code error;
                std::filesystem::create_directories(path.parent_path(), error);
                if( error )
                {
                    result.diagnostics.push_back(Diagnostic{
                        DiagnosticSeverity::error,
                        DiagnosticCategory::io,
                        "Could not create OpenVDB output directory: " + error.message(),
                        -1,
                        "file"});
                    return result;
                }
            }

            openvdb::initialize();
            openvdb::FloatGrid::Ptr vdbGrid = openvdb::FloatGrid::create(grid.background());
            vdbGrid->setName(grid.name());
            vdbGrid->setGridClass(toOpenVdbClass(grid.gridClass()));
            vdbGrid->setTransform(openvdb::math::Transform::createLinearTransform(
                toOpenVdbMatrix(grid.indexToWorld())));
            for( const auto& [key, value] : grid.metadata() )
            {
                if( !isReservedMetadataName(key) )
                    vdbGrid->insertMeta(key, openvdb::StringMetadata(value));
            }

            openvdb::FloatGrid::Accessor accessor = vdbGrid->getAccessor();
            grid.forEachActiveVoxel(
                [&](const SparseFloatVoxel& voxel)
                {
                    accessor.setValueOn(
                        openvdb::Coord(voxel.index.x, voxel.index.y, voxel.index.z),
                        voxel.value);
                });

            openvdb::GridPtrVec grids;
            grids.push_back(vdbGrid);
            openvdb::io::File file(path.string());
            file.write(grids);
            file.close();
            result.succeeded = true;
        }
        catch( const std::exception& exception )
        {
            result.diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::io,
                std::string("OpenVDB write failed: ") + exception.what(),
                -1,
                "file"});
        }
#else
        static_cast<void>(path);
        static_cast<void>(grid);
        static_cast<void>(overwriteExisting);
        static_cast<void>(createParentDirectories);
        result.diagnostics.push_back(unavailableDiagnostic());
#endif
        return result;
    }
}
