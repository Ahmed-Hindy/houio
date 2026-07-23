#include <houio/HouGeoIO.h>

#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
class DictionaryAttributeAdapter : public houio::HouGeoAdapter::AttributeAdapter
{
public:
    DictionaryAttributeAdapter()
    {
        auto label = houio::json::Object::create();
        label->appendValue("type", std::string("string"));
        label->appendValue("value", std::string("adapter"));
        dictionary_ = houio::json::Object::create();
        dictionary_->append("label", label);
    }

    std::string getName() const override
    {
        return "settings";
    }

    Type getType() const override
    {
        return ATTR_TYPE_DICT;
    }

    int getTupleSize() const override
    {
        return 1;
    }

    Storage getStorage() const override
    {
        return ATTR_STORAGE_INT32;
    }

    int getNumElements() const override
    {
        return 1;
    }

    std::string getString(int) const override
    {
        return "";
    }

    std::shared_ptr<houio::json::Object> getDictionary(int index) const override
    {
        return index == 0 ? dictionary_ : std::shared_ptr<houio::json::Object>();
    }

private:
    std::shared_ptr<houio::json::Object> dictionary_;
};

class DictionaryGeometryAdapter : public houio::HouGeoAdapter
{
public:
    DictionaryGeometryAdapter()
        : attribute_(std::make_shared<DictionaryAttributeAdapter>())
    {
    }

    void getPrimitiveAttributeNames(std::vector<std::string>& names) const override
    {
        names.clear();
    }

    AttributeAdapter::Ptr getPrimitiveAttribute(const std::string&) override
    {
        return AttributeAdapter::Ptr();
    }

    void getGlobalAttributeNames(std::vector<std::string>& names) const override
    {
        names = {"settings"};
    }

    AttributeAdapter::Ptr getGlobalAttribute(const std::string& name) override
    {
        return name == "settings" ? attribute_ : AttributeAdapter::Ptr();
    }

private:
    AttributeAdapter::Ptr attribute_;
};

class RejectingStreamBuffer : public std::streambuf
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

int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

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
    auto label = dictionary ? dictionary->getObject("label")
                            : std::shared_ptr<houio::json::Object>();
    if (!label || label->get<std::string>("value") != "adapter")
        return fail("abstract adapter dictionary value changed during export");
    return 0;
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

    if (const int result = verifyAdapterDictionaryExport(); result != 0)
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
