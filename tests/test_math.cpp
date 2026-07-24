#include <houio/math/Color.h>
#include <houio/math/Matrix22Algo.h>
#include <houio/math/Matrix33Algo.h>
#include <houio/math/Matrix44Algo.h>
#include <houio/math/Math.h>
#include <houio/math/Ray3.h>
#include <houio/math/Vec2.h>
#include <houio/math/Vec3.h>
#include <houio/math/Vec4.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <optional>
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
    if (!nearlyEqual(houio::math::degreesToRadians(180.0f), std::numbers::pi_v<float>)
        || !nearlyEqual(houio::math::radiansToDegrees(std::numbers::pi), 180.0))
    {
        return fail("typed angle conversion produced the wrong value");
    }
    if (!nearlyEqual(houio::math::normalizeRange(5.0f, 0.0f, 10.0f), 0.5f)
        || !nearlyEqual(houio::math::remap(5.0f, 0.0f, 10.0f, -1.0f, 1.0f), 0.0f)
        || !nearlyEqual(houio::math::smoothstep(-1.0f), 0.0f)
        || !nearlyEqual(houio::math::smoothstep(2.0f), 1.0f)
        || !nearlyEqual(houio::math::smoothstep(0.0f, 10.0f, 5.0f), 0.5f))
    {
        return fail("range mapping or smoothstep produced an incorrect value");
    }
    try
    {
        static_cast<void>(houio::math::normalizeRange(1.0f, 2.0f, 2.0f));
        return fail("normalizeRange accepted a collapsed source range");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        static_cast<void>(houio::math::smoothstep(1.0f, 1.0f, 1.0f));
        return fail("smoothstep accepted identical edges");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        static_cast<void>(houio::math::smoothstep(2.0f, 1.0f, 1.5f));
        return fail("smoothstep accepted reversed edges");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        static_cast<void>(houio::math::smoothstep(
            std::numeric_limits<float>::quiet_NaN()));
        return fail("smoothstep accepted a non-finite value");
    }
    catch (const std::invalid_argument&)
    {
    }

    const std::optional<houio::math::QuadraticRoots> distinct_roots =
        houio::math::solveQuadratic(1.0f, -3.0f, 2.0f);
    const std::optional<houio::math::QuadraticRoots> repeated_root =
        houio::math::solveQuadratic(1.0f, -2.0f, 1.0f);
    const std::optional<houio::math::QuadraticRoots> complex_roots =
        houio::math::solveQuadratic(1.0f, 0.0f, 1.0f);
    if (!distinct_roots
        || !nearlyEqual(distinct_roots->first, 1.0f)
        || !nearlyEqual(distinct_roots->second, 2.0f)
        || !repeated_root
        || !repeated_root->repeated()
        || !nearlyEqual(repeated_root->first, 1.0f)
        || complex_roots)
    {
        return fail("value-returning quadratic solver produced incorrect roots");
    }
    try
    {
        static_cast<void>(houio::math::solveQuadratic(0.0f, 1.0f, 1.0f));
        return fail("quadratic solver accepted a zero leading coefficient");
    }
    catch (const std::invalid_argument&)
    {
    }

    houio::math::RandomGenerator default_random;
    if (default_random.nextUInt() != 3499211612u)
        return fail("RandomGenerator changed the standard MT19937 sequence");
    houio::math::RandomGenerator first_random(123u);
    houio::math::RandomGenerator second_random(123u);
    for (int sample = 0; sample < 4; ++sample)
    {
        if (first_random.nextUInt() != second_random.nextUInt())
            return fail("RandomGenerator is not deterministic for a fixed seed");
    }
    first_random.seed(123u);
    const float random_unit = first_random.uniform01();
    if (!(random_unit >= 0.0f && random_unit < 1.0f))
        return fail("RandomGenerator uniform sample is outside [0, 1)");

    const houio::math::Vec3f spherical_point(1.0f, 2.0f, 3.0f);
    const houio::math::SphericalCoordinates<float> spherical_coordinates =
        houio::math::toSphericalCoordinates(spherical_point);
    const houio::math::Vec3f spherical_round_trip =
        houio::math::fromSphericalCoordinates(spherical_coordinates);
    const houio::math::SphericalCoordinates<float> zero_coordinates =
        houio::math::toSphericalCoordinates(houio::math::Vec3f(0.0f));
    if (!nearlyEqual(spherical_round_trip.x, spherical_point.x)
        || !nearlyEqual(spherical_round_trip.y, spherical_point.y)
        || !nearlyEqual(spherical_round_trip.z, spherical_point.z)
        || zero_coordinates.radius != 0.0f
        || zero_coordinates.azimuth != 0.0f
        || zero_coordinates.polarAngle != 0.0f)
    {
        return fail("spherical coordinate conversion did not round-trip safely");
    }

    const houio::math::Vec3f spherical_start(1.0f, 0.0f, 0.0f);
    const houio::math::Vec3f spherical_end(0.0f, 1.0f, 0.0f);
    const houio::math::Vec3f spherical_midpoint =
        houio::math::sphericalLerp(spherical_start, spherical_end, 0.5f);
    const houio::math::Vec3f opposite_midpoint = houio::math::sphericalLerp(
        spherical_start,
        -spherical_start,
        0.5f);
    if (!nearlyEqual(spherical_midpoint.length(), 1.0f)
        || !nearlyEqual(spherical_midpoint.x, std::sqrt(0.5f))
        || !nearlyEqual(spherical_midpoint.y, std::sqrt(0.5f))
        || !nearlyEqual(opposite_midpoint.length(), 1.0f)
        || !nearlyEqual(houio::math::dot(opposite_midpoint, spherical_start), 0.0f))
    {
        return fail("sphericalLerp produced an incorrect direction");
    }
    try
    {
        static_cast<void>(houio::math::sphericalLerp(
            houio::math::Vec3f(0.0f),
            spherical_end,
            0.5f));
        return fail("sphericalLerp accepted a zero direction");
    }
    catch (const std::invalid_argument&)
    {
    }

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
    if (std::abs(normalized2.length() - 1.0f) > 0.00001f)
        return fail("Vec2 normalization produced the wrong length");
    houio::math::Vec2f compound2(1.0f, 2.0f);
    houio::math::Vec2f& compound2_reference = (compound2 += houio::math::Vec2f(2.0f, 3.0f));
    if (&compound2_reference != &compound2 || compound2 != houio::math::Vec2f(3.0f, 5.0f)
        || houio::math::dot(houio::math::Vec2f(1.0f, 2.0f), houio::math::Vec2f(3.0f, 4.0f)) != 11.0f
        || houio::math::cross(houio::math::Vec2f(1.0f, 0.0f), houio::math::Vec2f(0.0f, 1.0f)) != 1.0f
        || houio::math::componentMin(
            houio::math::Vec2f(1.0f, 4.0f),
            houio::math::Vec2f(2.0f, 3.0f)) != houio::math::Vec2f(1.0f, 3.0f))
    {
        return fail("Vec2 arithmetic or geometric helpers are inconsistent");
    }

    houio::math::Vec3f reflected3(1.0f, -1.0f, 0.0f);
    reflected3.reflect(houio::math::Vec3f(0.0f, 1.0f, 0.0f));
    const auto [basis_tangent, basis_bitangent] =
        houio::math::orthonormalBasis(houio::math::Vec3f(0.0f, 0.0f, 2.0f));
    if (reflected3 != houio::math::Vec3f(1.0f, 1.0f, 0.0f)
        || houio::math::reflected(
            houio::math::Vec3f(1.0f, -1.0f, 0.0f),
            houio::math::Vec3f(0.0f, 1.0f, 0.0f))
            != houio::math::Vec3f(1.0f, 1.0f, 0.0f)
        || houio::math::leastAbsoluteAxis(houio::math::Vec3f(3.0f, -1.0f, 2.0f)) != 1u
        || houio::math::unitAxis3<float>(2) != houio::math::Vec3f(0.0f, 0.0f, 1.0f)
        || !nearlyEqual(basis_tangent.length(), 1.0f)
        || !nearlyEqual(basis_bitangent.length(), 1.0f)
        || !nearlyEqual(
            houio::math::dot(basis_tangent, basis_bitangent),
            0.0f))
    {
        return fail("Vec3 reflection or basis helpers are inconsistent");
    }

    const houio::math::Vec4f vector4_a(1.0f, 2.0f, 3.0f, 4.0f);
    const houio::math::Vec4f vector4_b(2.0f, 3.0f, 4.0f, 5.0f);
    houio::math::Vec4f normalized4 = vector4_a.normalized();
    houio::math::Vec4f compound4 = vector4_a;
    houio::math::Vec4f& compound4_reference = (compound4 += vector4_b);
    if (&compound4_reference != &compound4
        || compound4 != houio::math::Vec4f(3.0f, 5.0f, 7.0f, 9.0f)
        || vector4_a + vector4_b != houio::math::Vec4f(3.0f, 5.0f, 7.0f, 9.0f)
        || vector4_a * vector4_b != houio::math::Vec4f(2.0f, 6.0f, 12.0f, 20.0f)
        || 20.0f / vector4_b != houio::math::Vec4f(10.0f, 20.0f / 3.0f, 5.0f, 4.0f)
        || houio::math::dot(vector4_a, vector4_a) != 30.0f
        || !nearlyEqual(normalized4.length(), 1.0f)
        || houio::math::leastAbsoluteAxis(houio::math::Vec4f(4.0f, 3.0f, 2.0f, -1.0f)) != 3u
        || houio::math::unitAxis4<float>(3) != houio::math::Vec4f(0.0f, 0.0f, 0.0f, 1.0f))
    {
        return fail("Vec4 arithmetic lost a component or produced an incorrect dot product");
    }

    houio::math::BoundingBox2f bounds2(
        houio::math::Vec2f(-2.0f, -4.0f),
        houio::math::Vec2f(2.0f, 4.0f));
    if (bounds2.center() != houio::math::Vec2f(0.0f)
        || bounds2.size() != houio::math::Vec2f(4.0f, 8.0f)
        || !bounds2.encloses(houio::math::Vec2f(0.0f)))
    {
        return fail("BoundingBox2 modern accessors are inconsistent");
    }

    houio::math::BoundingBox3f bounds3;
    if (!bounds3.empty())
        return fail("default BoundingBox3 is not empty");
    bounds3.extend(houio::math::Vec3f(-1.0f, -2.0f, -3.0f));
    bounds3.extend(houio::math::Vec3f(1.0f, 2.0f, 3.0f));
    if (bounds3.empty() || bounds3.center() != houio::math::Vec3f(0.0f)
        || bounds3.longestAxis() != 2)
    {
        return fail("BoundingBox3 modern accessors are inconsistent");
    }

    const houio::math::Ray3f ray(
        houio::math::Vec3f(0.0f, 0.0f, -2.0f),
        houio::math::Vec3f(0.0f, 0.0f, 1.0f));
    if (ray.position(2.0f) != houio::math::Vec3f(0.0f)
        || !ray.contains(0.0f) || ray.contains(-1.0f))
    {
        return fail("Ray3 modern accessors are inconsistent");
    }
    float near_distance = 0.0f;
    float far_distance = 0.0f;
    if (!houio::math::intersectionRaySphere(ray, 1.0f, near_distance, far_distance)
        || !nearlyEqual(near_distance, 1.0f) || !nearlyEqual(far_distance, 3.0f))
    {
        return fail("Ray-sphere intersection produced incorrect distances");
    }
    houio::math::Vec3f plane_hit;
    if (!houio::math::intersectionRayPlane(ray, houio::math::Vec3f(0.0f, 0.0f, 1.0f),
            0.0f, plane_hit)
        || plane_hit != houio::math::Vec3f(0.0f))
    {
        return fail("Ray-plane intersection produced the wrong point");
    }
    if (!houio::math::intersectionRayBox(
            ray,
            houio::math::BoundingBox3f(
                houio::math::Vec3f(-1.0f), houio::math::Vec3f(1.0f)),
            near_distance,
            far_distance)
        || !nearlyEqual(near_distance, 1.0f) || !nearlyEqual(far_distance, 3.0f))
    {
        return fail("Ray-box intersection produced incorrect distances");
    }

    const houio::math::Vec3f triangle_a(0.0f, 0.0f, 0.0f);
    const houio::math::Vec3f triangle_b(1.0f, 0.0f, 0.0f);
    const houio::math::Vec3f triangle_c(0.0f, 1.0f, 0.0f);
    if (!nearlyEqual(
            houio::math::distancePointToTriangle(
                houio::math::Vec3f(0.25f, 0.25f, 2.0f),
                triangle_a,
                triangle_b,
                triangle_c),
            2.0f)
        || !nearlyEqual(
            houio::math::distancePointToTriangle(
                houio::math::Vec3f(-1.0f, -1.0f, 0.0f),
                triangle_a,
                triangle_b,
                triangle_c),
            std::sqrt(2.0f))
        || !nearlyEqual(
            houio::math::distancePointToTriangle(
                houio::math::Vec3f(0.5f, -1.0f, 0.0f),
                triangle_a,
                triangle_b,
                triangle_c),
            1.0f))
    {
        return fail("point-triangle distance failed an interior, vertex, or edge region");
    }
    if (!nearlyEqual(
            houio::math::distancePointToTriangle(
                houio::math::Vec3f(1.0f, 1.0f, 0.0f),
                houio::math::Vec3f(0.0f),
                houio::math::Vec3f(1.0f, 0.0f, 0.0f),
                houio::math::Vec3f(2.0f, 0.0f, 0.0f)),
            1.0f)
        || !nearlyEqual(
            houio::math::distancePointToTriangle(
                houio::math::Vec3f(0.0f, 3.0f, 4.0f),
                houio::math::Vec3f(0.0f),
                houio::math::Vec3f(0.0f),
                houio::math::Vec3f(0.0f)),
            5.0f))
    {
        return fail("point-triangle distance failed degenerate triangle handling");
    }

    if (!nearlyEqual(houio::math::triangleArea(triangle_a, triangle_b, triangle_c), 0.5f)
        || !nearlyEqual(
            houio::math::distancePointToLine(
                houio::math::Vec3f(0.5f, 2.0f, 0.0f),
                triangle_a,
                triangle_b),
            2.0f)
        || !nearlyEqual(
            houio::math::distancePointToLine(
                houio::math::Vec3f(0.0f, 3.0f, 4.0f),
                triangle_a,
                triangle_a),
            5.0f))
    {
        return fail("triangle area or point-line distance is inconsistent");
    }

    const houio::math::Vec3f non_unit_plane_normal(0.0f, 0.0f, 2.0f);
    const houio::math::Vec3f plane_point(1.0f, 2.0f, 5.0f);
    if (!nearlyEqual(
            houio::math::signedDistanceToPlane(
                plane_point,
                non_unit_plane_normal,
                -6.0f),
            2.0f)
        || houio::math::projectPointOntoPlane(
            plane_point,
            non_unit_plane_normal,
            -6.0f) != houio::math::Vec3f(1.0f, 2.0f, 3.0f)
        || houio::math::projectPointOntoLine(
            houio::math::Vec3f(0.5f, 2.0f, 0.0f),
            triangle_a,
            triangle_b) != houio::math::Vec3f(0.5f, 0.0f, 0.0f)
        || houio::math::projectPointOntoLine(
            houio::math::Vec3f(1.0f, 2.0f, 3.0f),
            triangle_a,
            triangle_a) != triangle_a)
    {
        return fail("plane or line projection produced an incorrect result");
    }
    try
    {
        static_cast<void>(houio::math::signedDistanceToPlane(
            plane_point,
            houio::math::Vec3f(0.0f),
            0.0f));
        return fail("signed plane distance accepted a zero normal");
    }
    catch (const std::invalid_argument&)
    {
    }

    houio::math::Vec3f compound3(1.0f, 2.0f, 3.0f);
    houio::math::Vec3f& compound3_reference = (compound3 *= 2.0f);
    if (&compound3_reference != &compound3 || compound3 != houio::math::Vec3f(2.0f, 4.0f, 6.0f))
        return fail("Vec3 compound assignment does not return the modified vector");

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
    const houio::math::Color byte_color = houio::math::Color::fromBytes(1, 2, 3, 4);
    const std::uint32_t packed_color = byte_color.packedRgba();
    if (houio::math::rgbaRed(packed_color) != 1
        || houio::math::rgbaGreen(packed_color) != 2
        || houio::math::rgbaBlue(packed_color) != 3
        || houio::math::rgbaAlpha(packed_color) != 4
        || houio::math::packRgba(1, 2, 3, 4) != packed_color
        || houio::math::redByte(byte_color) != 1
        || houio::math::greenByte(byte_color) != 2
        || houio::math::blueByte(byte_color) != 3
        || houio::math::alphaByte(byte_color) != 4)
    {
        return fail("fixed-width RGBA conversion changed channel values");
    }
    houio::math::Color accumulated_color(0.0f, 0.0f, 0.0f, 0.0f);
    houio::math::Color& accumulated_reference = (accumulated_color += houio::math::Color::red());
    const houio::math::Color channel_values(1.0f, 2.0f, 4.0f, 8.0f);
    if (&accumulated_reference != &accumulated_color
        || accumulated_color != houio::math::Color::red()
        || 16.0f - channel_values != houio::math::Color(15.0f, 14.0f, 12.0f, 8.0f)
        || 16.0f / channel_values != houio::math::Color(16.0f, 8.0f, 4.0f, 2.0f)
        || houio::math::Color(0.25f, 0.5f, 0.75f, 0.4f).invertedRgb()
            != houio::math::Color(0.75f, 0.5f, 0.25f, 0.4f))
    {
        return fail("Color arithmetic or RGB inversion has incorrect semantics");
    }
    try
    {
        static_cast<void>(color[4]);
        return fail("Color accepted an out-of-range index");
    }
    catch (const std::out_of_range&)
    {
    }

    const houio::math::Matrix22f matrix22(4.0f, 7.0f, 2.0f, 6.0f);
    const houio::math::Matrix22f inverse22 = matrix22.inverted();
    const houio::math::Matrix22f outer22 = houio::math::outer(
        houio::math::Vec2f(2.0f, 3.0f),
        houio::math::Vec2f(5.0f, 7.0f));
    const houio::math::Vec2f rotated22 = houio::math::transform(
        houio::math::Vec2f(1.0f, 0.0f),
        houio::math::Matrix22f::rotation(std::numbers::pi_v<float> * 0.5f));
    if (!nearlyEqual(matrix22.determinant(), 10.0f)
        || !isIdentity(matrix22 * inverse22)
        || outer22 != houio::math::Matrix22f(10.0f, 14.0f, 15.0f, 21.0f)
        || rotated22 != houio::math::Vec2f(0.0f, -1.0f))
    {
        return fail("Matrix22 determinant, inverse, outer product, or rotation is incorrect");
    }
    try
    {
        static_cast<void>(houio::math::Matrix22f(1.0f, 2.0f, 2.0f, 4.0f).inverted());
        return fail("Matrix22 accepted a singular inverse");
    }
    catch (const std::domain_error&)
    {
    }

    const houio::math::Matrix33f matrix33(
        1.0f, 2.0f, 3.0f,
        0.0f, 1.0f, 4.0f,
        5.0f, 6.0f, 0.0f);
    const houio::math::Matrix33f inverse33 = matrix33.inverted();
    const houio::math::Matrix33f outer33 = houio::math::outer(
        houio::math::Vec3f(1.0f, 2.0f, 3.0f),
        houio::math::Vec3f(4.0f, 5.0f, 6.0f));
    const houio::math::Vec3f rotated33 = houio::math::transform(
        houio::math::Vec3f(1.0f, 0.0f, 0.0f),
        houio::math::Matrix33f::rotation(
            houio::math::Vec3f(0.0f, 0.0f, 1.0f),
            std::numbers::pi_v<float> * 0.5f));
    if (!nearlyEqual(matrix33.determinant(), 1.0f)
        || !isIdentity(matrix33 * inverse33)
        || outer33 != houio::math::Matrix33f(
            4.0f, 5.0f, 6.0f,
            8.0f, 10.0f, 12.0f,
            12.0f, 15.0f, 18.0f)
        || rotated33 != houio::math::Vec3f(0.0f, -1.0f, 0.0f))
    {
        return fail("Matrix33 determinant, inverse, outer product, or rotation is incorrect");
    }
    try
    {
        static_cast<void>(houio::math::Matrix33f::rotation(
            houio::math::Vec3f(0.0f), 1.0f));
        return fail("Matrix33 accepted a zero rotation axis");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        static_cast<void>(houio::math::Matrix33f(
            1.0f, 2.0f, 3.0f,
            2.0f, 4.0f, 6.0f,
            0.0f, 0.0f, 0.0f).inverted());
        return fail("Matrix33 accepted a singular inverse");
    }
    catch (const std::domain_error&)
    {
    }

    houio::math::Matrix44f matrix44 =
        houio::math::Matrix44f::scaleMatrix(2.0f, 3.0f, 4.0f)
        * houio::math::Matrix44f::translationMatrix(5.0f, 6.0f, 7.0f);
    const houio::math::Vec3f transformed =
        houio::math::transform(houio::math::Vec3f(1.0f, 1.0f, 1.0f), matrix44);
    if (transformed != houio::math::Vec3f(7.0f, 9.0f, 11.0f))
        return fail("Matrix44 row-vector transform changed scale/translation semantics");
    if (matrix44.translation() != houio::math::Vec3f(5.0f, 6.0f, 7.0f)
        || matrix44.right(false) != houio::math::Vec3f(2.0f, 0.0f, 0.0f)
        || matrix44.up(false) != houio::math::Vec3f(0.0f, 3.0f, 0.0f)
        || matrix44.direction(false) != houio::math::Vec3f(0.0f, 0.0f, 4.0f)
        || matrix44.orientation().translation() != houio::math::Vec3f(0.0f)
        || !nearlyEqual(matrix44.normalizedOrientation().right().length(), 1.0f)
        || !nearlyEqual(matrix44.determinant(), 24.0f))
    {
        return fail("Matrix44 modern orientation accessors or determinant are inconsistent");
    }

    const float half_pi = std::numbers::pi_v<float> * 0.5f;
    const houio::math::Matrix44f rotation_z = houio::math::Matrix44f::rotationZ(half_pi);
    const houio::math::Matrix44f axis_rotation_z = houio::math::Matrix44f::axisRotation(
        houio::math::Vec3f(0.0f, 0.0f, 1.0f), half_pi);
    const houio::math::Matrix44f rotation_between = houio::math::Matrix44f::rotationBetween(
        houio::math::Vec3f(1.0f, 0.0f, 0.0f),
        houio::math::Vec3f(0.0f, 1.0f, 0.0f));
    const houio::math::Vec3f euler_z = houio::math::eulerAnglesXYZ(rotation_z);
    const houio::math::AxisAngle<float> axis_angle_z = houio::math::axisAngle(rotation_z);
    if (houio::math::transform(houio::math::Vec3f(1.0f, 0.0f, 0.0f), rotation_z)
            != houio::math::Vec3f(0.0f, -1.0f, 0.0f)
        || houio::math::transform(houio::math::Vec3f(1.0f, 0.0f, 0.0f), axis_rotation_z)
            != houio::math::Vec3f(0.0f, -1.0f, 0.0f)
        || houio::math::transform(houio::math::Vec3f(1.0f, 0.0f, 0.0f), rotation_between)
            != houio::math::Vec3f(0.0f, 1.0f, 0.0f)
        || !nearlyEqual(euler_z.x, 0.0f)
        || !nearlyEqual(euler_z.y, 0.0f)
        || !nearlyEqual(euler_z.z, half_pi)
        || axis_angle_z.axis != houio::math::Vec3f(0.0f, 0.0f, 1.0f)
        || !nearlyEqual(axis_angle_z.angle, half_pi))
    {
        return fail("Matrix44 row-vector rotation or decomposition semantics are inconsistent");
    }

    houio::math::Matrix44f chained44 = houio::math::Matrix44f::identity();
    houio::math::Matrix44f& chained44_reference = chained44
        .scale(2.0f, 3.0f, 4.0f)
        .translate(5.0f, 6.0f, 7.0f);
    if (&chained44_reference != &chained44 || chained44 != matrix44)
        return fail("Matrix44 mutators do not preserve chaining or composition order");

    const houio::math::Matrix44f look_at = houio::math::lookAtTransform(
        houio::math::Vec3f(0.0f, 0.0f, 5.0f),
        houio::math::Vec3f(0.0f),
        houio::math::Vec3f(0.0f, 1.0f, 0.0f));
    const houio::math::Matrix44f direction_transform = houio::math::transformFromDirection(
        houio::math::Vec3f(0.0f, 0.0f, 2.0f),
        houio::math::Vec3f(1.0f, 2.0f, 3.0f));
    if (look_at.translation() != houio::math::Vec3f(0.0f, 0.0f, 5.0f)
        || look_at.direction() != houio::math::Vec3f(0.0f, 0.0f, 1.0f)
        || direction_transform.translation() != houio::math::Vec3f(1.0f, 2.0f, 3.0f)
        || direction_transform.direction() != houio::math::Vec3f(0.0f, 0.0f, 1.0f)
        || !nearlyEqual(houio::math::dot(
            direction_transform.right(), direction_transform.up()), 0.0f))
    {
        return fail("Matrix44 look-at or direction transform is not orthonormal");
    }

    try
    {
        static_cast<void>(houio::math::Matrix44f::axisRotation(
            houio::math::Vec3f(0.0f), 1.0f));
        return fail("Matrix44 accepted a zero rotation axis");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        static_cast<void>(houio::math::Matrix44f::scaleMatrix(1.0f, 0.0f, 1.0f).inverted());
        return fail("Matrix44 accepted a singular inverse");
    }
    catch (const std::domain_error&)
    {
    }
    try
    {
        static_cast<void>(houio::math::orthographicProjection(
            0.0f, 0.0f, -1.0f, 1.0f, 0.1f, 10.0f));
        return fail("Orthographic projection accepted collapsed bounds");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        static_cast<void>(houio::math::perspectiveProjection(
            half_pi, 0.0f, 0.1f, 10.0f));
        return fail("Perspective projection accepted a zero aspect ratio");
    }
    catch (const std::invalid_argument&)
    {
    }

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
    try
    {
        const std::array<float, 2> invalid_times = {
            0.0f, std::numeric_limits<float>::infinity()};
        houio::math::evaluateLinear(
            linear_positions, invalid_times, 2, 0.5f, linear_output);
        return fail("interpolation accepted a non-finite key time");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        const std::array<float, 4> invalid_positions = {
            0.0f, 0.0f, std::numeric_limits<float>::quiet_NaN(), 4.0f};
        houio::math::evaluateLinear(
            invalid_positions, linear_times, 2, 0.5f, linear_output);
        return fail("interpolation accepted a non-finite key value");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        houio::math::evaluateCatmullRom(
            spline_positions,
            spline_times,
            1,
            std::numeric_limits<float>::quiet_NaN(),
            spline_output);
        return fail("interpolation accepted a non-finite sample time");
    }
    catch (const std::invalid_argument&)
    {
    }

    return 0;
}
