#include <houio/OpenVdbBackend.h>

#include <array>
#include <exception>
#include <limits>
#include <sstream>
#include <streambuf>
#include <system_error>
#include <utility>

#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
#include <openvdb/openvdb.h>
#include <openvdb/io/File.h>
#include <openvdb/io/Stream.h>
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

        template<typename SparseGrid, typename Encoder>
        GeometryReadResult<std::vector<ubyte>> encodeGridBytes(
            const SparseGrid& grid,
            Encoder&& encoder)
        {
            GeometryReadResult<std::vector<ubyte>> result;
            std::ostringstream output(std::ios::out | std::ios::binary);
            GeometryWriteResult writeResult = encoder(output, grid);
            if( !writeResult )
            {
                result.diagnostics = std::move(writeResult.diagnostics);
                return result;
            }
            const std::string bytes = output.str();
            const auto* first = reinterpret_cast<const ubyte*>(bytes.data());
            result.value.assign(first, first + bytes.size());
            result.succeeded = true;
            return result;
        }

#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        class SpanInputBuffer final : public std::streambuf
        {
        public:
            explicit SpanInputBuffer(std::span<const ubyte> bytes)
            {
                char* first = const_cast<char*>(
                    reinterpret_cast<const char*>(bytes.data()));
                setg(first, first,
                    first + static_cast<std::ptrdiff_t>(bytes.size()));
            }
        };

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

        template<typename OpenGrid, typename SparseGrid>
        GeometryReadResult<SparseGrid> sparseFromGrid(
            const openvdb::GridBase::Ptr& baseGrid,
            const std::string& missingMessage,
            const char* gridTypeName)
        {
            GeometryReadResult<SparseGrid> result;
            if( !baseGrid )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::unsupported_input,
                    missingMessage,
                    -1,
                    "grid"});
                return result;
            }

            typename OpenGrid::Ptr grid = openvdb::gridPtrCast<OpenGrid>(baseGrid);
            if( !grid )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::unsupported_input,
                    std::string("Requested OpenVDB grid is not a ") + gridTypeName,
                    -1,
                    "grid"});
                return result;
            }
            if( !grid->transform().isLinear() )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::unsupported_input,
                    std::string("HouIO sparse ") + gridTypeName
                        + " currently supports only linear OpenVDB transforms",
                    -1,
                    "transform"});
                return result;
            }

            SparseGrid sparse(grid->background());
            sparse.setName(grid->getName());
            sparse.setGridClass(fromOpenVdbClass(grid->getGridClass()));
            if( !grid->getCreator().empty() )
                sparse.setMetadata("creator", grid->getCreator());
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
                if( iterator.isVoxelValue() )
                {
                    const openvdb::Coord coordinate = iterator.getCoord();
                    sparse.setVoxel(
                        math::V3i(coordinate.x(), coordinate.y(), coordinate.z()),
                        *iterator);
                    continue;
                }

                openvdb::CoordBBox bounds;
                iterator.getBoundingBox(bounds);
                sparse.addActiveTile(
                    SparseIndexBounds{
                        math::V3i(bounds.min().x(), bounds.min().y(), bounds.min().z()),
                        math::V3i(bounds.max().x(), bounds.max().y(), bounds.max().z())},
                    *iterator);
            }

            result.value = std::move(sparse);
            result.succeeded = true;
            return result;
        }

        template<typename OpenGrid, typename SparseGrid>
        typename OpenGrid::Ptr openVdbFromSparse(const SparseGrid& grid)
        {
            typename OpenGrid::Ptr vdbGrid = OpenGrid::create(grid.background());
            vdbGrid->setName(grid.name());
            vdbGrid->setGridClass(toOpenVdbClass(grid.gridClass()));
            vdbGrid->setTransform(openvdb::math::Transform::createLinearTransform(
                toOpenVdbMatrix(grid.indexToWorld())));
            if( const auto creator = grid.metadata("creator") )
                vdbGrid->setCreator(*creator);
            for( const auto& [key, value] : grid.metadata() )
            {
                if( !isReservedMetadataName(key) )
                    vdbGrid->insertMeta(key, openvdb::StringMetadata(value));
            }

            for( const auto& tile : grid.activeTiles() )
            {
                vdbGrid->fill(
                    openvdb::CoordBBox(
                        openvdb::Coord(
                            tile.bounds.minimum.x,
                            tile.bounds.minimum.y,
                            tile.bounds.minimum.z),
                        openvdb::Coord(
                            tile.bounds.maximum.x,
                            tile.bounds.maximum.y,
                            tile.bounds.maximum.z)),
                    tile.value,
                    true);
            }

            typename OpenGrid::Accessor accessor = vdbGrid->getAccessor();
            grid.forEachActiveVoxel(
                [&](const auto& voxel)
                {
                    accessor.setValueOn(
                        openvdb::Coord(voxel.index.x, voxel.index.y, voxel.index.z),
                        voxel.value);
                });
            return vdbGrid;
        }

        template<typename OpenGrid, typename SparseGrid>
        GeometryReadResult<SparseGrid> readGridFile(
            const std::filesystem::path& path,
            const std::string& gridName,
            const char* gridTypeName)
        {
            GeometryReadResult<SparseGrid> result;
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
                        if( probe && probe->isType<OpenGrid>() )
                        {
                            baseGrid = file.readGrid(iterator.gridName());
                            break;
                        }
                    }
                }
                file.close();

                const std::string typeName(gridTypeName);
                return sparseFromGrid<OpenGrid, SparseGrid>(
                    baseGrid,
                    gridName.empty()
                        ? "OpenVDB file contains no " + typeName
                        : "OpenVDB file contains no requested " + typeName
                            + " named '" + gridName + "'",
                    gridTypeName);
            }
            catch( const std::exception& exception )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::io,
                    std::string("OpenVDB ") + gridTypeName
                        + " read failed: " + exception.what(),
                    -1,
                    "file"});
            }
            return result;
        }

        template<typename OpenGrid, typename SparseGrid>
        GeometryWriteResult writeGridFile(
            const std::filesystem::path& path,
            const SparseGrid& grid,
            bool overwriteExisting,
            bool createParentDirectories,
            const char* gridTypeName)
        {
            GeometryWriteResult result;
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
                openvdb::GridPtrVec grids;
                grids.push_back(openVdbFromSparse<OpenGrid>(grid));
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
                    std::string("OpenVDB ") + gridTypeName
                        + " write failed: " + exception.what(),
                    -1,
                    "file"});
            }
            return result;
        }

        template<typename OpenGrid, typename SparseGrid>
        GeometryWriteResult encodeGridStream(
            std::ostream& output,
            const SparseGrid& grid,
            const char* gridTypeName)
        {
            GeometryWriteResult result;
            try
            {
                openvdb::initialize();
                openvdb::GridPtrVec grids;
                grids.push_back(openVdbFromSparse<OpenGrid>(grid));
                openvdb::io::Stream archive(output);
                archive.write(grids);
                output.flush();
                if( !output )
                {
                    result.diagnostics.push_back(Diagnostic{
                        DiagnosticSeverity::error,
                        DiagnosticCategory::io,
                        std::string("OpenVDB ") + gridTypeName + " stream write failed",
                        -1,
                        "openvdb_stream"});
                    return result;
                }
                result.succeeded = true;
            }
            catch( const std::exception& exception )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::conversion,
                    std::string("OpenVDB ") + gridTypeName
                        + " stream write failed: " + exception.what(),
                    -1,
                    "openvdb_stream"});
            }
            return result;
        }

        template<typename OpenGrid, typename SparseGrid>
        GeometryReadResult<SparseGrid> decodeGridStream(
            std::span<const ubyte> openvdbStream,
            const std::string& gridName,
            const char* gridTypeName)
        {
            GeometryReadResult<SparseGrid> result;
            if( openvdbStream.empty() )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::malformed_input,
                    "OpenVDB stream cannot be empty",
                    -1,
                    "openvdb_stream"});
                return result;
            }
            if( openvdbStream.size()
                > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::unsupported_input,
                    "OpenVDB stream exceeds this platform's stream-buffer range",
                    -1,
                    "openvdb_stream"});
                return result;
            }

            try
            {
                openvdb::initialize();
                SpanInputBuffer inputBuffer(openvdbStream);
                std::istream input(&inputBuffer);
                openvdb::io::Stream archive(input, false);
                const openvdb::GridPtrVecPtr grids = archive.getGrids();
                openvdb::GridBase::Ptr selected;
                if( grids )
                {
                    for( const openvdb::GridBase::Ptr& candidate : *grids )
                    {
                        if( !candidate || !candidate->isType<OpenGrid>() )
                            continue;
                        if( gridName.empty() || candidate->getName() == gridName )
                        {
                            selected = candidate;
                            break;
                        }
                    }
                }

                const std::string typeName(gridTypeName);
                return sparseFromGrid<OpenGrid, SparseGrid>(
                    selected,
                    gridName.empty()
                        ? "OpenVDB stream contains no " + typeName
                        : "OpenVDB stream contains no requested " + typeName
                            + " named '" + gridName + "'",
                    gridTypeName);
            }
            catch( const std::exception& exception )
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::malformed_input,
                    std::string("OpenVDB ") + gridTypeName
                        + " in-memory read failed: " + exception.what(),
                    -1,
                    "openvdb_stream"});
            }
            return result;
        }
