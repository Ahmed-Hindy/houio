#include <houio/math/Color.h>
#include <houio/math/Matrix22Algo.h>
#include <houio/math/Matrix33Algo.h>
#include <houio/math/Matrix44Algo.h>
#include <houio/math/Math.h>
#include <houio/math/Vec2.h>
#include <houio/math/Vec3.h>
#include <houio/math/Vec4.h>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

template<typename T>
bool nearlyEqual(T lhs, T rhs, T tolerance = static_cast<T>(1.0e-5))
{
    return std::abs(lhs - rhs) <= tolerance;
}

template<typename Matrix>
bool isIdentity(const Matrix& matrix)
{
    using T = typename decltype(matrix.ma)::value_type;
    for (std::size_t row = 0; row < Matrix::dimension; ++row)
    {
        for (std::size_t column = 0; column < Matrix::dimension; ++column)
        {
            const T expected = row == column ? T{1} : T{};
            if (!nearlyEqual(matrix(row, column), expected))
                return false;
        }
    }
    return true;
}
}

int main()
{
    const houio::math::Vec4f zero;
    if (zero.x != 0.0f || zero.y != 0.0f || zero.z != 0.0f || zero.w != 0.0f)
        return fail("default Vec4 construction did not initialize every component");

    const houio::math::Vec4f uniform(3.5f);
    if (uniform.x != 3.5f || uniform.y != 3.5f || uniform.z != 3.5f || uniform.w != 3.5f)
        return fail("uniform Vec4 construction did not initialize every component");

    const houio::math::Vec4d source(1.0, 2.0, 3.0, 4.0);
    const houio::math::Vec4f converted(source);
    if (converted.x != 1.0f || converted.y != 2.0f || converted.z != 3.0f || converted.w != 4.0f)
        return fail("converting Vec4 construction changed a component");

    const houio::math::Vec2i integer2(2, 4);
    if (!(integer2 == houio::math::Vec2i(2, 4)) || integer2 == houio::math::Vec2i(2, 5))
        return fail("Vec2 integer comparison is not exact");

    const houio::math::Vec3i integer3(2, 4, 6);
    if (!(integer3 == houio::math::Vec3i(2, 4, 6)) || integer3 == houio::math::Vec3i(2, 4, 7))
        return fail("Vec3 integer comparison is not exact");

    const houio::math::Vec3f nearValue(1.0f, 2.0f, 3.0f);
    if (!(nearValue == houio::math::Vec3f(1.000001f, 2.0f, 3.0f)))
        return fail("Vec3 floating-point comparison lost its tolerance");

    const houio::math::Vec4i integer4(2, 4, 6, 8);
    if (!(integer4 == houio::math::Vec4i(2, 4, 6, 8))
        || integer4 != houio::math::Vec4i(2, 4, 6, 8)
        || integer4 == houio::math::Vec4i(2, 4, 6, 9))
    {
        return fail("Vec4 integer comparison is not exact");
    }

    const houio::math::Vec4f nearValue4(1.0f, 2.0f, 3.0f, 4.0f);
    if (!(nearValue4 == houio::math::Vec4f(1.000001f, 2.0f, 3.0f, 4.0f))
        || nearValue4 != houio::math::Vec4f(1.000001f, 2.0f, 3.0f, 4.0f))
    {
        return fail("Vec4 floating-point comparison lost its tolerance");
    }

    houio::math::Vec2f normalized2(3.0f, 4.0f);
    normalized2.normalize();
    if (std::abs(normalized2.getLength() - 1.0f) > 0.00001f)
        return fail("Vec2 normalization produced the wrong length");

    houio::math::Vec3f indexed3(1.0f, 2.0f, 3.0f);
    indexed3[1] = 4.0f;
    if (indexed3.y != 4.0f || indexed3[2] != 3.0f)
        return fail("Vec3 checked indexing changed valid access");
    try
    {
        static_cast<void>(indexed3[3]);
        return fail("Vec3 accepted an out-of-range index");
    }
    catch (const std::out_of_range&)
    {
    }

    houio::math::Color color(2.0f, -1.0f, 0.5f, 1.5f);
    color.clamp();
    if (color.r != 1.0f || color.g != 0.0f || color.b != 0.5f || color.a != 1.0f)
        return fail("Color clamp did not constrain every component");
    try
    {
        static_cast<void>(color[4]);
        return fail("Color accepted an out-of-range index");
    }
    catch (const std::out_of_range&)
    {
    }

    const houio::math::Matrix22f matrix22(4.0f, 7.0f, 2.0f, 6.0f);
    houio::math::Matrix22f inverse22 = matrix22;
    inverse22.invert();
    if (!isIdentity(matrix22 * inverse22))
        return fail("Matrix22 inverse did not produce identity");

    const houio::math::Matrix33f matrix33(
        1.0f, 2.0f, 3.0f,
        0.0f, 1.0f, 4.0f,
        5.0f, 6.0f, 0.0f);
    houio::math::Matrix33f inverse33 = matrix33;
    inverse33.invert();
    if (!isIdentity(matrix33 * inverse33))
        return fail("Matrix33 inverse did not produce identity");

    houio::math::Matrix44f matrix44 =
        houio::math::Matrix44f::ScaleMatrix(2.0f, 3.0f, 4.0f)
        * houio::math::Matrix44f::TranslationMatrix(5.0f, 6.0f, 7.0f);
    const houio::math::Vec3f transformed =
        houio::math::transform(houio::math::Vec3f(1.0f, 1.0f, 1.0f), matrix44);
    if (transformed != houio::math::Vec3f(7.0f, 9.0f, 11.0f))
        return fail("Matrix44 row-vector transform changed scale/translation semantics");

    const houio::math::Matrix44f inverse44 = matrix44.inverted();
    if (!isIdentity(matrix44 * inverse44))
        return fail("Matrix44 inverse did not produce identity");
    const houio::math::Vec3f recovered = houio::math::transform(transformed, inverse44);
    if (recovered != houio::math::Vec3f(1.0f, 1.0f, 1.0f))
        return fail("Matrix44 inverse did not recover the original point");

    try
    {
        static_cast<void>(matrix44(4, 0));
        return fail("Matrix44 accepted an out-of-range row");
    }
    catch (const std::out_of_range&)
    {
    }

    const std::array<float, 2> linear_times = {0.0f, 1.0f};
    const std::array<float, 4> linear_positions = {0.0f, 0.0f, 2.0f, 4.0f};
    std::array<float, 2> linear_output{};
    houio::math::evaluateLinear(
        linear_positions, linear_times, 2, 0.5f, linear_output);
    if (!nearlyEqual(linear_output[0], 1.0f)
        || !nearlyEqual(linear_output[1], 2.0f))
    {
        return fail("bounded linear interpolation produced the wrong midpoint");
    }
    houio::math::evaluateLinear(
        linear_positions, linear_times, 2, -1.0f, linear_output);
    if (linear_output[0] != 0.0f || linear_output[1] != 0.0f)
        return fail("linear interpolation did not clamp before the first key");
    houio::math::evaluateLinear(
        linear_positions, linear_times, 2, 2.0f, linear_output);
    if (linear_output[0] != 2.0f || linear_output[1] != 4.0f)
        return fail("linear interpolation did not clamp after the last key");

    const std::array<float, 4> spline_times = {0.0f, 1.0f, 2.0f, 3.0f};
    const std::array<float, 4> spline_positions = {0.0f, 1.0f, 2.0f, 3.0f};
    std::array<float, 1> spline_output{};
    houio::math::evaluateCatmullRom(
        spline_positions, spline_times, 1, 1.5f, spline_output);
    if (!nearlyEqual(spline_output[0], 1.5f))
        return fail("bounded Catmull-Rom interpolation produced the wrong midpoint");

    try
    {
        const std::array<float, 2> duplicate_times = {0.0f, 0.0f};
        houio::math::evaluateLinear(
            linear_positions, duplicate_times, 2, 0.0f, linear_output);
        return fail("interpolation accepted non-increasing key times");
    }
    catch (const std::invalid_argument&)
    {
    }

    try
    {
        std::array<float, 1> undersized_output{};
        houio::math::evaluateLinear(
            linear_positions, linear_times, 2, 0.5f, undersized_output);
        return fail("interpolation accepted an output span with the wrong size");
    }
    catch (const std::invalid_argument&)
    {
    }

    return 0;
}
