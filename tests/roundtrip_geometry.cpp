#include <houio/HouGeoIO.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

int runRoundtrip(const std::string& inputPath, const std::string& outputPath)
{
    std::ifstream input(inputPath, std::ios::binary);
    if (!input)
    {
        return fail("unable to open input file: " + inputPath);
    }

    houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(&input);
    if (!geometry)
    {
        return fail("HouIO failed to parse input file: " + inputPath);
    }

    const std::filesystem::path outputFilePath(outputPath);
    if (outputFilePath.has_parent_path())
    {
        std::error_code directoryError;
        std::filesystem::create_directories(outputFilePath.parent_path(), directoryError);
        if (directoryError)
        {
            return fail(
                "unable to create output directory: " + outputFilePath.parent_path().string()
                + ": " + directoryError.message());
        }
    }

    std::ofstream output(outputFilePath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return fail("unable to open output file: " + outputPath);
    }

    if (!houio::HouGeoIO::xport(&output, geometry, true))
    {
        return fail("HouIO failed to export output file: " + outputPath);
    }

    output.flush();
    if (!output)
    {
        return fail("failed while flushing output file: " + outputPath);
    }

    std::cout << "points=" << geometry->pointcount() << '\n';
    std::cout << "vertices=" << geometry->vertexcount() << '\n';
    std::cout << "primitives=" << geometry->primitivecount() << '\n';
    return 0;
}
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        return fail("usage: houio_roundtrip_geometry <input.geo|bgeo> <output.bgeo>");
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
