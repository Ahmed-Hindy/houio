#include <houio/GeometryIO.h>
#include <houio/GeometryModels.h>

#include <filesystem>
#include <iostream>
#include <type_traits>

static_assert(std::is_same_v<houio::HoudiniGeometry, houio::HouGeo>);
static_assert(std::is_same_v<houio::SimplifiedMesh, houio::Geometry>);

int main()
{
    const std::filesystem::path outputPath =
        std::filesystem::temp_directory_path() / "houio_package_consumer.bgeo";

    houio::SimplifiedMesh::Ptr sourceGeometry =
        houio::SimplifiedMesh::createQuad(houio::SimplifiedMesh::QUAD);
    const houio::GeometryWriteResult writeResult =
        houio::GeometryIO::writeGeometry(outputPath, sourceGeometry);
    if (!sourceGeometry || !writeResult)
    {
        std::cerr << "failed to export geometry through installed HouIO package\n";
        return 1;
    }

    const houio::GeometryReadResult<houio::SimplifiedMesh::Ptr> readResult =
        houio::GeometryIO::readGeometry(outputPath);
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    if (!readResult || readResult.value->primitiveCount() != 1)
    {
        std::cerr << "failed to import geometry through installed HouIO package\n";
        return 1;
    }

    return 0;
}
