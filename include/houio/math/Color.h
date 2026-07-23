#pragma once

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
            const float& red,
            const float& green,
            const float& blue,
            const float& alpha = 1.0f) noexcept
            : r(red), g(green), b(blue), a(alpha)
        {
        }

        ~Color() = default;

        [[nodiscard]] static constexpr Color White() noexcept
        {
            return Color(1.0f, 1.0f, 1.0f);
        }

        [[nodiscard]] static constexpr Color Black() noexcept
        {
            return Color(0.0f, 0.0f, 0.0f);
        }

        [[nodiscard]] static constexpr Color Blue() noexcept
        {
            return Color(0.0f, 0.0f, 1.0f);
        }

        [[nodiscard]] static constexpr Color Yellow() noexcept
        {
            return Color(1.0f, 1.0f, 0.0f);
        }

        [[nodiscard]] static constexpr Color Green() noexcept
        {
            return Color(0.0f, 1.0f, 0.0f);
        }

        [[nodiscard]] static constexpr Color Red() noexcept
        {
            return Color(1.0f, 0.0f, 0.0f);
        }

        [[nodiscard]] static Color From255(
            const unsigned char& red,
            const unsigned char& green,
            const unsigned char& blue,
            const unsigned char& alpha = 255) noexcept;

        constexpr void set(
            const float& red,
            const float& green,
            const float& blue,
            const float& alpha = 1.0f) noexcept
        {
            r = red;
            g = green;
            b = blue;
            a = alpha;
        }

        void clamp() noexcept;
        constexpr void invert() noexcept
        {
            r = 1.0f - r;
            g = 1.0f - g;
            b = 1.0f - b;
            a = 1.0f - a;
        }

        [[nodiscard]] unsigned long makeDWORD();

        [[nodiscard]] constexpr bool operator==(const Color& rhs) const noexcept
        {
            return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a;
        }

        [[nodiscard]] constexpr bool operator!=(const Color& rhs) const noexcept
        {
            return !(*this == rhs);
        }

        constexpr bool operator+=(const Color& rhs) noexcept
        {
            r += rhs.r;
            g += rhs.g;
            b += rhs.b;
            a += rhs.a;
            return true;
        }

        constexpr bool operator-=(const Color& rhs) noexcept
        {
            r -= rhs.r;
            g -= rhs.g;
            b -= rhs.b;
            a -= rhs.a;
            return true;
        }

        constexpr bool operator*=(const Color& rhs) noexcept
        {
            r *= rhs.r;
            g *= rhs.g;
            b *= rhs.b;
            a *= rhs.a;
            return true;
        }

        constexpr bool operator+=(const float& rhs) noexcept
        {
            r += rhs;
            g += rhs;
            b += rhs;
            a += rhs;
            return true;
        }

        constexpr bool operator-=(const float& rhs) noexcept
        {
            r -= rhs;
            g -= rhs;
            b -= rhs;
            a -= rhs;
            return true;
        }

        constexpr bool operator*=(const float& rhs) noexcept
        {
            r *= rhs;
            g *= rhs;
            b *= rhs;
            a *= rhs;
            return true;
        }

        constexpr bool operator/=(const float& rhs)
        {
            r /= rhs;
            g /= rhs;
            b /= rhs;
            a /= rhs;
            return true;
        }

        [[nodiscard]] const float& operator[](int index) const;
        [[nodiscard]] float& operator[](int index);
    };
}
