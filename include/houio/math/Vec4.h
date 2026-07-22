#pragma once

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
