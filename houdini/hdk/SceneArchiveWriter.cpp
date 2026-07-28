#include "SceneArchiveWriter.h"

#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>
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

namespace houio::hdk
{
    namespace
    {
        namespace Abc = Alembic::Abc;
        namespace AbcGeom = Alembic::AbcGeom;
        namespace AbcCoreOgawa = Alembic::AbcCoreOgawa;
        namespace Usd = PXR_NS;

        class OutputTransaction
        {
        public:
            explicit OutputTransaction(const SceneArchiveOptions& options)
                : destination_(options.destination),
                  overwrite_existing_(options.overwrite_existing),
                  atomic_replace_(options.atomic_replace)
            {
                if (destination_.empty())
                    throw std::invalid_argument("Scene archive destination is empty");

                const std::filesystem::path parent = destination_.parent_path();
                if (!parent.empty() && !std::filesystem::exists(parent))
                {
                    if (!options.create_parent_directories)
                    {
                        throw std::runtime_error(
                            "Scene archive parent directory does not exist");
                    }
                    std::filesystem::create_directories(parent);
                }

                if (std::filesystem::exists(destination_) && !overwrite_existing_)
                    throw std::runtime_error("Scene archive destination already exists");

                working_path_ = destination_;
                if (atomic_replace_)
                {
                    const std::filesystem::path temporary_parent =
                        destination_.parent_path();
                    const std::string temporary_name =
                        destination_.stem().string()
                        + ".houio.tmp"
                        + destination_.extension().string();
                    working_path_ = temporary_parent / temporary_name;
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

        [[nodiscard]] std::size_t pointCount(const NativePolygonDetail& geometry)
        {
            if ((geometry.positions_xyzw.size() % 4U) != 0U)
                throw std::runtime_error("Native scene position storage is malformed");
            return geometry.positions_xyzw.size() / 4U;
        }

        struct SceneTopology
        {
            std::vector<std::int32_t> mesh_point_indices;
            std::vector<std::int32_t> mesh_indices;
            std::vector<std::int32_t> mesh_counts;
            std::vector<std::int32_t> curve_indices;
            std::vector<std::int32_t> curve_counts;

            [[nodiscard]] bool operator==(const SceneTopology& other) const
            {
                return mesh_point_indices == other.mesh_point_indices
                    && mesh_indices == other.mesh_indices
                    && mesh_counts == other.mesh_counts
                    && curve_indices == other.curve_indices
                    && curve_counts == other.curve_counts;
            }
        };

        [[nodiscard]] SceneTopology buildTopology(const NativePolygonDetail& geometry)
        {
            const std::size_t point_count = pointCount(geometry);
            std::vector<std::int32_t> mesh_remap(point_count, -1);
            SceneTopology result;
            std::size_t expected_offset = 0;
            for (const HouIONativePolygon& polygon : geometry.polygons)
            {
                if (polygon.vertex_offset != expected_offset
                    || polygon.vertex_offset > geometry.topology.size()
                    || polygon.vertex_count
                        > geometry.topology.size() - polygon.vertex_offset)
                {
                    throw std::runtime_error(
                        "Native scene topology contains an invalid primitive range");
                }

                const auto begin = geometry.topology.begin()
                    + static_cast<std::ptrdiff_t>(polygon.vertex_offset);
                const auto end = begin
                    + static_cast<std::ptrdiff_t>(polygon.vertex_count);
                if (polygon.closed != 0U)
                {
                    result.mesh_counts.push_back(
                        static_cast<std::int32_t>(polygon.vertex_count));
                    for (auto iterator = begin; iterator != end; ++iterator)
                    {
                        const std::int32_t point_index = *iterator;
                        if (point_index < 0
                            || static_cast<std::size_t>(point_index) >= point_count)
                        {
                            throw std::runtime_error(
                                "Native scene mesh references a point outside the detail");
                        }
                        std::int32_t& remapped =
                            mesh_remap[static_cast<std::size_t>(point_index)];
                        if (remapped < 0)
                        {
                            remapped = static_cast<std::int32_t>(
                                result.mesh_point_indices.size());
                            result.mesh_point_indices.push_back(point_index);
                        }
                        result.mesh_indices.push_back(remapped);
                    }
                }
                else
                {
                    result.curve_counts.push_back(
                        static_cast<std::int32_t>(polygon.vertex_count));
                    result.curve_indices.insert(result.curve_indices.end(), begin, end);
                }
                expected_offset += polygon.vertex_count;
            }
            if (expected_offset != geometry.topology.size())
                throw std::runtime_error("Native scene topology is incomplete");
            return result;
        }

        [[nodiscard]] std::vector<Abc::V3f> alembicMeshPoints(
            const NativePolygonDetail& geometry,
            const SceneTopology& topology)
        {
            const std::size_t count = pointCount(geometry);
            std::vector<Abc::V3f> points;
            points.reserve(topology.mesh_point_indices.size());
            for (const std::int32_t point_index : topology.mesh_point_indices)
            {
                if (point_index < 0
                    || static_cast<std::size_t>(point_index) >= count)
                {
                    throw std::runtime_error(
                        "Native scene mesh references a point outside the detail");
                }
                const std::size_t offset = static_cast<std::size_t>(point_index) * 4U;
                points.emplace_back(
                    geometry.positions_xyzw[offset],
                    geometry.positions_xyzw[offset + 1U],
                    geometry.positions_xyzw[offset + 2U]);
            }
            return points;
        }

        [[nodiscard]] std::vector<Abc::V3f> alembicCurvePoints(
            const NativePolygonDetail& geometry,
            const SceneTopology& topology)
        {
            const std::size_t count = pointCount(geometry);
            std::vector<Abc::V3f> points;
            points.reserve(topology.curve_indices.size());
            for (const std::int32_t point_index : topology.curve_indices)
            {
                if (point_index < 0
                    || static_cast<std::size_t>(point_index) >= count)
                {
                    throw std::runtime_error(
                        "Native scene curve references a point outside the detail");
                }
                const std::size_t offset = static_cast<std::size_t>(point_index) * 4U;
                points.emplace_back(
                    geometry.positions_xyzw[offset],
                    geometry.positions_xyzw[offset + 1U],
                    geometry.positions_xyzw[offset + 2U]);
            }
            return points;
        }

        [[nodiscard]] Usd::VtArray<Usd::GfVec3f> usdMeshPoints(
            const NativePolygonDetail& geometry,
            const SceneTopology& topology)
        {
            const std::size_t count = pointCount(geometry);
            Usd::VtArray<Usd::GfVec3f> points(topology.mesh_point_indices.size());
            for (std::size_t index = 0;
                 index < topology.mesh_point_indices.size();
                 ++index)
            {
                const std::int32_t point_index = topology.mesh_point_indices[index];
                if (point_index < 0
                    || static_cast<std::size_t>(point_index) >= count)
                {
                    throw std::runtime_error(
                        "Native scene mesh references a point outside the detail");
                }
                const std::size_t offset = static_cast<std::size_t>(point_index) * 4U;
                points[index] = Usd::GfVec3f(
                    geometry.positions_xyzw[offset],
                    geometry.positions_xyzw[offset + 1U],
                    geometry.positions_xyzw[offset + 2U]);
            }
            return points;
        }

        [[nodiscard]] Usd::VtArray<Usd::GfVec3f> usdCurvePoints(
            const NativePolygonDetail& geometry,
            const SceneTopology& topology)
        {
            const std::size_t count = pointCount(geometry);
            Usd::VtArray<Usd::GfVec3f> points(topology.curve_indices.size());
            for (std::size_t index = 0; index < topology.curve_indices.size(); ++index)
            {
                const std::int32_t point_index = topology.curve_indices[index];
                if (point_index < 0
                    || static_cast<std::size_t>(point_index) >= count)
                {
                    throw std::runtime_error(
                        "Native scene curve references a point outside the detail");
                }
                const std::size_t offset = static_cast<std::size_t>(point_index) * 4U;
                points[index] = Usd::GfVec3f(
                    geometry.positions_xyzw[offset],
                    geometry.positions_xyzw[offset + 1U],
                    geometry.positions_xyzw[offset + 2U]);
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

        class AlembicArchiveWriter final : public SceneArchiveWriter
        {
        public:
            explicit AlembicArchiveWriter(const SceneArchiveOptions& options)
                : transaction_(options),
                  archive_(std::make_unique<Abc::OArchive>(
                      AbcCoreOgawa::WriteArchive(),
                      transaction_.workingPath().string()))
            {
                if (options.frames_per_second <= 0.0)
                    throw std::invalid_argument("Alembic frames per second must be positive");
                const Abc::TimeSampling sampling(
                    1.0 / options.frames_per_second,
                    options.start_frame / options.frames_per_second);
                time_sampling_index_ = archive_->addTimeSampling(sampling);
            }

            void writeSample(
                const NativePolygonDetail& geometry,
                double) override
            {
                const SceneTopology topology = buildTopology(geometry);
                if (!has_topology_)
                {
                    topology_ = topology;
                    has_topology_ = true;
                    if (!topology.mesh_counts.empty())
                    {
                        mesh_ = std::make_unique<AbcGeom::OPolyMesh>(
                            archive_->getTop(),
                            "mesh",
                            Abc::Argument(time_sampling_index_));
                    }
                    if (!topology.curve_counts.empty())
                    {
                        curves_ = std::make_unique<AbcGeom::OCurves>(
                            archive_->getTop(),
                            "curves",
                            Abc::Argument(time_sampling_index_));
                    }
                }
                else if (!(topology == topology_))
                {
                    throw std::runtime_error(
                        "Alembic export currently requires constant polygon and curve topology");
                }

                if (mesh_)
                {
                    const std::vector<Abc::V3f> points =
                        alembicMeshPoints(geometry, topology_);
                    mesh_->getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
                        Abc::P3fArraySample(points),
                        Abc::Int32ArraySample(topology_.mesh_indices),
                        Abc::Int32ArraySample(topology_.mesh_counts)));
                }

                if (curves_)
                {
                    const std::vector<Abc::V3f> points =
                        alembicCurvePoints(geometry, topology_);
                    curves_->getSchema().set(AbcGeom::OCurvesSchema::Sample(
                        Abc::P3fArraySample(points),
                        Abc::Int32ArraySample(topology_.curve_counts),
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

        class UsdArchiveWriter final : public SceneArchiveWriter
        {
        public:
            explicit UsdArchiveWriter(const SceneArchiveOptions& options)
                : transaction_(options),
                  stage_(Usd::UsdStage::CreateNew(transaction_.workingPath().string()))
            {
                if (!stage_)
                    throw std::runtime_error("USD could not create the output stage");
                if (options.frames_per_second <= 0.0)
                    throw std::invalid_argument("USD frames per second must be positive");
                stage_->SetFramesPerSecond(options.frames_per_second);
                stage_->SetTimeCodesPerSecond(options.frames_per_second);
                stage_->SetStartTimeCode(options.start_frame);
                Usd::UsdGeomSetStageMetersPerUnit(stage_, 1.0);
                Usd::UsdGeomSetStageUpAxis(stage_, Usd::UsdGeomTokens->y);
                root_ = Usd::UsdGeomXform::Define(stage_, Usd::SdfPath("/HouIO"));
                stage_->SetDefaultPrim(root_.GetPrim());
            }

            void writeSample(
                const NativePolygonDetail& geometry,
                double frame) override
            {
                const SceneTopology topology = buildTopology(geometry);
                if (!has_topology_)
                {
                    topology_ = topology;
                    has_topology_ = true;
                    if (!topology.mesh_counts.empty())
                    {
                        mesh_ = Usd::UsdGeomMesh::Define(
                            stage_,
                            Usd::SdfPath("/HouIO/mesh"));
                        mesh_.CreateFaceVertexIndicesAttr().Set(
                            usdIntArray(topology_.mesh_indices));
                        mesh_.CreateFaceVertexCountsAttr().Set(
                            usdIntArray(topology_.mesh_counts));
                        mesh_.CreateSubdivisionSchemeAttr().Set(
                            Usd::UsdGeomTokens->none);
                    }
                    if (!topology.curve_counts.empty())
                    {
                        curves_ = Usd::UsdGeomBasisCurves::Define(
                            stage_,
                            Usd::SdfPath("/HouIO/curves"));
                        curves_.CreateCurveVertexCountsAttr().Set(
                            usdIntArray(topology_.curve_counts));
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
                    mesh_.CreatePointsAttr().Set(
                        usdMeshPoints(geometry, topology_),
                        time_code);
                if (curves_)
                {
                    curves_.CreatePointsAttr().Set(
                        usdCurvePoints(geometry, topology_),
                        time_code);
                }
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

        [[nodiscard]] bool endsWith(const std::string& value, const char* suffix)
        {
            const std::size_t suffix_length = std::char_traits<char>::length(suffix);
            return value.size() >= suffix_length
                && value.compare(value.size() - suffix_length, suffix_length, suffix) == 0;
        }
    }

    NativeOutputFormat detectNativeOutputFormat(const std::string& path)
    {
        std::string lower = path;
        std::transform(
            lower.begin(),
            lower.end(),
            lower.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        if (endsWith(lower, ".bgeo") || endsWith(lower, ".bgeo.sc"))
            return NativeOutputFormat::bgeo;
        if (endsWith(lower, ".abc"))
            return NativeOutputFormat::alembic;
        if (endsWith(lower, ".usd")
            || endsWith(lower, ".usda")
            || endsWith(lower, ".usdc"))
        {
            return NativeOutputFormat::usd;
        }
        return NativeOutputFormat::unsupported;
    }

    std::unique_ptr<SceneArchiveWriter> createSceneArchiveWriter(
        NativeOutputFormat format,
        const SceneArchiveOptions& options)
    {
        switch (format)
        {
        case NativeOutputFormat::alembic:
            return std::make_unique<AlembicArchiveWriter>(options);
        case NativeOutputFormat::usd:
            return std::make_unique<UsdArchiveWriter>(options);
        case NativeOutputFormat::bgeo:
        case NativeOutputFormat::unsupported:
            break;
        }
        throw std::invalid_argument("The requested format is not a scene archive format");
    }
}
