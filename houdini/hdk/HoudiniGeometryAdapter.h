#pragma once

#include <houio/NativePolygonWriter.h>

#include <cstdint>
#include <vector>

class GU_Detail;
class UT_Interrupt;

namespace houio::hdk
{
    /// Owned C-ABI payload extracted from one cooked Houdini detail.
    struct NativePolygonDetail
    {
        std::vector<float> positions_xyzw;
        std::vector<std::int32_t> topology;
        std::vector<HouIONativePolygon> polygons;
    };

    /// Extract polygons and canonical point positions without using HOM.
    ///
    /// The first native ROP slice accepts polygon and polyline primitives with
    /// canonical point positions. Other public attributes and groups are
    /// rejected explicitly so the exporter never reports a lossy write as a
    /// successful one.
    NativePolygonDetail adaptDetail(
        const GU_Detail& detail,
        UT_Interrupt* interrupt);
}
