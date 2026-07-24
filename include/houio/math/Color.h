#pragma once

#include <cstddef>
#include <cstdint>

namespace houio::math
{
    class Color
    {
    public:
        float r{};
        float g{};
        float b{};
        float a{};

        constexpr Color() noexcept = default;
        constexpr Color(
            float red,
            float green,
            float blue,
            float alpha = 1.0f) noexcept
            : r(red), g(green), b(blue), a(alpha)
        {
        }

        [[nodiscard]] static constexpr Color white() noexcept
        {
            return Color(1.0f, 1.0f, 1.0f);
        }

        [[nodiscard]] static constexpr Color black() noexcept
        {
            return Color(0.0f, 0.0f, 0.0f);
        }

        [[nodiscard]] static constexpr Color blue() noexcept
        {
            return Color(0.0f, 0.0f, 1.0f);
        }

        [[nodiscard]] static constexpr Color yellow() noexcept
        {
            return Color(1.0f, 1.0f, 0.0f);
        }

        [[nodiscard]] static constexpr Color green() noexcept
        {
            return Color(0.0f, 1.0f, 0.0f);
        }

        [[nodiscard]] static constexpr Color red() noexcept
        {
            return Color(1.0f, 0.0f, 0.0f);
        }

        [[nodiscard]] static constexpr Color fromBytes(
            std::uint8_t red,
            std::uint8_t green,
            std::uint8_t blue,
            std::uint8_t alpha = 255) noexcept
        {
            constexpr float scale = 1.0f / 255.0f;
            return Color(
                static_cast<float>(red) * scale,
                static_cast<float>(green) * scale,
                static_cast<float>(blue) * scale,
                static_cast<float>(alpha) * scale);
        }

        constexpr void set(
            float red,
            float green,
            float blue,
            float alpha = 1.0f) noexcept
        {
            r = red;
            g = green;
            b = blue;
            a = alpha;
        }

        void clamp() noexcept;

        [[nodiscard]] Color clamped() const noexcept
        {
            Color result = *this;
            result.clamp();
            return result;
        }

        constexpr void invertRgb() noexcept
        {
            r = 1.0f - r;
            g = 1.0f - g;
            b = 1.0f - b;
        }

        [[nodiscard]] constexpr Color invertedRgb() const noexcept
        {
            Color result = *this;
            result.invertRgb();
            return result;
        }

        [[nodiscard]] std::uint32_t packedRgba() const noexcept;

        [[nodiscard]] constexpr bool operator==(const Color& rhs) const noexcept
        {
            return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a;
        }

        [[nodiscard]] constexpr bool operator!=(const Color& rhs) const noexcept
        {
            return !(*this == rhs);
        }

        constexpr Color& operator+=(const Color& rhs) noexcept
        {
            r += rhs.r;
            g += rhs.g;
            b += rhs.b;
            a += rhs.a;
            return *this;
        }

        constexpr Color& operator-=(const Color& rhs) noexcept
        {
            r -= rhs.r;
            g -= rhs.g;
            b -= rhs.b;
            a -= rhs.a;
            return *this;
        }

        constexpr Color& operator*=(const Color& rhs) noexcept
        {
            r *= rhs.r;
            g *= rhs.g;
            b *= rhs.b;
            a *= rhs.a;
            return *this;
        }

        constexpr Color& operator+=(float value) noexcept
        {
            r += value;
            g += value;
            b += value;
            a += value;
            return *this;
        }

        constexpr Color& operator-=(float value) noexcept
        {
            r -= value;
            g -= value;
            b -= value;
            a -= value;
            return *this;
        }

        constexpr Color& operator*=(float value) noexcept
        {
            r *= value;
            g *= value;
            b *= value;
            a *= value;
            return *this;
        }

        constexpr Color& operator/=(float value)
        {
            r /= value;
            g /= value;
            b /= value;
            a /= value;
            return *this;
        }

        [[nodiscard]] const float& operator[](std::size_t index) const;
        [[nodiscard]] float& operator[](std::size_t index);
    };

    [[nodiscard]] constexpr Color operator+(Color lhs, const Color& rhs) noexcept
    {
        return lhs += rhs;
    }

    [[nodiscard]] constexpr Color operator-(Color lhs, const Color& rhs) noexcept
    {
        return lhs -= rhs;
    }

    [[nodiscard]] constexpr Color operator*(Color lhs, const Color& rhs) noexcept
    {
        return lhs *= rhs;
    }

    [[nodiscard]] constexpr Color operator/(Color lhs, const Color& rhs)
    {
        lhs.r /= rhs.r;
        lhs.g /= rhs.g;
        lhs.b /= rhs.b;
        lhs.a /= rhs.a;
        return lhs;
    }

    [[nodiscard]] constexpr Color operator+(Color lhs, float rhs) noexcept
    {
        return lhs += rhs;
    }

    [[nodiscard]] constexpr Color operator-(Color lhs, float rhs) noexcept
    {
        return lhs -= rhs;
    }

    [[nodiscard]] constexpr Color operator*(Color lhs, float rhs) noexcept
    {
        return lhs *= rhs;
    }

    [[nodiscard]] constexpr Color operator/(Color lhs, float rhs)
    {
        return lhs /= rhs;
    }

    [[nodiscard]] constexpr Color operator+(float lhs, Color rhs) noexcept
    {
        return rhs += lhs;
    }

    [[nodiscard]] constexpr Color operator-(float lhs, const Color& rhs) noexcept
    {
        return Color(lhs - rhs.r, lhs - rhs.g, lhs - rhs.b, lhs - rhs.a);
    }

    [[nodiscard]] constexpr Color operator*(float lhs, Color rhs) noexcept
    {
        return rhs *= lhs;
    }

    [[nodiscard]] constexpr Color operator/(float lhs, const Color& rhs)
    {
        return Color(lhs / rhs.r, lhs / rhs.g, lhs / rhs.b, lhs / rhs.a);
    }
}
