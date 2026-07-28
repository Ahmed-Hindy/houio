#include <houio/SceneArchive.h>

#ifndef HOUIO_HAS_ALEMBIC
#define HOUIO_HAS_ALEMBIC 0
#endif
#ifndef HOUIO_HAS_USD
#define HOUIO_HAS_USD 0
#endif

#if HOUIO_HAS_ALEMBIC
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>
#endif

#if HOUIO_HAS_USD
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xform.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace houio
{
    namespace
    {
#if HOUIO_HAS_ALEMBIC
        namespace Abc = Alembic::Abc;
        namespace AbcGeom = Alembic::AbcGeom;
        namespace AbcCoreOgawa = Alembic::AbcCoreOgawa;
#endif
#if HOUIO_HAS_USD
        namespace Usd = PXR_NS;
#endif

        class OutputTransaction
        {
        public:
            explicit OutputTransaction(const SceneArchiveOptions& options)
                : destination_(options.destination),
                  overwrite_existing_(options.overwriteExisting),
                  atomic_replace_(options.atomicReplace)
            {
                if (destination_.empty())
                    throw std::invalid_argument("Scene archive destination is empty");

                const std::filesystem::path parent = destination_.parent_path();
                if (!parent.empty() && !std::filesystem::exists(parent))
                {
                    if (!options.createParentDirectories)
                        throw std::runtime_error("Scene archive parent directory does not exist");
                    std::filesystem::create_directories(parent);
                }

                if (std::filesystem::exists(destination_) && !overwrite_existing_)
                    throw std::runtime_error("Scene archive destination already exists");

                working_path_ = destination_;
                if (atomic_replace_)
                {
                    const std::string temporary_name = destination_.stem().string()
                        + ".houio.tmp" + destination_.extension().string();
                    working_path_ = destination_.parent_path() / temporary_name;
                }
                if (std::filesystem::exists(working_path_))
                    std::filesystem::remove(working_path_);
                if (!atomic_replace_ && std::filesystem::exists(destination_))
                    std::filesystem::remove(destination_);
            }

            ~OutputTransaction()
            {
                if (!committed_ && atomic_replace_)
                {
                    std::error_code error;
                    std::filesystem::remove(working_path_, error);
                }
            }

            [[nodiscard]] const std::filesystem::path& workingPath() const noexcept
            {
                return working_path_;
            }

            void commit()
            {
                if (committed_)
                    return;
                if (!std::filesystem::exists(working_path_))
                    throw std::runtime_error("Scene archive writer produced no output file");
                if (atomic_replace_)
                {
                    if (std::filesystem::exists(destination_))
                    {
                        if (!overwrite_existing_)
                            throw std::runtime_error("Scene archive destination already exists");
                        std::filesystem::remove(destination_);
                    }
                    std::filesystem::rename(working_path_, destination_);
                }
                committed_ = true;
            }

        private:
            std::filesystem::path destination_;
            std::filesystem::path working_path_;
            bool overwrite_existing_ = true;
            bool atomic_replace_ = true;
            bool committed_ = false;
        };

        [[nodiscard]] std::size_t pointCount(const SceneGeometrySample& geometry)
        {
            if ((geometry.positionsXyzw.size() % 4U) != 0U)
                throw std::runtime_error("Scene position storage is malformed");
            return geometry.positionsXyzw.size() / 4U;
        }

        struct SceneTopology
        {
            std::vector<std::int32_t> meshPointIndices;
            std::vector<std::int32_t> meshIndices;
            std::vector<std::int32_t> meshCounts;
            std::vector<std::int32_t> curveIndices;
            std::vector<std::int32_t> curveCounts;

            [[nodiscard]] bool operator==(const SceneTopology& other) const
            {
                return meshPointIndices == other.meshPointIndices
                    && meshIndices == other.meshIndices
                    && meshCounts == other.meshCounts
                    && curveIndices == other.curveIndices
                    && curveCounts == other.curveCounts;
            }
        };

        [[nodiscard]] SceneTopology buildTopology(const SceneGeometrySample& geometry)
        {
            const std::size_t point_count = pointCount(geometry);
            std::vector<std::int32_t> mesh_remap(point_count, -1);
            SceneTopology result;
            std::size_t expected_offset = 0;
            for (const ScenePolygon& polygon : geometry.polygons)
            {
                if (polygon.vertexOffset != expected_offset
                    || polygon.vertexOffset > geometry.topology.size()
                    || polygon.vertexCount > geometry.topology.size() - polygon.vertexOffset)
                {
                    throw std::runtime_error("Scene topology contains an invalid primitive range");
                }
                const std::size_t minimum = polygon.closed ? 3U : 2U;
                if (polygon.vertexCount < minimum)
                {
                    throw std::runtime_error(
                        polygon.closed
                            ? "Scene closed polygon requires at least three vertices"
                            : "Scene open polyline requires at least two vertices");
                }

                const auto begin = geometry.topology.begin()
                    + static_cast<std::ptrdiff_t>(polygon.vertexOffset);
                const auto end = begin + static_cast<std::ptrdiff_t>(polygon.vertexCount);
                if (polygon.closed)
                {
                    result.meshCounts.push_back(static_cast<std::int32_t>(polygon.vertexCount));
                    for (auto iterator = begin; iterator != end; ++iterator)
                    {
                        const std::int32_t point_index = *iterator;
                        if (point_index < 0
                            || static_cast<std::size_t>(point_index) >= point_count)
                        {
                            throw std::runtime_error(
                                "Scene mesh references a point outside the detail");
                        }
                        std::int32_t& remapped =
                            mesh_remap[static_cast<std::size_t>(point_index)];
                        if (remapped < 0)
                        {
                            remapped = static_cast<std::int32_t>(result.meshPointIndices.size());
                            result.meshPointIndices.push_back(point_index);
                        }
                        result.meshIndices.push_back(remapped);
                    }
                }
                else
                {
                    result.curveCounts.push_back(static_cast<std::int32_t>(polygon.vertexCount));
                    result.curveIndices.insert(result.curveIndices.end(), begin, end);
                }
                expected_offset += polygon.vertexCount;
            }
            if (expected_offset != geometry.topology.size())
                throw std::runtime_error("Scene topology is incomplete");
            return result;
        }

#if HOUIO_HAS_ALEMBIC
        [[nodiscard]] std::vector<Abc::V3f> alembicMeshPoints(
            const SceneGeometrySample& geometry,
            const SceneTopology& topology)
        {
            const std::size_t count = pointCount(geometry);
            std::vector<Abc::V3f> points;
            points.reserve(topology.meshPointIndices.size());
            for (const std::int32_t point_index : topology.meshPointIndices)
            {
                if (point_index < 0 || static_cast<std::size_t>(point_index) >= count)
                    throw std::runtime_error("Scene mesh references a point outside the detail");
                const std::size_t offset = static_cast<std::size_t>(point_index) * 4U;
                points.emplace_back(
                    geometry.positionsXyzw[offset],
                    geometry.positionsXyzw[offset + 1U],
                    geometry.positionsXyzw[offset + 2U]);
            }
            return points;
        }

        [[nodiscard]] std::vector<Abc::V3f> alembicCurvePoints(
            const SceneGeometrySample& geometry,
            const SceneTopology& topology)
        {
            const std::size_t count = pointCount(geometry);
            std::vector<Abc::V3f> points;
            points.reserve(topology.curveIndices.size());
            for (const std::int32_t point_index : topology.curveIndices)
            {
                if (point_index < 0 || static_cast<std::size_t>(point_index) >= count)
                    throw std::runtime_error("Scene curve references a point outside the detail");
                const std::size_t offset = static_cast<std::size_t>(point_index) * 4U;
                points.emplace_back(
                    geometry.positionsXyzw[offset],
                    geometry.positionsXyzw[offset + 1U],
                    geometry.positionsXyzw[offset + 2U]);
            }
            return points;
        }

        class AlembicArchiveWriter final : public SceneArchiveWriter
        {
        public:
            explicit AlembicArchiveWriter(const SceneArchiveOptions& options)
                : transaction_(options),
                  archive_(std::make_unique<Abc::OArchive>(
                      AbcCoreOgawa::WriteArchive(),
                      transaction_.workingPath().string()))
            {
                if (options.framesPerSecond <= 0.0)
                    throw std::invalid_argument("Alembic frames per second must be positive");
                const Abc::TimeSampling sampling(
                    1.0 / options.framesPerSecond,
                    options.startFrame / options.framesPerSecond);
                time_sampling_index_ = archive_->addTimeSampling(sampling);
            }

            void writeSample(const SceneGeometrySample& geometry, double) override
            {
                const SceneTopology topology = buildTopology(geometry);
                if (!has_topology_)
                {
                    topology_ = topology;
                    has_topology_ = true;
                    if (!topology.meshCounts.empty())
                    {
                        mesh_ = std::make_unique<AbcGeom::OPolyMesh>(
                            archive_->getTop(), "mesh", Abc::Argument(time_sampling_index_));
                    }
                    if (!topology.curveCounts.empty())
                    {
                        curves_ = std::make_unique<AbcGeom::OCurves>(
                            archive_->getTop(), "curves", Abc::Argument(time_sampling_index_));
                    }
                }
                else if (!(topology == topology_))
                {
                    throw std::runtime_error(
                        "Alembic export currently requires constant polygon and curve topology");
                }

                if (mesh_)
                {
                    const std::vector<Abc::V3f> points = alembicMeshPoints(geometry, topology_);
                    mesh_->getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
                        Abc::P3fArraySample(points),
                        Abc::Int32ArraySample(topology_.meshIndices),
                        Abc::Int32ArraySample(topology_.meshCounts)));
                }
                if (curves_)
                {
                    const std::vector<Abc::V3f> points = alembicCurvePoints(geometry, topology_);
                    curves_->getSchema().set(AbcGeom::OCurvesSchema::Sample(
                        Abc::P3fArraySample(points),
                        Abc::Int32ArraySample(topology_.curveCounts),
                        AbcGeom::kLinear,
                        AbcGeom::kNonPeriodic,
                        AbcGeom::OFloatGeomParam::Sample(),
                        AbcGeom::OV2fGeomParam::Sample(),
                        AbcGeom::ON3fGeomParam::Sample(),
                        AbcGeom::kNoBasis));
                }
            }

            void finish() override
            {
                curves_.reset();
                mesh_.reset();
                archive_.reset();
                transaction_.commit();
            }

        private:
            OutputTransaction transaction_;
            std::unique_ptr<Abc::OArchive> archive_;
            std::unique_ptr<AbcGeom::OPolyMesh> mesh_;
            std::unique_ptr<AbcGeom::OCurves> curves_;
            SceneTopology topology_;
            std::uint32_t time_sampling_index_ = 0U;
            bool has_topology_ = false;
        };
