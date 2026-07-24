#include <houio/HouGeo.h>
#include <houio/json.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void printIndent(int depth)
{
    for (int index = 0; index < depth; ++index)
    {
        std::cout << "  ";
    }
}

void printValueSummary(houio::json::Value value, int depth);

void printArraySummary(const houio::json::ArrayPtr& array, int depth)
{
    printIndent(depth);
    if (!array)
    {
        std::cout << "array=null\n";
        return;
    }

    std::cout << "array size=" << array->size() << " uniform=" << std::boolalpha
              << array->isUniform() << '\n';

    const houio::sint64 elementLimit = std::min<houio::sint64>(array->size(), 16);
    for (houio::sint64 index = 0; index < elementLimit; ++index)
    {
        printIndent(depth + 1);
        std::cout << '[' << index << "] ";
        printValueSummary(array->value(static_cast<int>(index)), depth + 1);
    }
}

void printObjectSummary(const houio::json::ObjectPtr& object, int depth)
{
    printIndent(depth);
    if (!object)
    {
        std::cout << "object=null\n";
        return;
    }

    const std::vector<std::string> keys = object->keys();
    std::cout << "object keys=" << keys.size() << '\n';
    for (const std::string& key : keys)
    {
        printIndent(depth + 1);
        std::cout << key << ": ";
        printValueSummary(object->value(key), depth + 1);
    }
}

void printValueSummary(houio::json::Value value, int depth)
{
    if (value.isArray())
    {
        std::cout << '\n';
        printArraySummary(value.asArray(), depth + 1);
    }
    else if (value.isObject())
    {
        std::cout << '\n';
        printObjectSummary(value.asObject(), depth + 1);
    }
    else if (value.isString())
    {
        std::cout << "string=" << value.as<std::string>() << '\n';
    }
    else if (value.isNull())
    {
        std::cout << "null\n";
    }
    else
    {
        std::cout << "scalar\n";
    }
}

int inspectPrimitiveAttributes(const std::string& inputPath)
{
    std::ifstream input(inputPath, std::ios::binary);
    if (!input)
    {
        std::cerr << "error: unable to open " << inputPath << '\n';
        return 1;
    }

    houio::json::JSONReader reader;
    houio::json::Parser parser;
    if (!parser.parse(input, reader))
    {
        std::cerr << "error: parse failed\n";
        return 1;
    }

    houio::json::ObjectPtr root = houio::HouGeo::toObject(reader.root().asArray());
    houio::json::ObjectPtr attributes = houio::HouGeo::toObject(root->array("attributes"));
    houio::json::ArrayPtr primitiveAttributes = attributes->array("primitiveattributes");
    if (!primitiveAttributes)
    {
        std::cout << "no primitive attributes\n";
        return 0;
    }

    std::cout << "primitive attribute count=" << primitiveAttributes->size() << '\n';
    for (houio::sint64 index = 0; index < primitiveAttributes->size(); ++index)
    {
        houio::json::ArrayPtr attribute = primitiveAttributes->array(static_cast<int>(index));
        houio::json::ObjectPtr definition = houio::HouGeo::toObject(attribute->array(0));
        houio::json::ObjectPtr data = houio::HouGeo::toObject(attribute->array(1));
        std::cout << "attribute " << index << " name=" << definition->get<std::string>("name")
                  << " type=" << definition->get<std::string>("type") << '\n';
        printObjectSummary(data, 1);
    }

    return 0;
}
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "usage: houio_inspect_attributes <input.geo|bgeo>\n";
        return 1;
    }

    try
    {
        return inspectPrimitiveAttributes(argv[1]);
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
