#include <houio/HouGeoIO.h>

#include "TestSupport.h"

#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
class DictionaryAttributeAdapter final : public houio::HouGeoAdapter::AttributeAdapter
{
public:
    DictionaryAttributeAdapter()
    {
        auto label = houio::json::Object::create();
        label->appendValue("type", std::string("string"));
        label->appendValue("value", std::string("adapter"));
        dictionary_ = houio::json::Object::create();
        dictionary_->append("label", label);
        dictionary_->append("missing", houio::json::Value{});
    }

    std::string name() const override
    {
        return "settings";
    }

    Type type() const override
    {
        return Type::dictionary;
    }

    TupleSize tupleSize() const override
    {
        return TupleSize(1);
    }

    Storage storage() const override
    {
        return Storage::int32;
    }

    int elementCount() const override
    {
        return 1;
    }

    std::string stringValue(int) const override
    {
        return "";
    }

    std::shared_ptr<houio::json::Object> dictionaryValue(int index) const override
    {
        return index == 0 ? dictionary_ : std::shared_ptr<houio::json::Object>();
    }

private:
    std::shared_ptr<houio::json::Object> dictionary_;
};

class DictionaryGeometryAdapter final : public houio::HouGeoAdapter
{
public:
    DictionaryGeometryAdapter()
        : attribute_(std::make_shared<DictionaryAttributeAdapter>())
    {
    }

    std::vector<std::string> primitiveAttributeNames() const override
    {
        return {};
    }

    AttributeAdapter::Ptr primitiveAttribute(const std::string&) override
    {
        return nullptr;
    }

    AttributeAdapter::ConstPtr primitiveAttribute(const std::string&) const override
    {
        return nullptr;
    }

    std::vector<std::string> globalAttributeNames() const override
    {
        return {"settings"};
    }

    AttributeAdapter::Ptr globalAttribute(const std::string& name) override
    {
        return name == "settings" ? attribute_ : AttributeAdapter::Ptr();
    }

    AttributeAdapter::ConstPtr globalAttribute(const std::string& name) const override
    {
        return name == "settings" ? attribute_ : AttributeAdapter::ConstPtr();
    }

private:
    AttributeAdapter::Ptr attribute_;
};

class CountingTopologyAdapter final : public houio::HouGeoAdapter::Topology
{
public:
    explicit CountingTopologyAdapter(
        bool expose_view,
        std::vector<int> indices = {0, 40000, 1})
        : expose_view_(expose_view), indices_(std::move(indices))
    {
    }

    std::vector<int> indexValues() const override
    {
        ++copy_calls_;
        return indices_;
    }

    std::span<const int> indexView() const noexcept override
    {
        return expose_view_ ? std::span<const int>(indices_) : std::span<const int>();
    }

    void appendIndices(std::span<const int> indices) override
    {
        indices_.insert(indices_.end(), indices.begin(), indices.end());
    }

    houio::sint64 indexCount() const override
    {
        return static_cast<houio::sint64>(indices_.size());
    }

    int copyCalls() const noexcept
    {
        return copy_calls_;
    }

private:
    bool expose_view_ = false;
    std::vector<int> indices_;
    mutable int copy_calls_ = 0;
};

class TopologyGeometryAdapter final : public houio::HouGeoAdapter
{
public:
    explicit TopologyGeometryAdapter(std::shared_ptr<CountingTopologyAdapter> topology)
        : topology_(std::move(topology))
    {
    }

    houio::sint64 pointCount() const override { return 40001; }
    houio::sint64 vertexCount() const override { return 3; }
    houio::sint64 primitiveCount() const override { return 0; }
    std::vector<std::string> primitiveAttributeNames() const override { return {}; }
    AttributeAdapter::Ptr primitiveAttribute(const std::string&) override { return nullptr; }
    AttributeAdapter::ConstPtr primitiveAttribute(const std::string&) const override { return nullptr; }
    Topology::Ptr topology() override { return topology_; }
    Topology::ConstPtr topology() const override { return topology_; }

private:
    std::shared_ptr<CountingTopologyAdapter> topology_;
};

class TrianglePrimitiveAdapter final : public houio::HouGeoAdapter::PolyPrimitive
{
public:
    int polygonCount() const override { return 1; }
    int polygonVertexCount(int polygon_index) const override
    {
        if (polygon_index != 0)
            throw std::out_of_range("triangle polygon index is out of range");
        return 3;
    }
    std::span<const int> polygonVertexIndices(int polygon_index) const override
    {
        if (polygon_index != 0)
            throw std::out_of_range("triangle polygon index is out of range");
        return indices_;
    }
    bool isClosed() const override { return true; }

private:
    std::vector<int> indices_{0, 1, 2};
};

