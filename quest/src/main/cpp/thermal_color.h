#pragma once
#include <array>
#include <cmath>
#include <algorithm>
// Continuous visual temperature scale; red is reserved for robot-reported alarms.
inline std::array<float,3> thermalColor(float celsius,bool alarm){
    if(alarm)return {1.f,.12f,.1f};
    if(!std::isfinite(celsius))return {.5f,.6f,.65f};
    constexpr float stops[4][3]={{.25f,.65f,1.f},{.3f,.95f,.65f},{1.f,.8f,.25f},{1.f,.45f,.15f}};
    float t=std::max(0.f,std::min(3.f,(celsius-20.f)/20.f));int i=std::min(2,int(t));float blend=t-i;
    return {stops[i][0]*(1-blend)+stops[i+1][0]*blend,stops[i][1]*(1-blend)+stops[i+1][1]*blend,stops[i][2]*(1-blend)+stops[i+1][2]*blend};
}
