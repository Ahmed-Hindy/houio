#include <houio/NativeSceneWriter.h>
#include <houio/SceneArchive.h>
#include <houio/Writer.h>

#include "TestSupport.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>

#ifndef HOUIO_HAS_ALEMBIC
#define HOUIO_HAS_ALEMBIC 0
#endif
#ifndef HOUIO_HAS_USD
#define HOUIO_HAS_USD 0
#endif

static_assert(static_cast<int>(houio::WriterDataType::alembic_scene) == 10);
static_assert(static_cast<int>(houio::WriterDataType::usd_scene) == 11);

namespace
{
    using houio::test::fail;

    bool containsMessage(
        const houio::DiagnosticList& diagnostics,
        const std::string& needle)
    {
        for (const houio::Diagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.message.find(needle) != std::string::npos)
                return true;
        }
        return false;
    }

    houio::Geometry::Ptr createQuad()
    {
        houio::Geometry::Ptr geometry = houio::Geometry::createQuadGeometry();
        geometry->attribute("P")->appendElement(houio::math::V3f(0.0F, 0.0F, 0.0F));
        geometry->attribute("P")->appendElement(houio::math::V3f(1.0F, 0.0F, 0.0F));
        geometry->attribute("P")->appendElement(houio::math::V3f(1.0F, 1.0F, 0.0F));
        geometry->attribute("P")->appendElement(houio::math::V3f(0.0F, 1.0F, 0.0F));
        geometry->addQuad(0, 1, 2, 3);
        return geometry;
    }

    int verifyFormatDetection()
    {
        if (houio::detectSceneArchiveFormat("mesh.ABC")
                != houio::SceneArchiveFormat::alembic
            || houio::detectSceneArchiveFormat("mesh.usd")
                != houio::SceneArchiveFormat::usd
            || houio::detectSceneArchiveFormat("mesh.usda")
                != houio::SceneArchiveFormat::usd
            || houio::detectSceneArchiveFormat("mesh.usdc")
                != houio::SceneArchiveFormat::usd
            || houio::detectSceneArchiveFormat("mesh.bgeo")
                != houio::SceneArchiveFormat::unsupported)
        {
            return fail("Scene archive extension detection is inconsistent");
        }
        return 0;
    }

    int verifyWriter(
        const std::filesystem::path& directory,
        const std::string& extension,
        bool available,
        const std::string& unavailable_message)
    {
        const std::filesystem::path output = directory / ("quad" + extension);
        std::filesystem::remove(output);
        const houio::WriteResult result = houio::Writer::write(output, createQuad());
        if (available)
        {
            if (!result)
                return fail("Compiled scene writer failed for " + extension);
            if (!std::filesystem::is_regular_file(output)
                || std::filesystem::file_size(output) == 0U)
            {
                return fail("Compiled scene writer produced no file for " + extension);
            }
        }
        else
        {
            if (result || !containsMessage(result.diagnostics, unavailable_message))
                return fail("Unavailable scene writer was not reported for " + extension);
            if (std::filesystem::exists(output))
                return fail("Unavailable scene writer left a partial " + extension + " file");
        }
        return 0;
    }

    int verifyCapabilities()
    {
        const auto alembic =
            houio::Writer::capability(houio::WriterDataType::alembic_scene);
        const auto usd = houio::Writer::capability(houio::WriterDataType::usd_scene);
        if (!alembic || !usd)
            return fail("Scene writer capabilities are missing");
        if (alembic->writable != (HOUIO_HAS_ALEMBIC != 0)
            || usd->writable != (HOUIO_HAS_USD != 0))
        {
            return fail("Scene writer capabilities do not match compiled backends");
        }
        return 0;
    }

    int verifyNativeUnavailableContract()
    {
#if !HOUIO_HAS_ALEMBIC
        const std::string destination = "native_scene_writer_unavailable.abc";
        HouIONativeSceneArchiveOptions options = {};
        options.destination_utf8 = destination.c_str();
        options.frames_per_second = 24.0;
        options.start_frame = 1.0;
        options.create_parent_directories = 1U;
        options.overwrite_existing = 1U;
        options.atomic_replace = 1U;
        std::array<char, 512> error = {};
        HouIONativeSceneArchive* archive = houio_create_native_scene_archive(
            &options,
            error.data(),
            error.size());
        if (archive != nullptr)
        {
            houio_destroy_native_scene_archive(archive);
            return fail("Native C ABI opened an unavailable Alembic backend");
        }
        if (std::string(error.data()).find("without Alembic") == std::string::npos)
            return fail("Native C ABI returned no actionable backend diagnostic");
#endif
        return 0;
    }
}

int main()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "houio_scene_writer_test";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    if (const int result = verifyFormatDetection(); result != 0)
        return result;
    if (const int result = verifyCapabilities(); result != 0)
        return result;
    if (const int result = verifyWriter(
            directory,
            ".abc",
            HOUIO_HAS_ALEMBIC != 0,
            "without Alembic");
        result != 0)
    {
        return result;
    }
    if (const int result = verifyWriter(
            directory,
            ".usda",
            HOUIO_HAS_USD != 0,
            "without USD");
        result != 0)
    {
        return result;
    }
    if (const int result = verifyWriter(
            directory,
            ".usdc",
            HOUIO_HAS_USD != 0,
            "without USD");
        result != 0)
    {
        return result;
    }
    return verifyNativeUnavailableContract();
}
