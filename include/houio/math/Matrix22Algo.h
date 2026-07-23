#pragma once

#include <cmath>

#include <houio/math/Matrix22.h>
#include <houio/math/Vec2.h>

namespace houio::math
{
    template<typename T>
    [[nodiscard]] constexpr Vec2<T> transform(const Vec2<T>& vector, const Matrix22<T>& matrix)
    {
        return Vec2<T>(
            vector.x * matrix(0, 0) + vector.y * matrix(1, 0),
            vector.x * matrix(0, 1) + vector.y * matrix(1, 1));
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix22<T> outer(const Vec2<T>& lhs, const Vec2<T>& rhs)
    {
        return Matrix22<T>(
            lhs.x * rhs.x,
            lhs.y * rhs.x,
            lhs.x * rhs.y,
            lhs.y * rhs.y);
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix22<T> operator+(
        Matrix22<T> lhs,
        const Matrix22<T>& rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix22<T> operator-(
        Matrix22<T> lhs,
        const Matrix22<T>& rhs) noexcept
    {
        lhs -= rhs;
        return lhs;
    }

    template<typename T>
    constexpr void matrixMultiply(
        Matrix22<T>& result,
        const Matrix22<T>& lhs,
        const Matrix22<T>& rhs) noexcept
    {
        Matrix22<T> product = Matrix22<T>::Zero();
        for (std::size_t row = 0; row < Matrix22<T>::dimension; ++row)
        {
            for (std::size_t column = 0; column < Matrix22<T>::dimension; ++column)
            {
                for (std::size_t inner = 0; inner < Matrix22<T>::dimension; ++inner)
                    product(row, column) += lhs(row, inner) * rhs(inner, column);
            }
        }
        result = product;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix22<T> operator/(Matrix22<T> lhs, const T& rhs)
    {
        lhs /= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix22<T> operator*(Matrix22<T> lhs, const T& rhs) noexcept
    {
        lhs *= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix22<T> operator-(const Matrix22<T>& value) noexcept
    {
        Matrix22<T> result = value;
        result *= T{-1};
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix22<T> operator*(const T& lhs, Matrix22<T> rhs) noexcept
    {
        rhs *= lhs;
        return rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix22<T> operator/(const T& lhs, const Matrix22<T>& rhs)
    {
        Matrix22<T> result;
        for (std::size_t index = 0; index < result.ma.size(); ++index)
            result.ma[index] = lhs / rhs.ma[index];
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix22<T> operator*(
        const Matrix22<T>& lhs,
        const Matrix22<T>& rhs) noexcept
    {
        Matrix22<T> result;
        matrixMultiply(result, lhs, rhs);
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix22<T> transpose(const Matrix22<T>& matrix) noexcept
    {
        return matrix.transposed();
    }

    template<typename T>
    [[nodiscard]] T frobeniusNorm(const Matrix22<T>& matrix)
    {
        T squared{};
        for (const T& value : matrix.ma)
            squared += value * value;
        using std::sqrt;
        return static_cast<T>(sqrt(squared));
    }
}
