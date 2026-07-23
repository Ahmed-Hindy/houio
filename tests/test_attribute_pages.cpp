#include <houio/HouGeoIO.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

std::string geometryDocument(const std::string& valuesObject)
{
    return "[\"pointcount\",5,\"vertexcount\",0,\"primitivecount\",0,"
           "\"attributes\",[\"pointattributes\",[[[\"type\",\"numeric\","
           "\"name\",\"paged\"],[\"size\",3,\"storage\",\"int32\","
           "\"values\"," + valuesObject + "]]]]]";
}

houio::HouGeo::Ptr importPagedAttribute(const std::string& valuesObject)
{
    std::istringstream input(geometryDocument(valuesObject));
    return houio::HouGeoIO::import(input);
}

std::vector<houio::sint32> attributeValues(const houio::HouGeo::Ptr& geometry)
{
    if (!geometry)
    {
        throw std::runtime_error("Paged geometry did not import");
    }
    houio::HouGeoAdapter::AttributeAdapter::Ptr attribute = geometry->pointAttribute("paged");
    if (!attribute || attribute->getStorage() != houio::HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT32
        || attribute->getTupleSize() != 3 || attribute->getNumElements() != 5)
    {
        throw std::runtime_error("Paged attribute metadata was not preserved");
    }
    const houio::HouGeoAdapter::RawDataView raw_data = attribute->rawData();
    std::vector<houio::sint32> values(15);
    const std::size_t expected_bytes = values.size() * sizeof(values.front());
    if (!raw_data.available() || raw_data.sizeBytes() != expected_bytes)
    {
        throw std::runtime_error("Paged attribute has inconsistent data");
    }
    std::memcpy(values.data(), raw_data.bytes().data(), expected_bytes);
    return values;
}

int verifyValues(const std::string& valuesObject, const std::vector<houio::sint32>& expected,
                 const std::string& description)
{
    try
    {
        if (attributeValues(importPagedAttribute(valuesObject)) != expected)
        {
            return fail(description + " produced incorrect values");
        }
    }
    catch (const std::exception& error)
    {
        return fail(description + " failed: " + error.what());
    }
    return 0;
}

int verifyPagedLayouts()
{
    const std::vector<houio::sint32> expected = {
        10, 20, 30,
        11, 21, 31,
        12, 22, 32,
        13, 23, 33,
        14, 24, 34,
    };

    if (const int result = verifyValues(
            R"JSON(["size",3,"storage","int32","pagesize",2,"packing",[3],"rawpagedata",[10,20,30,11,21,31,12,22,32,13,23,33,14,24,34]])JSON",
            expected, "interleaved packing");
        result != 0)
    {
        return result;
    }

    if (const int result = verifyValues(
            R"JSON(["size",3,"storage","int32","pagesize",2,"packing",[1,1,1],"rawpagedata",[10,11,20,21,30,31,12,13,22,23,32,33,14,24,34]])JSON",
            expected, "component-major packing");
        result != 0)
    {
        return result;
    }

    const std::vector<houio::sint32> mixedExpected = {
        10, 20, 30,
        10, 21, 31,
        12, 22, 32,
        13, 22, 32,
        14, 24, 34,
    };
    return verifyValues(
        R"JSON(["size",3,"storage","int32","pagesize",2,"packing",[1,2],"constantpageflags",[[true,false,true],[false,true,false]],"rawpagedata",[10,20,30,21,31,12,13,22,32,14,24,34]])JSON",
        mixedExpected, "mixed constant and varying pages");
}

bool importRejected(const std::string& valuesObject)
{
    std::istringstream input(geometryDocument(valuesObject));
    houio::DiagnosticList diagnostics;
    return !houio::HouGeoIO::import(input, &diagnostics) && diagnostics.size() == 1
        && diagnostics.front().severity == houio::DiagnosticSeverity::error
        && diagnostics.front().path == "attributes.pointattributes[0]";
}

int verifyMalformedLayouts()
{
    const std::vector<std::pair<std::string, std::string>> malformed = {
        {"zero page size",
         R"JSON(["size",3,"storage","int32","pagesize",0,"packing",[3],"rawpagedata",[]])JSON"},
        {"empty packing",
         R"JSON(["size",3,"storage","int32","pagesize",2,"packing",[],"rawpagedata",[]])JSON"},
        {"zero pack",
         R"JSON(["size",3,"storage","int32","pagesize",2,"packing",[0,3],"rawpagedata",[]])JSON"},
        {"incomplete packing",
         R"JSON(["size",3,"storage","int32","pagesize",2,"packing",[1,1],"rawpagedata",[]])JSON"},
        {"flag pack count mismatch",
         R"JSON(["size",3,"storage","int32","pagesize",2,"packing",[1,2],"constantpageflags",[[false,false,false]],"rawpagedata",[]])JSON"},
        {"flag page count mismatch",
         R"JSON(["size",3,"storage","int32","pagesize",2,"packing",[3],"constantpageflags",[[false,false]],"rawpagedata",[]])JSON"},
        {"short payload",
         R"JSON(["size",3,"storage","int32","pagesize",2,"packing",[3],"rawpagedata",[1,2,3]])JSON"},
        {"extra payload",
         R"JSON(["size",3,"storage","int32","pagesize",2,"packing",[3],"rawpagedata",[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]])JSON"},
    };

    for (const auto& [description, valuesObject] : malformed)
    {
        if (!importRejected(valuesObject))
        {
            return fail(description + " was not rejected with an attribute diagnostic");
        }
    }
    return 0;
}
}

int main()
{
    if (const int result = verifyPagedLayouts(); result != 0)
    {
        return result;
    }
    return verifyMalformedLayouts();
}
