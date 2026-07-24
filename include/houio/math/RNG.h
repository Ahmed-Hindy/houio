#pragma once

#include <cstdint>
#include <random>

namespace houio::math
{
    class RandomGenerator
    {
    public:
        using result_type = std::uint32_t;

        explicit RandomGenerator(result_type seed_value = 5489u) noexcept
            : engine_(seed_value)
        {
        }

        void seed(result_type seed_value) noexcept
        {
            engine_.seed(seed_value);
        }

        [[nodiscard]] result_type nextUInt() noexcept
        {
            return engine_();
        }

        [[nodiscard]] float uniform01() noexcept
        {
            constexpr float reciprocal = 1.0f / 16777216.0f;
            return static_cast<float>(nextUInt() & 0x00ffffffu) * reciprocal;
        }

        [[nodiscard]] float operator()() noexcept
        {
            return uniform01();
        }

    private:
        std::mt19937 engine_;
    };
}
