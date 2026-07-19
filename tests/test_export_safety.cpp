#include <houio/HouGeoIO.h>

#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
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
    if (!houio::HouGeoIO::xport(&output, source, true))
    {
        return false;
    }

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::HouGeo::Ptr imported = houio::HouGeoIO::import(&input);
    return imported && imported->pointcount() == 4 && imported->vertexcount() == 0
           && imported->primitivecount() == 0;
}

int verifyExceptionRecovery(const houio::HouGeoAdapter::Ptr& validGeometry)
{
    std::ostringstream invalidOutput(std::ios::out | std::ios::binary);
    try
    {
        houio::HouGeoIO::xport(&invalidOutput, createInvalidPointGeometry(), true);
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
    if (houio::HouGeoIO::xport(&output, validGeometry, false))
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
    if (houio::HouGeoIO::xport(&output, validGeometry, true))
    {
        return fail("export did not report a failed output stream");
    }
    if (!roundtripOnce(validGeometry))
    {
        return fail("export did not recover after an output-stream failure");
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
    return verifyConcurrentExports(validGeometry);
}
