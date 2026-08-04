#pragma once

#include <houio/HouGeo.h>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace houio::hougeo_attribute_detail
{
    [[nodiscard]] std::vector<int> expandPagedIntValues(
        json::ObjectPtr values,
        sint64 elementCount,
        int tupleSize,
        const std::string& attributeName);

    [[nodiscard]] Attribute::ComponentType componentTypeForStorage(
        HouGeoAdapter::AttributeAdapter::Storage storage) noexcept;

    void storeNumericComponent(
        std::span<std::byte> data,
        std::size_t destinationIndex,
        HouGeoAdapter::AttributeAdapter::Storage storage,
        const json::Value& value);
}
