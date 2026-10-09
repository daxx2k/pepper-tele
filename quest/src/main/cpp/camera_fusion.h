#pragma once
#include <cmath>
#include <vector>
#include "dashboard_layout.h"
namespace camera_fusion {
// Nominal Pepper optics: 57.2 x 44.3 deg, Bottom pitched down by 40 deg.
// Spherical projection ignores camera translation: nearby objects can have parallax.
constexpr float radians=.01745329252f;
constexpr float horizontal=57.2f,vertical=44.3f,bottomPitch=40.f;
constexpr float minYaw=-50.f,maxYaw=50.f,minElevation=-63.f,maxElevation=23.f;
struct Point {float yaw,elevation;};
inline Point ray(int camera,float u,float v){
    float x=(2*u-1)*std::tan(horizontal*.5f*radians),y=(1-2*v)*std::tan(vertical*.5f*radians),z=1;
    float angle=(camera==1?bottomPitch:0)*radians,c=std::cos(angle),s=std::sin(angle);
    float ry=y*c-z*s,rz=y*s+z*c;
    return {std::atan2(x,rz)/radians,std::atan2(ry,std::sqrt(x*x+rz*rz))/radians};
}
inline dashboard_layout::Rect viewport(dashboard_layout::Rect bounds){return dashboard_layout::contain(bounds,100,86);}
inline std::vector<float> mesh(int camera,dashboard_layout::Rect bounds){
    const auto r=viewport(bounds);std::vector<float> result;constexpr int cols=32,rows=24;result.reserve(cols*rows*24);
    auto vertex=[&](float u,float v){auto p=ray(camera,u,v);float x=r.x+(p.yaw-minYaw)/(maxYaw-minYaw)*r.w,y=r.y+(maxElevation-p.elevation)/(maxElevation-minElevation)*r.h;
        result.insert(result.end(),{2*x/1600-1,1-2*y/1060,u,v});};
    for(int row=0;row<rows;++row)for(int col=0;col<cols;++col){float u=float(col)/cols,v=float(row)/rows,du=1.f/cols,dv=1.f/rows;
        vertex(u,v+dv);vertex(u+du,v+dv);vertex(u+du,v);vertex(u,v+dv);vertex(u+du,v);vertex(u,v);}
    return result;
}
}
