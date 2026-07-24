#pragma once

#include <cmath>
#include <cstddef>
#include <ostream>
#include <stdexcept>
#include <type_traits>

namespace houio::math
{
    template<typename T>
    struct Vec4
    {
        T x{};
        T y{};
        T z{};
        T w{};

        constexpr Vec4() noexcept = default;

        constexpr Vec4(T x_value, T y_value, T z_value, T w_value) noexcept
            : x(x_value), y(y_value), z(z_value), w(w_value)
        {
        }

        explicit constexpr Vec4(T value) noexcept
            : x(value), y(value), z(value), w(value)
        {
        }

        template<typename S>
        constexpr Vec4(const Vec4<S>& value) noexcept
            : x(static_cast<T>(value.x)),
              y(static_cast<T>(value.y)),
              z(static_cast<T>(value.z)),
              w(static_cast<T>(value.w))
        {
        }

        constexpr void set(T x_value, T y_value, T z_value, T w_value) noexcept
        {
            x = x_value;
            y = y_value;
            z = z_value;
            w = w_value;
        }

        [[nodiscard]] T length() const
        {
            using std::sqrt;
            return static_cast<T>(sqrt(squaredLength()));
        }

        [[nodiscard]] constexpr T squaredLength() const noexcept
        {
            return x * x + y * y + z * z + w * w;
        }

        void setLength(T requested_length)
        {
            normalize();
            *this *= requested_length;
        }

        void normalize()
        {
            const T current_length = length();
            if (current_length != T{})
                *this /= current_length;
        }

        [[nodiscard]] Vec4 normalized() const
        {
            const T current_length = length();
            return current_length == T{} ? Vec4{} : Vec4(
                x / current_length,
                y / current_length,
                z / current_length,
                w / current_length);
        }

        constexpr void negate() noexcept
        {
            x = -x;
            y = -y;
            z = -z;
            w = -w;
        }

        [[nodiscard]] constexpr bool operator==(const Vec4& rhs) const noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                constexpr T tolerance = static_cast<T>(0.00001);
                return std::abs(x - rhs.x) < tolerance
                    && std::abs(y - rhs.y) < tolerance
                    && std::abs(z - rhs.z) < tolerance
                    && std::abs(w - rhs.w) < tolerance;
            }
            return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
        }

        [[nodiscard]] constexpr bool operator!=(const Vec4& rhs) const noexcept
        {
            return !(*this == rhs);
        }

        constexpr Vec4& operator+=(const Vec4& rhs) noexcept
        {
            x += rhs.x;
            y += rhs.y;
            z += rhs.z;
            w += rhs.w;
            return *this;
        }

        constexpr Vec4& operator-=(const Vec4& rhs) noexcept
        {
            x -= rhs.x;
            y -= rhs.y;
            z -= rhs.z;
            w -= rhs.w;
            return *this;
        }

        constexpr Vec4& operator+=(T rhs) noexcept
        {
            x += rhs;
            y += rhs;
            z += rhs;
            w += rhs;
            return *this;
        }

        constexpr Vec4& operator-=(T rhs) noexcept
        {
            x -= rhs;
            y -= rhs;
            z -= rhs;
            w -= rhs;
            return *this;
        }

        constexpr Vec4& operator*=(T rhs) noexcept
        {
            x *= rhs;
            y *= rhs;
            z *= rhs;
            w *= rhs;
            return *this;
        }

        constexpr Vec4& operator/=(T rhs)
        {
            x /= rhs;
            y /= rhs;
            z /= rhs;
            w /= rhs;
            return *this;
        }

        [[nodiscard]] constexpr const T& operator[](std::size_t index) const
        {
            if (index == 0)
                return x;
            if (index == 1)
                return y;
            if (index == 2)
                return z;
            if (index == 3)
                return w;
            throw std::out_of_range("Vec4 index is out of range");
        }

        [[nodiscard]] constexpr T& operator[](std::size_t index)
        {
            if (index == 0)
                return x;
            if (index == 1)
                return y;
            if (index == 2)
                return z;
            if (index == 3)
                return w;
            throw std::out_of_range("Vec4 index is out of range");
        }
    };

    template<typename T>
    std::ostream& operator<<(std::ostream& stream, const Vec4<T>& value)
    {
        return stream << '(' << value.x << ' ' << value.y << ' '
                      << value.z << ' ' << value.w << ')';
    }

    using Vec4f = Vec4<float>;
    using V4f = Vec4<float>;
    using Vec4d = Vec4<double>;
    using V4d = Vec4<double>;
    using Vec4i = Vec4<int>;
    using V4i = Vec4<int>;
}
