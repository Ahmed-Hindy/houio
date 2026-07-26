#pragma once

#include <cstddef>
#include <iosfwd>
#include <map>
#include <span>
#include <string>
#include <vector>

#include <houio/Diagnostic.h>
#include <houio/Geometry.h>
#include <houio/HouGeo.h>

namespace houio
{
    class GeometryIO;

    struct GeometryConversionReport
    {
        std::size_t sourcePointCount = 0;
        std::size_t outputPointCount = 0;
        std::size_t splitSourcePointCount = 0;
        std::size_t duplicatedPointCount = 0;
        bool windingReversed = false;
        bool polygonClosureLost = false;
        std::vector<std::string> skippedPointAttributes;
        std::vector<std::string> skippedVertexAttributes;
        std::vector<std::string> skippedPrimitiveAttributes;
        std::vector<std::string> skippedGlobalAttributes;
        std::vector<std::string> droppedPointGroups;
        std::vector<std::string> droppedVertexGroups;
        std::vector<std::string> droppedPrimitiveGroups;
    };

    struct GeometryConversionResult
    {
        Geometry::Ptr value;
        DiagnosticList diagnostics;
        GeometryConversionReport report;

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(value);
        }
    };

    class HouGeoIO final
    {
    public:
        [[nodiscard]] static HouGeo::Ptr import(std::istream& input);
        [[nodiscard]] static HouGeo::Ptr import(
            std::istream& input,
            DiagnosticList* diagnostics);
        [[nodiscard]] static HouGeo::Ptr import(
            std::istream& input,
            const json::ParserLimits& limits);
        [[nodiscard]] static HouGeo::Ptr import(
            std::istream& input,
            const json::ParserLimits& limits,
            DiagnosticList* diagnostics);

        [[nodiscard]] static Geometry::Ptr importGeometry(const std::string& path);
        [[nodiscard]] static Geometry::Ptr importGeometry(
            const std::string& path,
            DiagnosticList* diagnostics);
        [[nodiscard]] static ScalarField::Ptr importVolume(const std::string& path);
        [[nodiscard]] static ScalarField::Ptr importVolume(
            const std::string& path,
            DiagnosticList* diagnostics);

        static void makeLog(const std::string& path, std::ostream& output);

        // Lossy convenience conversion. Requires P, one fixed polygon size,
        // and domain-consistent attributes. Vertex attributes are flattened to
        // points by duplicating points where values differ.
        [[nodiscard]] static Geometry::Ptr convertToGeometry(
            HouGeo::ConstPtr houdini_geometry,
            HouGeoAdapter::Primitive::ConstPtr primitive);
        [[nodiscard]] static Geometry::Ptr convertToGeometry(
            HouGeo::ConstPtr houdini_geometry,
            HouGeoAdapter::Primitive::ConstPtr primitive,
            DiagnosticList* diagnostics);
        [[nodiscard]] static GeometryConversionResult convertToGeometryResult(
            HouGeo::ConstPtr houdini_geometry,
            HouGeoAdapter::Primitive::ConstPtr primitive);

        [[nodiscard]] static bool exportVolume(
            const std::string& filename,
            ScalarField::Ptr volume);
        [[nodiscard]] static bool exportGeometry(
            const std::string& filename,
            Geometry::Ptr geometry);
        [[nodiscard]] static bool exportGeometry(
            std::ostream& output,
            HouGeoAdapter::ConstPtr geometry,
            bool binary = true);
        [[nodiscard]] static bool exportPoints(
            const std::string& filename,
            std::span<const math::V3f> points);
        [[nodiscard]] static bool exportPointAttributes(
            const std::string& filename,
            const std::map<std::string, std::vector<math::V3f>>& point_attributes);

    private:
        HouGeoIO() = delete;
        friend class GeometryIO;

        [[nodiscard]] static Geometry::Ptr convertToGeometry(
            HouGeo::ConstPtr houdini_geometry,
            HouGeoAdapter::Primitive::ConstPtr primitive,
            DiagnosticList* diagnostics,
            GeometryConversionReport* report);
        [[nodiscard]] static HouGeo::Ptr adaptVolume(ScalarField::Ptr volume);
        [[nodiscard]] static HouGeo::Ptr adaptGeometry(Geometry::Ptr geometry);

        struct ExportContext
        {
            explicit ExportContext(json::BinaryWriter& active_writer)
                : writer(active_writer)
            {
            }

            json::BinaryWriter& writer;
        };

        static bool exportAttribute(
            ExportContext& context,
            HouGeoAdapter::AttributeAdapter::ConstPtr attribute);
        static bool exportTopology(
            ExportContext& context,
            HouGeoAdapter::Topology::ConstPtr topology);
        static bool exportPrimitive(
            ExportContext& context,
            HouGeoAdapter::VolumePrimitive::ConstPtr volume);
        static bool exportPrimitive(
            ExportContext& context,
            HouGeoAdapter::PolyPrimitive::ConstPtr polygon_run,
            int start_vertex);
        static bool exportGroup(
            ExportContext& context,
            const std::string& name,
            const std::vector<bool>& membership);
    };
}
