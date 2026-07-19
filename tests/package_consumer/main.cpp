#include <houio/Geometry.h>
#include <houio/GeometryIO.h>

#include <filesystem>
#include <iostream>

int main()
{
    const std::filesystem::path outputPath =
        std::filesystem::temp_directory_path() / "houio_package_consumer.bgeo";

    houio::Geometry::Ptr sourceGeometry = houio::Geometry::createQuad(houio::Geometry::QUAD);
    const houio::GeometryWriteResult writeResult =
        houio::GeometryIO::writeGeometry(outputPath, sourceGeometry);
    if (!sourceGeometry || !writeResult)
    {
        std::cerr << "failed to export geometry through installed HouIO package\n";
        return 1;
    }

    const houio::GeometryReadResult<houio::Geometry::Ptr> readResult =
        houio::GeometryIO::readGeometry(outputPath);
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    if (!readResult || readResult.value->numPrimitives() != 1)
    {
        std::cerr << "failed to import geometry through installed HouIO package\n";
        return 1;
    }

    return 0;
}
