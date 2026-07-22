#pragma once

#include <cmath>
#include <type_traits>

namespace houio::math
{
    template<typename T>
    struct Vec4
    {
        constexpr Vec4() : x{}, y{}, z{}, w{} {}

        constexpr Vec4(const T& xValue, const T& yValue, const T& zValue, const T& wValue)
            : x(xValue), y(yValue), z(zValue), w(wValue)
        {
        }

        constexpr Vec4(const T& value) : x(value), y(value), z(value), w(value) {}

        template<typename S>
        constexpr Vec4(const Vec4<S>& value)
            : x(static_cast<T>(value.x)),
              y(static_cast<T>(value.y)),
              z(static_cast<T>(value.z)),
              w(static_cast<T>(value.w))
        {
        }

        bool operator==(const Vec4<T>& rhs) const
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

        bool operator!=(const Vec4<T>& rhs) const
        {
            return !(*this == rhs);
        }

        union
        {
            struct
            {
                T x;
                T y;
                T z;
                T w;
            };
            T v[4];
        };
    };

    using Vec4f = Vec4<float>;
    using V4f = Vec4<float>;
    using Vec4d = Vec4<double>;
    using V4d = Vec4<double>;
    using Vec4i = Vec4<int>;
    using V4i = Vec4<int>;
}
