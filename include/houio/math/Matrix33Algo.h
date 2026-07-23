#pragma once

#include <cmath>

#include <houio/math/Matrix33.h>
#include <houio/math/Vec3.h>

namespace houio::math
{
    template<typename T>
    constexpr void matrixMultiply(
        Matrix33<T>& result,
        const Matrix33<T>& lhs,
        const Matrix33<T>& rhs) noexcept
    {
        Matrix33<T> product = Matrix33<T>::Zero();
        for (std::size_t row = 0; row < Matrix33<T>::dimension; ++row)
        {
            for (std::size_t column = 0; column < Matrix33<T>::dimension; ++column)
            {
                for (std::size_t inner = 0; inner < Matrix33<T>::dimension; ++inner)
                    product(row, column) += lhs(row, inner) * rhs(inner, column);
            }
        }
        result = product;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator/(Matrix33<T> lhs, const T& rhs)
    {
        lhs /= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator*(Matrix33<T> lhs, const T& rhs) noexcept
    {
        lhs *= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator-(Matrix33<T> lhs, const T& rhs) noexcept
    {
        lhs -= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator+(Matrix33<T> lhs, const T& rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator-(const Matrix33<T>& matrix) noexcept
    {
        Matrix33<T> result = matrix;
        result *= T{-1};
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator-(
        Matrix33<T> lhs,
        const Matrix33<T>& rhs) noexcept
    {
        lhs -= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator+(
        Matrix33<T> lhs,
        const Matrix33<T>& rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator*(const T& lhs, Matrix33<T> rhs) noexcept
    {
        rhs *= lhs;
        return rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator/(const T& lhs, const Matrix33<T>& rhs)
    {
        Matrix33<T> result;
        for (std::size_t index = 0; index < result.ma.size(); ++index)
            result.ma[index] = lhs / rhs.ma[index];
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator-(const T& lhs, const Matrix33<T>& rhs) noexcept
    {
        Matrix33<T> result;
        for (std::size_t index = 0; index < result.ma.size(); ++index)
            result.ma[index] = lhs - rhs.ma[index];
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator+(const T& lhs, Matrix33<T> rhs) noexcept
    {
        rhs += lhs;
        return rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> operator*(
        const Matrix33<T>& lhs,
        const Matrix33<T>& rhs) noexcept
    {
        Matrix33<T> result;
        matrixMultiply(result, lhs, rhs);
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> transform(const Vec3<T>& vector, const Matrix33<T>& matrix)
    {
        return Vec3<T>(
            vector.x * matrix(0, 0) + vector.y * matrix(1, 0) + vector.z * matrix(2, 0),
            vector.x * matrix(0, 1) + vector.y * matrix(1, 1) + vector.z * matrix(2, 1),
            vector.x * matrix(0, 2) + vector.y * matrix(1, 2) + vector.z * matrix(2, 2));
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> transpose(const Matrix33<T>& matrix) noexcept
    {
        return matrix.transposed();
    }

    template<typename T>
    [[nodiscard]] T frobeniusNorm(const Matrix33<T>& matrix)
    {
        T squared{};
        for (const T& value : matrix.ma)
            squared += value * value;
        using std::sqrt;
        return static_cast<T>(sqrt(squared));
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix33<T> outer(const Vec3<T>& lhs, const Vec3<T>& rhs)
    {
        return Matrix33<T>(
            lhs.x * rhs.x, lhs.x * rhs.y, lhs.x * rhs.z,
            lhs.y * rhs.x, lhs.y * rhs.y, lhs.y * rhs.z,
            lhs.z * rhs.x, lhs.z * rhs.y, lhs.z * rhs.z);
    }
}
