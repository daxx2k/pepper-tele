#pragma once
#include <algorithm>
#include <cmath>
namespace panel_layout {
// Conservative common field of view of both eyes; use tangent extents so the
// entire flat quad, including its corners, remains inside the selected margin.
inline float fit(float left,float right,float down,float up,float distance,float aspect){
    float horizontal=std::min(std::abs(left),std::abs(right));
    float vertical=std::min(std::abs(down),std::abs(up));
    if(!std::isfinite(horizontal)||!std::isfinite(vertical)||horizontal<.1f||vertical<.1f)return 1.25f;
    return std::min(1.65f,.72f*2.f*distance*std::min(std::tan(horizontal),aspect*std::tan(vertical)));
}
inline float resize(float scale,float stick,float dt){
    if(std::abs(stick)<.2f)return scale;
    return std::max(.65f,std::min(1.f,scale+stick*std::min(dt,.05f)*.4f));
}
}
