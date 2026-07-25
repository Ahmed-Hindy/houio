#pragma once

#include <cmath>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace houio::test
{
    inline int fail(std::string_view message)
    {
        std::cerr << "error: " << message << '\n';
        return 1;
    }

    inline int check(bool condition, std::string_view message)
    {
        return condition ? 0 : fail(message);
    }

    template<typename Left, typename Right>
    int checkEqual(const Left& actual, const Right& expected, std::string_view message)
    {
        return actual == expected ? 0 : fail(message);
    }

    template<typename Value>
    int checkNear(Value actual, Value expected, Value tolerance, std::string_view message)
    {
        static_assert(std::is_floating_point_v<Value>);
        if (!std::isfinite(actual) || !std::isfinite(expected) || tolerance < Value{})
            return fail(message);
        return std::abs(actual - expected) <= tolerance ? 0 : fail(message);
    }

    template<typename ExpectedException, typename Callable>
    int expectThrows(Callable&& callable, std::string_view message)
    {
        try
        {
            std::forward<Callable>(callable)();
        }
        catch (const ExpectedException&)
        {
            return 0;
        }
        catch (const std::exception& exception)
        {
            return fail(std::string(message) + ": unexpected exception: " + exception.what());
        }
        catch (...)
        {
            return fail(std::string(message) + ": unexpected non-standard exception");
        }
        return fail(std::string(message) + ": expected exception was not thrown");
    }
}
