#pragma once
#include "dashboard_layout.h"
#include "panel_anchor.h"
#include <array>
#include <limits>
namespace floating_layout {
using dashboard_layout::Rect;
constexpr int count=13;
constexpr Rect groupCard(int g){return {dashboard_layout::x[g],dashboard_layout::groupY[g],dashboard_layout::width[g],dashboard_layout::groupHeight[g]};}
constexpr std::array<Rect,count> cards{{dashboard_layout::fusion,dashboard_layout::comparison,dashboard_layout::temperatures,groupCard(4),dashboard_layout::depthCard,dashboard_layout::lidarCard,dashboard_layout::tablet,groupCard(0),groupCard(1),groupCard(2),groupCard(6),{20,0,1360,56},{20,962,1360,98}}};
constexpr std::array<Rect,count> previousCards{{{480,64,460,736},{20,64,440,455},{20,529,440,252},{20,794,440,156},{480,812,156,138},{640,812,144,138},{792,812,148,138},{960,64,420,280},{960,356,420,292},{960,806,420,144},{960,660,420,134},{20,0,1360,56},{20,958,1360,102}}};
inline XrPosef initial(int i){auto r=cards.at(i);return {{0,0,0,1},{(r.x+r.w*.5f)/1400.f-.5f,(.5f-(r.y+r.h*.5f)/1060.f)*1060.f/1400.f,0}};}
inline void migrateDefault(int i,XrPosef& p,float scale){
    if(std::abs(p.position.z)>1e-5f||std::abs(p.orientation.x)+std::abs(p.orientation.y)+std::abs(p.orientation.z)>1e-5f||std::abs(std::abs(p.orientation.w)-1)>1e-5f||std::abs(scale-1)>1e-5f)return;
    auto at=[&](float cx,float cy){return std::abs(p.position.x-(cx/1400.f-.5f))<1e-5f&&std::abs(p.position.y-(530.f-cy)/1400.f)<1e-5f;};
    auto r=previousCards.at(i);if(at(r.x+r.w*.5f,r.y+r.h*.5f))p=initial(i);
}
// Positions are stored in workspace-width units so global resizing keeps the arrangement.
inline XrPosef world(XrPosef root,XrPosef relative,float width){relative.position.x*=width;relative.position.y*=width;relative.position.z*=width;return panel_anchor::compose(root,relative);}
inline XrPosef relative(XrPosef root,XrPosef world,float width){auto p=panel_anchor::compose(panel_anchor::inverse(root),world);p.position.x/=width;p.position.y/=width;p.position.z/=width;return p;}
inline bool valid(XrPosef& p){float q=p.orientation.x*p.orientation.x+p.orientation.y*p.orientation.y+p.orientation.z*p.orientation.z+p.orientation.w*p.orientation.w;if(!std::isfinite(q)||q<.01f)return false;for(float v:{p.position.x,p.position.y,p.position.z})if(!std::isfinite(v)||std::abs(v)>10)return false;float k=1/std::sqrt(q);p.orientation.x*=k;p.orientation.y*=k;p.orientation.z*=k;p.orientation.w*=k;return true;}
inline float distance(XrPosef ray,XrPosef panel){auto r=panel_anchor::compose(panel_anchor::inverse(panel),ray);auto d=panel_anchor::rotate(r.orientation,{0,0,-1});return d.z<-.001f?-r.position.z/d.z:std::numeric_limits<float>::infinity();}
}
