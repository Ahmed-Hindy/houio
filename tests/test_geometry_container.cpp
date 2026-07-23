#include <houio/Geometry.h>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

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
            || box->numPrimitives() != 12 || box->indexBuffer().size() != 36)
        {
            return fail("triangle box topology is incorrect");
        }

        const houio::Attribute::Ptr positions = box->getAttr("P");
        const houio::Attribute::Ptr texture_coordinates = box->getAttr("UV");
        if (!positions || positions->numElements() != 24
            || !texture_coordinates || texture_coordinates->numElements() != 24)
        {
            return fail("triangle box did not preserve face-varying positions and UVs");
        }

        box->addNormals();
        const houio::Attribute::Ptr normals = box->getAttr("N");
        if (!normals || normals->numElements() != 24)
            return fail("triangle box normal generation failed");
        for (int point_index = 0; point_index < normals->numElements(); ++point_index)
        {
            const houio::math::V3f normal = normals->get<houio::math::V3f>(
                static_cast<unsigned int>(point_index));
            if (!std::isfinite(normal.x) || !std::isfinite(normal.y)
                || !std::isfinite(normal.z) || normal.getLength() < 0.99f)
            {
                return fail("triangle box contains an invalid generated normal");
            }
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
        const houio::Attribute::Ptr sphere_positions = sphere->getAttr("P");
        if (!sphere_positions || sphere_positions->numElements() != 26
            || sphere->numPrimitives() != 48)
        {
            return fail("sphere generator produced unexpected counts");
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
        if (!merged || merged->numPrimitives() != 2 || merged->indexBuffer().size() != 4)
            return fail("line merge produced incorrect topology");
        const houio::Attribute::Ptr positions = merged->getAttr("P");
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
