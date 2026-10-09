#pragma once
#include <array>
// Reflection across Pepper's sagittal plane, before robot-specific offsets.
inline void mirrorPose(std::array<float,14>& q,std::array<float,2>& torso){
    auto source=q;q[0]=-source[0];
    for(int j=0;j<5;++j){float sign=j==0?1.f:-1.f;q[2+j]=sign*source[7+j];q[7+j]=sign*source[2+j];}
    q[12]=source[13];q[13]=source[12];torso[0]=-torso[0];
}
