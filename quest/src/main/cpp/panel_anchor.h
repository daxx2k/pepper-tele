#pragma once
#include <openxr/openxr.h>
#include <cmath>
#include <algorithm>
namespace panel_anchor {
inline XrQuaternionf multiply(XrQuaternionf a,XrQuaternionf b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
inline XrVector3f rotate(XrQuaternionf q,XrVector3f v){
    XrVector3f t{2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
    return {v.x+q.w*t.x+q.y*t.z-q.z*t.y,v.y+q.w*t.y+q.z*t.x-q.x*t.z,v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
// Independent cards share this root in either LOCAL (pinned) or VIEW (follow).
inline XrPosef workspace(bool locked,bool pending,XrPosef room){return locked&&!pending?room:XrPosef{{0,0,0,1},{0,0,-1.3f}};}
// Positive stick Y pushes a grabbed surface along the controller's -Z ray.
inline XrPosef depth(XrPosef relative,float stick,float dt){
    if(!std::isfinite(stick)||!std::isfinite(dt)||std::abs(stick)<.2f)return relative;
    relative.position.z=std::max(-4.f,std::min(-.35f,relative.position.z-stick*std::max(0.f,std::min(.05f,dt))*.65f));
    return relative;
}
inline XrPosef front(XrPosef head,float distance){auto offset=rotate(head.orientation,{0,0,-distance});return {head.orientation,{head.position.x+offset.x,head.position.y+offset.y,head.position.z+offset.z}};}
inline XrPosef compose(XrPosef a,XrPosef b){auto p=rotate(a.orientation,b.position);return {multiply(a.orientation,b.orientation),{a.position.x+p.x,a.position.y+p.y,a.position.z+p.z}};}
inline XrPosef inverse(XrPosef a){XrQuaternionf q{-a.orientation.x,-a.orientation.y,-a.orientation.z,a.orientation.w};return {q,rotate(q,{-a.position.x,-a.position.y,-a.position.z})};}
inline bool hit(XrPosef ray,XrPosef panel,float width,float height,float& u,float& v,bool clip=true){
    XrQuaternionf inverse{-panel.orientation.x,-panel.orientation.y,-panel.orientation.z,panel.orientation.w};
    auto p=rotate(inverse,{ray.position.x-panel.position.x,ray.position.y-panel.position.y,ray.position.z-panel.position.z});
    auto d=rotate(inverse,rotate(ray.orientation,{0,0,-1}));
    if(d.z>=-.001f)return false;float t=-p.z/d.z;if(t<0)return false;
    u=.5f+(p.x+t*d.x)/width;v=.5f-(p.y+t*d.y)/height;
    return !clip||(u>=0&&u<1&&v>=0&&v<1);
}
}
