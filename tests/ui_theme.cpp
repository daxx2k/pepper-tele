#include "../quest/src/main/cpp/dashboard_theme.h"
#include <cassert>
#include <cstdio>
float luminance(dashboard_theme::Color c){float value=0;float weights[]={.2126f,.7152f,.0722f};for(int i=0;i<3;++i)value+=weights[i]*(c[i]<=.04045f?c[i]/12.92f:std::pow((c[i]+.055f)/1.055f,2.4f));return value;}
float contrast(dashboard_theme::Color a,dashboard_theme::Color b){float x=luminance(a),y=luminance(b);return (std::max(x,y)+.05f)/(std::min(x,y)+.05f);}
int main(){using namespace dashboard_theme;
    auto p=palette();
    assert(contrast(p.primary,p.surface)>=4.5f);assert(contrast(p.secondary,p.surface)>=4.5f);
    assert(contrast(p.primary,p.button)>=4.5f);assert(contrast(rgb(0xffffff),p.selected)>=4.5f);
    for(float c:p.surface)assert(c>=26/255.f&&c<=218/255.f);
    assert(uiColor(text,true)==p.primary);
    Color stop{.75f,.12f,.16f};assert(uiColor(stop)==stop);
    for(auto c:{background,surface,border,text,secondary,accent})for(float v:uiColor(c))assert(std::isfinite(v)&&v>=0&&v<=1);
    std::puts("PASS: single Horizon-inspired palette, readable captions and preserved red STOP");
}
