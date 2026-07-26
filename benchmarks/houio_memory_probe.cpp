#include <houio/Attribute.h>
#include <houio/HouGeo.h>
#include <houio/HouGeoIO.h>
#include <houio/json.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#    include <psapi.h>
#elif defined(__linux__)
#    include <fstream>
#    include <unistd.h>
#endif

namespace
{
    struct Options
    {
        int elements = 500000;
        bool csv = false;
    };

    [[nodiscard]] int parsePositiveInt(std::string_view text, std::string_view option_name)
    {
        int value = 0;
        const char* const begin = text.data();
        const char* const end = text.data() + text.size();
        const auto [position, error] = std::from_chars(begin, end, value);
        if (error != std::errc{} || position != end || value <= 0)
            throw std::invalid_argument(std::string(option_name) + " requires a positive integer");
        return value;
    }

    [[nodiscard]] Options parseOptions(int argument_count, char** arguments)
    {
        Options options;
        for (int index = 1; index < argument_count; ++index)
        {
            const std::string_view argument(arguments[index]);
            if (argument == "--csv")
            {
                options.csv = true;
                continue;
            }
            if (argument == "--help")
            {
                std::cout
                    << "Usage: houio_memory_probe [options]\n"
                    << "  --elements N  Point elements in the generated binary document (default 500000)\n"
                    << "  --csv         Emit one CSV header and row\n";
                std::exit(0);
            }
            if (argument == "--elements")
            {
                if (index + 1 >= argument_count)
                    throw std::invalid_argument("--elements requires a value");
                options.elements = parsePositiveInt(arguments[++index], argument);
                continue;
            }
            throw std::invalid_argument("Unknown memory-probe option: " + std::string(argument));
        }
        return options;
    }

    [[nodiscard]] std::size_t currentWorkingSetBytes()
    {
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS counters{};
        counters.cb = sizeof(counters);
        if (!GetProcessMemoryInfo(
                GetCurrentProcess(),
                &counters,
                static_cast<DWORD>(sizeof(counters))))
        {
            throw std::runtime_error("GetProcessMemoryInfo failed");
        }
        return static_cast<std::size_t>(counters.WorkingSetSize);
#elif defined(__linux__)
        std::ifstream statm("/proc/self/statm");
        std::uint64_t total_pages = 0;
        std::uint64_t resident_pages = 0;
        if (!(statm >> total_pages >> resident_pages))
            throw std::runtime_error("Could not read /proc/self/statm");
        static_cast<void>(total_pages);
        const long page_size = sysconf(_SC_PAGESIZE);
        if (page_size <= 0)
            throw std::runtime_error("Could not determine the system page size");
        if (resident_pages > std::numeric_limits<std::size_t>::max()
                / static_cast<std::size_t>(page_size))
        {
            throw std::overflow_error("Working-set byte count exceeds size_t range");
        }
        return static_cast<std::size_t>(resident_pages) * static_cast<std::size_t>(page_size);
#else
        throw std::runtime_error("Working-set measurement is supported on Windows and Linux");
#endif
    }

    [[nodiscard]] std::size_t positiveDelta(std::size_t after, std::size_t before) noexcept
    {
        return after > before ? after - before : 0u;
    }

    [[nodiscard]] std::string createBinaryPointDocument(int element_count)
    {
        const houio::HouGeo::Ptr geometry = houio::HouGeo::create();
        const houio::Attribute::Ptr positions = houio::Attribute::createV4f(element_count);
        const houio::Attribute::Ptr identifiers = houio::Attribute::createInt(element_count);
        for (int index = 0; index < element_count; ++index)
        {
            const float value = static_cast<float>(index) * 0.001f;
            positions->set<houio::math::V4f>(
                static_cast<unsigned int>(index),
                houio::math::V4f(value, value + 1.0f, value + 2.0f, 1.0f));
            identifiers->set<houio::sint32>(static_cast<unsigned int>(index), index);
        }
        geometry->setPointAttribute(
            std::make_shared<houio::HouGeo::HouAttribute>("P", positions));
        geometry->setPointAttribute(
            std::make_shared<houio::HouGeo::HouAttribute>("id", identifiers));

        std::ostringstream output(std::ios::out | std::ios::binary);
        if (!houio::HouGeoIO::exportGeometry(output, geometry, true))
            throw std::runtime_error("Could not generate the memory-probe geometry document");
        return output.str();
    }

    struct ProbeResult
    {
        std::size_t input_bytes = 0;
        std::size_t json_tree_delta_bytes = 0;
        std::size_t hougeo_delta_bytes = 0;
        std::size_t combined_extra_bytes = 0;
        double parsed_stage_amplification = 0.0;
        double loaded_stage_amplification = 0.0;
        std::uint64_t checksum = 0;
    };

