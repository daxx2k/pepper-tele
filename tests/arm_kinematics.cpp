#include "../quest/src/main/cpp/head_assist.h"
#include "../quest/src/main/cpp/arm_kinematics.h"
#include "../quest/src/main/cpp/drive_input.h"
#include "../quest/src/main/cpp/laser_map.h"
#include <cassert>
#include <cstdio>
#include <chrono>
using namespace pepper_arm;
int main(){
    laser_map::Map map;
    assert(map.update({0,0,0},{{0,1,0},{1,1,0},{2,1,0}},1));
    assert(map.points.size()==3&&map.fresh(1.5)&&!map.fresh(1.8));
    // A forward reading in each side camera must point left/right in base axes.
    auto left=map.points.at({-6,26});auto right=map.points.at({-6,-27});
    assert(left.y>1.07&&right.y<-1.07&&std::abs(left.y+right.y)<1e-6);
    map.clear();map.update({2,3,1.57079632679},{{0,1,0}},2);
    auto q=map.points.begin()->second;assert(std::abs(q.x-2)<1e-6&&std::abs(q.y-4.0562)<1e-6);
    assert(!map.update({2,3,0},{{0,1,0}},2));
    map.update({2,3,0},{{0,NAN,0},{0,0,0},{3,1,0},{0,20,0}},2.1);assert(map.points.size()==1);
    map.update({10,3,0},{},2.2);assert(map.points.empty()); // odometry reset
    for(int i=0;i<65;++i)map.update({0,0,0},i==0?std::vector<laser_map::Hit>{{0,1,0}}:std::vector<laser_map::Hit>{},3+i);
    assert(map.points.empty());
    std::puts("PASS laser axes, odometry rotation/reset, stale data, invalid returns, trace expiry");
    auto assist=headAssistance(.2f,0);assert(assist[0]==0&&assist[1]==0);
    assist=headAssistance(1.f,.5f);assert(std::abs(assist[0]-.15f)<1e-6&&std::abs(assist[1]-.2f)<1e-6);
    assist=headAssistance(-1.f,-.5f);assert(std::abs(assist[0]+.15f)<1e-6&&std::abs(assist[1]+.2f)<1e-6);
    // Robot coordinates have +Y to its left. Positive logical right lean must
    // place the top of the torso on its right; hardware HipRoll is negated server-side.
    auto rightLean=rx({0,0,.3f},headAssistance(0,.3f)[0]);assert(rightLean.y<0);
    auto leftLean=rx({0,0,.3f},headAssistance(0,-.3f)[0]);assert(leftLean.y>0);
    std::puts("PASS head assistance: neutral, down/up extension and lateral caps");
    DriveInput drive;
    auto v=drive.update(true,true,0,1,0);assert(v[0]==0&&!drive.ready);
    drive.update(true,true,0,0,0);assert(drive.ready);
    v=drive.update(true,true,0,1,0);assert(v[0]>.49f&&v[1]==0&&v[2]==0);
    v=drive.update(true,true,0,-1,0);assert(v[0]<-.49f);
    v=drive.update(true,true,1,0,0);assert(v[1]<-.39f);
    v=drive.update(true,true,-1,0,0);assert(v[1]>.39f);
    v=drive.update(true,true,0,0,1);assert(v[2]<-.89f);
    v=drive.update(true,true,0,0,-1);assert(v[2]>.89f);
    v=drive.update(true,true,0,0,0);assert(v[0]==0&&v[1]==0&&v[2]==0);
    v=drive.update(true,false,0,1,1);assert(v[0]==0&&v[2]==0&&!drive.ready);
    drive.update(true,true,0,0,0);
    v=drive.update(false,true,0,1,1);assert(v[0]==0&&v[2]==0&&!drive.ready);
    // Independently calculated zero-pose geometry includes the fixed elbow pitch.
    auto zero=forward({0,0,0,0},true);
    assert(std::abs(zero.wrist.x-(.1812f+.15f*std::cos(.157079f)))<1e-6f);
    assert(std::abs(zero.wrist.y-.015f)<1e-6f);
    assert(std::abs(zero.wrist.z-(.00013f+.15f*std::sin(.157079f)))<1e-6f);
    // Human horizontal arms around full lateral extension: the tiny model
    // elbow offset must not produce a downward pitch or flip across 90 degrees.
    for(bool isLeft:{true,false}){
        Q previous{};bool havePrevious=false;
        for(float degrees:{80.f,85.f,89.f,89.9f,90.f,90.1f,91.f}){
            float a=degrees*.01745329252f,sign=isLeft?1.f:-1.f;
            V upper{.3f*std::cos(a),sign*.3f*std::sin(a),0};
            V elbow=scale(upper,.18182f/.3f),wrist=scale(upper,.33182f/.3f);
            Q seed=directionSeed(upper,upper,isLeft);
            // Near the expanded lateral limit a small pitch compensates the
            // 0.13 mm fixed offset; actual bone elevation is checked below.
            assert(std::abs(seed[0])<.1f);
            Q q=solve(wrist,elbow,isLeft,seed,1.f);
            if(havePrevious){
                Q continuous=solve(wrist,elbow,isLeft,previous,1.f);
                if(cost(continuous,isLeft,wrist,elbow,1.f)<=cost(q,isLeft,wrist,elbow,1.f)+.000025f)q=continuous;
                if(degrees>=89.9f)assert(std::abs(q[0]-previous[0])<.1f);
            }
            auto pose=forward(q,isLeft);
            assert(std::abs(std::atan2(pose.elbow.z,std::hypot(pose.elbow.x,pose.elbow.y)))<.035f);
            assert(std::abs(std::atan2(pose.wrist.z,std::hypot(pose.wrist.x,pose.wrist.y)))<.035f);
            previous=q;havePrevious=true;
        }
    }
    std::puts("PASS horizontal T-pose elevation and continuity across 90 degrees, both arms");
    // Different human upper/forearm proportions must map to the same robot
    // configuration when bone directions match (old whole-arm scaling failed).
    for(bool side:{true,false})for(float pitch:{-.6f,.2f,1.1f})for(float yaw:{-1.f,.3f,1.1f}){
        Q reference{pitch,side?.65f:-.65f,yaw,side?-1.1f:1.1f};auto expected=forward(reference,side);
        for(float upperLength:{.22f,.31f,.4f})for(float foreLength:{.18f,.27f,.36f}){
            V upper=scale(expected.elbow,upperLength/length(expected.elbow));
            V fore=scale(sub(expected.wrist,expected.elbow),foreLength/.15f);
            auto target=retarget(upper,fore);
            auto actual=forward(solve(target.wrist,target.elbow,side,directionSeed(upper,fore,side),1.f),side);
            assert(length(sub(actual.elbow,expected.elbow))<.002f);
            assert(length(sub(actual.wrist,expected.wrist))<.002f);
        }
    }
    std::puts("PASS unequal human bone lengths preserve robot elbow and wrist directions");
    int count=0;float worst=0;
    auto began=std::chrono::steady_clock::now();
    for(bool left:{true,false})for(float pitch:{-1.f,-.4f,.3f,1.2f})for(float roll:{.1f,.5f,1.f})for(float yaw:{-1.2f,0.f,1.2f})for(float bend:{.2f,.8f,1.4f}){
        Q q={pitch,left?roll:-roll,yaw,left?-bend:bend};auto p=forward(q,left);
        auto seed=directionSeed(p.elbow,sub(p.wrist,p.elbow),left);
        auto solved=solve(p.wrist,p.elbow,left,seed);
        float error=length(sub(forward(solved,left).wrist,p.wrist));worst=std::max(worst,error);
        assert(error<.002f);++count;
    }
    // Bent arm with hand beside the head: bone scaling alone leaves the wrist
    // low; a face goal lifts it without changing the far-from-face mapping.
    for(bool left:{true,false}){
        float sign=left?1.f:-1.f;
        V upper{.06f,sign*.12f,-.20f},fore{-.02f,sign*.02f,.27f};
        auto mapped=retarget(upper,fore);
        V delta{.06f,sign*.16f,0};
        auto goal=faceGoal(mapped,delta,length(fore),left,0,0);
        assert(goal.blend==1&&goal.wrist.z>mapped.wrist.z+.10f);
        auto q=solveFace(goal.wrist,mapped.elbow,left,directionSeed(upper,fore,left),goal.elbowWeight);
        auto result=forward(q,left);
        assert(result.wrist.z>mapped.wrist.z+.08f);
        assert(length(sub(result.wrist,goal.wrist))<.04f);
        auto far=faceGoal(mapped,{.6f,0,0},length(fore),left,0,0);
        assert(far.blend==0&&length(sub(far.wrist,mapped.wrist))<1e-6f);
    }
    std::puts("PASS face-relative wave reach, both arms, distant-pose preservation");
    // Same hand goal on opposite sides must produce mirrored robot positions.
    V goal{.16f,.055f,.12f},elbow{.12f,.06f,-.12f};
    auto l=solve(goal,elbow,true,{.5f,.2f,-1.f,-1.f});
    auto r=solve({goal.x,-goal.y,goal.z},{elbow.x,-elbow.y,elbow.z},false,{.5f,-.2f,1.f,1.f});
    auto lp=forward(l,true).wrist,rp=forward(r,false).wrist;
    assert(length(sub(lp,{rp.x,-rp.y,rp.z}))<.002f);
    std::printf("Near-face goal error %.2f mm\n",length(sub(lp,goal))*1000);
    // This goal is inside the minimum reach with the conservative elbow limit;
    // report the residual, rather than claiming exact full human workspace.
    assert(length(sub(lp,goal))<.026f);
    // Impossible goals remain finite and inside the existing conservative limits.
    for(bool left:{true,false})for(V goal2:{V{2,0,2},V{0,0,0},V{-.5f,.5f,-.5f}}){
        auto q=solve(goal2,{0,0,-.18f},left,{1.3f,left?.15f:-.15f,0,left?-.5f:.5f});
        auto bounded=clamp(q,left);for(int k=0;k<4;++k){assert(std::isfinite(q[k]));assert(q[k]==bounded[k]);}
    }
    auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
    std::printf("PASS %d round trips, mirror symmetry, near-face goal, unreachable bounds; worst %.3f mm; %.3f ms/solve\n",count,worst*1000,ms/(count+8));
}
