#pragma once
#include <array>
#include <algorithm>
#include <cmath>
inline std::array<float,2> headAssistance(float pitch,float roll){
    float local=std::max(-.58f,std::min(.33f,pitch));
    float lean=std::copysign(std::max(0.f,std::abs(roll)-.05236f),roll);
    return {std::max(-.15f,std::min(.15f,lean)),std::max(-.20f,std::min(.20f,pitch-local))};
}
