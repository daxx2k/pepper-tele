#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace dashboard_theme {
using Color=std::array<float,3>;
constexpr Color background{.035f,.038f,.047f},surface{.055f,.10f,.15f},border{.14f,.26f,.35f};
constexpr Color button{.11f,.22f,.31f},selected{.17f,.40f,.57f},text{.93f,.94f,.96f},secondary{.64f,.68f,.74f};
constexpr Color accent{.20f,.55f,1.f};
constexpr Color section[]={ {.23f,.64f,1.f},{.70f,.53f,1.f},{.28f,.79f,.72f},{.25f,.64f,1.f},{.32f,.73f,.91f},{.75f,.60f,1.f},{1.f,.60f,.35f}};
constexpr Color rgb(unsigned value){return {((value>>16)&255)/255.f,((value>>8)&255)/255.f,(value&255)/255.f};}
struct Palette {Color background,surface,border,button,selected,primary,secondary,accent;};
inline Palette palette(){
    return {rgb(0x1a1a1a),rgb(0x333333),rgb(0x54575e),rgb(0x414348),rgb(0x0064e0),rgb(0xebebeb),rgb(0xb4b4b4),rgb(0x0064e0)};
}
// Media/semantic colours bypass theming. Explicit cards/buttons use palette roles.
inline Color uiColor(Color c,bool label=false){
    auto p=palette();float hi=std::max({c[0],c[1],c[2]}),lo=std::min({c[0],c[1],c[2]});
    if(label){
        if(hi-lo<.35f)return hi>=.85f?p.primary:p.secondary;
        return c;
    }
    if(hi<.5f)return hi<.08f?p.background:hi<.18f?p.surface:p.button;
    return c;
}
inline Color batteryColor(float fraction){
    if(!std::isfinite(fraction))return secondary;
    constexpr Color red{1.f,.27f,.30f},amber{1.f,.75f,.20f},green{.22f,.84f,.40f};
    float t=std::max(0.f,std::min(2.f,(fraction-.2f)/.3f));
    const auto& a=t<1?red:amber;const auto& b=t<1?amber:green;float blend=t<1?t:t-1;
    return {a[0]+(b[0]-a[0])*blend,a[1]+(b[1]-a[1])*blend,a[2]+(b[2]-a[2])*blend};
}
}
