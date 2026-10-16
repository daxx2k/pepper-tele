#pragma once
#include "arm_kinematics.h"

namespace pepper_arm {
// OpenXR LOCAL is gravity aligned (+Y up). Use the robot torso compensation separately;
// estimated hip/spine lean must not rotate a level human arm downward.
struct ArmFrame {
    V right,back;
    V robotVector(V local)const{return {-dot(local,back),-dot(local,right),local.y};}
};
inline ArmFrame armFrame(V shoulderSpan){
    V right{shoulderSpan.x,0,shoulderSpan.z};
    float horizontal=length(right);
    right=horizontal>.0001f?scale(right,1.f/horizontal):V{1,0,0};
    return {right,{-right.z,0,right.x}};
}
}
