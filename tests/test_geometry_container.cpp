#include <houio/Geometry.h>

#include "TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using houio::test::fail;

    int verifyTriangleBox()
    {
        const houio::math::BoundingBox3f bounds(
            houio::math::V3f(-1.0f, -2.0f, -3.0f),
            houio::math::V3f(1.0f, 2.0f, 3.0f));
        const houio::Geometry::Ptr box = houio::Geometry::createBox(bounds);
        if (!box || box->primitiveType() != houio::Geometry::PrimitiveType::triangle
            || box->primitiveCount() != 12 || box->verticesPerPrimitive() != 3
            || box->indexBuffer().size() != 36)
        {
            return fail("triangle box topology is incorrect");
        }

        const houio::Attribute::Ptr positions = box->attribute("P");
        const houio::Attribute::Ptr texture_coordinates = box->attribute("UV");
        if (!positions || positions->numElements() != 24
            || !texture_coordinates || texture_coordinates->numElements() != 24)
        {
            return fail("triangle box did not preserve face-varying positions and UVs");
        }

        box->addNormals();
        const houio::Attribute::Ptr normals = box->attribute("N");
        if (!normals || normals->numElements() != 24)
            return fail("triangle box normal generation failed");
        for (int point_index = 0; point_index < normals->numElements(); ++point_index)
        {
            const houio::math::V3f normal = normals->get<houio::math::V3f>(
                static_cast<unsigned int>(point_index));
            if (!std::isfinite(normal.x) || !std::isfinite(normal.y)
                || !std::isfinite(normal.z) || normal.length() < 0.99f)
            {
                return fail("triangle box contains an invalid generated normal");
            }
        }

        const std::vector<std::string> attribute_names = box->attributeNames();
        if (attribute_names != std::vector<std::string>({"N", "P", "UV"})
            || !box->hasAttribute("P") || box->bounds().minPoint != bounds.minPoint
            || box->bounds().maxPoint != bounds.maxPoint)
        {
            return fail("modern Geometry attribute or bounds API is inconsistent");
        }
        return 0;
    }

    int verifyGeneratorsAndValidation()
    {
        try
        {
            static_cast<void>(houio::Geometry::createGrid(1, 4));
            return fail("2D grid accepted a degenerate resolution");
        }
        catch (const std::invalid_argument&)
        {
        }

        try
        {
            static_cast<void>(houio::Geometry::createSphere(2, 4, 1.0f));
            return fail("sphere accepted too few longitudinal subdivisions");
        }
        catch (const std::invalid_argument&)
        {
        }

        const houio::Geometry::Ptr sphere = houio::Geometry::createSphere(
            8,
            4,
            2.0f,
            houio::math::V3f(1.0f, 2.0f, 3.0f));
        const houio::Attribute::Ptr sphere_positions = sphere->attribute("P");
        if (!sphere_positions || sphere_positions->numElements() != 26
            || sphere->primitiveCount() != 48)
        {
            return fail("sphere generator produced unexpected counts");
        }

        try
        {
            static_cast<void>(houio::Geometry::createGrid(std::numeric_limits<int>::max(), 2));
            return fail("2D grid accepted an overflowing point count");
        }
        catch (const std::overflow_error&)
        {
        }

        try
        {
            static_cast<void>(houio::Geometry::createGrid(
                std::numeric_limits<int>::max(), 1, 1));
            return fail("3D grid accepted an overflowing axis point count");
        }
        catch (const std::overflow_error&)
        {
        }

        try
        {
            static_cast<void>(houio::Geometry::createSphere(
                std::numeric_limits<int>::max(), 3, 1.0f));
            return fail("sphere accepted an overflowing point count");
        }
        catch (const std::overflow_error&)
        {
        }
        return 0;
    }

    int verifyMergeAndDuplicate()
    {
        const houio::Geometry::Ptr first = houio::Geometry::createLine(
            houio::math::V3f(0.0f),
            houio::math::V3f(1.0f, 0.0f, 0.0f));
        const houio::Geometry::Ptr second = houio::Geometry::createLine(
            houio::math::V3f(2.0f, 0.0f, 0.0f),
            houio::math::V3f(3.0f, 0.0f, 0.0f));
        const std::array<houio::Geometry::Ptr, 2> mutable_sources = {first, second};
        const houio::Geometry::Ptr merged = houio::Geometry::merge(mutable_sources);
        const houio::Geometry::Ptr merged_from_initializer = houio::Geometry::merge({first, second});
        if (!merged_from_initializer || merged_from_initializer->primitiveCount() != 2
            || merged_from_initializer->indexBuffer().size() != 4)
        {
            return fail("initializer-list geometry merge compatibility was lost");
        }
        if (!merged || merged->primitiveCount() != 2 || merged->indexBuffer().size() != 4)
            return fail("line merge produced incorrect topology");
        const houio::Attribute::Ptr positions = merged->attribute("P");
        if (!positions || positions->numElements() != 4
            || merged->indexBuffer()[2] != 2 || merged->indexBuffer()[3] != 3)
        {
            return fail("line merge produced incorrect point offsets");
        }

        const std::array<houio::Geometry::ConstPtr, 2> immutable_sources = {first, second};
        const houio::Geometry::Ptr merged_from_const = houio::Geometry::merge(immutable_sources);
        const houio::Attribute::CPtr const_positions = merged_from_const
            ? merged_from_const->attribute("P") : houio::Attribute::CPtr();
        if (!merged_from_const || merged_from_const->primitiveCount() != 2
            || merged_from_const->indexBuffer().size() != 4 || !const_positions
            || const_positions->numElements() != 4
            || merged_from_const->indexBuffer()[2] != 2
            || merged_from_const->indexBuffer()[3] != 3)
        {
            return fail("const geometry merge produced incorrect point offsets");
        }

        const houio::Geometry::Index duplicate_index = merged->duplicatePoint(1);
        if (duplicate_index != 4 || positions->numElements() != 5
            || positions->get<houio::math::V3f>(duplicate_index)
                != positions->get<houio::math::V3f>(1))
        {
            return fail("point duplication did not copy all point data");
        }

        const houio::Geometry::Ptr inconsistent = houio::Geometry::createLine(
            houio::math::V3f(0.0f), houio::math::V3f(1.0f, 0.0f, 0.0f));
        const houio::Attribute::Ptr inconsistent_positions = inconsistent->attribute("P");
        const houio::Attribute::Ptr incomplete_weights = houio::Attribute::createFloat(1);
        inconsistent->setAttribute("weight", incomplete_weights);
        try
        {
            static_cast<void>(inconsistent->duplicatePoint(0));
            return fail("point duplication accepted inconsistent attribute counts");
        }
        catch (const std::runtime_error&)
        {
        }
        if (inconsistent_positions->numElements() != 2
            || incomplete_weights->numElements() != 1)
        {
            return fail("rejected point duplication partially mutated attributes");
        }
        return 0;
    }

    int verifyAttributeDomains()
    {
        const auto makeTriangle = [](float x_offset, houio::sint32 primitive_id) {
            const houio::Geometry::Ptr geometry = houio::Geometry::createTriangleGeometry();
            const houio::Attribute::Ptr positions = geometry->pointAttribute("P");
            positions->appendElement(houio::math::V3f(x_offset, 0.0f, 0.0f));
            positions->appendElement(houio::math::V3f(x_offset + 1.0f, 0.0f, 0.0f));
            positions->appendElement(houio::math::V3f(x_offset, 1.0f, 0.0f));
            geometry->addTriangle(0, 1, 2);

            const houio::Attribute::Ptr uv = houio::Attribute::createV2f();
            uv->appendElement(houio::math::V2f(0.0f, 0.0f));
            uv->appendElement(houio::math::V2f(1.0f, 0.0f));
            uv->appendElement(houio::math::V2f(0.0f, 1.0f));
            geometry->setVertexAttribute("UV", uv);

            const houio::Attribute::Ptr primitive_ids = houio::Attribute::createInt();
            primitive_ids->appendElement<houio::sint32>(primitive_id);
            geometry->setPrimitiveAttribute("id", primitive_ids);

            const houio::Attribute::Ptr scale = houio::Attribute::createFloat();
            scale->appendElement(2.5f);
            geometry->setGlobalAttribute("scale", scale);
            return geometry;
        };

        const houio::Geometry::Ptr first = makeTriangle(0.0f, 7);
        const houio::Geometry::Ptr second = makeTriangle(2.0f, 9);
        first->reverse();
        const houio::Attribute::Ptr reversed_uv = first->vertexAttribute("UV");
        if (!reversed_uv
            || reversed_uv->get<houio::math::V2f>(0) != houio::math::V2f(0.0f, 1.0f)
            || reversed_uv->get<houio::math::V2f>(1) != houio::math::V2f(1.0f, 0.0f)
            || reversed_uv->get<houio::math::V2f>(2) != houio::math::V2f(0.0f, 0.0f)
            || first->primitiveAttribute("id")->get<houio::sint32>(0) != 7
            || first->globalAttribute("scale")->get<houio::real32>(0) != 2.5f)
        {
            return fail("domain-aware reverse did not keep vertex data aligned with topology");
        }

        const houio::Geometry::Index duplicate = first->duplicatePoint(1);
        if (duplicate != 3 || first->pointAttribute("P")->numElements() != 4
            || first->vertexAttribute("UV")->numElements() != 3
            || first->primitiveAttribute("id")->numElements() != 1
            || first->globalAttribute("scale")->numElements() != 1)
        {
            return fail("point duplication changed a non-point attribute domain");
        }

        const houio::Geometry::Ptr merged = houio::Geometry::merge({first, second});
        const houio::Attribute::Ptr merged_uv = merged ? merged->vertexAttribute("UV") : nullptr;
        const houio::Attribute::Ptr merged_ids = merged ? merged->primitiveAttribute("id") : nullptr;
        const houio::Attribute::Ptr merged_scale = merged ? merged->globalAttribute("scale") : nullptr;
        if (!merged || merged->pointAttribute("P")->numElements() != 7
            || merged->indexBuffer().size() != 6 || !merged_uv || merged_uv->numElements() != 6
            || !merged_ids || merged_ids->numElements() != 2
            || merged_ids->get<houio::sint32>(0) != 7
            || merged_ids->get<houio::sint32>(1) != 9
            || !merged_scale || merged_scale->numElements() != 1
            || merged_scale->get<houio::real32>(0) != 2.5f)
        {
            return fail("domain-aware merge did not preserve all attribute domains");
        }

        const houio::Geometry::Ptr incompatible = makeTriangle(4.0f, 11);
        incompatible->globalAttribute("scale")->set<houio::real32>(0, 3.0f);
        try
        {
            static_cast<void>(houio::Geometry::merge({second, incompatible}));
            return fail("geometry merge accepted conflicting global attribute values");
        }
        catch (const std::invalid_argument&)
        {
        }

        merged->clear();
        if (!merged->pointAttributes().empty() || !merged->vertexAttributes().empty()
            || !merged->primitiveAttributes().empty() || !merged->globalAttributes().empty()
            || !merged->indexBuffer().empty() || merged->primitiveCount() != 0)
        {
            return fail("Geometry::clear retained an attribute domain");
        }
        return 0;
    }

    int verifyVariablePolygonTopology()
    {
        const houio::Geometry::Ptr pentagon = houio::Geometry::createPolyGeometry();
        for (unsigned int point_index = 0; point_index < 5; ++point_index)
        {
            pentagon->attribute("P")->appendElement(houio::math::V3f(
                static_cast<float>(point_index),
                0.0f,
                0.0f));
        }
        const std::array<houio::Geometry::Index, 5> pentagon_indices = {0, 1, 2, 3, 4};
        if (pentagon->addPolygon(pentagon_indices) != 0
            || pentagon->verticesPerPrimitive() != 5
            || pentagon->primitiveVertexCount(0) != 5)
        {
            return fail("first variable polygon returned incorrect topology metadata");
        }

        try
        {
            const std::span<const houio::Geometry::Index> empty;
            static_cast<void>(pentagon->addPolygon(empty));
            return fail("polygon geometry accepted an empty primitive");
        }
        catch (const std::invalid_argument&)
        {
        }

        const houio::Geometry::Ptr triangle = houio::Geometry::createPolyGeometry();
        for (unsigned int point_index = 0; point_index < 3; ++point_index)
        {
            triangle->attribute("P")->appendElement(houio::math::V3f(
                static_cast<float>(point_index),
                1.0f,
                0.0f));
        }
        const std::array<houio::Geometry::Index, 3> triangle_indices = {0, 1, 2};
        triangle->addPolygon(triangle_indices);

        const houio::Geometry::Ptr merged = houio::Geometry::merge({pentagon, triangle});
        const std::span<const unsigned int> counts = merged
            ? merged->primitiveVertexCounts() : std::span<const unsigned int>();
        const std::array<houio::Geometry::Index, 8> expected_indices = {
            0, 1, 2, 3, 4, 5, 6, 7};
        if (!merged || merged->primitiveType() != houio::Geometry::PrimitiveType::polygon
            || merged->primitiveCount() != 2 || merged->verticesPerPrimitive() != 0
            || counts.size() != 2 || counts[0] != 5 || counts[1] != 3
            || merged->primitiveVertexCount(0) != 5
            || merged->primitiveVertexCount(1) != 3
            || !std::equal(
                merged->indexBuffer().begin(),
                merged->indexBuffer().end(),
                expected_indices.begin()))
        {
            return fail("variable polygon merge did not preserve exact boundaries");
        }

        try
        {
            static_cast<void>(merged->primitiveVertexCount(2));
            return fail("polygon primitive count query accepted an out-of-range index");
        }
        catch (const std::out_of_range&)
        {
        }

        merged->reverse();
        const std::array<houio::Geometry::Index, 8> reversed = {4, 3, 2, 1, 0, 7, 6, 5};
        if (!std::equal(merged->indexBuffer().begin(), merged->indexBuffer().end(), reversed.begin()))
            return fail("variable polygon reverse crossed a primitive boundary");

        const houio::Geometry::Ptr compatibility = houio::Geometry::createPolyGeometry();
        compatibility->addPolygonVertex(0);
        compatibility->addPolygonVertex(1);
        compatibility->addPolygonVertex(2);
        if (compatibility->primitiveCount() != 1
            || compatibility->verticesPerPrimitive() != 3
            || compatibility->primitiveVertexCounts().size() != 1
            || compatibility->primitiveVertexCounts()[0] != 3)
        {
            return fail("legacy addPolygonVertex compatibility changed");
        }
        compatibility->clear();
        if (compatibility->primitiveCount() != 0
            || !compatibility->primitiveVertexCounts().empty()
            || !compatibility->indexBuffer().empty())
        {
            return fail("clearing polygon geometry retained topology metadata");
        }
        return 0;
    }
}

int main()
{
    if (const int result = verifyTriangleBox(); result != 0)
        return result;
    if (const int result = verifyGeneratorsAndValidation(); result != 0)
        return result;
    if (const int result = verifyMergeAndDuplicate(); result != 0)
        return result;
    if (const int result = verifyAttributeDomains(); result != 0)
        return result;
    return verifyVariablePolygonTopology();
}
