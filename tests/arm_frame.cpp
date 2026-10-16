#include "../quest/src/main/cpp/arm_frame.h"
#include <cassert>
#include <cstdio>
using namespace pepper_arm;
int main(){
    constexpr float rad=.01745329252f;
    for(float yaw:{-170.f,-90.f,-25.f,0.f,45.f,90.f,175.f}){
        V right{std::cos(yaw*rad),0,-std::sin(yaw*rad)};
        V forward{right.z,0,-right.x};
        for(float shoulderHeight:{-.2f,0.f,.2f}){
            auto frame=armFrame({right.x*.4f,shoulderHeight,right.z*.4f});
            // Forward horizontal arms remain level regardless of estimated
            // spine tilt or shoulder height differences, at every body heading.
            auto horizontal=frame.robotVector(forward);
            assert(std::abs(horizontal.x-1)<1e-6f&&std::abs(horizontal.y)<1e-6f&&horizontal.z==0);
            for(float elevation:{-70.f,-20.f,0.f,20.f,70.f,110.f}){
                V world=add(scale(forward,std::cos(elevation*rad)),{0,std::sin(elevation*rad),0});
                auto mapped=frame.robotVector(world);
                assert(std::abs(mapped.x-std::cos(elevation*rad))<1e-6f);
                assert(std::abs(mapped.z-std::sin(elevation*rad))<1e-6f);
            }
            for(bool left:{true,false}){
                auto goal=retarget(horizontal,horizontal);
                auto q=solve(goal.wrist,goal.elbow,left,directionSeed(horizontal,horizontal,left),1.f);
                auto actual=pepper_arm::forward(q,left);
                // Collinear human bones are not exactly reachable with Pepper's
                // lateral elbow offset and native elbow limits. Bound that small
                // solver residual separately from the exact frame invariants.
                float elbowElevation=std::atan2(actual.elbow.z,std::hypot(actual.elbow.x,actual.elbow.y));
                float wristElevation=std::atan2(actual.wrist.z,std::hypot(actual.wrist.x,actual.wrist.y));
                std::printf("heading %.0f side %d: elbow %.2f wrist %.2f degrees\n",yaw,left,elbowElevation/rad,wristElevation/rad);
                assert(std::abs(elbowElevation)<3*rad);
                assert(std::abs(wristElevation)<3*rad);
            }
        }
    }
    // Reproduce the old systematic error: a 20-degree backward lean in the
    // estimated spine rotated a world-horizontal arm 20 degrees downward.
    for(float tilt:{15.f,20.f}){
        V spine{0,std::cos(tilt*rad),std::sin(tilt*rad)},world{0,0,-1};
        assert(std::abs(std::asin(dot(world,spine))+tilt*rad)<1e-6f);
        assert(armFrame({.4f,0,0}).robotVector(world).z==0);
    }
    assert(armFrame({0,1,0}).robotVector({0,0,-1}).x==1);
    std::puts("PASS level forward arms, elevation, yaw invariance, unequal shoulders and 15/20-degree spine-tilt regression");
}
