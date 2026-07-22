#include <houio/math/Vec2.h>
#include <houio/math/Vec3.h>
#include <houio/math/Vec4.h>

#include <iostream>
#include <string>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
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

    return 0;
}