#endif
    }

    OpenVdbBackendInfo OpenVdbBackend::info()
    {
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        return OpenVdbBackendInfo{
            true,
            OPENVDB_LIBRARY_VERSION_STRING,
            "Optional OpenVDB backend is compiled; FloatGrid and Int32Grid read and write are available."};
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
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        return readGridFile<openvdb::FloatGrid, SparseFloatGrid>(
            path, gridName, "FloatGrid");
#else
        static_cast<void>(path);
        static_cast<void>(gridName);
        GeometryReadResult<SparseFloatGrid> result;
        result.diagnostics.push_back(unavailableDiagnostic());
        return result;
#endif
    }

    GeometryWriteResult OpenVdbBackend::writeFloatGrid(
        const std::filesystem::path& path,
        const SparseFloatGrid& grid,
        bool overwriteExisting,
        bool createParentDirectories)
    {
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        return writeGridFile<openvdb::FloatGrid>(
            path, grid, overwriteExisting, createParentDirectories, "FloatGrid");
#else
        static_cast<void>(path);
        static_cast<void>(grid);
        static_cast<void>(overwriteExisting);
        static_cast<void>(createParentDirectories);
        GeometryWriteResult result;
        result.diagnostics.push_back(unavailableDiagnostic());
        return result;
#endif
    }

    GeometryWriteResult OpenVdbBackend::encodeFloatGrid(
        std::ostream& output,
        const SparseFloatGrid& grid)
    {
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        return encodeGridStream<openvdb::FloatGrid>(output, grid, "FloatGrid");
#else
        static_cast<void>(output);
        static_cast<void>(grid);
        GeometryWriteResult result;
        result.diagnostics.push_back(unavailableDiagnostic());
        return result;
#endif
    }

    GeometryReadResult<std::vector<ubyte>> OpenVdbBackend::encodeFloatGrid(
        const SparseFloatGrid& grid)
    {
        return encodeGridBytes(grid,
            [](std::ostream& output, const SparseFloatGrid& value)
            {
                return OpenVdbBackend::encodeFloatGrid(output, value);
            });
    }

    GeometryReadResult<SparseFloatGrid> OpenVdbBackend::decodeFloatGrid(
        std::span<const ubyte> openvdbStream,
        const std::string& gridName)
    {
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        return decodeGridStream<openvdb::FloatGrid, SparseFloatGrid>(
            openvdbStream, gridName, "FloatGrid");
#else
        static_cast<void>(openvdbStream);
        static_cast<void>(gridName);
        GeometryReadResult<SparseFloatGrid> result;
        result.diagnostics.push_back(unavailableDiagnostic());
        return result;
#endif
    }

    GeometryReadResult<SparseInt32Grid> OpenVdbBackend::readInt32Grid(
        const std::filesystem::path& path,
        const std::string& gridName)
    {
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        return readGridFile<openvdb::Int32Grid, SparseInt32Grid>(
            path, gridName, "Int32Grid");
#else
        static_cast<void>(path);
        static_cast<void>(gridName);
        GeometryReadResult<SparseInt32Grid> result;
        result.diagnostics.push_back(unavailableDiagnostic());
        return result;
#endif
    }

    GeometryWriteResult OpenVdbBackend::writeInt32Grid(
        const std::filesystem::path& path,
        const SparseInt32Grid& grid,
        bool overwriteExisting,
        bool createParentDirectories)
    {
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        return writeGridFile<openvdb::Int32Grid>(
            path, grid, overwriteExisting, createParentDirectories, "Int32Grid");
#else
        static_cast<void>(path);
        static_cast<void>(grid);
        static_cast<void>(overwriteExisting);
        static_cast<void>(createParentDirectories);
        GeometryWriteResult result;
        result.diagnostics.push_back(unavailableDiagnostic());
        return result;
#endif
    }

    GeometryWriteResult OpenVdbBackend::encodeInt32Grid(
        std::ostream& output,
        const SparseInt32Grid& grid)
    {
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        return encodeGridStream<openvdb::Int32Grid>(output, grid, "Int32Grid");
#else
        static_cast<void>(output);
        static_cast<void>(grid);
        GeometryWriteResult result;
        result.diagnostics.push_back(unavailableDiagnostic());
        return result;
#endif
    }

    GeometryReadResult<std::vector<ubyte>> OpenVdbBackend::encodeInt32Grid(
        const SparseInt32Grid& grid)
    {
        return encodeGridBytes(grid,
            [](std::ostream& output, const SparseInt32Grid& value)
            {
                return OpenVdbBackend::encodeInt32Grid(output, value);
            });
    }

    GeometryReadResult<SparseInt32Grid> OpenVdbBackend::decodeInt32Grid(
        std::span<const ubyte> openvdbStream,
        const std::string& gridName)
    {
#if defined(HOUIO_HAS_OPENVDB) && HOUIO_HAS_OPENVDB
        return decodeGridStream<openvdb::Int32Grid, SparseInt32Grid>(
            openvdbStream, gridName, "Int32Grid");
#else
        static_cast<void>(openvdbStream);
        static_cast<void>(gridName);
        GeometryReadResult<SparseInt32Grid> result;
        result.diagnostics.push_back(unavailableDiagnostic());
        return result;
#endif
    }
}
