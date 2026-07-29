#include <houio/GeometryIO.h>

#include "TestSupport.h"

#include <iostream>
#include <string>

namespace
{
using houio::test::fail;

void printDiagnostics(const houio::DiagnosticList& diagnostics)
{
    for (const houio::Diagnostic& diagnostic : diagnostics)
    {
        std::cerr
            << (diagnostic.severity == houio::DiagnosticSeverity::error ? "error" : "warning")
            << ": " << diagnostic.message;
        if (diagnostic.byteOffset >= 0)
            std::cerr << " at byte " << diagnostic.byteOffset;
        if (!diagnostic.path.empty())
            std::cerr << " [" << diagnostic.path << ']';
        std::cerr << '\n';
    }
}

int runRoundtrip(const std::string& input_path, const std::string& output_path)
{
    const houio::GeometryReadResult<houio::Geometry::Ptr> read_result =
        houio::GeometryIO::readGeometry(input_path);
    if (!read_result)
    {
        printDiagnostics(read_result.diagnostics);
        return fail("HouIO failed to read simplified geometry: " + input_path);
    }

    const houio::GeometryWriteResult write_result =
        houio::GeometryIO::writeGeometry(output_path, read_result.value);
    if (!write_result)
    {
        printDiagnostics(write_result.diagnostics);
        return fail("HouIO failed to write simplified geometry: " + output_path);
    }

    const houio::Attribute::CPtr positions = read_result.value->pointAttribute("P");
    std::cout << "points=" << (positions ? positions->numElements() : 0) << '\n';
    std::cout << "vertices=" << read_result.value->indexBuffer().size() << '\n';
    std::cout << "primitives=" << read_result.value->primitiveCount() << '\n';
    return 0;
}
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        return fail(
            "usage: houio_roundtrip_simplified_geometry "
            "<input.geo|bgeo|bgeo.sc> <output.bgeo|bgeo.sc>");
    }

    try
    {
        return runRoundtrip(argv[1], argv[2]);
    }
    catch (const std::exception& error)
    {
        return fail(error.what());
    }
}
