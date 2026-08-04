#include "HouGeoAttributeLoad.h"

#include <houio/HalfFloat.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace houio::hougeo_attribute_detail
{
    namespace
    {
        int checkedArrayCount(
            const json::ArrayPtr& array,
            const std::string& description)
        {
            if( !array )
                throw std::runtime_error(description + " must be an array");
            const sint64 count = array->size();
            if( count < 0 )
                throw std::runtime_error(description + " has a negative element count");
            if( count > static_cast<sint64>(std::numeric_limits<int>::max()) )
                throw std::length_error(description + " exceeds supported indexing");
            return static_cast<int>(count);
        }

        template<typename T>
        void storeNumericValue(
            std::span<std::byte> data,
            std::size_t destinationIndex,
            const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            if( destinationIndex > std::numeric_limits<std::size_t>::max() / sizeof(T) )
                throw std::out_of_range("Numeric attribute destination index overflow");
            const std::size_t byteOffset = destinationIndex * sizeof(T);
            if( byteOffset > data.size() || sizeof(T) > data.size() - byteOffset )
                throw std::out_of_range("Numeric attribute destination index is outside storage");
            std::memcpy(data.data() + byteOffset, &value, sizeof(T));
        }
    }

    std::vector<int> expandPagedIntValues(
        json::ObjectPtr values,
        sint64 elementCount,
        int tupleSize,
        const std::string& attributeName)
    {
        if( !values )
            throw std::runtime_error(
                "HouGeo::loadAttribute missing integer value object for attribute "
                + attributeName);
        if( elementCount < 0
            || elementCount > static_cast<sint64>(std::numeric_limits<int>::max()) )
        {
            throw std::length_error(
                "HouGeo::loadAttribute integer element count exceeds supported indexing for attribute "
                + attributeName);
        }
        if( tupleSize <= 0 || values->get<int>("size", 1) != tupleSize )
        {
            throw std::runtime_error(
                "HouGeo::loadAttribute integer tuple size mismatch for attribute "
                + attributeName);
        }

        const std::size_t elementCountSize = static_cast<std::size_t>(elementCount);
        const std::size_t tupleSizeValue = static_cast<std::size_t>(tupleSize);
        if( elementCountSize != 0
            && tupleSizeValue > std::numeric_limits<std::size_t>::max() / elementCountSize )
        {
            throw std::length_error(
                "HouGeo::loadAttribute integer tuple count overflow for attribute "
                + attributeName);
        }
        const std::size_t scalarCount = elementCountSize * tupleSizeValue;
        if( scalarCount > static_cast<std::size_t>(std::numeric_limits<int>::max()) )
        {
            throw std::length_error(
                "HouGeo::loadAttribute integer scalar count exceeds supported indexing for attribute "
                + attributeName);
        }

        std::vector<int> result(scalarCount);
        if( values->contains("arrays") )
        {
            json::ArrayPtr arrays = values->array("arrays");
            if( !arrays || arrays->size() != tupleSize )
            {
                throw std::runtime_error(
                    "HouGeo::loadAttribute invalid integer component arrays for attribute "
                    + attributeName);
            }
            for( int componentIndex = 0; componentIndex < tupleSize; ++componentIndex )
            {
                json::ArrayPtr componentValues = arrays->array(componentIndex);
                if( !componentValues || componentValues->size() != elementCount )
                {
                    throw std::runtime_error(
                        "HouGeo::loadAttribute integer value count mismatch for attribute "
                        + attributeName);
                }
                for( int elementIndex = 0;
                    elementIndex < static_cast<int>(elementCount);
                    ++elementIndex )
                {
                    const std::size_t destinationIndex =
                        static_cast<std::size_t>(elementIndex) * tupleSizeValue
                        + static_cast<std::size_t>(componentIndex);
                    result[destinationIndex] = componentValues->get<int>(elementIndex);
                }
            }
            return result;
        }

        if( !values->contains("rawpagedata") )
        {
            throw std::runtime_error(
                "HouGeo::loadAttribute missing integer payload for attribute "
                + attributeName);
        }
        json::ArrayPtr rawPageData = values->array("rawpagedata");
        if( !rawPageData )
        {
            throw std::runtime_error(
                "HouGeo::loadAttribute invalid integer payload for attribute "
                + attributeName);
        }

        const int elementsPerPage = values->get<int>("pagesize", 0);
        if( elementsPerPage <= 0 )
        {
            throw std::runtime_error(
                "HouGeo::loadAttribute invalid page size for attribute "
                + attributeName);
        }
        const std::size_t pageCount = elementCountSize == 0
            ? 0
            : (elementCountSize + static_cast<std::size_t>(elementsPerPage) - 1u)
                / static_cast<std::size_t>(elementsPerPage);

        std::vector<int> packing;
        if( values->contains("packing") )
        {
            json::ArrayPtr packingValues = values->array("packing");
            const int packingCount = checkedArrayCount(
                packingValues,
                "HouGeo::loadAttribute integer packing for attribute " + attributeName);
            if( packingCount == 0 )
            {
                throw std::runtime_error(
                    "HouGeo::loadAttribute integer packing cannot be empty for attribute "
                    + attributeName);
            }
            std::size_t packedTupleSize = 0;
            for( int packingIndex = 0; packingIndex < packingCount; ++packingIndex )
            {
                const int packSize = packingValues->get<int>(packingIndex);
                if( packSize <= 0 )
                {
                    throw std::runtime_error(
                        "HouGeo::loadAttribute integer packing must be positive for attribute "
                        + attributeName);
                }
                const std::size_t packSizeValue = static_cast<std::size_t>(packSize);
                if( packedTupleSize > tupleSizeValue
                    || packSizeValue > tupleSizeValue - packedTupleSize )
                {
                    throw std::runtime_error(
                        "HouGeo::loadAttribute integer packing exceeds tuple size for attribute "
                        + attributeName);
                }
                packedTupleSize += packSizeValue;
                packing.push_back(packSize);
            }
            if( packedTupleSize != tupleSizeValue )
            {
                throw std::runtime_error(
                    "HouGeo::loadAttribute integer packing does not cover tuple size for attribute "
                    + attributeName);
            }
        }
        else
        {
            packing.push_back(tupleSize);
        }
        if( std::accumulate(packing.begin(), packing.end(), std::size_t{0})
            != tupleSizeValue )
        {
            throw std::runtime_error(
                "HouGeo::loadAttribute integer packing does not cover tuple size for attribute "
                + attributeName);
        }

        std::vector<std::vector<bool>> constantFlags(
            packing.size(), std::vector<bool>(pageCount, false));
        if( values->contains("constantpageflags") )
        {
            json::ArrayPtr flagsPerPack = values->array("constantpageflags");
            if( !flagsPerPack
                || flagsPerPack->size() != static_cast<sint64>(packing.size()) )
            {
                throw std::runtime_error(
                    "HouGeo::loadAttribute invalid constant page flags for attribute "
                    + attributeName);
            }
            for( std::size_t packIndex = 0; packIndex < packing.size(); ++packIndex )
            {
                json::ArrayPtr pageFlags = flagsPerPack->array(
                    static_cast<int>(packIndex));
                if( !pageFlags
                    || pageFlags->size() != static_cast<sint64>(pageCount) )
                {
                    throw std::runtime_error(
                        "HouGeo::loadAttribute constant page flag count mismatch for attribute "
                        + attributeName);
                }
                for( std::size_t pageIndex = 0; pageIndex < pageCount; ++pageIndex )
                {
                    constantFlags[packIndex][pageIndex] = pageFlags->get<bool>(
                        static_cast<int>(pageIndex));
                }
            }
        }

        std::size_t expectedRawCount = 0;
        for( std::size_t pageIndex = 0; pageIndex < pageCount; ++pageIndex )
        {
            const std::size_t pageStart =
                pageIndex * static_cast<std::size_t>(elementsPerPage);
            const std::size_t pageElementCount = std::min(
                elementCountSize - pageStart,
                static_cast<std::size_t>(elementsPerPage));
            for( std::size_t packIndex = 0; packIndex < packing.size(); ++packIndex )
            {
                const std::size_t repeatedElements =
                    constantFlags[packIndex][pageIndex] ? 1u : pageElementCount;
                const std::size_t packSize = static_cast<std::size_t>(packing[packIndex]);
                if( repeatedElements != 0
                    && packSize > std::numeric_limits<std::size_t>::max() / repeatedElements )
                {
                    throw std::length_error(
                        "HouGeo::loadAttribute integer page payload overflow for attribute "
                        + attributeName);
                }
                const std::size_t packValueCount = repeatedElements * packSize;
                if( packValueCount
                    > std::numeric_limits<std::size_t>::max() - expectedRawCount )
                {
                    throw std::length_error(
                        "HouGeo::loadAttribute integer page payload overflow for attribute "
                        + attributeName);
                }
                expectedRawCount += packValueCount;
            }
        }
        if( rawPageData->size() != static_cast<sint64>(expectedRawCount) )
        {
            throw std::runtime_error(
                "HouGeo::loadAttribute integer page payload size mismatch for attribute "
                + attributeName);
        }

        std::size_t rawIndex = 0;
        for( std::size_t pageIndex = 0; pageIndex < pageCount; ++pageIndex )
        {
            const std::size_t pageStart =
                pageIndex * static_cast<std::size_t>(elementsPerPage);
            const std::size_t pageElementCount = std::min(
                elementCountSize - pageStart,
                static_cast<std::size_t>(elementsPerPage));
            std::size_t componentStart = 0;
            for( std::size_t packIndex = 0; packIndex < packing.size(); ++packIndex )
            {
                const std::size_t packSize = static_cast<std::size_t>(packing[packIndex]);
                const bool constantPage = constantFlags[packIndex][pageIndex];
                for( std::size_t pageElement = 0;
                    pageElement < pageElementCount;
                    ++pageElement )
                {
                    const std::size_t sourceStart = rawIndex
                        + (constantPage ? 0u : pageElement * packSize);
                    const std::size_t destinationStart =
                        (pageStart + pageElement) * tupleSizeValue + componentStart;
                    for( std::size_t component = 0; component < packSize; ++component )
                    {
                        result[destinationStart + component] = rawPageData->get<int>(
                            static_cast<int>(sourceStart + component));
                    }
                }
                rawIndex += (constantPage ? 1u : pageElementCount) * packSize;
                componentStart += packSize;
            }
        }
        if( rawIndex != expectedRawCount )
        {
            throw std::runtime_error(
                "HouGeo::loadAttribute integer page expansion mismatch for attribute "
                + attributeName);
        }
        return result;
    }

    Attribute::ComponentType componentTypeForStorage(
        HouGeoAdapter::AttributeAdapter::Storage storage) noexcept
    {
        using Storage = HouGeoAdapter::AttributeAdapter::Storage;
        switch( storage )
        {
        case Storage::uint8:
            return Attribute::ComponentType::uint8;
        case Storage::float16:
            return Attribute::ComponentType::float16;
        case Storage::float32:
            return Attribute::ComponentType::float32;
        case Storage::float64:
            return Attribute::ComponentType::float64;
        case Storage::int32:
            return Attribute::ComponentType::int32;
        case Storage::int64:
            return Attribute::ComponentType::int64;
        case Storage::invalid:
            return Attribute::ComponentType::invalid;
        }
        return Attribute::ComponentType::invalid;
    }

    bool copyUniformNumericValues(
        std::span<std::byte> destination,
        std::size_t destinationStart,
        std::size_t destinationStride,
        HouGeoAdapter::AttributeAdapter::Storage storage,
        const json::ArrayPtr& source)
    {
        if( !source || !source->isUniform() )
            return false;

        json::Token::Type expectedStorage = json::Token::Type::nullValue;
        using Storage = HouGeoAdapter::AttributeAdapter::Storage;
        switch( storage )
        {
        case Storage::uint8:
            expectedStorage = json::Token::JID_UINT8;
            break;
        case Storage::float16:
            expectedStorage = json::Token::JID_REAL16;
            break;
        case Storage::float32:
            expectedStorage = json::Token::JID_REAL32;
            break;
        case Storage::float64:
            expectedStorage = json::Token::JID_REAL64;
            break;
        case Storage::int32:
            expectedStorage = json::Token::JID_INT32;
            break;
        case Storage::int64:
            expectedStorage = json::Token::JID_INT64;
            break;
        case Storage::invalid:
            return false;
        }
        if( source->uniformStorageType() != expectedStorage )
            return false;

        const std::optional<std::size_t> componentWidth =
            HouGeoAdapter::AttributeAdapter::storageByteWidth(storage);
        if( !componentWidth )
            return false;
        const sint64 sourceCountValue = source->size();
        if( sourceCountValue < 0 )
            throw std::runtime_error("Uniform numeric source has a negative element count");
        const std::size_t sourceCount = static_cast<std::size_t>(sourceCountValue);
        if( sourceCount != 0
            && *componentWidth > std::numeric_limits<std::size_t>::max() / sourceCount )
        {
            throw std::length_error("Uniform numeric source byte count overflow");
        }
        const std::size_t sourceByteCount = sourceCount * *componentWidth;
        const std::span<const std::byte> sourceData = source->uniformData();
        if( sourceData.size() != sourceByteCount )
            throw std::runtime_error("Uniform numeric source byte count is inconsistent");
        if( sourceCount == 0 )
            return true;
        if( sourceCount > 1 && destinationStride == 0 )
            throw std::invalid_argument("Uniform numeric destination stride cannot be zero");

        const std::size_t finalOffset = sourceCount - 1;
        if( finalOffset != 0
            && destinationStride > (std::numeric_limits<std::size_t>::max()
                - destinationStart) / finalOffset )
        {
            throw std::out_of_range("Uniform numeric destination index overflow");
        }
        const std::size_t finalIndex = destinationStart + finalOffset * destinationStride;
        if( finalIndex > std::numeric_limits<std::size_t>::max() / *componentWidth )
            throw std::out_of_range("Uniform numeric destination byte offset overflow");
        const std::size_t finalByteOffset = finalIndex * *componentWidth;
        if( finalByteOffset > destination.size()
            || *componentWidth > destination.size() - finalByteOffset )
        {
            throw std::out_of_range("Uniform numeric destination is outside storage");
        }

        if( destinationStride == 1 )
        {
            if( destinationStart > std::numeric_limits<std::size_t>::max()
                    / *componentWidth )
            {
                throw std::out_of_range("Uniform numeric destination byte offset overflow");
            }
            const std::size_t destinationByteOffset = destinationStart * *componentWidth;
            std::memcpy(
                destination.data() + destinationByteOffset,
                sourceData.data(),
                sourceByteCount);
            return true;
        }

        for( std::size_t sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex )
        {
            const std::size_t destinationIndex =
                destinationStart + sourceIndex * destinationStride;
            std::memcpy(
                destination.data() + destinationIndex * *componentWidth,
                sourceData.data() + sourceIndex * *componentWidth,
                *componentWidth);
        }
        return true;
    }

    void storeNumericComponent(
        std::span<std::byte> data,
        std::size_t destinationIndex,
        HouGeoAdapter::AttributeAdapter::Storage storage,
        const json::Value& value)
    {
        switch( storage )
        {
        case HouGeoAdapter::AttributeAdapter::Storage::uint8:
            storeNumericValue(data, destinationIndex, value.as<ubyte>());
            break;
        case HouGeoAdapter::AttributeAdapter::Storage::float16:
            storeNumericValue(data, destinationIndex, floatToHalfBits(value.as<real32>()));
            break;
        case HouGeoAdapter::AttributeAdapter::Storage::float32:
            storeNumericValue(data, destinationIndex, value.as<real32>());
            break;
        case HouGeoAdapter::AttributeAdapter::Storage::float64:
            storeNumericValue(data, destinationIndex, value.as<real64>());
            break;
        case HouGeoAdapter::AttributeAdapter::Storage::int32:
            storeNumericValue(data, destinationIndex, value.as<sint32>());
            break;
        case HouGeoAdapter::AttributeAdapter::Storage::int64:
            storeNumericValue(data, destinationIndex, value.as<sint64>());
            break;
        default:
            throw std::runtime_error("Unsupported numeric attribute storage");
        }
    }
}
