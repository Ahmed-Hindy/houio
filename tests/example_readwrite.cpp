#include <houio/GeometryIO.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace
{
void printDiagnostics(const houio::DiagnosticList &diagnostics)
{
    for( const houio::Diagnostic &diagnostic : diagnostics )
    {
        std::cerr << (diagnostic.severity == houio::DiagnosticSeverity::error ? "error" : "warning")
                  << ": " << diagnostic.message;
        if( diagnostic.byteOffset >= 0 )
            std::cerr << " at byte " << diagnostic.byteOffset;
        if( !diagnostic.path.empty() )
            std::cerr << " [" << diagnostic.path << ']';
        std::cerr << '\n';
    }
}
}

int main()
{
    const std::filesystem::path outputPath =
        std::filesystem::temp_directory_path() / "houio_example.bgeo";

    houio::Geometry::Ptr source = houio::Geometry::createQuad(houio::Geometry::QUAD);
    const houio::GeometryWriteResult writeResult =
        houio::GeometryIO::writeGeometry(outputPath, source);
    if( !writeResult )
    {
        printDiagnostics(writeResult.diagnostics);
        return 1;
    }

    const houio::GeometryReadResult<houio::HouGeo::Ptr> readResult =
        houio::GeometryIO::readHouGeo(outputPath);
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);
    if( !readResult )
    {
        printDiagnostics(readResult.diagnostics);
        return 1;
    }

    std::cout << "points=" << readResult.value->pointCount() << '\n';
    std::cout << "vertices=" << readResult.value->vertexCount() << '\n';
    std::cout << "primitives=" << readResult.value->primitiveCount() << '\n';
    return 0;
}
