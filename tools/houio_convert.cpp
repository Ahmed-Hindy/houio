#include <houio/GeometryIO.h>

#include <iostream>
#include <string>

namespace
{
int fail(const std::string &message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

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

int convert(const std::string &inputPath, const std::string &outputPath)
{
    houio::GeometryReadResult<houio::HouGeo::Ptr> readResult =
        houio::GeometryIO::readHouGeo(inputPath);
    if( !readResult )
    {
        printDiagnostics(readResult.diagnostics);
        return fail("HouIO failed to read input file: " + inputPath);
    }

    const houio::GeometryWriteResult writeResult =
        houio::GeometryIO::writeHouGeo(outputPath, readResult.value);
    if( !writeResult )
    {
        printDiagnostics(writeResult.diagnostics);
        return fail("HouIO failed to write output file: " + outputPath);
    }

    std::cout << "points=" << readResult.value->pointcount() << '\n';
    std::cout << "vertices=" << readResult.value->vertexcount() << '\n';
    std::cout << "primitives=" << readResult.value->primitivecount() << '\n';
    return 0;
}
}

int main(int argc, char *argv[])
{
    if( argc != 3 )
        return fail("usage: houio_convert <input.geo|bgeo|bgeo.sc> <output.bgeo|bgeo.sc>");

    try
    {
        return convert(argv[1], argv[2]);
    }
    catch( const std::exception &error )
    {
        return fail(error.what());
    }
}
