#pragma once

#include <cmath>
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
        constexpr Vec4(
            const T& x_value,
            const T& y_value,
            const T& z_value,
            const T& w_value) noexcept
            : x(x_value), y(y_value), z(z_value), w(w_value)
        {
        }

        explicit constexpr Vec4(const T& value) noexcept
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

        [[nodiscard]] constexpr const T& operator[](int index) const
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

        [[nodiscard]] constexpr T& operator[](int index)
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
