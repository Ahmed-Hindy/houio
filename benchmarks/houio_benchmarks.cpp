#include <houio/Attribute.h>
#include <houio/Geometry.h>
#include <houio/HouGeo.h>
#include <houio/HouGeoIO.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    struct Options
    {
        int iterations = 5;
        int attribute_elements = 250000;
        int grid_resolution = 256;
        int volume_resolution = 96;
        bool csv = false;
    };

    struct BenchmarkResult
    {
        std::string name;
        double median_milliseconds = 0.0;
        double work_units = 0.0;
        std::string unit_name;
        double checksum = 0.0;
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
                    << "Usage: houio_benchmarks [options]\n"
                    << "  --iterations N          Repetitions per benchmark (default 5)\n"
                    << "  --attribute-elements N  Vec3f attribute elements (default 250000)\n"
                    << "  --grid-resolution N     Triangle-grid x/z resolution (default 256)\n"
                    << "  --volume-resolution N   Cubic dense-volume resolution (default 96)\n"
                    << "  --csv                   Emit CSV output\n";
                std::exit(0);
            }

            const auto readValue = [&](std::string_view option_name) -> int
            {
                if (index + 1 >= argument_count)
                    throw std::invalid_argument(std::string(option_name) + " requires a value");
                ++index;
                return parsePositiveInt(arguments[index], option_name);
            };

            if (argument == "--iterations")
                options.iterations = readValue(argument);
            else if (argument == "--attribute-elements")
                options.attribute_elements = readValue(argument);
            else if (argument == "--grid-resolution")
                options.grid_resolution = readValue(argument);
            else if (argument == "--volume-resolution")
                options.volume_resolution = readValue(argument);
            else
                throw std::invalid_argument("Unknown benchmark option: " + std::string(argument));
        }
        if (options.grid_resolution < 2)
            throw std::invalid_argument("--grid-resolution must be at least 2");
        return options;
    }

    template<typename Function>
    [[nodiscard]] double medianMilliseconds(int iterations, Function&& function, double& checksum)
    {
        std::vector<double> samples;
        samples.reserve(static_cast<std::size_t>(iterations));
        double observed_checksum = 0.0;
        for (int iteration = 0; iteration < iterations; ++iteration)
        {
            const auto start = Clock::now();
            observed_checksum += function();
            const auto end = Clock::now();
            samples.push_back(std::chrono::duration<double, std::milli>(end - start).count());
        }
        std::sort(samples.begin(), samples.end());
        checksum = observed_checksum;
        const std::size_t middle = samples.size() / 2u;
        if ((samples.size() % 2u) == 0u)
            return (samples[middle - 1u] + samples[middle]) * 0.5;
        return samples[middle];
    }

    [[nodiscard]] BenchmarkResult benchmarkNumericAttribute(const Options& options)
    {
        BenchmarkResult result;
        result.name = "numeric_attribute_write_read";
        result.work_units = static_cast<double>(options.attribute_elements) * 2.0;
        result.unit_name = "elements";
        result.median_milliseconds = medianMilliseconds(
            options.iterations,
            [&options]()
            {
                const houio::Attribute::Ptr attribute =
                    houio::Attribute::createV3f(options.attribute_elements);
                for (int index = 0; index < options.attribute_elements; ++index)
                {
                    const float value = static_cast<float>(index) * 0.25f;
                    attribute->set<houio::math::V3f>(
                        static_cast<unsigned int>(index),
                        houio::math::V3f(value, value + 1.0f, value + 2.0f));
                }

                double checksum = 0.0;
                for (int index = 0; index < options.attribute_elements; ++index)
                {
                    const houio::math::V3f value =
                        attribute->get<houio::math::V3f>(static_cast<unsigned int>(index));
                    checksum += static_cast<double>(value.x + value.y + value.z);
                }
                return checksum;
            },
            result.checksum);
        return result;
    }

    [[nodiscard]] BenchmarkResult benchmarkTopology(const Options& options)
    {
        BenchmarkResult result;
        result.name = "triangle_grid_generate_traverse";
        const double cells = static_cast<double>(options.grid_resolution - 1)
            * static_cast<double>(options.grid_resolution - 1);
        result.work_units = cells * 6.0;
        result.unit_name = "indices";
        result.median_milliseconds = medianMilliseconds(
            options.iterations,
            [&options]()
            {
                const houio::Geometry::Ptr geometry = houio::Geometry::createGrid(
                    options.grid_resolution,
                    options.grid_resolution,
                    houio::Geometry::PrimitiveType::triangle);
                std::uint64_t checksum = geometry->primitiveCount();
                for (const houio::Geometry::Index index : geometry->indexBuffer())
                    checksum += index;
                return static_cast<double>(checksum);
            },
            result.checksum);
        return result;
    }

    [[nodiscard]] std::string makeConstantVolumeDocument(int resolution)
    {
        std::ostringstream source;
        source
            << "[\"pointcount\",1,\"vertexcount\",1,\"primitivecount\",1,"
            << "\"topology\",[\"pointref\",[\"indices\",[0]]],"
            << "\"attributes\",[\"pointattributes\",[["
            << "[\"scope\",\"public\",\"type\",\"numeric\",\"name\",\"P\"],"
            << "[\"size\",3,\"storage\",\"fpreal32\",\"values\",["
            << "\"size\",3,\"storage\",\"fpreal32\",\"arrays\",[[0.0],[0.0],[0.0]]]]]]],"
            << "\"primitives\",[[[\"type\",\"Volume\"],["
            << "\"vertex\",0,\"transform\",[1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0],"
            << "\"res\",[" << resolution << ',' << resolution << ',' << resolution << "],"
            << "\"voxels\",[\"constantarray\",1.25]]]]]";
        return source.str();
    }

    [[nodiscard]] BenchmarkResult benchmarkDenseVolumeImport(const Options& options)
    {
        BenchmarkResult result;
        result.name = "dense_constant_volume_import";
        const double resolution = static_cast<double>(options.volume_resolution);
        result.work_units = resolution * resolution * resolution;
        result.unit_name = "voxels";
        const std::string source_text = makeConstantVolumeDocument(options.volume_resolution);
        result.median_milliseconds = medianMilliseconds(
            options.iterations,
            [&source_text, &options]()
            {
                std::istringstream source(source_text);
                houio::DiagnosticList diagnostics;
                const houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(source, &diagnostics);
                if (!geometry)
                {
                    const std::string message = diagnostics.empty()
                        ? "unknown import failure"
                        : diagnostics.back().message;
                    throw std::runtime_error("Dense-volume benchmark import failed: " + message);
                }
                const std::vector<houio::HouGeoAdapter::Primitive::Ptr> primitives =
                    geometry->primitives();
                if (primitives.size() != 1u)
                    throw std::runtime_error("Dense-volume benchmark imported an unexpected primitive count");
                const auto volume =
                    std::dynamic_pointer_cast<houio::HouGeo::HouVolume>(primitives.front());
                if (!volume)
                    throw std::runtime_error("Dense-volume benchmark did not import a volume");
                const int last = options.volume_resolution - 1;
                return static_cast<double>(volume->voxelValue(0, 0, 0))
                    + static_cast<double>(volume->voxelValue(last, last, last));
            },
            result.checksum);
        return result;
    }

    void printResults(const Options& options, const std::vector<BenchmarkResult>& results)
    {
        if (options.csv)
        {
            std::cout << "benchmark,median_ms,work_units,unit,units_per_second,checksum\n";
            for (const BenchmarkResult& result : results)
            {
                const double units_per_second = result.median_milliseconds > 0.0
                    ? result.work_units * 1000.0 / result.median_milliseconds
                    : 0.0;
                std::cout << result.name << ',' << std::fixed << std::setprecision(3)
                    << result.median_milliseconds << ',' << std::setprecision(0)
                    << result.work_units << ',' << result.unit_name << ','
                    << std::setprecision(3) << units_per_second << ','
                    << std::setprecision(6) << result.checksum << '\n';
            }
            return;
        }

        std::cout
            << "HouIO benchmark configuration\n"
            << "  iterations: " << options.iterations << '\n'
            << "  attribute elements: " << options.attribute_elements << '\n'
            << "  grid resolution: " << options.grid_resolution << " x "
            << options.grid_resolution << '\n'
            << "  volume resolution: " << options.volume_resolution << "^3\n\n";

        for (const BenchmarkResult& result : results)
        {
            const double units_per_second = result.median_milliseconds > 0.0
                ? result.work_units * 1000.0 / result.median_milliseconds
                : 0.0;
            std::cout << std::left << std::setw(34) << result.name
                << std::right << std::fixed << std::setprecision(3)
                << std::setw(12) << result.median_milliseconds << " ms  "
                << std::setprecision(0) << std::setw(14) << units_per_second << ' '
                << result.unit_name << "/s  checksum=" << std::setprecision(6)
                << result.checksum << '\n';
        }
    }
}

int main(int argument_count, char** arguments)
{
    try
    {
        const Options options = parseOptions(argument_count, arguments);
        std::vector<BenchmarkResult> results;
        results.reserve(3);
        results.push_back(benchmarkNumericAttribute(options));
        results.push_back(benchmarkTopology(options));
        results.push_back(benchmarkDenseVolumeImport(options));
        printResults(options, results);
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "houio_benchmarks: " << exception.what() << '\n';
        return 1;
    }
}
