#pragma once
#include <cmath>
#include <limits>
#include <type_traits>
namespace CustomComponents::Level {
template<class T> bool ReadInteger(T number,int& value) {
    if constexpr(std::is_floating_point_v<T>)
        if(!std::isfinite(number) || std::trunc(number)!=number)return false;
    // Promote before comparing: float cannot represent INT_MAX exactly.
    const long double n=number;
    if(n<std::numeric_limits<int>::min() || n>std::numeric_limits<int>::max())return false;
    value=static_cast<int>(number);return true;
}
}
