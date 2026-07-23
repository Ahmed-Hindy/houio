#include <houio/math/Color.h>
#include <houio/math/Math.h>

#include <algorithm>
#include <stdexcept>

namespace houio::math
{
    Color Color::From255(
        const unsigned char& red,
        const unsigned char& green,
        const unsigned char& blue,
        const unsigned char& alpha) noexcept
    {
        constexpr float scale = 1.0f / 255.0f;
        return Color(
            static_cast<float>(red) * scale,
            static_cast<float>(green) * scale,
            static_cast<float>(blue) * scale,
            static_cast<float>(alpha) * scale);
    }

    void Color::clamp() noexcept
    {
        r = std::clamp(r, 0.0f, 1.0f);
        g = std::clamp(g, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
        a = std::clamp(a, 0.0f, 1.0f);
    }

    unsigned long Color::makeDWORD()
    {
        clamp();
        return setColor(
            static_cast<unsigned int>(r * 255.0f),
            static_cast<unsigned int>(g * 255.0f),
            static_cast<unsigned int>(b * 255.0f),
            static_cast<unsigned int>(a * 255.0f));
    }

    const float& Color::operator[](int index) const
    {
        if (index == 0)
            return r;
        if (index == 1)
            return g;
        if (index == 2)
            return b;
        if (index == 3)
            return a;
        throw std::out_of_range("Color index is out of range");
    }

    float& Color::operator[](int index)
    {
        if (index == 0)
            return r;
        if (index == 1)
            return g;
        if (index == 2)
            return b;
        if (index == 3)
            return a;
        throw std::out_of_range("Color index is out of range");
    }
}
