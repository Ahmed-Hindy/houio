#include <houio/HouGeoIO.h>

#include "TestSupport.h"

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

namespace
{
using houio::test::fail;

bool nearlyEqual(houio::real32 actual, houio::real32 expected)
{
    return std::isfinite(actual) && std::isfinite(expected)
        && std::abs(actual - expected) <= 1.0e-6f;
}

const char* quadricGeometry()
{
    return R"JSON([
        "pointcount",2,
        "vertexcount",2,
        "primitivecount",2,
        "topology",["pointref",["indices",[0,1]]],
        "attributes",[
            "pointattributes",[
                [
                    ["scope","public","type","numeric","name","P"],
                    ["size",3,"storage","fpreal32","values",[
                        "size",3,"storage","fpreal32","tuples",[
                            [1,2,3],[-1,0.5,2]
                        ]
                    ]]
                ]
            ]
        ],
        "primitives",[
            [
                ["type","Sphere"],
                ["vertex",0,"transform",[2,0,0,0,3,0,0,0,4]]
            ],
            [
                ["type","Tube"],
                ["vertex",1,"transform",[1,0,0,0,5,0,0,0,2],"caps",true,"taper",0.5]
            ]
        ]
    ])JSON";
}

bool matrixEqual(const houio::math::M33f& actual, const houio::math::M33f& expected)
{
    for( int index = 0; index < 9; ++index )
    {
        if( !nearlyEqual(actual.ma[index], expected.ma[index]) )
            return false;
    }
    return true;
}

int verifyGeometry(const houio::HouGeo::Ptr& geometry)
{
    if( !geometry || geometry->pointCount() != 2 || geometry->vertexCount() != 2
        || geometry->primitiveCount() != 2 )
    {
        return fail("quadric geometry counts changed");
    }

    const auto primitives = geometry->primitives();
    if( primitives.size() != 2 )
        return fail("quadric records were not preserved separately");

    const auto sphere =
        std::dynamic_pointer_cast<houio::HouGeoAdapter::SpherePrimitive>(primitives[0]);
    const auto tube =
        std::dynamic_pointer_cast<houio::HouGeoAdapter::TubePrimitive>(primitives[1]);
    if( !sphere || sphere->topologyVertex() != 0
        || !matrixEqual(sphere->transform(), houio::math::M33f(
            2,0,0, 0,3,0, 0,0,4)) )
    {
        return fail("sphere topology or transform changed");
    }
    if( !tube || tube->topologyVertex() != 1 || !tube->hasCaps()
        || !nearlyEqual(tube->taper(), 0.5f)
        || !matrixEqual(tube->transform(), houio::math::M33f(
            1,0,0, 0,5,0, 0,0,2)) )
    {
        return fail("tube topology, transform, caps, or taper changed");
    }
    return 0;
}

int verifyRoundTrip()
{
    std::istringstream source(quadricGeometry());
    const houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(source);
    if( const int result = verifyGeometry(geometry); result != 0 )
        return result;

    std::ostringstream binary(std::ios::binary);
    if( !houio::HouGeoIO::exportGeometry(binary, geometry, true) )
        return fail("quadric binary export failed");
    std::istringstream roundTripSource(binary.str(), std::ios::binary);
    return verifyGeometry(houio::HouGeoIO::import(roundTripSource));
}

int verifyProgrammaticValidation()
{
    houio::HouGeo::HouTube tube;
    try
    {
        tube.setTaper(std::numeric_limits<houio::real32>::infinity());
        return fail("non-finite tube taper was accepted");
    }
    catch( const std::invalid_argument& )
    {
    }

    tube.setTopologyVertex(0);
    tube.setCaps(true);
    tube.setTaper(-0.25f);
    if( tube.topologyVertex() != 0 || !tube.hasCaps()
        || !nearlyEqual(tube.taper(), -0.25f) )
    {
        return fail("programmatic tube state changed");
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
