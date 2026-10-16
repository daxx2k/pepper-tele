#pragma once
#include <array>
#include <cmath>
#include <algorithm>

// Pepper 1.0 URDF, ros-naoqi/pepper_robot: meters, X forward/Y left/Z up.
// Includes the lateral elbow offset and fixed -9 degree elbow-frame pitch.
namespace pepper_arm {
struct V { float x,y,z; };
inline V add(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V sub(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V scale(V a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline float length(V a){return std::sqrt(dot(a,a));}
inline float bound(float v,float a,float b){return std::max(a,std::min(b,v));}
inline V rx(V v,float a){return {v.x,std::cos(a)*v.y-std::sin(a)*v.z,std::sin(a)*v.y+std::cos(a)*v.z};}
inline V ry(V v,float a){return {std::cos(a)*v.x+std::sin(a)*v.z,v.y,-std::sin(a)*v.x+std::cos(a)*v.z};}
inline V rz(V v,float a){return {std::cos(a)*v.x-std::sin(a)*v.y,std::sin(a)*v.x+std::cos(a)*v.y,v.z};}
using Q=std::array<float,4>;
struct Pose {V elbow,wrist;};
inline Pose forward(Q q,bool left){
    V elbow=ry(rz({.1812f,left?.015f:-.015f,.00013f},q[1]),q[0]);
    V fore=ry(rz(ry(rx(rz({.15f,0,0},q[3]),q[2]),-.157079f),q[1]),q[0]);
    return {elbow,add(elbow,fore)};
}
// Compatibility fallback from Pepper's published native joint ranges.
// Live ALMotion.getLimits values replace this profile for a connected robot.
struct Limits { Q lo,hi; };
inline Limits nativeLimits(bool left){
    return {{-2.0857f,left?.0087f:-1.5620f,-2.0857f,left?-1.5620f:.0087f},
            {2.0857f,left?1.5620f:-.0087f,2.0857f,left?-.0087f:1.5620f}};
}
inline Q clamp(Q q,const Limits& limits){
    for(int i=0;i<4;++i)q[i]=bound(q[i],limits.lo[i],limits.hi[i]);
    return q;
}
inline Q clamp(Q q,bool left){return clamp(q,nativeLimits(left));}
inline float wrap(float a){return std::atan2(std::sin(a),std::cos(a));}
inline Q directionSeed(V upper,V fore,bool left,const Limits& limits){
    upper=scale(upper,1.f/std::max(length(upper),.0001f));
    fore=scale(fore,1.f/std::max(length(fore),.0001f));
    const float lateral=left?.015f:-.015f;
    const float armLength=std::sqrt(.1812f*.1812f+.015f*.015f+.00013f*.00013f);
    float a=std::asin(bound(upper.y*armLength/std::hypot(.1812f,lateral),-1.f,1.f));
    Q best{};float score=1e10f;
    // Keep both shoulder branches. asin(z) folds the arm back below vertical;
    // atan2 retains elevation and the second roll branch handles lateral poses.
    for(float branch:{a,3.14159265359f-a}){
        float roll=bound(wrap(branch-std::atan2(lateral,.1812f)),limits.lo[1],limits.hi[1]);
        float projectedX=.1812f*std::cos(roll)-lateral*std::sin(roll);
        float pitch=(std::hypot(upper.x,upper.z)<.001f||(std::abs(upper.z)<.001f&&std::abs(projectedX)<.01f))?0.f:
            wrap(std::atan2(.00013f,projectedX)-std::atan2(upper.z,upper.x));
        pitch=bound(pitch,limits.lo[0],limits.hi[0]);
        V f=ry(rz(ry(fore,-pitch),-roll),.157079f);
        float sign=left?-1.f:1.f;
        float bend=sign*std::acos(bound(f.x,-1.f,1.f));
        float yaw=std::abs(std::sin(bend))>.0001f?std::atan2(sign*f.z,sign*f.y):0.f;
        Q q=clamp({pitch,roll,yaw,bend},limits);Pose p=forward(q,left);
        V e=sub(scale(p.elbow,1.f/armLength),upper);
        V w=sub(scale(sub(p.wrist,p.elbow),1.f/.15f),fore);
        float c=dot(e,e)+dot(w,w);
        if(c<score){best=q;score=c;}
    }
    return best;
}
inline Q directionSeed(V upper,V fore,bool left){return directionSeed(upper,fore,left,nativeLimits(left));}
// Preserve each tracked bone direction independently of the user's proportions.
inline Pose retarget(V upper,V fore){
    V elbow=scale(upper,.1818198f/std::max(length(upper),.001f));
    return {elbow,add(elbow,scale(fore,.15f/std::max(length(fore),.001f)))};
}
// Blend toward a hand-to-head goal near the face. The headset approximates
// the user's eye centre; robot face centre is 10 cm above its head joint.
struct FaceGoal {V wrist;float blend,elbowWeight;};
inline FaceGoal faceGoal(Pose mapped,V handToHead,float humanForeLength,bool left,float yaw,float pitch){
    float t=bound((.45f-length(handToHead))/.25f,0,1);t=t*t*(3-2*t);
    V head=add({.019f,left?-.14974f:.14974f,.08308f},rz(ry({0,0,.10f},pitch),yaw));
    float ratio=bound(.15f/std::max(humanForeLength,.10f),.3f,.9f);
    V near=add(head,scale(handToHead,ratio));
    return {add(scale(mapped.wrist,1-t),scale(near,t)),t,1-t*.98f};
}
inline float cost(Q q,bool left,V wrist,V elbow,float elbowWeight=.015f){
    Pose p=forward(q,left);V a=sub(p.wrist,wrist),b=sub(p.elbow,elbow);
    return dot(a,a)+elbowWeight*dot(b,b);
}
inline Q solve(V wrist,V elbow,bool left,Q seed,float elbowWeight,const Limits& limits){
    const float weight=std::sqrt(elbowWeight);
    Q q=clamp(seed,limits);
    for(int iteration=0;iteration<18;++iteration){
        Pose p=forward(q,left);V e=sub(wrist,p.wrist),ee=scale(sub(elbow,p.elbow),weight);
        float err[6]={e.x,e.y,e.z,ee.x,ee.y,ee.z},j[6][4]{};
        for(int k=0;k<4;++k){Q d=q;d[k]+=.001f;Pose pp=forward(d,left);
            V a=scale(sub(pp.wrist,p.wrist),1000),b=scale(sub(pp.elbow,p.elbow),1000.f*weight);
            j[0][k]=a.x;j[1][k]=a.y;j[2][k]=a.z;j[3][k]=b.x;j[4][k]=b.y;j[5][k]=b.z;
        }
        float a[4][5]{};
        for(int r=0;r<4;++r){for(int c=0;c<4;++c){for(int k=0;k<6;++k)a[r][c]+=j[k][r]*j[k][c];if(r==c)a[r][c]+=.0001f;}
            for(int k=0;k<6;++k)a[r][4]+=j[k][r]*err[k];}
        for(int k=0;k<4;++k){int pivot=k;for(int r=k+1;r<4;++r)if(std::abs(a[r][k])>std::abs(a[pivot][k]))pivot=r;
            for(int c=k;c<5;++c)std::swap(a[k][c],a[pivot][c]);
            float divisor=a[k][k];for(int c=k;c<5;++c)a[k][c]/=divisor;
            for(int r=0;r<4;++r)if(r!=k){float f=a[r][k];for(int c=k;c<5;++c)a[r][c]-=f*a[k][c];}}
        bool improved=false;float old=cost(q,left,wrist,elbow,elbowWeight);
        for(float step:{1.f,.5f,.25f}){Q candidate=q;for(int k=0;k<4;++k)candidate[k]+=step*bound(a[k][4],-.25f,.25f);
            candidate=clamp(candidate,limits);if(cost(candidate,left,wrist,elbow,elbowWeight)<old){q=candidate;improved=true;break;}}
        if(!improved)break;
    }
    return q;
}
inline Q solve(V wrist,V elbow,bool left,Q seed,float weight=.015f){return solve(wrist,elbow,left,seed,weight,nativeLimits(left));}
inline Q solveFace(V wrist,V elbow,bool left,Q seed,float weight,const Limits& limits){
    Q best=solve(wrist,elbow,left,seed,weight,limits);float score=cost(best,left,wrist,elbow,weight);
    float sign=left?1.f:-1.f;
    for(float yaw:{-1.5f,0.f,1.5f}){
        Q q=solve(wrist,elbow,left,{-1.2f,sign*.2f,yaw,-sign*1.3f},weight,limits);
        float c=cost(q,left,wrist,elbow,weight);if(c<score){best=q;score=c;}
    }
    return best;
}
inline Q solveFace(V wrist,V elbow,bool left,Q seed,float weight){return solveFace(wrist,elbow,left,seed,weight,nativeLimits(left));}

}
