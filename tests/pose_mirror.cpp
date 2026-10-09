#include "../quest/src/main/cpp/pose_mirror.h"
#include "../quest/src/main/cpp/arm_kinematics.h"
#include <cassert>
#include <cstdio>
int main(){
    std::array<float,14> q{.4f,-.3f,-.6f,.4f,.8f,-1.f,.5f,1.2f,-.2f,-.6f,.9f,-.4f,.1f,.9f};
    const auto original=q;std::array<float,2> torso{.12f,.2f};
    auto left=pepper_arm::forward({q[2],q[3],q[4],q[5]},true);
    auto right=pepper_arm::forward({q[7],q[8],q[9],q[10]},false);
    mirrorPose(q,torso);
    auto mirroredLeft=pepper_arm::forward({q[2],q[3],q[4],q[5]},true);
    auto mirroredRight=pepper_arm::forward({q[7],q[8],q[9],q[10]},false);
    auto reflected=[](pepper_arm::V a,pepper_arm::V b){assert(std::abs(a.x-b.x)<1e-6);assert(std::abs(a.y+b.y)<1e-6);assert(std::abs(a.z-b.z)<1e-6);};
    reflected(left.elbow,mirroredRight.elbow);reflected(left.wrist,mirroredRight.wrist);
    reflected(right.elbow,mirroredLeft.elbow);reflected(right.wrist,mirroredLeft.wrist);
    assert(q[0]==-original[0]&&q[1]==original[1]);assert(q[6]==-original[11]&&q[11]==-original[6]);
    assert(q[12]==original[13]&&q[13]==original[12]);assert(torso[0]==-.12f&&torso[1]==.2f);
    mirrorPose(q,torso);assert(q==original&&torso[0]==.12f);
    std::puts("PASS asymmetric arm FK reflection, head, wrist, hand swap, torso and double reflection");
}