#endif

#if HOUIO_HAS_USD
        [[nodiscard]] Usd::VtArray<Usd::GfVec3f> usdMeshPoints(
            const SceneGeometrySample& geometry,
            const SceneTopology& topology)
        {
            const std::size_t count = pointCount(geometry);
            Usd::VtArray<Usd::GfVec3f> points(topology.meshPointIndices.size());
            for (std::size_t index = 0; index < topology.meshPointIndices.size(); ++index)
            {
                const std::int32_t point_index = topology.meshPointIndices[index];
                if (point_index < 0 || static_cast<std::size_t>(point_index) >= count)
                    throw std::runtime_error("Scene mesh references a point outside the detail");
                const std::size_t offset = static_cast<std::size_t>(point_index) * 4U;
                points[index] = Usd::GfVec3f(
                    geometry.positionsXyzw[offset],
                    geometry.positionsXyzw[offset + 1U],
                    geometry.positionsXyzw[offset + 2U]);
            }
            return points;
        }

        [[nodiscard]] Usd::VtArray<Usd::GfVec3f> usdCurvePoints(
            const SceneGeometrySample& geometry,
            const SceneTopology& topology)
        {
            const std::size_t count = pointCount(geometry);
            Usd::VtArray<Usd::GfVec3f> points(topology.curveIndices.size());
            for (std::size_t index = 0; index < topology.curveIndices.size(); ++index)
            {
                const std::int32_t point_index = topology.curveIndices[index];
                if (point_index < 0 || static_cast<std::size_t>(point_index) >= count)
                    throw std::runtime_error("Scene curve references a point outside the detail");
                const std::size_t offset = static_cast<std::size_t>(point_index) * 4U;
                points[index] = Usd::GfVec3f(
                    geometry.positionsXyzw[offset],
                    geometry.positionsXyzw[offset + 1U],
                    geometry.positionsXyzw[offset + 2U]);
            }
            return points;
        }

        [[nodiscard]] Usd::VtArray<int> usdIntArray(
            const std::vector<std::int32_t>& source)
        {
            Usd::VtArray<int> output(source.size());
            std::transform(
                source.begin(),
                source.end(),
                output.begin(),
                [](std::int32_t value) { return static_cast<int>(value); });
            return output;
        }

        class UsdArchiveWriter final : public SceneArchiveWriter
        {
        public:
            explicit UsdArchiveWriter(const SceneArchiveOptions& options)
                : transaction_(options),
                  stage_(Usd::UsdStage::CreateNew(transaction_.workingPath().string()))
            {
                if (!stage_)
                    throw std::runtime_error("USD could not create the output stage");
                if (options.framesPerSecond <= 0.0)
                    throw std::invalid_argument("USD frames per second must be positive");
                stage_->SetFramesPerSecond(options.framesPerSecond);
                stage_->SetTimeCodesPerSecond(options.framesPerSecond);
                stage_->SetStartTimeCode(options.startFrame);
                Usd::UsdGeomSetStageMetersPerUnit(stage_, 1.0);
                Usd::UsdGeomSetStageUpAxis(stage_, Usd::UsdGeomTokens->y);
                root_ = Usd::UsdGeomXform::Define(stage_, Usd::SdfPath("/HouIO"));
                stage_->SetDefaultPrim(root_.GetPrim());
            }

            void writeSample(const SceneGeometrySample& geometry, double frame) override
            {
                const SceneTopology topology = buildTopology(geometry);
                if (!has_topology_)
                {
                    topology_ = topology;
                    has_topology_ = true;
                    if (!topology.meshCounts.empty())
                    {
                        mesh_ = Usd::UsdGeomMesh::Define(stage_, Usd::SdfPath("/HouIO/mesh"));
                        mesh_.CreateFaceVertexIndicesAttr().Set(usdIntArray(topology_.meshIndices));
                        mesh_.CreateFaceVertexCountsAttr().Set(usdIntArray(topology_.meshCounts));
                        mesh_.CreateSubdivisionSchemeAttr().Set(Usd::UsdGeomTokens->none);
                    }
                    if (!topology.curveCounts.empty())
                    {
                        curves_ = Usd::UsdGeomBasisCurves::Define(
                            stage_, Usd::SdfPath("/HouIO/curves"));
                        curves_.CreateCurveVertexCountsAttr().Set(
                            usdIntArray(topology_.curveCounts));
                        curves_.CreateTypeAttr().Set(Usd::UsdGeomTokens->linear);
                        curves_.CreateWrapAttr().Set(Usd::UsdGeomTokens->nonperiodic);
                    }
                }
                else if (!(topology == topology_))
                {
                    throw std::runtime_error(
                        "USD export currently requires constant polygon and curve topology");
                }

                const Usd::UsdTimeCode time_code(frame);
                if (mesh_)
                    mesh_.CreatePointsAttr().Set(usdMeshPoints(geometry, topology_), time_code);
                if (curves_)
                    curves_.CreatePointsAttr().Set(usdCurvePoints(geometry, topology_), time_code);
                stage_->SetEndTimeCode(frame);
            }

            void finish() override
            {
                if (!stage_->GetRootLayer()->Save())
                    throw std::runtime_error("USD failed to save the output layer");
                stage_.Reset();
                transaction_.commit();
            }

        private:
            OutputTransaction transaction_;
            Usd::UsdStageRefPtr stage_;
            Usd::UsdGeomXform root_;
            Usd::UsdGeomMesh mesh_;
            Usd::UsdGeomBasisCurves curves_;
            SceneTopology topology_;
            bool has_topology_ = false;
        };
