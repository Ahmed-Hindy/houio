#include <houio/HouGeo.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace houio
{
    namespace
    {
        template<typename Function>
        void withSchemaPath(const std::string& path, Function&& function)
        {
            try
            {
                function();
            }
            catch( const DiagnosticException& exception )
            {
                throw DiagnosticException(
                    withDiagnosticPath(exception.diagnostic(), path));
            }
            catch( const std::exception& exception )
            {
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    exception.what(),
                    -1,
                    path});
            }
        }

        std::size_t checkedProduct(
            std::size_t left,
            std::size_t right,
            const std::string& description)
        {
            if( left != 0
                && right > std::numeric_limits<std::size_t>::max() / left )
            {
                throw std::length_error(
                    description + " exceeds addressable storage");
            }
            return left * right;
        }

        std::size_t volumeVoxelCount(const math::V3i& resolution)
        {
            if( resolution.x <= 0 || resolution.y <= 0 || resolution.z <= 0 )
                throw std::invalid_argument(
                    "Volume resolution dimensions must be positive");
            const std::size_t xy = checkedProduct(
                static_cast<std::size_t>(resolution.x),
                static_cast<std::size_t>(resolution.y),
                "Volume resolution");
            return checkedProduct(
                xy,
                static_cast<std::size_t>(resolution.z),
                "Volume resolution");
        }

        std::size_t volumeIndex(
            int x,
            int y,
            int z,
            const math::V3i& resolution)
        {
            const std::size_t planeSize = checkedProduct(
                static_cast<std::size_t>(resolution.x),
                static_cast<std::size_t>(resolution.y),
                "Volume plane");
            return static_cast<std::size_t>(z) * planeSize
                + static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(resolution.x)
                + static_cast<std::size_t>(x);
        }
    }

    void HouGeo::loadVolumePrimitive(
        json::ObjectPtr volume,
        SharedPrimitiveData& sharedPrimitiveData)
    {
        if( !volume )
            throw std::invalid_argument(
                "HouGeo::loadVolumePrimitive received null volume data");
        if( !volume->contains("res") )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "HouGeo::loadVolumePrimitive missing resolution",
                -1,
                "res"});

        HouVolume::Ptr volumePrimitive = std::make_shared<HouVolume>();
        volumePrimitive->scalar_field_ = std::make_shared<ScalarField>();

        math::V3i resolution;
        withSchemaPath("res", [&]()
        {
            json::ArrayPtr resolutionValues = volume->array("res");
            if( !resolutionValues || resolutionValues->size() != 3 )
                throw std::runtime_error(
                    "HouGeo::loadVolumePrimitive resolution must contain three values");
            resolution = math::V3i(
                resolutionValues->get<int>(0),
                resolutionValues->get<int>(1),
                resolutionValues->get<int>(2));
            static_cast<void>(volumeVoxelCount(resolution));
        });

        const bool hasVertex = volume->contains("vertex");
        const bool hasTransform = volume->contains("transform");
        if( hasVertex != hasTransform )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "HouGeo::loadVolumePrimitive requires vertex and transform together",
                -1,
                hasVertex ? "transform" : "vertex"});

        if( hasVertex )
        {
            math::Matrix44d rotationScale;
            withSchemaPath("transform", [&]()
            {
                json::ArrayPtr transformValues = volume->array("transform");
                if( !transformValues || transformValues->size() != 9 )
                    throw std::runtime_error(
                        "HouGeo::loadVolumePrimitive transform must contain nine values");
                rotationScale = math::Matrix44d(
                    transformValues->get<float>(0),
                    transformValues->get<float>(1),
                    transformValues->get<float>(2),
                    0.0,
                    transformValues->get<float>(3),
                    transformValues->get<float>(4),
                    transformValues->get<float>(5),
                    0.0,
                    transformValues->get<float>(6),
                    transformValues->get<float>(7),
                    transformValues->get<float>(8),
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    1.0);
            });

            math::V3f position;
            withSchemaPath("vertex", [&]()
            {
                if( !m_topology )
                    throw std::runtime_error(
                        "HouGeo::loadVolumePrimitive requires topology for transformed volumes");
                const int topologyVertex = volume->get<int>("vertex");
                if( topologyVertex < 0
                    || static_cast<std::size_t>(topologyVertex)
                        >= m_topology->indexBuffer.size() )
                {
                    throw std::runtime_error(
                        "HouGeo::loadVolumePrimitive vertex index is outside topology");
                }
                volumePrimitive->topology_vertex_ = topologyVertex;

                const int pointIndex = m_topology->indexBuffer[
                    static_cast<std::size_t>(topologyVertex)];
                const auto positionIterator = m_pointAttributes.find("P");
                HouAttribute::Ptr positionAttribute =
                    positionIterator != m_pointAttributes.end()
                    ? positionIterator->second
                    : nullptr;
                if( !positionAttribute || !positionAttribute->numeric_attribute_ )
                    throw std::runtime_error(
                        "HouGeo::loadVolumePrimitive requires a point P attribute");
                if( positionAttribute->tupleSize().value() < 3 )
                    throw std::runtime_error(
                        "HouGeo::loadVolumePrimitive P requires at least three components");
                if( pointIndex < 0
                    || static_cast<sint64>(pointIndex)
                        >= positionAttribute->elementCount() )
                {
                    throw std::runtime_error(
                        "HouGeo::loadVolumePrimitive point index is outside P");
                }

                const HouGeoAdapter::RawDataView positionData =
                    positionAttribute->rawData();
                if( !positionData.available() )
                    throw std::runtime_error(
                        "HouGeo::loadVolumePrimitive P has no data");

                const std::size_t tupleOffset =
                    static_cast<std::size_t>(pointIndex)
                    * positionAttribute->tupleSize().asSize();
                if( positionAttribute->storage_
                    == AttributeAdapter::Storage::float16 )
                {
                    position = math::V3f(
                        halfBitsToFloat(positionData.read<uword>(tupleOffset)),
                        halfBitsToFloat(positionData.read<uword>(tupleOffset + 1)),
                        halfBitsToFloat(positionData.read<uword>(tupleOffset + 2)));
                }
                else if( positionAttribute->storage_
                    == AttributeAdapter::Storage::float32 )
                {
                    position = math::V3f(
                        positionData.read<real32>(tupleOffset),
                        positionData.read<real32>(tupleOffset + 1),
                        positionData.read<real32>(tupleOffset + 2));
                }
                else if( positionAttribute->storage_
                    == AttributeAdapter::Storage::float64 )
                {
                    position = math::V3f(
                        static_cast<real32>(
                            positionData.read<real64>(tupleOffset)),
                        static_cast<real32>(
                            positionData.read<real64>(tupleOffset + 1)),
                        static_cast<real32>(
                            positionData.read<real64>(tupleOffset + 2)));
                }
                else
                {
                    throw DiagnosticException(Diagnostic{
                        DiagnosticSeverity::error,
                        DiagnosticCategory::unsupported_input,
                        "HouGeo::loadVolumePrimitive supports only floating-point P storage",
                        -1,
                        "P.storage"});
                }
            });

            const math::Matrix44d translation =
                math::Matrix44d::translationMatrix(position);
            const math::Matrix44d localToWorld =
                math::Matrix44d::scaleMatrix(2.0)
                * math::Matrix44d::translationMatrix(-1.0, -1.0, -1.0)
                * rotationScale
                * translation;
            volumePrimitive->scalar_field_->setLocalToWorld(localToWorld);
        }

        const bool hasSharedVoxels = volume->contains("sharedvoxels");
        const bool hasInlineVoxels = volume->contains("voxels");
        if( hasSharedVoxels == hasInlineVoxels )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "HouGeo::loadVolumePrimitive requires exactly one voxel payload",
                -1,
                "voxels"});

        std::vector<float> voxelValues;
        if( hasSharedVoxels )
        {
            withSchemaPath("sharedvoxels", [&]()
            {
                const std::string dataId =
                    volume->get<std::string>("sharedvoxels");
                const auto sharedData =
                    sharedPrimitiveData.sharedVoxelData.find(dataId);
                if( sharedData == sharedPrimitiveData.sharedVoxelData.end() )
                    throw std::runtime_error(
                        "HouGeo::loadVolumePrimitive shared voxel data was not found");
                voxelValues = loadVoxelData(sharedData->second, resolution);
            });
        }
        else
        {
            withSchemaPath("voxels", [&]()
            {
                voxelValues = loadVoxelData(
                    toObject(volume->array("voxels")), resolution);
            });
        }

        volumePrimitive->scalar_field_->resize(resolution);
        std::copy(
            voxelValues.begin(),
            voxelValues.end(),
            volumePrimitive->scalar_field_->values().begin());

        if( volume->contains("visualization") )
        {
            withSchemaPath("visualization", [&]()
            {
                json::ObjectPtr visualization = volume->object("visualization");
                if( !visualization )
                    throw std::runtime_error(
                        "HouGeo::loadVolumePrimitive visualization must be an object");
                volumePrimitive->visualization_mode_ =
                    visualization->get<std::string>("mode", "smoke");
                if( volumePrimitive->visualization_mode_.empty() )
                    volumePrimitive->visualization_mode_ = "smoke";
                volumePrimitive->visualization_iso_ =
                    visualization->get<real32>("iso", 0.0f);
                volumePrimitive->visualization_density_ =
                    visualization->get<real32>("density", 1.0f);
            });
        }

        m_primitives.push_back(volumePrimitive);
    }

    std::vector<float> HouGeo::loadVoxelData(
        json::ObjectPtr voxelObject,
        const math::V3i& resolution)
    {
        if( !voxelObject )
            throw std::invalid_argument(
                "HouGeo::loadVoxelData received null voxel data");
        const std::size_t voxelCount = volumeVoxelCount(resolution);

        const bool hasTiledArray = voxelObject->contains("tiledarray");
        const bool hasConstantArray = voxelObject->contains("constantarray");
        if( hasTiledArray == hasConstantArray )
            throw std::runtime_error(
                "HouGeo::loadVoxelData requires exactly one supported voxel representation");

        if( hasConstantArray )
        {
            const float constantValue =
                voxelObject->get<float>("constantarray");
            return std::vector<float>(voxelCount, constantValue);
        }

        json::ObjectPtr tiledArray =
            toObject(voxelObject->array("tiledarray"));
        if( !tiledArray || !tiledArray->contains("tiles") )
            throw std::runtime_error(
                "HouGeo::loadVoxelData tiled array is missing tiles");
        json::ArrayPtr tiles = tiledArray->array("tiles");
        if( !tiles )
            throw std::runtime_error(
                "HouGeo::loadVoxelData tiles must be an array");

        const std::size_t tilesX =
            (static_cast<std::size_t>(resolution.x) + 15u) / 16u;
        const std::size_t tilesY =
            (static_cast<std::size_t>(resolution.y) + 15u) / 16u;
        const std::size_t tilesZ =
            (static_cast<std::size_t>(resolution.z) + 15u) / 16u;
        const std::size_t expectedTileCount = checkedProduct(
            checkedProduct(tilesX, tilesY, "Volume tile count"),
            tilesZ,
            "Volume tile count");
        if( expectedTileCount
            > static_cast<std::size_t>(std::numeric_limits<int>::max()) )
        {
            throw std::length_error(
                "Volume tile count exceeds supported indexing");
        }
        if( tiles->size() != static_cast<sint64>(expectedTileCount) )
            throw std::runtime_error(
                "HouGeo::loadVoxelData tile count does not match resolution");

        struct TileInfo
        {
            json::ObjectPtr tile;
            int compression = 0;
            int voxelOffsetX = 0;
            int voxelOffsetY = 0;
            int voxelOffsetZ = 0;
            int tileSizeX = 0;
            int tileSizeY = 0;
            int tileSizeZ = 0;
            std::size_t voxelCount = 0;
        };

        std::vector<TileInfo> tileInfos;
        tileInfos.reserve(expectedTileCount);
        std::size_t currentTileIndex = 0;
        for( std::size_t tileZ = 0; tileZ < tilesZ; ++tileZ )
        {
            const int voxelOffsetZ = static_cast<int>(tileZ * 16u);
            const int tileSizeZ = std::min(16, resolution.z - voxelOffsetZ);
            for( std::size_t tileY = 0; tileY < tilesY; ++tileY )
            {
                const int voxelOffsetY = static_cast<int>(tileY * 16u);
                const int tileSizeY =
                    std::min(16, resolution.y - voxelOffsetY);
                for( std::size_t tileX = 0;
                    tileX < tilesX;
                    ++tileX, ++currentTileIndex )
                {
                    const int voxelOffsetX = static_cast<int>(tileX * 16u);
                    const int tileSizeX =
                        std::min(16, resolution.x - voxelOffsetX);
                    const std::size_t tileVoxelCount = checkedProduct(
                        checkedProduct(
                            static_cast<std::size_t>(tileSizeX),
                            static_cast<std::size_t>(tileSizeY),
                            "Volume tile"),
                        static_cast<std::size_t>(tileSizeZ),
                        "Volume tile");
                    const std::string tilePath =
                        "tiledarray.tiles["
                        + std::to_string(currentTileIndex)
                        + "]";
                    withSchemaPath(tilePath, [&]()
                    {
                        json::ArrayPtr tileValues =
                            tiles->array(static_cast<int>(currentTileIndex));
                        if( !tileValues )
                            throw std::runtime_error(
                                "HouGeo::loadVoxelData tile must be a flattened object");
                        json::ObjectPtr tile = toObject(tileValues);
                        if( !tile->contains("data") )
                            throw std::runtime_error(
                                "HouGeo::loadVoxelData tile is missing data");

                        const int compression =
                            tile->get<int>("compression", 1);
                        if( compression == 0 || compression == 1 )
                        {
                            json::ArrayPtr data = tile->array("data");
                            if( !data
                                || data->size()
                                    != static_cast<sint64>(tileVoxelCount) )
                            {
                                throw std::runtime_error(
                                    "HouGeo::loadVoxelData raw tile payload size mismatch");
                            }
                        }
                        else if( compression == 2 )
                        {
                            static_cast<void>(tile->get<float>("data"));
                        }
                        else
                        {
                            throw DiagnosticException(Diagnostic{
                                DiagnosticSeverity::error,
                                DiagnosticCategory::unsupported_input,
                                "HouGeo::loadVoxelData does not support tile compression "
                                    + std::to_string(compression),
                                -1,
                                "compression"});
                        }

                        tileInfos.push_back(TileInfo{
                            tile,
                            compression,
                            voxelOffsetX,
                            voxelOffsetY,
                            voxelOffsetZ,
                            tileSizeX,
                            tileSizeY,
                            tileSizeZ,
                            tileVoxelCount});
                    });
                }
            }
        }

        std::vector<float> voxelData(voxelCount);
        for( const TileInfo& tileInfo : tileInfos )
        {
            if( tileInfo.compression == 0 || tileInfo.compression == 1 )
            {
                json::ArrayPtr data = tileInfo.tile->array("data");
                std::size_t sourceIndex = 0;
                for( int localZ = 0; localZ < tileInfo.tileSizeZ; ++localZ )
                {
                    for( int localY = 0; localY < tileInfo.tileSizeY; ++localY )
                    {
                        for( int localX = 0;
                            localX < tileInfo.tileSizeX;
                            ++localX, ++sourceIndex )
                        {
                            const std::size_t destinationIndex = volumeIndex(
                                tileInfo.voxelOffsetX + localX,
                                tileInfo.voxelOffsetY + localY,
                                tileInfo.voxelOffsetZ + localZ,
                                resolution);
                            if( destinationIndex >= voxelCount
                                || sourceIndex >= tileInfo.voxelCount )
                            {
                                throw std::out_of_range(
                                    "HouGeo::loadVoxelData tile index exceeds validated storage");
                            }
                            voxelData[destinationIndex] =
                                data->get<float>(static_cast<int>(sourceIndex));
                        }
                    }
                }
            }
            else
            {
                const float constantValue =
                    tileInfo.tile->get<float>("data");
                for( int localZ = 0; localZ < tileInfo.tileSizeZ; ++localZ )
                {
                    for( int localY = 0; localY < tileInfo.tileSizeY; ++localY )
                    {
                        for( int localX = 0;
                            localX < tileInfo.tileSizeX;
                            ++localX )
                        {
                            const std::size_t destinationIndex = volumeIndex(
                                tileInfo.voxelOffsetX + localX,
                                tileInfo.voxelOffsetY + localY,
                                tileInfo.voxelOffsetZ + localZ,
                                resolution);
                            if( destinationIndex >= voxelCount )
                            {
                                throw std::out_of_range(
                                    "HouGeo::loadVoxelData tile index exceeds validated storage");
                            }
                            voxelData[destinationIndex] = constantValue;
                        }
                    }
                }
            }
        }
        return voxelData;
    }
}
