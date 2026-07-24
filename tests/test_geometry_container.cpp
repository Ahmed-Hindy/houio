#include <houio/Geometry.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    int fail(const std::string& message)
    {
        std::cerr << "error: " << message << '\n';
        return 1;
    }

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
        const houio::Geometry::Ptr merged = houio::Geometry::merge({first, second});
        if (!merged || merged->primitiveCount() != 2 || merged->indexBuffer().size() != 4)
            return fail("line merge produced incorrect topology");
        const houio::Attribute::Ptr positions = merged->attribute("P");
        if (!positions || positions->numElements() != 4
            || merged->indexBuffer()[2] != 2 || merged->indexBuffer()[3] != 3)
        {
            return fail("line merge produced incorrect point offsets");
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
}

int main()
{
    if (const int result = verifyTriangleBox(); result != 0)
        return result;
    if (const int result = verifyGeneratorsAndValidation(); result != 0)
        return result;
    return verifyMergeAndDuplicate();
}
