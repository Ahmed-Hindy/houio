#include <houio/HouGeo.h>

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace houio
{
    namespace
    {
        int checkedArrayCount(const json::ArrayPtr& array, const std::string& description)
        {
            if (!array)
                throw std::runtime_error(description + " must be an array");
            const sint64 count = array->size();
            if (count < 0)
                throw std::runtime_error(description + " has a negative element count");
            if (count > static_cast<sint64>(std::numeric_limits<int>::max()))
                throw std::length_error(description + " exceeds supported indexing");
            return static_cast<int>(count);
        }

        math::M33f parseFiniteMatrix33(
            const json::ArrayPtr& transform_values,
            const std::string& primitive_name)
        {
            if (!transform_values || transform_values->size() != 9)
            {
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    primitive_name + " transform requires nine values",
                    -1,
                    "transform"});
            }

            std::array<real32, 9> values{};
            for (int index = 0; index < 9; ++index)
            {
                values[static_cast<std::size_t>(index)] = transform_values->get<real32>(index);
                if (!std::isfinite(values[static_cast<std::size_t>(index)]))
                {
                    throw DiagnosticException(Diagnostic{
                        DiagnosticSeverity::error,
                        DiagnosticCategory::schema,
                        primitive_name + " transform values must be finite",
                        -1,
                        "transform"});
                }
            }

            return math::M33f(
                values[0], values[1], values[2],
                values[3], values[4], values[5],
                values[6], values[7], values[8]);
        }
    }

    void HouGeo::loadNativeVdbPrimitive(json::ObjectPtr native_vdb)
    {
        if (!native_vdb)
            throw std::invalid_argument("HouGeo::loadNativeVdbPrimitive received null data");
        const int topology_vertex = native_vdb->get<int>("vertex", -1);
        if (topology_vertex < 0 || !m_topology
            || static_cast<sint64>(topology_vertex) >= m_topology->indexCount())
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "VDB topology vertex is outside vertexcount",
                -1,
                "vertex"});
        }
        json::ArrayPtr payload = native_vdb->array("vdb");
        if (!payload || payload->size() < 2)
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "VDB primitive is missing its serialized sparse payload",
                -1,
                "vdb"});
        }

        auto result = std::make_shared<HouVdb>();
        result->topology_vertex_ = topology_vertex;
        result->serialized_payload_ = std::move(payload);
        m_primitives.push_back(std::move(result));
    }

    void HouGeo::loadSpherePrimitive(json::ObjectPtr sphere)
    {
        if (!sphere)
            throw std::invalid_argument("HouGeo::loadSpherePrimitive received null data");
        const int topology_vertex = sphere->get<int>("vertex", -1);
        if (topology_vertex < 0 || !m_topology
            || static_cast<sint64>(topology_vertex) >= m_topology->indexCount())
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "Sphere topology vertex is outside vertexcount",
                -1,
                "vertex"});
        }
        auto result = std::make_shared<HouSphere>();
        result->setTopologyVertex(topology_vertex);
        result->setTransform(parseFiniteMatrix33(sphere->array("transform"), "Sphere"));
        m_primitives.push_back(std::move(result));
    }

    void HouGeo::loadTubePrimitive(json::ObjectPtr tube)
    {
        if (!tube)
            throw std::invalid_argument("HouGeo::loadTubePrimitive received null data");
        const int topology_vertex = tube->get<int>("vertex", -1);
        if (topology_vertex < 0 || !m_topology
            || static_cast<sint64>(topology_vertex) >= m_topology->indexCount())
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "Tube topology vertex is outside vertexcount",
                -1,
                "vertex"});
        }
        auto result = std::make_shared<HouTube>();
        result->setTopologyVertex(topology_vertex);
        result->setTransform(parseFiniteMatrix33(tube->array("transform"), "Tube"));
        result->setCaps(tube->get<bool>("caps", false));
        result->setTaper(tube->get<real32>("taper", 1.0f));
        m_primitives.push_back(std::move(result));
    }

    void HouGeo::loadCurvePrimitive(
        json::ObjectPtr curve_object,
        CurvePrimitive::Basis expected_basis)
    {
        if (!curve_object)
            throw std::invalid_argument("HouGeo::loadCurvePrimitive received null data");
        if (!m_topology)
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "Curve primitive requires topology",
                -1,
                "vertex"});
        }

        json::ArrayPtr vertex_values = curve_object->array("vertex");
        if (!vertex_values)
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "Curve primitive is missing vertex indices",
                -1,
                "vertex"});
        }
        const int vertex_count = checkedArrayCount(
            vertex_values,
            "HouGeo::loadCurvePrimitive vertex indices");
        std::vector<int> vertex_indices;
        vertex_indices.reserve(static_cast<std::size_t>(vertex_count));
        for (int index = 0; index < vertex_count; ++index)
        {
            const int topology_vertex = vertex_values->get<int>(index);
            if (topology_vertex < 0
                || static_cast<sint64>(topology_vertex) >= m_topology->indexCount())
            {
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "Curve topology vertex is outside vertexcount",
                    -1,
                    "vertex"});
            }
            vertex_indices.push_back(topology_vertex);
        }

        json::ObjectPtr basis_object = toObject(curve_object->array("basis"));
        if (!basis_object)
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "Curve primitive is missing basis metadata",
                -1,
                "basis"});
        }
        const std::string basis_name = basis_object->get<std::string>("type", "");
        const std::string expected_name = expected_basis == CurvePrimitive::Basis::nurbs
            ? "NURBS" : "Bezier";
        if (basis_name != expected_name)
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "Curve primitive basis does not match its record type",
                -1,
                "basis.type"});
        }

        json::ArrayPtr knot_values = basis_object->array("knots");
        if (!knot_values)
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "Curve primitive is missing knot values",
                -1,
                "basis.knots"});
        }
        const int knot_count = checkedArrayCount(
            knot_values,
            "HouGeo::loadCurvePrimitive knots");
        std::vector<real64> knots;
        knots.reserve(static_cast<std::size_t>(knot_count));
        for (int index = 0; index < knot_count; ++index)
            knots.push_back(knot_values->get<real64>(index));

        auto result = std::make_shared<HouCurve>();
        result->setCurveData(
            expected_basis,
            std::move(vertex_indices),
            curve_object->get<bool>("closed", false),
            basis_object->get<int>("order", 0),
            std::move(knots),
            basis_object->get<bool>("endinterpolation", true));
        m_primitives.push_back(std::move(result));
    }
}