    [[nodiscard]] ProbeResult runProbe(const Options& options)
    {
        const std::string input_bytes = createBinaryPointDocument(options.elements);
        if (input_bytes.empty())
            throw std::runtime_error("The generated memory-probe document is empty");

        ProbeResult result;
        result.input_bytes = input_bytes.size();
        const std::size_t baseline_working_set = currentWorkingSetBytes();

        std::istringstream input(input_bytes, std::ios::in | std::ios::binary);
        houio::json::JSONReader reader;
        houio::json::Parser parser;
        if (!parser.parse(input, reader))
            throw std::runtime_error("The memory-probe JSON parser rejected its generated document");
        houio::json::Value root = reader.root();
        const houio::json::ArrayPtr root_array = root.asArray();
        if (!root_array)
            throw std::runtime_error("The memory-probe parser did not produce a root array");
        const std::size_t parsed_working_set = currentWorkingSetBytes();

        const houio::HouGeo::Ptr geometry = houio::HouGeo::create();
        geometry->load(houio::HouGeo::toObject(root_array));
        const std::size_t loaded_working_set = currentWorkingSetBytes();

        const auto positions = geometry->pointAttribute("P");
        const auto identifiers = geometry->pointAttribute("id");
        if (!positions || !identifiers || geometry->pointCount() != options.elements)
            throw std::runtime_error("The memory-probe semantic load lost point attributes");
        const houio::HouGeoAdapter::RawDataView position_data = positions->rawData();
        const houio::HouGeoAdapter::RawDataView identifier_data = identifiers->rawData();
        if (!position_data.available() || !identifier_data.available())
            throw std::runtime_error("The memory-probe semantic attribute data is unavailable");
        const std::size_t last = static_cast<std::size_t>(options.elements - 1);
        result.checksum = static_cast<std::uint64_t>(identifier_data.read<houio::sint32>(last))
            + static_cast<std::uint64_t>(position_data.read<houio::real32>(last * 4u) * 1000.0f);

        result.json_tree_delta_bytes = positiveDelta(parsed_working_set, baseline_working_set);
        result.hougeo_delta_bytes = positiveDelta(loaded_working_set, parsed_working_set);
        if (result.json_tree_delta_bytes > std::numeric_limits<std::size_t>::max()
                - result.hougeo_delta_bytes)
        {
            throw std::overflow_error("Combined memory-probe delta exceeds size_t range");
        }
        result.combined_extra_bytes = result.json_tree_delta_bytes + result.hougeo_delta_bytes;
        const double input_size = static_cast<double>(result.input_bytes);
        result.parsed_stage_amplification =
            (input_size + static_cast<double>(result.json_tree_delta_bytes)) / input_size;
        result.loaded_stage_amplification =
            (input_size + static_cast<double>(result.combined_extra_bytes)) / input_size;
        return result;
    }

    void printResult(const Options& options, const ProbeResult& result)
    {
        if (options.csv)
        {
            std::cout
                << "elements,input_bytes,json_tree_delta_bytes,hougeo_delta_bytes,"
                << "combined_extra_bytes,parsed_stage_amplification,loaded_stage_amplification,checksum\n"
                << options.elements << ',' << result.input_bytes << ','
                << result.json_tree_delta_bytes << ',' << result.hougeo_delta_bytes << ','
                << result.combined_extra_bytes << ',' << std::fixed << std::setprecision(3)
                << result.parsed_stage_amplification << ',' << result.loaded_stage_amplification << ','
                << result.checksum << '\n';
            return;
        }

        std::cout
            << "HouIO memory amplification probe\n"
            << "  elements:                    " << options.elements << '\n'
            << "  input bytes:                 " << result.input_bytes << '\n'
            << "  JSON tree working-set delta: " << result.json_tree_delta_bytes << '\n'
            << "  HouGeo working-set delta:    " << result.hougeo_delta_bytes << '\n'
            << "  combined extra bytes:        " << result.combined_extra_bytes << '\n'
            << std::fixed << std::setprecision(3)
            << "  parsed-stage amplification:  " << result.parsed_stage_amplification << "x\n"
            << "  loaded-stage amplification:  " << result.loaded_stage_amplification << "x\n"
            << "  checksum:                     " << result.checksum << '\n';
    }
}

int main(int argument_count, char** arguments)
{
    try
    {
        const Options options = parseOptions(argument_count, arguments);
        printResult(options, runProbe(options));
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "houio_memory_probe: " << exception.what() << '\n';
        return 1;
    }
}
