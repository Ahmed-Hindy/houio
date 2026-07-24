#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

#include <houio/math/Vec3.h>

namespace houio::math
{
    template<typename T>
    class Matrix33
    {
    public:
        static constexpr std::size_t dimension = 3;
        std::array<T, dimension * dimension> ma{};

        constexpr Matrix33() noexcept = default;

        constexpr Matrix33(
            T value00,
            T value01,
            T value02,
            T value10,
            T value11,
            T value12,
            T value20,
            T value21,
            T value22) noexcept
            : ma{
                  value00, value01, value02,
                  value10, value11, value12,
                  value20, value21, value22}
        {
        }

        [[nodiscard]] static constexpr Matrix33 zero() noexcept
        {
            return Matrix33{};
        }

        [[nodiscard]] static constexpr Matrix33 identity() noexcept
        {
            return Matrix33(
                T{1}, T{}, T{},
                T{}, T{1}, T{},
                T{}, T{}, T{1});
        }

        [[nodiscard]] static Matrix33 rotation(const Vec3<T>& axis, T angle)
            requires std::floating_point<T>
        {
            const T axis_length_squared = axis.squaredLength();
            if (axis_length_squared <= std::numeric_limits<T>::epsilon())
                throw std::invalid_argument("Matrix33 rotation requires a non-zero axis");

            const Vec3<T> normalized_axis = axis.normalized();
            using std::cos;
            using std::sin;
            const T cosine = static_cast<T>(cos(angle));
            const T sine = static_cast<T>(sin(angle));
            const T one_minus_cosine = T{1} - cosine;
            const T xy = one_minus_cosine * normalized_axis.x * normalized_axis.y;
            const T xz = one_minus_cosine * normalized_axis.x * normalized_axis.z;
            const T yz = one_minus_cosine * normalized_axis.y * normalized_axis.z;
            const T sx = sine * normalized_axis.x;
            const T sy = sine * normalized_axis.y;
            const T sz = sine * normalized_axis.z;
            return Matrix33(
                one_minus_cosine * normalized_axis.x * normalized_axis.x + cosine,
                xy - sz,
                xz + sy,
                xy + sz,
                one_minus_cosine * normalized_axis.y * normalized_axis.y + cosine,
                yz - sx,
                xz - sy,
                yz + sx,
                one_minus_cosine * normalized_axis.z * normalized_axis.z + cosine);
        }

        [[nodiscard]] constexpr T& operator()(std::size_t row, std::size_t column)
        {
            return ma.at(row * dimension + column);
        }

        [[nodiscard]] constexpr const T& operator()(
            std::size_t row,
            std::size_t column) const
        {
            return ma.at(row * dimension + column);
        }

        constexpr void transpose() noexcept
        {
            std::swap(ma[1], ma[3]);
            std::swap(ma[2], ma[6]);
            std::swap(ma[5], ma[7]);
        }

        [[nodiscard]] constexpr Matrix33 transposed() const noexcept
        {
            Matrix33 result = *this;
            result.transpose();
            return result;
        }

        [[nodiscard]] constexpr T trace() const noexcept
        {
            return ma[0] + ma[4] + ma[8];
        }

        [[nodiscard]] constexpr T determinant() const noexcept
        {
            return ma[0] * (ma[4] * ma[8] - ma[5] * ma[7])
                - ma[1] * (ma[3] * ma[8] - ma[5] * ma[6])
                + ma[2] * (ma[3] * ma[7] - ma[4] * ma[6]);
        }

        void invert()
            requires std::floating_point<T>
        {
            using std::abs;
            const T scale = std::max({
                T{1},
                static_cast<T>(abs(ma[0])), static_cast<T>(abs(ma[1])),
                static_cast<T>(abs(ma[2])), static_cast<T>(abs(ma[3])),
                static_cast<T>(abs(ma[4])), static_cast<T>(abs(ma[5])),
                static_cast<T>(abs(ma[6])), static_cast<T>(abs(ma[7])),
                static_cast<T>(abs(ma[8])),
            });
            const T determinant_value = determinant();
            const T tolerance = std::numeric_limits<T>::epsilon()
                * scale * scale * scale * T{64};
            if (abs(determinant_value) <= tolerance)
                throw std::domain_error("Matrix33 is singular");

            const T reciprocal = T{1} / determinant_value;
            const Matrix33 source = *this;
            ma = {
                (source.ma[4] * source.ma[8] - source.ma[5] * source.ma[7]) * reciprocal,
                (source.ma[2] * source.ma[7] - source.ma[1] * source.ma[8]) * reciprocal,
                (source.ma[1] * source.ma[5] - source.ma[2] * source.ma[4]) * reciprocal,
                (source.ma[5] * source.ma[6] - source.ma[3] * source.ma[8]) * reciprocal,
                (source.ma[0] * source.ma[8] - source.ma[2] * source.ma[6]) * reciprocal,
                (source.ma[2] * source.ma[3] - source.ma[0] * source.ma[5]) * reciprocal,
                (source.ma[3] * source.ma[7] - source.ma[4] * source.ma[6]) * reciprocal,
                (source.ma[1] * source.ma[6] - source.ma[0] * source.ma[7]) * reciprocal,
                (source.ma[0] * source.ma[4] - source.ma[1] * source.ma[3]) * reciprocal,
            };
        }

        [[nodiscard]] Matrix33 inverted() const
            requires std::floating_point<T>
        {
            Matrix33 result = *this;
            result.invert();
            return result;
        }

        [[nodiscard]] constexpr bool operator==(const Matrix33& rhs) const noexcept
        {
            return ma == rhs.ma;
        }

        [[nodiscard]] constexpr bool operator!=(const Matrix33& rhs) const noexcept
        {
            return !(*this == rhs);
        }

        constexpr Matrix33& operator+=(const Matrix33& rhs) noexcept
        {
            for (std::size_t index = 0; index < ma.size(); ++index)
                ma[index] += rhs.ma[index];
            return *this;
        }

        constexpr Matrix33& operator-=(const Matrix33& rhs) noexcept
        {
            for (std::size_t index = 0; index < ma.size(); ++index)
                ma[index] -= rhs.ma[index];
            return *this;
        }

        constexpr Matrix33& operator+=(T rhs) noexcept
        {
            for (T& value : ma)
                value += rhs;
            return *this;
        }

        constexpr Matrix33& operator-=(T rhs) noexcept
        {
            for (T& value : ma)
                value -= rhs;
            return *this;
        }

        constexpr Matrix33& operator*=(T rhs) noexcept
        {
            for (T& value : ma)
                value *= rhs;
            return *this;
        }

        constexpr Matrix33& operator/=(T rhs)
        {
            for (T& value : ma)
                value /= rhs;
            return *this;
        }
    };

    using Matrix33f = Matrix33<float>;
    using M33f = Matrix33<float>;
    using Matrix33d = Matrix33<double>;
    using M33d = Matrix33<double>;
}
