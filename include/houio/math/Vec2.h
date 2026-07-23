#pragma once

#include <cmath>
#include <cstddef>
#include <ostream>
#include <stdexcept>
#include <type_traits>

namespace houio::math
{
    template<typename T>
    struct Vec2
    {
        T x{};
        T y{};

        constexpr Vec2() noexcept = default;
        constexpr Vec2(const T& x_value, const T& y_value) noexcept
            : x(x_value), y(y_value)
        {
        }
        explicit constexpr Vec2(const T& value) noexcept
            : x(value), y(value)
        {
        }

        [[nodiscard]] T getLength() const
        {
            using std::sqrt;
            return static_cast<T>(sqrt(x * x + y * y));
        }

        [[nodiscard]] constexpr T getSquaredLength() const noexcept
        {
            return x * x + y * y;
        }

        void setLength(const T& length)
        {
            normalize();
            x *= length;
            y *= length;
        }

        void normalize()
        {
            const T length = getLength();
            if (length != T{})
            {
                x /= length;
                y /= length;
            }
        }

        [[nodiscard]] Vec2 normalized() const
        {
            const T length = getLength();
            return length == T{} ? Vec2{} : Vec2(x / length, y / length);
        }

        constexpr void negate() noexcept
        {
            x = -x;
            y = -y;
        }

        [[nodiscard]] constexpr bool operator==(const Vec2& rhs) const noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                constexpr T tolerance = static_cast<T>(0.00001);
                return std::abs(x - rhs.x) < tolerance
                    && std::abs(y - rhs.y) < tolerance;
            }
            return x == rhs.x && y == rhs.y;
        }

        [[nodiscard]] constexpr bool operator!=(const Vec2& rhs) const noexcept
        {
            return !(*this == rhs);
        }

        constexpr bool operator+=(const Vec2& rhs) noexcept
        {
            x += rhs.x;
            y += rhs.y;
            return true;
        }

        constexpr bool operator-=(const Vec2& rhs) noexcept
        {
            x -= rhs.x;
            y -= rhs.y;
            return true;
        }

        constexpr bool operator+=(const T& rhs) noexcept
        {
            x += rhs;
            y += rhs;
            return true;
        }

        constexpr bool operator-=(const T& rhs) noexcept
        {
            x -= rhs;
            y -= rhs;
            return true;
        }

        constexpr bool operator*=(const T& rhs) noexcept
        {
            x *= rhs;
            y *= rhs;
            return true;
        }

        constexpr bool operator/=(const T& rhs)
        {
            x /= rhs;
            y /= rhs;
            return true;
        }

        [[nodiscard]] constexpr const T& operator[](int index) const
        {
            if (index == 0)
                return x;
            if (index == 1)
                return y;
            throw std::out_of_range("Vec2 index is out of range");
        }

        [[nodiscard]] constexpr T& operator[](int index)
        {
            if (index == 0)
                return x;
            if (index == 1)
                return y;
            throw std::out_of_range("Vec2 index is out of range");
        }
    };

    template<typename T>
    std::ostream& operator<<(std::ostream& stream, const Vec2<T>& value)
    {
        return stream << '(' << value.x << ' ' << value.y << ')';
    }

    using Vec2f = Vec2<float>;
    using Vec2d = Vec2<double>;
    using Vec2i = Vec2<int>;
    using V2f = Vec2<float>;
    using V2d = Vec2<double>;
    using V2i = Vec2<int>;
}
