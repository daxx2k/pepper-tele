#pragma once
#include "arm_kinematics.h"
#include <json.hpp>
namespace pepper_arm {
inline Limits liveLimits(const nlohmann::json& status,bool left){
    Limits result=nativeLimits(left);
    const char* names[]={left?"LShoulderPitch":"RShoulderPitch",left?"LShoulderRoll":"RShoulderRoll",left?"LElbowYaw":"RElbowYaw",left?"LElbowRoll":"RElbowRoll"};
    auto it=status.find("native_arm_limits");
    if(it==status.end()||!it->is_object())return result;
    for(int i=0;i<4;++i){
        auto item=it->find(names[i]);
        if(item==it->end()||!item->is_array()||item->size()!=2||!(*item)[0].is_number()||!(*item)[1].is_number())return nativeLimits(left);
        float lo=(*item)[0].get<float>(),hi=(*item)[1].get<float>();
        if(!std::isfinite(lo)||!std::isfinite(hi)||lo>=hi||lo<-3.1416f||hi>3.1416f)return nativeLimits(left);
        result.lo[i]=lo;result.hi[i]=hi;
    }
    return result;
}
inline Limits mirroredLimits(Limits other){
    for(int i=1;i<4;++i){float lo=other.lo[i];other.lo[i]=-other.hi[i];other.hi[i]=-lo;}
    return other;
}
}