class CountingPrimitiveGeometryAdapter final : public houio::HouGeoAdapter
{
public:
    explicit CountingPrimitiveGeometryAdapter(bool expose_view)
        : expose_view_(expose_view),
          topology_(std::make_shared<CountingTopologyAdapter>(true, std::vector<int>{0, 1, 2})),
          primitives_{std::make_shared<TrianglePrimitiveAdapter>()}
    {
    }

    houio::sint64 pointCount() const override { return 3; }
    houio::sint64 vertexCount() const override { return 3; }
    houio::sint64 primitiveCount() const override { return 1; }
    std::vector<std::string> primitiveAttributeNames() const override { return {}; }
    AttributeAdapter::Ptr primitiveAttribute(const std::string&) override { return nullptr; }
    AttributeAdapter::ConstPtr primitiveAttribute(const std::string&) const override { return nullptr; }
    std::vector<Primitive::ConstPtr> primitives() const override
    {
        ++copy_calls_;
        return {primitives_.begin(), primitives_.end()};
    }
    std::span<const Primitive::Ptr> primitiveView() const noexcept override
    {
        return expose_view_ ? std::span<const Primitive::Ptr>(primitives_)
                            : std::span<const Primitive::Ptr>();
    }
    Topology::Ptr topology() override { return topology_; }
    Topology::ConstPtr topology() const override { return topology_; }
    int copyCalls() const noexcept { return copy_calls_; }

private:
    bool expose_view_ = false;
    std::shared_ptr<CountingTopologyAdapter> topology_;
    std::vector<Primitive::Ptr> primitives_;
    mutable int copy_calls_ = 0;
};

class RejectingStreamBuffer final : public std::streambuf
{
protected:
    std::streamsize xsputn(const char*, std::streamsize) override
    {
        return 0;
    }

    int_type overflow(int_type) override
    {
        return traits_type::eof();
    }
};

using houio::test::fail;

houio::HouGeo::Ptr createPointGeometry()
{
    houio::Attribute::Ptr positions = houio::Attribute::createV4f();
    positions->appendElement(houio::math::V4f(0.0f, 0.0f, 0.0f, 1.0f));
    positions->appendElement(houio::math::V4f(1.0f, 0.0f, 0.0f, 1.0f));
    positions->appendElement(houio::math::V4f(0.0f, 1.0f, 0.0f, 1.0f));
    positions->appendElement(houio::math::V4f(0.0f, 0.0f, 1.0f, 1.0f));

    houio::HouGeo::Ptr geometry = houio::HouGeo::create();
    geometry->setPointAttribute(
        std::make_shared<houio::HouGeo::HouAttribute>("P", positions));
    return geometry;
}

houio::HouGeo::Ptr createInvalidPointGeometry()
{
    houio::Attribute::Ptr positions = houio::Attribute::createV2f();
    positions->appendElement(houio::math::V2f(0.0f, 0.0f));

    houio::HouGeo::Ptr geometry = houio::HouGeo::create();
    geometry->setPointAttribute(
        std::make_shared<houio::HouGeo::HouAttribute>("P", positions));
    return geometry;
}

bool roundtripOnce(const houio::HouGeoAdapter::Ptr& source)
{
    std::ostringstream output(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::exportGeometry(output, source, true))
        return false;

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::HouGeo::Ptr imported = houio::HouGeoIO::import(input);
    return imported && imported->pointCount() == 4 && imported->vertexCount() == 0
           && imported->primitiveCount() == 0;
}

int verifyNonNumericPositionNameRoundtrip()
{
    auto attribute = std::make_shared<houio::HouGeo::HouAttribute>();
    attribute->setName("P");
    attribute->setStringValues(
        {"left", "center", "right"},
        houio::HouGeoAdapter::AttributeAdapter::TupleSize(3));

    houio::HouGeo::Ptr source = houio::HouGeo::create();
    source->setPointAttribute(attribute);

    std::ostringstream output(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::exportGeometry(output, source, true))
        return fail("non-numeric P attribute export failed");

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::HouGeo::Ptr imported = houio::HouGeoIO::import(input);
    const auto importedAttribute = imported ? imported->pointAttribute("P")
                                            : houio::HouGeoAdapter::AttributeAdapter::Ptr();
    if (!importedAttribute
        || importedAttribute->type() != houio::HouGeoAdapter::AttributeAdapter::Type::string
        || importedAttribute->tupleSize().value() != 3
        || importedAttribute->elementCount() != 1
        || importedAttribute->stringValue(0, 0) != "left"
        || importedAttribute->stringValue(0, 1) != "center"
        || importedAttribute->stringValue(0, 2) != "right")
    {
        return fail("non-numeric P attribute metadata changed during export");
    }
    return 0;
}

