#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace houio::math
{
    template<typename T>
    class Matrix22
    {
    public:
        static constexpr std::size_t dimension = 2;
        std::array<T, dimension * dimension> ma{};

        constexpr Matrix22() noexcept = default;

        constexpr Matrix22(
            T value00,
            T value01,
            T value10,
            T value11) noexcept
            : ma{value00, value01, value10, value11}
        {
        }

        [[nodiscard]] static constexpr Matrix22 zero() noexcept
        {
            return Matrix22{};
        }

        [[nodiscard]] static constexpr Matrix22 identity() noexcept
        {
            return Matrix22(T{1}, T{}, T{}, T{1});
        }

        [[nodiscard]] static Matrix22 rotation(T angle)
            requires std::floating_point<T>
        {
            using std::cos;
            using std::sin;
            const T sine = static_cast<T>(sin(angle));
            const T cosine = static_cast<T>(cos(angle));
            return Matrix22(cosine, -sine, sine, cosine);
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
            std::swap(ma[1], ma[2]);
        }

        [[nodiscard]] constexpr Matrix22 transposed() const noexcept
        {
            Matrix22 result = *this;
            result.transpose();
            return result;
        }

        [[nodiscard]] constexpr T trace() const noexcept
        {
            return ma[0] + ma[3];
        }

        [[nodiscard]] constexpr T determinant() const noexcept
        {
            return ma[0] * ma[3] - ma[1] * ma[2];
        }

        void invert()
            requires std::floating_point<T>
        {
            using std::abs;
            const T scale = std::max({
                T{1},
                static_cast<T>(abs(ma[0])),
                static_cast<T>(abs(ma[1])),
                static_cast<T>(abs(ma[2])),
                static_cast<T>(abs(ma[3])),
            });
            const T determinant_value = determinant();
            const T tolerance = std::numeric_limits<T>::epsilon() * scale * scale * T{16};
            if (abs(determinant_value) <= tolerance)
                throw std::domain_error("Matrix22 is singular");

            const T reciprocal = T{1} / determinant_value;
            *this = Matrix22(
                ma[3] * reciprocal,
                -ma[1] * reciprocal,
                -ma[2] * reciprocal,
                ma[0] * reciprocal);
        }

        [[nodiscard]] Matrix22 inverted() const
            requires std::floating_point<T>
        {
            Matrix22 result = *this;
            result.invert();
            return result;
        }

        [[nodiscard]] constexpr bool operator==(const Matrix22& rhs) const noexcept
        {
            return ma == rhs.ma;
        }

        [[nodiscard]] constexpr bool operator!=(const Matrix22& rhs) const noexcept
        {
            return !(*this == rhs);
        }

        constexpr Matrix22& operator+=(const Matrix22& rhs) noexcept
        {
            for (std::size_t index = 0; index < ma.size(); ++index)
                ma[index] += rhs.ma[index];
            return *this;
        }

        constexpr Matrix22& operator-=(const Matrix22& rhs) noexcept
        {
            for (std::size_t index = 0; index < ma.size(); ++index)
                ma[index] -= rhs.ma[index];
            return *this;
        }

        constexpr Matrix22& operator+=(T rhs) noexcept
        {
            for (T& value : ma)
                value += rhs;
            return *this;
        }

        constexpr Matrix22& operator-=(T rhs) noexcept
        {
            for (T& value : ma)
                value -= rhs;
            return *this;
        }

        constexpr Matrix22& operator*=(T rhs) noexcept
        {
            for (T& value : ma)
                value *= rhs;
            return *this;
        }

        constexpr Matrix22& operator/=(T rhs)
        {
            for (T& value : ma)
                value /= rhs;
            return *this;
        }
    };

    using Matrix22f = Matrix22<float>;
    using Matrix22d = Matrix22<double>;
    using M22f = Matrix22<float>;
    using M22d = Matrix22<double>;
}