#endif

        [[nodiscard]] bool endsWith(const std::string& value, const char* suffix)
        {
            const std::size_t suffix_length = std::char_traits<char>::length(suffix);
            return value.size() >= suffix_length
                && value.compare(value.size() - suffix_length, suffix_length, suffix) == 0;
        }
    }

    SceneArchiveFormat detectSceneArchiveFormat(const std::filesystem::path& path)
    {
        std::string lower = path.string();
        std::transform(
            lower.begin(),
            lower.end(),
            lower.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        if (endsWith(lower, ".abc"))
            return SceneArchiveFormat::alembic;
        if (endsWith(lower, ".usd")
            || endsWith(lower, ".usda")
            || endsWith(lower, ".usdc"))
        {
            return SceneArchiveFormat::usd;
        }
        return SceneArchiveFormat::unsupported;
    }

    bool sceneArchiveFormatAvailable(SceneArchiveFormat format) noexcept
    {
        switch (format)
        {
        case SceneArchiveFormat::alembic:
#if HOUIO_HAS_ALEMBIC
            return true;
#else
            return false;
#endif
        case SceneArchiveFormat::usd:
#if HOUIO_HAS_USD
            return true;
#else
            return false;
#endif
        case SceneArchiveFormat::automatic:
        case SceneArchiveFormat::unsupported:
            return false;
        }
        return false;
    }

    std::unique_ptr<SceneArchiveWriter> createSceneArchiveWriter(
        const SceneArchiveOptions& options)
    {
        const SceneArchiveFormat format = options.format == SceneArchiveFormat::automatic
            ? detectSceneArchiveFormat(options.destination)
            : options.format;
        switch (format)
        {
        case SceneArchiveFormat::alembic:
#if HOUIO_HAS_ALEMBIC
            return std::make_unique<AlembicArchiveWriter>(options);
#else
            throw std::runtime_error("HouIO was built without Alembic writer support");
#endif
        case SceneArchiveFormat::usd:
#if HOUIO_HAS_USD
            return std::make_unique<UsdArchiveWriter>(options);
#else
            throw std::runtime_error("HouIO was built without USD writer support");
#endif
        case SceneArchiveFormat::automatic:
        case SceneArchiveFormat::unsupported:
            break;
        }
        throw std::invalid_argument("The requested path is not an Alembic or USD archive");
    }
}
