#include "HomManifestInternal.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace houio::hom_manifest_detail
{
    namespace
    {
        HouGeoAdapter::PackedFragmentPrimitive::Bounds parseBounds(
            const json::ArrayPtr& values,
            const std::string& path)
        {
            const std::vector<real32> components = scalarArray<real32>(values, path);
            if( components.size() != 6 )
                failManifest(DiagnosticCategory::schema,
                    "Bounds require exactly six values", path);
            HouGeoAdapter::PackedFragmentPrimitive::Bounds result{};
            std::copy(components.begin(), components.end(), result.begin());
            return result;
        }

        void parsePolygon(
            const json::ObjectPtr& definition,
            const std::vector<int>& topology,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            const int vertexCount = checkedCount(
                definition->get<sint64>("vertex_count"),
                path + ".vertex_count");
            if( vertexCount <= 0
                || static_cast<std::size_t>(vertexOffset) > topology.size()
                || static_cast<std::size_t>(vertexCount)
                    > topology.size() - static_cast<std::size_t>(vertexOffset) )
            {
                failManifest(DiagnosticCategory::schema,
                    "Polygon vertex range exceeds the topology domain", path);
            }
            std::vector<int> pointIndices(
                topology.begin() + vertexOffset,
                topology.begin() + vertexOffset + vertexCount);
            auto polygon = std::make_shared<HouGeo::HouPoly>();
            polygon->setPolygonData(
                1,
                {vertexCount},
                {0},
                std::move(pointIndices),
                definition->get<bool>("closed", true));
            geometry->addPrimitive(
                std::static_pointer_cast<HouGeoAdapter::PolyPrimitive>(polygon));
        }

        void parseCurve(
            const std::string& type,
            const json::ObjectPtr& definition,
            const std::vector<int>& topology,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            const int vertexCount = checkedCount(
                definition->get<sint64>("vertex_count"),
                path + ".vertex_count");
            if( vertexCount < 2
                || static_cast<std::size_t>(vertexOffset) > topology.size()
                || static_cast<std::size_t>(vertexCount)
                    > topology.size() - static_cast<std::size_t>(vertexOffset) )
            {
                failManifest(DiagnosticCategory::schema,
                    "Curve vertex range exceeds the topology domain", path);
            }

            const std::vector<real64> knots = scalarArray<real64>(
                requireArray(definition, "knots", path),
                path + ".knots");
            if( knots.empty()
                || std::any_of(knots.begin(), knots.end(),
                    [](real64 value) { return !std::isfinite(value); })
                || !std::is_sorted(knots.begin(), knots.end()) )
            {
                failManifest(DiagnosticCategory::schema,
                    "Curve knots must be finite, nonempty, and nondecreasing",
                    path + ".knots");
            }

            std::vector<int> vertexIndices;
            vertexIndices.reserve(static_cast<std::size_t>(vertexCount));
            for( int vertex = 0; vertex < vertexCount; ++vertex )
                vertexIndices.push_back(vertexOffset + vertex);

            auto curve = std::make_shared<HouGeo::HouCurve>();
            try
            {
                curve->setCurveData(
                    type == "nurbs_curve"
                        ? HouGeoAdapter::CurvePrimitive::Basis::nurbs
                        : HouGeoAdapter::CurvePrimitive::Basis::bezier,
                    std::move(vertexIndices),
                    definition->get<bool>("closed", false),
                    definition->get<int>("order"),
                    knots,
                    definition->get<bool>("end_interpolation", true));
            }
            catch( const std::exception& exception )
            {
                failManifest(DiagnosticCategory::schema,
                    "Invalid curve definition: " + std::string(exception.what()),
                    path);
            }
            geometry->addPrimitive(
                std::static_pointer_cast<HouGeoAdapter::CurvePrimitive>(curve));
        }

        void parseSphere(
            const json::ObjectPtr& definition,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            auto sphere = std::make_shared<HouGeo::HouSphere>();
            sphere->setTopologyVertex(vertexOffset);
            sphere->setTransform(parseFiniteMatrix33(
                requireArray(definition, "transform", path),
                path + ".transform"));
            geometry->addPrimitive(
                std::static_pointer_cast<HouGeoAdapter::SpherePrimitive>(sphere));
        }

        void parseTube(
            const json::ObjectPtr& definition,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            auto tube = std::make_shared<HouGeo::HouTube>();
            tube->setTopologyVertex(vertexOffset);
            tube->setTransform(parseFiniteMatrix33(
                requireArray(definition, "transform", path),
                path + ".transform"));
            tube->setCaps(definition->get<bool>("caps", false));
            try
            {
                tube->setTaper(definition->get<real32>("taper", 1.0f));
            }
            catch( const std::exception& exception )
            {
                failManifest(DiagnosticCategory::schema,
                    "Invalid tube definition: " + std::string(exception.what()),
                    path + ".taper");
            }
            geometry->addPrimitive(
                std::static_pointer_cast<HouGeoAdapter::TubePrimitive>(tube));
        }

        void parsePackedGeometry(
            const json::ObjectPtr& definition,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            json::ObjectPtr embeddedManifest = definition->object("embedded_manifest");
            if( !embeddedManifest )
                failManifest(DiagnosticCategory::schema,
                    "Packed geometry requires an embedded manifest",
                    path + ".embedded_manifest");
            auto packed = std::make_shared<HouGeo::HouPackedGeometry>();
            packed->setEmbeddedGeometry(parseGeometryRoot(embeddedManifest));
            packed->setTopologyVertex(vertexOffset);
            packed->setPivot(parseVector3(
                requireArray(definition, "pivot", path),
                path + ".pivot"));
            packed->setTransform(parseMatrix33(
                requireArray(definition, "transform", path),
                path + ".transform"));
            packed->setViewportLod(
                definition->get<std::string>("viewport_lod", "full"));
            packed->setPointInstanceTransform(
                definition->get<bool>("point_instance_transform", false));
            packed->setTreatAsFolder(
                definition->get<bool>("treat_as_folder", false));
            geometry->addPrimitive(
                std::static_pointer_cast<HouGeoAdapter::PackedGeometryPrimitive>(packed));
        }

        void parsePackedFragment(
            const json::ObjectPtr& definition,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            json::ObjectPtr embeddedManifest = definition->object("embedded_manifest");
            if( !embeddedManifest )
                failManifest(DiagnosticCategory::schema,
                    "Packed fragment requires an embedded manifest",
                    path + ".embedded_manifest");
            const std::string fragmentAttribute =
                definition->get<std::string>("fragment_attribute", "");
            if( fragmentAttribute.empty() )
                failManifest(DiagnosticCategory::schema,
                    "Packed fragment requires a fragment attribute",
                    path + ".fragment_attribute");
            const std::string fragmentName =
                definition->get<std::string>("fragment_name", "");
            if( fragmentName.empty() )
                failManifest(DiagnosticCategory::schema,
                    "Packed fragment requires a fragment name",
                    path + ".fragment_name");
            auto packed = std::make_shared<HouGeo::HouPackedFragment>();
            packed->setEmbeddedGeometry(parseGeometryRoot(embeddedManifest));
            packed->setTopologyVertex(vertexOffset);
            packed->setPivot(parseVector3(
                requireArray(definition, "pivot", path),
                path + ".pivot"));
            packed->setTransform(parseMatrix33(
                requireArray(definition, "transform", path),
                path + ".transform"));
            packed->setViewportLod(
                definition->get<std::string>("viewport_lod", "full"));
            packed->setPointInstanceTransform(
                definition->get<bool>("point_instance_transform", false));
            packed->setFragmentAttribute(fragmentAttribute);
            packed->setFragmentName(fragmentName);
            const auto bounds = parseBounds(
                requireArray(definition, "bounds", path), path + ".bounds");
            packed->setBounds(bounds);
            json::ArrayPtr cachedBounds = definition->array("cached_bounds");
            packed->setCachedBounds(cachedBounds
                ? parseBounds(cachedBounds, path + ".cached_bounds")
                : bounds);
            geometry->addPrimitive(
                std::static_pointer_cast<HouGeoAdapter::PackedFragmentPrimitive>(packed));
        }

        void parsePackedDisk(
            const json::ObjectPtr& definition,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            const std::string filename = definition->get<std::string>("filename", "");
            if( filename.empty() )
                failManifest(DiagnosticCategory::schema,
                    "Packed disk requires a filename", path + ".filename");
            auto packed = std::make_shared<HouGeo::HouPackedDisk>();
            packed->setTopologyVertex(vertexOffset);
            packed->setFilename(filename);
            packed->setExpandFrame(definition->get<real32>("expand_frame", 1.0f));
            packed->setExpandFilename(
                definition->get<bool>("expand_filename", false));
            packed->setPivot(parseVector3(
                requireArray(definition, "pivot", path),
                path + ".pivot"));
            packed->setTransform(parseMatrix33(
                requireArray(definition, "transform", path),
                path + ".transform"));
            packed->setViewportLod(
                definition->get<std::string>("viewport_lod", "full"));
            packed->setPointInstanceTransform(
                definition->get<bool>("point_instance_transform", false));
            packed->setTreatAsFolder(
                definition->get<bool>("treat_as_folder", false));
            geometry->addPrimitive(
                std::static_pointer_cast<HouGeoAdapter::PackedDiskPrimitive>(packed));
        }

        void parsePackedDiskSequence(
            const json::ObjectPtr& definition,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            json::ArrayPtr filenameValues = requireArray(definition, "filenames", path);
            if( filenameValues->size() <= 0 )
                failManifest(DiagnosticCategory::schema,
                    "Packed disk sequence requires at least one filename",
                    path + ".filenames");
            const int filenameCount = checkedCount(
                filenameValues->size(), path + ".filenames");
            std::vector<std::string> filenames;
            filenames.reserve(static_cast<std::size_t>(filenameCount));
            for( int filenameIndex = 0; filenameIndex < filenameCount; ++filenameIndex )
            {
                const std::string filename =
                    filenameValues->get<std::string>(filenameIndex);
                if( filename.empty() )
                    failManifest(DiagnosticCategory::schema,
                        "Packed disk sequence filename cannot be empty",
                        path + ".filenames[" + std::to_string(filenameIndex) + "]");
                filenames.push_back(filename);
            }

            const std::string wrap = definition->get<std::string>("wrap", "cycle");
            HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode wrapMode;
            if( wrap == "cycle" )
                wrapMode = HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::cycle;
            else if( wrap == "clamp" )
                wrapMode = HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::clamp;
            else if( wrap == "strict" )
                wrapMode = HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::strict;
            else if( wrap == "mirror" )
                wrapMode = HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::mirror;
            else
                failManifest(DiagnosticCategory::schema,
                    "Packed disk sequence wrap mode is invalid",
                    path + ".wrap");

            auto packed = std::make_shared<HouGeo::HouPackedDiskSequence>();
            packed->setTopologyVertex(vertexOffset);
            packed->setFilenames(std::move(filenames));
            packed->setIndex(definition->get<real32>("index", 0.0f));
            packed->setWrapMode(wrapMode);
            packed->setPivot(parseVector3(
                requireArray(definition, "pivot", path),
                path + ".pivot"));
            packed->setTransform(parseMatrix33(
                requireArray(definition, "transform", path),
                path + ".transform"));
            packed->setViewportLod(
                definition->get<std::string>("viewport_lod", "full"));
            packed->setPointInstanceTransform(
                definition->get<bool>("point_instance_transform", false));
            geometry->addPrimitive(
                std::static_pointer_cast<
                    HouGeoAdapter::PackedDiskSequencePrimitive>(packed));
        }

        void parseDenseVolume(
            const json::ObjectPtr& definition,
            int vertexOffset,
            const HouGeo::Ptr& geometry,
            const std::string& path)
        {
            const json::ArrayPtr resolutionValues = requireArray(
                definition, "resolution", path);
            const std::vector<sint32> resolution = scalarArray<sint32>(
                resolutionValues, path + ".resolution");
            if( resolution.size() != 3
                || resolution[0] <= 0 || resolution[1] <= 0 || resolution[2] <= 0 )
            {
                failManifest(DiagnosticCategory::schema,
                    "Dense volume resolution must contain three positive values",
                    path + ".resolution");
            }
            const math::V3i fieldResolution(
                resolution[0], resolution[1], resolution[2]);
            ScalarField::Ptr field = ScalarField::create(fieldResolution);
            field->setLocalToWorld(parseMatrix44(
                requireArray(definition, "local_to_world", path),
                path + ".local_to_world"));
            const std::vector<real32> voxels = scalarArray<real32>(
                requireArray(definition, "voxels", path), path + ".voxels");
            if( voxels.size() != field->values().size() )
                failManifest(DiagnosticCategory::schema,
                    "Dense volume voxel count does not match its resolution",
                    path + ".voxels");
            std::copy(voxels.begin(), voxels.end(), field->values().begin());

            auto volume = std::make_shared<HouGeo::HouVolume>();
            volume->setField(std::move(field));
            volume->setTopologyVertex(vertexOffset);
            json::ObjectPtr visualization = definition->object("visualization");
            if( visualization )
            {
                volume->setVisualization(
                    visualization->get<std::string>("mode", "smoke"),
                    visualization->get<real32>("iso", 0.0f),
                    visualization->get<real32>("density", 1.0f));
            }
            geometry->addPrimitive(
                std::static_pointer_cast<HouGeoAdapter::VolumePrimitive>(volume));
        }
    }

    void parsePrimitives(
        const json::ObjectPtr& root,
        const std::vector<int>& topology,
        const HouGeo::Ptr& geometry)
    {
        const json::ArrayPtr definitions = requireArray(root, "primitives", "root");
        const int count = checkedCount(definitions->size(), "primitives");
        for( int index = 0; index < count; ++index )
        {
            const std::string path = "primitives[" + std::to_string(index) + "]";
            json::ObjectPtr definition = definitions->object(index);
            if( !definition )
                failManifest(DiagnosticCategory::schema,
                    "Primitive definition must be an object", path);
            const std::string type = definition->get<std::string>("type");
            const int vertexOffset = checkedCount(
                definition->get<sint64>("vertex_offset", 0),
                path + ".vertex_offset");
            const bool rangePrimitive = type == "polygon"
                || type == "nurbs_curve" || type == "bezier_curve";
            if( !rangePrimitive
                && static_cast<std::size_t>(vertexOffset) >= topology.size() )
            {
                failManifest(DiagnosticCategory::schema,
                    "Primitive topology vertex exceeds the topology domain",
                    path + ".vertex_offset");
            }

            if( type == "polygon" )
                parsePolygon(definition, topology, vertexOffset, geometry, path);
            else if( type == "nurbs_curve" || type == "bezier_curve" )
                parseCurve(type, definition, topology, vertexOffset, geometry, path);
            else if( type == "sphere" )
                parseSphere(definition, vertexOffset, geometry, path);
            else if( type == "tube" )
                parseTube(definition, vertexOffset, geometry, path);
            else if( type == "packed_geometry" )
                parsePackedGeometry(definition, vertexOffset, geometry, path);
            else if( type == "packed_fragment" )
                parsePackedFragment(definition, vertexOffset, geometry, path);
            else if( type == "packed_disk" )
                parsePackedDisk(definition, vertexOffset, geometry, path);
            else if( type == "packed_disk_sequence" )
                parsePackedDiskSequence(definition, vertexOffset, geometry, path);
            else if( parseSparseVdbPrimitive(
                type, definition, vertexOffset, geometry, path) )
            {
            }
            else if( type == "dense_volume" )
                parseDenseVolume(definition, vertexOffset, geometry, path);
            else
                failManifest(DiagnosticCategory::unsupported_input,
                    "Manifest primitive type is unsupported: " + type,
                    path + ".type");
        }
    }
}
