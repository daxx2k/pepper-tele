#pragma once
#include "panel_anchor.h"
#include <cmath>
namespace pointer_ray {
inline XrVector3f sub(XrVector3f a,XrVector3f b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline float dot(XrVector3f a,XrVector3f b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline XrVector3f cross(XrVector3f a,XrVector3f b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline XrVector3f unit(XrVector3f v){float n=std::sqrt(dot(v,v));return n>1e-6f?XrVector3f{v.x/n,v.y/n,v.z/n}:XrVector3f{1,0,0};}
inline XrVector3f endpoint(XrPosef panel,float width,float height,float u,float v){auto p=panel_anchor::rotate(panel.orientation,{(u-.5f)*width,(.5f-v)*height,0});return {panel.position.x+p.x,panel.position.y+p.y,panel.position.z+p.z};}
inline bool ribbon(XrVector3f start,XrVector3f end,XrVector3f eye,XrPosef& pose,float& length){
    auto delta=sub(end,start);length=std::sqrt(dot(delta,delta));
    if(!std::isfinite(length)||length<.01f||length>10)return false;
    pose.position={(start.x+end.x)*.5f,(start.y+end.y)*.5f,(start.z+end.z)*.5f};
    auto y=unit(delta),toward=sub(eye,pose.position),x=cross(y,toward);
    if(dot(x,x)<1e-8f)x=cross(y,std::abs(y.x)<.9f?XrVector3f{1,0,0}:XrVector3f{0,1,0});
    x=unit(x);auto z=cross(x,y);float trace=x.x+y.y+z.z,s;
    auto& q=pose.orientation;
    if(trace>0){s=std::sqrt(trace+1)*2;q={(y.z-z.y)/s,(z.x-x.z)/s,(x.y-y.x)/s,.25f*s};}
    else if(x.x>y.y&&x.x>z.z){s=std::sqrt(1+x.x-y.y-z.z)*2;q={.25f*s,(y.x+x.y)/s,(z.x+x.z)/s,(y.z-z.y)/s};}
    else if(y.y>z.z){s=std::sqrt(1+y.y-x.x-z.z)*2;q={(y.x+x.y)/s,.25f*s,(z.y+y.z)/s,(z.x-x.z)/s};}
    else{s=std::sqrt(1+z.z-x.x-y.y)*2;q={(z.x+x.z)/s,(z.y+y.z)/s,.25f*s,(x.y-y.x)/s};}
    return true;
}
}
