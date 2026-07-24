#include <houio/math/Color.h>

#include <algorithm>
#include <stdexcept>

namespace houio::math
{
    namespace
    {
        [[nodiscard]] std::uint8_t channelByte(float value) noexcept
        {
            const float clamped = std::clamp(value, 0.0f, 1.0f);
            return static_cast<std::uint8_t>(clamped * 255.0f);
        }
    }

    void Color::clamp() noexcept
    {
        r = std::clamp(r, 0.0f, 1.0f);
        g = std::clamp(g, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
        a = std::clamp(a, 0.0f, 1.0f);
    }

    std::uint32_t Color::packedRgba() const noexcept
    {
        return static_cast<std::uint32_t>(channelByte(r))
            | (static_cast<std::uint32_t>(channelByte(g)) << 8u)
            | (static_cast<std::uint32_t>(channelByte(b)) << 16u)
            | (static_cast<std::uint32_t>(channelByte(a)) << 24u);
    }

    const float& Color::operator[](std::size_t index) const
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

    float& Color::operator[](std::size_t index)
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
