#pragma once
#include <array>
#include <cmath>
struct DriveInput {
    bool ready=false;
    static float deadzone(float x){return std::abs(x)<.15f?0.f:std::copysign((std::abs(x)-.15f)/.85f,x);}
    std::array<float,3> update(bool armed,bool active,float lx,float ly,float rx){
        if(!armed||!active){ready=false;return {0,0,0};}
        float x=deadzone(lx),y=deadzone(ly),turn=deadzone(rx);
        if(x==0&&y==0&&turn==0)ready=true;
        if(!ready)return {0,0,0};
        // NAOqi: +X forward, +Y left, +theta counterclockwise (left).
        return {y*.50f,-x*.40f,-turn*.90f};
    }
};
