#include <houio/HouGeoIO.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace
{
houio::json::ParserLimits corpusLimits()
{
    houio::json::ParserLimits limits;
    limits.maxInputBytes = 8192;
    limits.maxStringBytes = 2048;
    limits.maxUniformArrayElements = 2048;
    limits.maxNestingDepth = 64;
    return limits;
}

bool exerciseInput(std::span<const std::uint8_t> bytes)
{
    const std::string inputData = bytes.empty()
        ? std::string()
        : std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    const houio::json::ParserLimits limits = corpusLimits();

    try
    {
        std::istringstream parserInput(inputData, std::ios::in | std::ios::binary);
        houio::json::JSONReader reader;
        houio::json::Parser parser(limits);
        houio::DiagnosticList parserDiagnostics;
        parser.parse(&parserInput, &reader, &parserDiagnostics);

        std::istringstream geometryInput(inputData, std::ios::in | std::ios::binary);
        houio::DiagnosticList geometryDiagnostics;
        houio::HouGeoIO::import(&geometryInput, limits, &geometryDiagnostics);
    }
    catch (const std::exception& error)
    {
        std::cerr << "unexpected diagnostics-aware parser exception: " << error.what() << '\n';
        return false;
    }
    catch (...)
    {
        std::cerr << "unexpected non-standard diagnostics-aware parser exception\n";
        return false;
    }
    return true;
}

std::vector<std::vector<std::uint8_t>> seedCorpus()
{
    return {
        {},
        {'1'},
        {'t', 'r', 'u', 'e'},
        {'"', 't', 'e', 'x', 't', '"'},
        {'[', ']'},
        {'{', '}'},
        {'[', '"', 'p', 'o', 'i', 'n', 't', 'c', 'o', 'u', 'n', 't', '"', ',', '0', ']'},
        {'{', '"', 'a', '"', ':', '[', 't', 'r', 'u', 'e', ',', 'f', 'a', 'l', 's', 'e', ']', '}'},
        {']'},
        {'[', '}'},
        {0x7f, 0x4e, 0x53, 0x4a, 0x62, 0x5b, 0x5d},
        {0x7f, 0x4e, 0x53, 0x4a, 0x62, 0x5b, 0x40, 0x11, 0x03, 0x01, 0x02, 0x03, 0x5d},
        {0x7f, 0x4e, 0x53, 0x4a, 0x62, 0x5b, 0x27, 0x01, 0x00, 0x00, 0x00, 'x', 0x5d},
    };
}

bool runDeterministicCorpus()
{
    const std::array<std::uint8_t, 10> mutationBytes = {
        0x00, 0x01, 0x11, 0x18, 0x22, 0x27, 0x40, 0x5b, 0x5d, 0xff,
    };
    const auto seeds = seedCorpus();

    for (const auto& seed : seeds)
    {
        for (std::size_t length = 0; length <= seed.size(); ++length)
        {
            if (!exerciseInput(std::span<const std::uint8_t>(seed.data(), length)))
            {
                return false;
            }
        }

        for (std::size_t index = 0; index < seed.size(); ++index)
        {
            for (std::uint8_t replacement : mutationBytes)
            {
                std::vector<std::uint8_t> mutation = seed;
                mutation[index] = replacement;
                if (!exerciseInput(mutation))
                {
                    return false;
                }
            }

            std::vector<std::uint8_t> deletion = seed;
            deletion.erase(deletion.begin() + static_cast<std::ptrdiff_t>(index));
            if (!exerciseInput(deletion))
            {
                return false;
            }
        }

        for (std::size_t index = 0; index <= seed.size(); ++index)
        {
            for (std::uint8_t inserted : mutationBytes)
            {
                std::vector<std::uint8_t> mutation = seed;
                mutation.insert(mutation.begin() + static_cast<std::ptrdiff_t>(index), inserted);
                if (!exerciseInput(mutation))
                {
                    return false;
                }
            }
        }
    }

    std::mt19937 random(0x484f5549u);
    std::uniform_int_distribution<std::size_t> seedDistribution(0, seeds.size() - 1);
    std::uniform_int_distribution<int> operationDistribution(0, 3);
    std::uniform_int_distribution<int> byteDistribution(0, 255);

    for (int iteration = 0; iteration < 4096; ++iteration)
    {
        std::vector<std::uint8_t> mutation = seeds[seedDistribution(random)];
        const int operationCount = 1 + operationDistribution(random);
        for (int operation = 0; operation < operationCount; ++operation)
        {
            const int selectedOperation = operationDistribution(random);
            if (selectedOperation == 0 && !mutation.empty())
            {
                std::uniform_int_distribution<std::size_t> indexDistribution(0, mutation.size() - 1);
                mutation[indexDistribution(random)] ^=
                    static_cast<std::uint8_t>(1u << (byteDistribution(random) % 8));
            }
            else if (selectedOperation == 1 && mutation.size() < 512)
            {
                std::uniform_int_distribution<std::size_t> indexDistribution(0, mutation.size());
                mutation.insert(
                    mutation.begin() + static_cast<std::ptrdiff_t>(indexDistribution(random)),
                    static_cast<std::uint8_t>(byteDistribution(random)));
            }
            else if (selectedOperation == 2 && !mutation.empty())
            {
                std::uniform_int_distribution<std::size_t> indexDistribution(0, mutation.size() - 1);
                mutation.erase(
                    mutation.begin() + static_cast<std::ptrdiff_t>(indexDistribution(random)));
            }
            else if (mutation.size() < 512)
            {
                mutation.push_back(static_cast<std::uint8_t>(byteDistribution(random)));
            }
        }

        if (!exerciseInput(mutation))
        {
            return false;
        }
    }
    return true;
}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (!exerciseInput(std::span<const std::uint8_t>(data, size)))
    {
        std::abort();
    }
    return 0;
}

#ifdef HOUIO_STANDALONE_CORPUS
int main()
{
    return runDeterministicCorpus() ? 0 : 1;
}
#endif
