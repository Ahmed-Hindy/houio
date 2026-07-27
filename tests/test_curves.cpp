#include "TestSupport.h"

#include <houio/HouGeoIO.h>

#include <sstream>
#include <string>
#include <vector>

namespace
{
using houio::test::fail;

const char* curveGeometry()
{
    return R"JSON([
        "pointcount",12,
        "vertexcount",12,
        "primitivecount",2,
        "topology",["pointref",["indices",[0,1,2,3,4,5,6,7,8,9,10,11]]],
        "attributes",[
            "pointattributes",[
                [
                    ["scope","public","type","numeric","name","P"],
                    ["size",3,"storage","fpreal32","values",[
                        "size",3,"storage","fpreal32","tuples",[
                            [0,0,0],[1,1,0],[2,0,0],[3,1,0],[4,0,0],
                            [0,2,0],[1,3,0],[2,4,0],[3,2,0],[4,3,0],[5,4,0],[6,2,0]
                        ]
                    ]]
                ],
                [
                    ["scope","public","type","numeric","name","Pw"],
                    ["size",1,"storage","fpreal32","values",[
                        "size",1,"storage","fpreal32","arrays",[
                            [1,1,0.5,1,1,1,1,1,1,1,1,1]
                        ]
                    ]]
                ]
            ]
        ],
        "primitives",[
            [
                ["type","NURBCurve"],
                ["vertex",[0,1,2,3,4],"closed",false,"basis",[
                    "type","NURBS","order",4,"endinterpolation",false,
                    "knots",[0,0,0,0,1,2,2,2,2]
                ]]
            ],
            [
                ["type","BezierCurve"],
                ["vertex",[5,6,7,8,9,10,11],"closed",false,"basis",[
                    "type","Bezier","order",4,"knots",[0,1,2]
                ]]
            ]
        ]
    ])JSON";
}

int verifyCurves(const houio::HouGeo::Ptr& geometry)
{
    if( !geometry || geometry->pointCount() != 12
        || geometry->vertexCount() != 12 || geometry->primitiveCount() != 2 )
    {
        return fail("curve geometry counts are incorrect");
    }

    const auto primitives = geometry->primitives();
    if( primitives.size() != 2 )
        return fail("curve records were not represented individually");

    const auto nurbs = std::dynamic_pointer_cast<houio::HouGeoAdapter::CurvePrimitive>(
        primitives[0]);
    const auto bezier = std::dynamic_pointer_cast<houio::HouGeoAdapter::CurvePrimitive>(
        primitives[1]);
    if( !nurbs || !bezier )
        return fail("curve records did not produce curve adapters");

    const std::vector<int> expectedNurbsVertices{0, 1, 2, 3, 4};
    const std::vector<houio::real64> expectedNurbsKnots{0, 0, 0, 0, 1, 2, 2, 2, 2};
    if( nurbs->basis() != houio::HouGeoAdapter::CurvePrimitive::Basis::nurbs
        || nurbs->isClosed() || nurbs->order() != 4
        || nurbs->endInterpolation()
        || !std::equal(nurbs->vertexIndices().begin(), nurbs->vertexIndices().end(),
            expectedNurbsVertices.begin(), expectedNurbsVertices.end())
        || !std::equal(nurbs->knots().begin(), nurbs->knots().end(),
            expectedNurbsKnots.begin(), expectedNurbsKnots.end()) )
    {
        return fail("NURBS curve basis or topology changed");
    }

    const std::vector<int> expectedBezierVertices{5, 6, 7, 8, 9, 10, 11};
    const std::vector<houio::real64> expectedBezierKnots{0, 1, 2};
    if( bezier->basis() != houio::HouGeoAdapter::CurvePrimitive::Basis::bezier
        || bezier->isClosed() || bezier->order() != 4
        || !std::equal(bezier->vertexIndices().begin(), bezier->vertexIndices().end(),
            expectedBezierVertices.begin(), expectedBezierVertices.end())
        || !std::equal(bezier->knots().begin(), bezier->knots().end(),
            expectedBezierKnots.begin(), expectedBezierKnots.end()) )
    {
        return fail("Bezier curve basis or topology changed");
    }

    const auto weights = geometry->pointAttribute("Pw");
    if( !weights || weights->elementCount() != 12
        || weights->rawData().read<houio::real32>(2) != 0.5f )
    {
        return fail("rational curve weights were not preserved through Pw");
    }
    return 0;
}

int verifyRoundTrip()
{
    std::istringstream input(curveGeometry());
    const houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(input);
    if( const int result = verifyCurves(geometry); result != 0 )
        return result;

    std::ostringstream output(std::ios::out | std::ios::binary);
    if( !houio::HouGeoIO::exportGeometry(output, geometry, true) )
        return fail("curve geometry failed binary export");
    std::istringstream binaryInput(output.str(), std::ios::in | std::ios::binary);
    return verifyCurves(houio::HouGeoIO::import(binaryInput));
}

int verifyProgrammaticValidation()
{
    houio::HouGeo::HouCurve curve;
    try
    {
        curve.setCurveData(
            houio::HouGeoAdapter::CurvePrimitive::Basis::bezier,
            {0, 1, 2, 3, 4},
            false,
            4,
            {0.0, 1.0});
        return fail("incompatible Bezier vertex count was accepted");
    }
    catch( const std::invalid_argument& )
    {
    }

    try
    {
        curve.setCurveData(
            houio::HouGeoAdapter::CurvePrimitive::Basis::nurbs,
            {0, 1, 2, 3},
            false,
            4,
            {0.0, 1.0, 0.5});
        return fail("decreasing NURBS knots were accepted");
    }
    catch( const std::invalid_argument& )
    {
    }
    return 0;
}
}

int main()
{
    if( const int result = verifyRoundTrip(); result != 0 )
        return result;
    return verifyProgrammaticValidation();
}
