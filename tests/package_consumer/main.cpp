#include <houio/Geometry.h>
#include <houio/HouGeoIO.h>

#include <filesystem>
#include <iostream>

int main()
{
    const std::filesystem::path outputPath =
        std::filesystem::temp_directory_path() / "houio_package_consumer.bgeo";

    houio::Geometry::Ptr sourceGeometry = houio::Geometry::createQuad(houio::Geometry::QUAD);
    if (!sourceGeometry || !houio::HouGeoIO::xport(outputPath.string(), sourceGeometry))
    {
        std::cerr << "failed to export geometry through installed HouIO package\n";
        return 1;
    }

    houio::Geometry::Ptr importedGeometry = houio::HouGeoIO::importGeometry(outputPath.string());
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    if (!importedGeometry || importedGeometry->numPrimitives() != 1)
    {
        std::cerr << "failed to import geometry through installed HouIO package\n";
        return 1;
    }

    return 0;
}
