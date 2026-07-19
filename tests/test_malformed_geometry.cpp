#include <houio/HouGeoIO.h>

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

bool importRejected(const std::string& sourceText)
{
    try
    {
        std::istringstream source(sourceText);
        return !houio::HouGeoIO::import(&source);
    }
    catch (const std::exception&)
    {
        return true;
    }
    catch (...)
    {
        return true;
    }
}

int expectImportDiagnostic(const std::string& sourceText, houio::DiagnosticCategory category,
                           const std::string& path, const std::string& description)
{
    std::istringstream source(sourceText);
    houio::DiagnosticList diagnostics;
    if (houio::HouGeoIO::import(&source, &diagnostics))
    {
        return fail(description + " unexpectedly imported");
    }
    if (diagnostics.size() != 1)
    {
        return fail(description + " did not produce exactly one diagnostic");
    }
    const houio::Diagnostic& diagnostic = diagnostics.front();
    if (diagnostic.severity != houio::DiagnosticSeverity::error
        || diagnostic.category != category || diagnostic.path != path
        || diagnostic.message.empty())
    {
        return fail(description + " produced incorrect diagnostic metadata");
    }
    return 0;
}

int verifyFileDiagnostics()
{
    houio::DiagnosticList diagnostics;
    if (houio::HouGeoIO::importGeometry("houio_missing_diagnostic_fixture.bgeo", &diagnostics))
    {
        return fail("missing geometry file unexpectedly imported");
    }
    if (diagnostics.size() != 1 || diagnostics.front().category != houio::DiagnosticCategory::io
        || diagnostics.front().severity != houio::DiagnosticSeverity::error)
    {
        return fail("missing geometry file produced incorrect diagnostic metadata");
    }
    return 0;
}

int verifyConversionDiagnostics()
{
    houio::HouGeo::Ptr geometry = houio::HouGeo::create();
    houio::HouGeo::HouVolume::Ptr volume = std::make_shared<houio::HouGeo::HouVolume>();
    houio::DiagnosticList diagnostics;
    if (houio::HouGeoIO::convertToGeometry(geometry, volume, &diagnostics))
    {
        return fail("unsupported volume conversion unexpectedly succeeded");
    }
    if (diagnostics.size() != 1
        || diagnostics.front().category != houio::DiagnosticCategory::unsupported_input
        || diagnostics.front().path != "conversion.primitive")
    {
        return fail("unsupported volume conversion produced incorrect diagnostic metadata");
    }
    return 0;
}

int verifyStructuredDiagnostics()
{
    if (const int result = expectImportDiagnostic(
            R"JSON(["pointcount", -1, "vertexcount", 0, "primitivecount", 0])JSON",
            houio::DiagnosticCategory::schema, "counts", "negative count diagnostic");
        result != 0)
    {
        return result;
    }

    if (const int result = expectImportDiagnostic(
            R"JSON(["pointcount", 1, "vertexcount", 0, "primitivecount", 0, "attributes", ["pointattributes", [[["type", "numeric", "name", "P"], ["size", 3, "storage", "fpreal32", "values", ["size", 3, "storage", "fpreal32", "tuples", []]]]]]])JSON",
            houio::DiagnosticCategory::schema, "attributes.pointattributes[0]",
            "point attribute diagnostic");
        result != 0)
    {
        return result;
    }

    if (const int result = expectImportDiagnostic(
            R"JSON(["pointcount", 0, "vertexcount", 0, "primitivecount", 1, "primitives", [[["type", "AlembicRef"], []]]])JSON",
            houio::DiagnosticCategory::unsupported_input,
            "primitives[0].definition.type", "unsupported primitive diagnostic");
        result != 0)
    {
        return result;
    }

    if (const int result = expectImportDiagnostic(
            R"JSON(["pointcount", 1, "vertexcount", 0, "primitivecount", 0, "pointgroups", [[["name", "bad"], ["selection", ["ordered", ["i8", [1]]]]]]])JSON",
            houio::DiagnosticCategory::unsupported_input, "pointgroups.selection",
            "unsupported group diagnostic");
        result != 0)
    {
        return result;
    }
    if (const int result = verifyFileDiagnostics(); result != 0)
    {
        return result;
    }
    return verifyConversionDiagnostics();
}
}

int main()
{
    const std::vector<std::pair<std::string, std::string>> malformedCases = {
        {"odd root key/value array", R"JSON(["pointcount", 0, "vertexcount"])JSON"},
        {"non-string root key", R"JSON([1, 0])JSON"},
        {"duplicate root key", R"JSON(["pointcount", 0, "pointcount", 0])JSON"},
        {"negative geometry count", R"JSON(["pointcount", -1, "vertexcount", 0, "primitivecount", 0])JSON"},
        {"topology count mismatch",
         R"JSON(["pointcount", 1, "vertexcount", 1, "primitivecount", 0, "topology", ["pointref", ["indices", []]]])JSON"},
        {"topology point index out of range",
         R"JSON(["pointcount", 1, "vertexcount", 1, "primitivecount", 0, "topology", ["pointref", ["indices", [1]]]])JSON"},
        {"point group membership count mismatch",
         R"JSON(["pointcount", 2, "vertexcount", 0, "primitivecount", 0, "pointgroups", [[["name", "bad"], ["selection", ["unordered", ["i8", [1]]]]]]])JSON"},
        {"point group non-binary membership",
         R"JSON(["pointcount", 1, "vertexcount", 0, "primitivecount", 0, "pointgroups", [[["name", "bad"], ["selection", ["unordered", ["i8", [2]]]]]]])JSON"},
        {"ordered group selection",
         R"JSON(["pointcount", 1, "vertexcount", 0, "primitivecount", 0, "pointgroups", [[["name", "bad"], ["selection", ["ordered", ["i8", [1]]]]]]])JSON"},
        {"unsupported group membership encoding",
         R"JSON(["pointcount", 1, "vertexcount", 0, "primitivecount", 0, "pointgroups", [[["name", "bad"], ["selection", ["unordered", ["i32", [1]]]]]]])JSON"},
        {"direct polygon topology index out of range",
         R"JSON(["pointcount", 1, "vertexcount", 1, "primitivecount", 1, "topology", ["pointref", ["indices", [0]]], "primitives", [[["type", "Poly"], ["vertex", [1], "closed", true]]]])JSON"},
        {"polygon run exceeds topology",
         R"JSON(["pointcount", 1, "vertexcount", 1, "primitivecount", 1, "topology", ["pointref", ["indices", [0]]], "primitives", [[["type", "p_r"], ["s_v", 0, "n_p", 1, "n_v", [2]]]]])JSON"},
    };

    for (const auto& [name, sourceText] : malformedCases)
    {
        if (!importRejected(sourceText))
        {
            return fail("malformed input was accepted: " + name);
        }
    }
    return verifyStructuredDiagnostics();
}
