#pragma once

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
            HouGeo::Ptr houdini_geometry,
            HouGeoAdapter::Primitive::Ptr primitive);
        [[nodiscard]] static Geometry::Ptr convertToGeometry(
            HouGeo::Ptr houdini_geometry,
            HouGeoAdapter::Primitive::Ptr primitive,
            DiagnosticList* diagnostics);

        [[nodiscard]] static bool exportVolume(
            const std::string& filename,
            ScalarField::Ptr volume);
        [[nodiscard]] static bool exportGeometry(
            const std::string& filename,
            Geometry::Ptr geometry);
        [[nodiscard]] static bool exportGeometry(
            std::ostream& output,
            HouGeoAdapter::Ptr geometry,
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
            HouGeoAdapter::AttributeAdapter::Ptr attribute);
        static bool exportTopology(
            ExportContext& context,
            HouGeoAdapter::Topology::Ptr topology);
        static bool exportPrimitive(
            ExportContext& context,
            HouGeoAdapter::VolumePrimitive::Ptr volume);
        static bool exportPrimitive(
            ExportContext& context,
            HouGeoAdapter::PolyPrimitive::Ptr polygon_run,
            int start_vertex);
        static bool exportGroup(
            ExportContext& context,
            const std::string& name,
            const std::vector<bool>& membership);
    };
}