int verifyAdapterDictionaryExport()
{
    auto source = std::make_shared<DictionaryGeometryAdapter>();
    std::ostringstream output(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::exportGeometry(output, source, true))
        return fail("abstract adapter dictionary export failed");

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::HouGeo::Ptr imported = houio::HouGeoIO::import(input);
    auto attribute = imported ? imported->globalAttribute("settings")
                              : houio::HouGeoAdapter::AttributeAdapter::Ptr();
    auto dictionary = attribute ? attribute->dictionaryValue(0)
                                : std::shared_ptr<houio::json::Object>();
    auto label = dictionary ? dictionary->object("label")
                            : std::shared_ptr<houio::json::Object>();
    if (!label || label->get<std::string>("value") != "adapter"
        || !dictionary->contains("missing") || !dictionary->value("missing").isNull())
    {
        return fail("abstract adapter dictionary value changed during export");
    }
    return 0;
}

int verifyTopologyViewExport()
{
    const auto verifyExport = [](bool exposeView, int expectedCopyCalls) -> int
    {
        auto topology = std::make_shared<CountingTopologyAdapter>(exposeView);
        auto geometry = std::make_shared<TopologyGeometryAdapter>(topology);
        std::ostringstream output(std::ios::out | std::ios::binary);
        if (!houio::HouGeoIO::exportGeometry(output, geometry, true))
            return fail("topology adapter export failed");
        if (topology->copyCalls() != expectedCopyCalls)
            return fail("topology export used the wrong copy path");

        std::istringstream input(output.str(), std::ios::in | std::ios::binary);
        houio::HouGeo::Ptr imported = houio::HouGeoIO::import(input);
        const auto importedTopology = imported ? imported->topology()
                                               : houio::HouGeoAdapter::Topology::Ptr();
        const std::span<const int> indices = importedTopology ? importedTopology->indexView()
                                                               : std::span<const int>();
        if (!imported || imported->pointCount() != 40001 || imported->vertexCount() != 3
            || indices.size() != 3 || indices[0] != 0 || indices[1] != 40000 || indices[2] != 1)
        {
            return fail("topology adapter values changed during export");
        }
        return 0;
    };

    if (const int result = verifyExport(true, 0); result != 0)
        return result;
    return verifyExport(false, 1);
}

int verifyPrimitiveViewExport()
{
    const auto verifyExport = [](bool exposeView, int expectedCopyCalls) -> int
    {
        auto geometry = std::make_shared<CountingPrimitiveGeometryAdapter>(exposeView);
        std::ostringstream output(std::ios::out | std::ios::binary);
        if (!houio::HouGeoIO::exportGeometry(output, geometry, true))
            return fail("primitive adapter export failed");
        if (geometry->copyCalls() != expectedCopyCalls)
            return fail("primitive export used the wrong copy path");

        std::istringstream input(output.str(), std::ios::in | std::ios::binary);
        houio::HouGeo::Ptr imported = houio::HouGeoIO::import(input);
        const std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives =
            imported ? imported->primitives() : std::vector<houio::HouGeoAdapter::Primitive::Ptr>();
        const auto polygon = primitives.size() == 1
            ? std::dynamic_pointer_cast<houio::HouGeoAdapter::PolyPrimitive>(primitives.front())
            : houio::HouGeoAdapter::PolyPrimitive::Ptr();
        if (!imported || imported->pointCount() != 3 || imported->vertexCount() != 3
            || imported->primitiveCount() != 1 || !polygon || polygon->polygonCount() != 1
            || polygon->polygonVertexCount(0) != 3)
        {
            return fail("primitive adapter values changed during export");
        }
        return 0;
    };

    if (const int result = verifyExport(true, 0); result != 0)
        return result;
    return verifyExport(false, 1);
}

