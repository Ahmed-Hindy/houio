#include "HouGeoAttributeLoad.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace houio
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

        std::size_t checkedProduct(
            std::size_t left,
            std::size_t right,
            const std::string& description)
        {
            if( left != 0 && right > std::numeric_limits<std::size_t>::max() / left )
                throw std::length_error(description + " exceeds addressable storage");
            return left * right;
        }
    }

    void HouGeo::loadGroups(
        json::ArrayPtr groups,
        sint64 elementCount,
        std::map<std::string, std::vector<bool>>& destination)
    {
        if( !groups || elementCount < 0 )
            throw std::runtime_error("HouGeo::loadGroups received invalid group data");
        if( elementCount > static_cast<sint64>(std::numeric_limits<int>::max()) )
        {
            throw std::length_error(
                "HouGeo::loadGroups element count exceeds supported indexing");
        }

        const int groupCount = checkedArrayCount(
            groups, "HouGeo::loadGroups group list");
        for( int groupIndex = 0; groupIndex < groupCount; ++groupIndex )
        {
            json::ArrayPtr group = groups->array(groupIndex);
            if( !group || group->size() != 2 )
            {
                throw std::runtime_error(
                    "HouGeo::loadGroups expected definition and data arrays");
            }

            json::ObjectPtr definition = toObject(group->array(0));
            json::ObjectPtr data = toObject(group->array(1));
            const std::string name = definition->get<std::string>("name", "");
            if( name.empty() )
            {
                throw std::runtime_error(
                    "HouGeo::loadGroups encountered a group without a name");
            }
            if( destination.find(name) != destination.end() )
            {
                throw std::runtime_error(
                    "HouGeo::loadGroups encountered duplicate group " + name);
            }
            if( !data->contains("selection") )
            {
                throw std::runtime_error(
                    "HouGeo::loadGroups missing selection for group " + name);
            }

            json::ArrayPtr selection = data->array("selection");
            if( !selection || selection->size() != 2
                || !selection->value(0).isString() )
            {
                throw std::runtime_error(
                    "HouGeo::loadGroups received malformed selection data for group "
                    + name);
            }
            if( selection->get<std::string>(0) != "unordered" )
            {
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::unsupported_input,
                    "HouGeo::loadGroups supports only unordered selections for group "
                        + name,
                    -1,
                    "selection"});
            }

            json::ArrayPtr encodedMembership = selection->array(1);
            if( !encodedMembership || encodedMembership->size() != 2
                || !encodedMembership->value(0).isString() )
            {
                throw std::runtime_error(
                    "HouGeo::loadGroups received malformed membership data for group "
                    + name);
            }
            if( encodedMembership->get<std::string>(0) != "i8" )
            {
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::unsupported_input,
                    "HouGeo::loadGroups requires i8 membership encoding for group "
                        + name,
                    -1,
                    "selection.encoding"});
            }

            json::ArrayPtr membershipValues = encodedMembership->array(1);
            if( !membershipValues || membershipValues->size() != elementCount )
            {
                throw std::runtime_error(
                    "HouGeo::loadGroups membership count mismatch for group " + name);
            }

            std::vector<bool> membership(
                static_cast<std::size_t>(elementCount), false);
            const int membershipCount = static_cast<int>(elementCount);
            for( int elementIndex = 0;
                elementIndex < membershipCount;
                ++elementIndex )
            {
                const int value = membershipValues->get<int>(elementIndex);
                if( value != 0 && value != 1 )
                {
                    throw std::runtime_error(
                        "HouGeo::loadGroups membership must contain only zero or one for group "
                        + name);
                }
                membership[static_cast<std::size_t>(elementIndex)] = value != 0;
            }
            destination.emplace(name, std::move(membership));
        }
    }

    HouGeo::HouAttribute::Ptr HouGeo::loadAttribute(
        json::ArrayPtr attribute,
        sint64 elementCount)
    {
        if( !attribute || attribute->size() != 2 )
        {
            throw std::runtime_error(
                "HouGeo::loadAttribute expected definition and data arrays");
        }
        if( elementCount < 0
            || elementCount > static_cast<sint64>(std::numeric_limits<int>::max()) )
        {
            throw std::length_error(
                "HouGeo::loadAttribute element count exceeds supported indexing");
        }

        json::ObjectPtr attrDef = toObject(attribute->array(0));
        json::ObjectPtr attrData = toObject(attribute->array(1));

        HouGeo::HouAttribute::Ptr attr = std::make_shared<HouGeo::HouAttribute>();

        std::string attrName = attrDef->get<std::string>("name");
        attr->name_ = attrName;
        attr->scope_ = attrDef->get<std::string>("scope", "public");
        if( attr->scope_.empty() )
        {
            throw std::runtime_error(
                "HouGeo::loadAttribute scope cannot be empty for attribute " + attrName);
        }
        if( attrDef->contains("options") )
        {
            attr->options_ = attrDef->object("options");
            if( !attr->options_ )
            {
                throw std::runtime_error(
                    "HouGeo::loadAttribute options must be an object for attribute "
                    + attrName);
            }
        }
        else
        {
            attr->options_ = json::Object::create();
        }
        AttributeAdapter::Type attrType = AttributeAdapter::parseType(
            attrDef->get<std::string>("type"));

        if( attrType == AttributeAdapter::Type::numeric )
        {
            const std::string storageName = attrData->get<std::string>("storage");
            const AttributeAdapter::Storage attrStorage =
                AttributeAdapter::parseStorage(storageName);
            const std::optional<std::size_t> componentByteWidth =
                AttributeAdapter::storageByteWidth(attrStorage);
            const Attribute::ComponentType attrComponentType =
                hougeo_attribute_detail::componentTypeForStorage(attrStorage);
            if( !componentByteWidth
                || attrComponentType == Attribute::ComponentType::invalid )
            {
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::unsupported_input,
                    "HouGeo::loadAttribute does not support storage " + storageName,
                    -1,
                    "storage"});
            }

            const AttributeAdapter::TupleSize tupleSize(attrData->get<int>("size"));
            const int attrTupleSize = tupleSize.value();
            attr->numeric_attribute_ = std::make_shared<Attribute>(
                attrTupleSize, attrComponentType);
            attr->numeric_attribute_->resize(elementCount);
            std::span<std::byte> data = attr->numeric_attribute_->mutableBytes();

            const int dstTupleSize = attrTupleSize;
            attr->name_ = attrName;
            attr->type_ = attrType;
            attr->storage_ = attrStorage;
            attr->tuple_size_ = tupleSize;
            attr->element_count_ = static_cast<int>(elementCount);

            if( attrData->contains("values") )
            {
                json::ObjectPtr values = toObject(attrData->array("values"));
                if( values->contains("tuples") )
                {
                    json::ArrayPtr tuples = values->array("tuples");
                    if( !tuples || tuples->size() != elementCount )
                    {
                        throw std::runtime_error(
                            "HouGeo::loadAttribute tuple count mismatch for attribute "
                            + attrName);
                    }

                    for( sint64 elementIndex = 0;
                        elementIndex < elementCount;
                        ++elementIndex )
                    {
                        json::ArrayPtr tuple = tuples->array(
                            static_cast<int>(elementIndex));
                        if( !tuple || tuple->size() != attrTupleSize )
                        {
                            throw std::runtime_error(
                                "HouGeo::loadAttribute tuple size mismatch for attribute "
                                + attrName);
                        }

                        for( int componentIndex = 0;
                            componentIndex < attrTupleSize;
                            ++componentIndex )
                        {
                            const std::size_t destinationIndex =
                                static_cast<std::size_t>(elementIndex)
                                    * static_cast<std::size_t>(attrTupleSize)
                                + static_cast<std::size_t>(componentIndex);
                            hougeo_attribute_detail::storeNumericComponent(
                                data,
                                destinationIndex,
                                attrStorage,
                                tuple->value(componentIndex));
                        }
                    }
                }
                else if( values->contains("arrays") )
                {
                    json::ArrayPtr componentArrays = values->array("arrays");
                    if( !componentArrays
                        || componentArrays->size() != attrTupleSize )
                    {
                        throw std::runtime_error(
                            "HouGeo::loadAttribute component array count mismatch for attribute "
                            + attrName);
                    }

                    for( int componentIndex = 0;
                        componentIndex < attrTupleSize;
                        ++componentIndex )
                    {
                        json::ArrayPtr componentValues = componentArrays->array(
                            componentIndex);
                        if( !componentValues
                            || componentValues->size() != elementCount )
                        {
                            throw std::runtime_error(
                                "HouGeo::loadAttribute component value count mismatch for attribute "
                                + attrName);
                        }

                        for( sint64 elementIndex = 0;
                            elementIndex < elementCount;
                            ++elementIndex )
                        {
                            const std::size_t destinationIndex =
                                static_cast<std::size_t>(elementIndex)
                                    * static_cast<std::size_t>(attrTupleSize)
                                + static_cast<std::size_t>(componentIndex);
                            hougeo_attribute_detail::storeNumericComponent(
                                data,
                                destinationIndex,
                                attrStorage,
                                componentValues->value(
                                    static_cast<int>(elementIndex)));
                        }
                    }
                }
                else if( values->contains("rawpagedata") )
                {
                    const int elementsPerPage = values->get<int>("pagesize");
                    if( elementsPerPage <= 0 )
                    {
                        throw std::runtime_error(
                            "HouGeo::loadAttribute pagesize must be positive for attribute "
                            + attrName);
                    }
                    const std::size_t pageCount = elementCount == 0
                        ? 0
                        : (static_cast<std::size_t>(elementCount)
                              + static_cast<std::size_t>(elementsPerPage) - 1u)
                            / static_cast<std::size_t>(elementsPerPage);

                    std::vector<ubyte> attrPacking;
                    if( values->contains("packing") )
                    {
                        json::ArrayPtr packingArray = values->array("packing");
                        if( !packingArray || packingArray->size() <= 0
                            || packingArray->size()
                                > std::numeric_limits<int>::max() )
                        {
                            throw std::runtime_error(
                                "HouGeo::loadAttribute packing must be a non-empty array for attribute "
                                + attrName);
                        }
                        const int packingCount = static_cast<int>(
                            packingArray->size());
                        for( int packingIndex = 0;
                            packingIndex < packingCount;
                            ++packingIndex )
                        {
                            const ubyte packSize = packingArray->get<ubyte>(
                                packingIndex);
                            if( packSize == 0 )
                            {
                                throw std::runtime_error(
                                    "HouGeo::loadAttribute packing cannot contain zero for attribute "
                                    + attrName);
                            }
                            attrPacking.push_back(packSize);
                        }
                    }
                    else
                    {
                        if( attrTupleSize < 0
                            || attrTupleSize > std::numeric_limits<ubyte>::max() )
                        {
                            throw std::runtime_error(
                                "HouGeo::loadAttribute tuple size exceeds packing range for attribute "
                                + attrName);
                        }
                        attrPacking.push_back(static_cast<ubyte>(attrTupleSize));
                    }
                    std::size_t packedComponentCount = 0;
                    for( ubyte packSize : attrPacking )
                        packedComponentCount += static_cast<std::size_t>(packSize);
                    if( packedComponentCount
                        != static_cast<std::size_t>(attrTupleSize) )
                    {
                        throw std::runtime_error(
                            "HouGeo::loadAttribute packing does not cover tuple size for attribute "
                            + attrName);
                    }

                    std::vector<std::vector<bool>> constantPageFlagsPerPack;
                    if( values->contains("constantpageflags") )
                    {
                        json::ArrayPtr constantPageFlags = values->array(
                            "constantpageflags");
                        if( !constantPageFlags
                            || constantPageFlags->size()
                                != static_cast<sint64>(attrPacking.size()) )
                        {
                            throw std::runtime_error(
                                "HouGeo::loadAttribute constantpageflags pack count mismatch for attribute "
                                + attrName);
                        }

                        for( std::size_t packIndex = 0;
                            packIndex < attrPacking.size();
                            ++packIndex )
                        {
                            json::ArrayPtr packConstantFlags =
                                constantPageFlags->array(
                                    static_cast<int>(packIndex));
                            if( !packConstantFlags
                                || packConstantFlags->size()
                                    != static_cast<sint64>(pageCount) )
                            {
                                throw std::runtime_error(
                                    "HouGeo::loadAttribute constantpageflags page count mismatch for attribute "
                                    + attrName);
                            }
                            constantPageFlagsPerPack.emplace_back();
                            constantPageFlagsPerPack.back().reserve(pageCount);
                            for( std::size_t pageIndex = 0;
                                pageIndex < pageCount;
                                ++pageIndex )
                            {
                                constantPageFlagsPerPack.back().push_back(
                                    packConstantFlags->get<bool>(
                                        static_cast<int>(pageIndex)));
                            }
                        }
                    }
                    else
                    {
                        constantPageFlagsPerPack.resize(
                            attrPacking.size(),
                            std::vector<bool>(pageCount, false));
                    }

                    json::ArrayPtr rawPageData = values->array("rawpagedata");
                    if( !rawPageData )
                    {
                        throw std::runtime_error(
                            "HouGeo::loadAttribute rawpagedata must be an array for attribute "
                            + attrName);
                    }

                    std::size_t expectedRawValueCount = 0;
                    for( std::size_t pageIndex = 0;
                        pageIndex < pageCount;
                        ++pageIndex )
                    {
                        const std::size_t pageStartElement =
                            pageIndex * static_cast<std::size_t>(elementsPerPage);
                        const std::size_t pageElementCount = std::min(
                            static_cast<std::size_t>(elementCount) - pageStartElement,
                            static_cast<std::size_t>(elementsPerPage));
                        for( std::size_t packIndex = 0;
                            packIndex < attrPacking.size();
                            ++packIndex )
                        {
                            const std::size_t repeatedElementCount =
                                constantPageFlagsPerPack[packIndex][pageIndex]
                                ? 1u
                                : pageElementCount;
                            const std::size_t packValueCount = checkedProduct(
                                static_cast<std::size_t>(attrPacking[packIndex]),
                                repeatedElementCount,
                                "Attribute page payload");
                            if( packValueCount
                                > std::numeric_limits<std::size_t>::max()
                                    - expectedRawValueCount )
                            {
                                throw std::length_error(
                                    "Attribute page payload exceeds addressable storage");
                            }
                            expectedRawValueCount += packValueCount;
                        }
                    }
                    if( rawPageData->size()
                        != static_cast<sint64>(expectedRawValueCount) )
                    {
                        throw std::runtime_error(
                            "HouGeo::loadAttribute rawpagedata size mismatch for attribute "
                            + attrName);
                    }

                    if( elementCount
                        > static_cast<sint64>(std::numeric_limits<int>::max()) )
                    {
                        throw std::overflow_error(
                            "HouGeo::loadAttribute element count exceeds int range for attribute "
                            + attrName);
                    }
                    attr->element_count_ = static_cast<int>(elementCount);
                    std::size_t elementsRemaining = static_cast<std::size_t>(
                        attr->element_count_);

                    std::size_t pageIndex = 0;
                    std::size_t pageStartIndex = 0;
                    while( elementsRemaining > 0 )
                    {
                        const std::size_t pageStartElement =
                            pageIndex * static_cast<std::size_t>(elementsPerPage);
                        const std::size_t numElements = std::min(
                            elementsRemaining,
                            static_cast<std::size_t>(elementsPerPage));

                        std::size_t packIndex = 0;
                        std::size_t startComponentIndex = 0;
                        for( std::vector<ubyte>::iterator it = attrPacking.begin();
                            it != attrPacking.end();
                            ++it, ++packIndex )
                        {
                            const ubyte pack = *it;
                            const int remainingComponents = std::max(
                                0,
                                dstTupleSize
                                    - static_cast<int>(startComponentIndex));
                            const std::size_t maxPack = std::min(
                                static_cast<std::size_t>(pack),
                                static_cast<std::size_t>(remainingComponents));

                            if( maxPack == 0 )
                                break;

                            const bool isConstant =
                                constantPageFlagsPerPack[packIndex].empty()
                                ? false
                                : constantPageFlagsPerPack[packIndex][pageIndex];
                            std::size_t elementIndex = pageStartIndex;

                            for( std::size_t index = 0;
                                index < numElements;
                                ++index )
                            {
                                if( !isConstant )
                                    elementIndex = pageStartIndex + index * pack;
                                const std::size_t destElementIndex =
                                    (pageStartElement + index)
                                    * static_cast<std::size_t>(dstTupleSize);

                                for( std::size_t component = 0;
                                    component < maxPack;
                                    ++component )
                                {
                                    const std::size_t rawIndex =
                                        elementIndex + component;
                                    if( rawIndex
                                        > static_cast<std::size_t>(
                                            std::numeric_limits<int>::max()) )
                                    {
                                        throw std::overflow_error(
                                            "HouGeo::loadAttribute raw page index exceeds int range for attribute "
                                            + attrName);
                                    }
                                    hougeo_attribute_detail::storeNumericComponent(
                                        data,
                                        destElementIndex + startComponentIndex
                                            + component,
                                        attrStorage,
                                        rawPageData->value(
                                            static_cast<int>(rawIndex)));
                                }
                            }

                            startComponentIndex += pack;
                            if( !isConstant )
                                pageStartIndex += numElements * pack;
                            else
                                pageStartIndex += pack;
                        }

                        elementsRemaining -= numElements;
                        ++pageIndex;
                    }
                    if( pageStartIndex != expectedRawValueCount )
                    {
                        throw std::runtime_error(
                            "HouGeo::loadAttribute did not consume the complete paged payload for attribute "
                            + attrName);
                    }
                }
            }
        }
        else if( attrType == AttributeAdapter::Type::string )
        {
            if( !attrData->contains("strings") )
            {
                throw std::runtime_error(
                    "HouGeo::loadAttribute missing string table for attribute "
                    + attrName);
            }

            json::ArrayPtr stringsArray = attrData->array("strings");
            const int stringCount = checkedArrayCount(
                stringsArray,
                "HouGeo::loadAttribute string table for attribute " + attrName);
            std::vector<std::string> stringTable;
            stringTable.reserve(static_cast<std::size_t>(stringCount));
            for( int stringIndex = 0; stringIndex < stringCount; ++stringIndex )
                stringTable.push_back(stringsArray->get<std::string>(stringIndex));

            const AttributeAdapter::TupleSize tupleSize(
                attrData->get<int>("size", 1));
            const std::size_t scalarCount = checkedProduct(
                static_cast<std::size_t>(elementCount),
                tupleSize.asSize(),
                "String attribute value count");
            attr->name_ = attrName;
            attr->type_ = attrType;
            attr->storage_ = AttributeAdapter::Storage::int32;
            attr->tuple_size_ = tupleSize;

            if( attrData->contains("indices") )
            {
                json::ObjectPtr indices = toObject(attrData->array("indices"));
                const std::vector<int> indexValues =
                    hougeo_attribute_detail::expandPagedIntValues(
                        indices,
                        elementCount,
                        tupleSize.value(),
                        attrName);

                attr->string_values_.reserve(scalarCount);
                for( int stringIndex : indexValues )
                {
                    if( stringIndex == -1 )
                    {
                        attr->string_values_.emplace_back();
                        continue;
                    }
                    if( stringIndex < 0
                        || static_cast<std::size_t>(stringIndex)
                            >= stringTable.size() )
                    {
                        throw std::runtime_error(
                            "HouGeo::loadAttribute string index out of range for attribute "
                            + attrName);
                    }
                    attr->string_values_.push_back(
                        stringTable[static_cast<std::size_t>(stringIndex)]);
                }
            }
            else if( stringTable.size() == scalarCount )
            {
                attr->string_values_ = stringTable;
            }
            else if( stringTable.size() == 1 && scalarCount > 0 )
            {
                attr->string_values_.assign(scalarCount, stringTable.front());
            }
            else if( scalarCount == 0 )
            {
                attr->string_values_.clear();
            }
            else
            {
                throw std::runtime_error(
                    "HouGeo::loadAttribute cannot map string table to elements for attribute "
                    + attrName);
            }

            if( attr->string_values_.size() != scalarCount )
            {
                throw std::runtime_error(
                    "HouGeo::loadAttribute string tuple storage mismatch for attribute "
                    + attrName);
            }
            attr->element_count_ = static_cast<int>(elementCount);
        }
        else if( attrType == AttributeAdapter::Type::dictionary )
        {
            json::ArrayPtr dictionaries = attrData->array("dicts");
            const int dictionaryCount = checkedArrayCount(
                dictionaries,
                "HouGeo::loadAttribute dictionary table for attribute " + attrName);
            std::vector<json::ObjectPtr> dictionaryTable;
            dictionaryTable.reserve(static_cast<std::size_t>(dictionaryCount));
            for( int dictionaryIndex = 0;
                dictionaryIndex < dictionaryCount;
                ++dictionaryIndex )
            {
                json::ObjectPtr dictionary = dictionaries->object(dictionaryIndex);
                if( !dictionary )
                {
                    throw std::runtime_error(
                        "HouGeo::loadAttribute expected a dictionary value for attribute "
                        + attrName);
                }
                dictionaryTable.push_back(dictionary);
            }

            attr->name_ = attrName;
            attr->type_ = attrType;
            attr->storage_ = AttributeAdapter::Storage::int32;
            attr->tuple_size_ = AttributeAdapter::TupleSize(1);
            attr->dictionary_values_.reserve(
                static_cast<std::size_t>(elementCount));

            if( attrData->contains("indices") )
            {
                json::ObjectPtr indices = toObject(attrData->array("indices"));
                const std::vector<int> indexValues =
                    hougeo_attribute_detail::expandPagedIntValues(
                        indices, elementCount, 1, attrName);
                for( int dictionaryIndex : indexValues )
                {
                    if( dictionaryIndex == -1 )
                    {
                        attr->dictionary_values_.push_back(json::Object::create());
                        continue;
                    }
                    if( dictionaryIndex < 0
                        || static_cast<std::size_t>(dictionaryIndex)
                            >= dictionaryTable.size() )
                    {
                        throw std::runtime_error(
                            "HouGeo::loadAttribute dictionary index out of range for attribute "
                            + attrName);
                    }
                    attr->dictionary_values_.push_back(
                        dictionaryTable[
                            static_cast<std::size_t>(dictionaryIndex)]);
                }
            }
            else if( dictionaryTable.size()
                == static_cast<std::size_t>(elementCount) )
            {
                attr->dictionary_values_ = dictionaryTable;
            }
            else if( dictionaryTable.size() == 1 && elementCount > 0 )
            {
                attr->dictionary_values_.assign(
                    static_cast<std::size_t>(elementCount),
                    dictionaryTable.front());
            }
            else if( elementCount != 0 )
            {
                throw std::runtime_error(
                    "HouGeo::loadAttribute cannot map dictionary table to elements for attribute "
                    + attrName);
            }

            attr->element_count_ = static_cast<int>(
                attr->dictionary_values_.size());
        }
        else if( attrType == AttributeAdapter::Type::invalid )
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::unsupported_input,
                "HouGeo::loadAttribute does not support attribute type "
                    + attrDef->get<std::string>("type"),
                -1,
                "type"});
        }

        return attr;
    }
}