int verifyReferenceStreamApi(const houio::HouGeoAdapter::Ptr& validGeometry)
{
    std::ostringstream output(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::exportGeometry(output, validGeometry, true))
        return fail("reference-based export API failed");
    if (houio::HouGeoIO::exportGeometry(
            output, houio::HouGeoAdapter::Ptr(), true))
    {
        return fail("exportGeometry accepted a null geometry");
    }
    return 0;
}

int verifyExceptionRecovery(const houio::HouGeoAdapter::Ptr& validGeometry)
{
    std::ostringstream invalidOutput(std::ios::out | std::ios::binary);
    try
    {
        static_cast<void>(houio::HouGeoIO::exportGeometry(
            invalidOutput, createInvalidPointGeometry(), true));
        return fail("invalid P attribute did not raise an exception");
    }
    catch (const std::runtime_error&)
    {
    }

    if (!roundtripOnce(validGeometry))
    {
        return fail("export did not recover after an exception");
    }
    return 0;
}

int verifyAsciiRejection(const houio::HouGeoAdapter::Ptr& validGeometry)
{
    std::ostringstream output;
    if (houio::HouGeoIO::exportGeometry(output, validGeometry, false))
    {
        return fail("ASCII geometry export unexpectedly succeeded");
    }
    if (!output.str().empty())
    {
        return fail("rejected ASCII export wrote partial output");
    }
    return 0;
}

int verifyOutputFailure(const houio::HouGeoAdapter::Ptr& validGeometry)
{
    RejectingStreamBuffer streamBuffer;
    std::ostream output(&streamBuffer);
    if (houio::HouGeoIO::exportGeometry(output, validGeometry, true))
    {
        return fail("export did not report a failed output stream");
    }
    if (!roundtripOnce(validGeometry))
    {
        return fail("export did not recover after an output-stream failure");
    }
    return 0;
}

int verifyNegativeTopologyRejection(const houio::HouGeoAdapter::Ptr& validGeometry)
{
    houio::HouGeo::Ptr geometry = createPointGeometry();
    houio::HouGeo::HouTopology::Ptr topology = std::make_shared<houio::HouGeo::HouTopology>();
    topology->appendIndex(-1);
    geometry->setTopology(topology);

    std::ostringstream output(std::ios::out | std::ios::binary);
    try
    {
        static_cast<void>(houio::HouGeoIO::exportGeometry(output, geometry, true));
        return fail("export accepted a negative topology index");
    }
    catch (const std::runtime_error&)
    {
    }

    if (!roundtripOnce(validGeometry))
    {
        return fail("export did not recover after rejecting negative topology");
    }
    return 0;
}

int verifyConcurrentExports(const houio::HouGeoAdapter::Ptr& validGeometry)
{
    constexpr int threadCount = 8;
    constexpr int exportsPerThread = 25;
    std::atomic<int> failures = 0;
    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex)
    {
        threads.emplace_back([&validGeometry, &failures]() {
            for (int exportIndex = 0; exportIndex < exportsPerThread; ++exportIndex)
            {
                try
                {
                    if (!roundtripOnce(validGeometry))
                    {
                        ++failures;
                    }
                }
                catch (...)
                {
                    ++failures;
                }
            }
        });
    }

    for (std::thread& thread : threads)
    {
        thread.join();
    }

    if (failures.load() != 0)
    {
        return fail("one or more concurrent exports failed");
    }
    return 0;
}
}

int main()
{
    const houio::HouGeoAdapter::Ptr validGeometry = createPointGeometry();

    if (const int result = verifyNonNumericPositionNameRoundtrip(); result != 0)
    {
        return result;
    }
    if (const int result = verifyAdapterDictionaryExport(); result != 0)
    {
        return result;
    }
    if (const int result = verifyTopologyViewExport(); result != 0)
    {
        return result;
    }
    if (const int result = verifyPrimitiveViewExport(); result != 0)
    {
        return result;
    }
    if (const int result = verifyReferenceStreamApi(validGeometry); result != 0)
    {
        return result;
    }
    if (const int result = verifyExceptionRecovery(validGeometry); result != 0)
    {
        return result;
    }
    if (const int result = verifyAsciiRejection(validGeometry); result != 0)
    {
        return result;
    }
    if (const int result = verifyOutputFailure(validGeometry); result != 0)
    {
        return result;
    }
    if (const int result = verifyNegativeTopologyRejection(validGeometry); result != 0)
    {
        return result;
    }
    return verifyConcurrentExports(validGeometry);
}
