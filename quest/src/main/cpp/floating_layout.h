#pragma once
#include "dashboard_layout.h"
#include "panel_anchor.h"
#include <array>
#include <limits>
namespace floating_layout {
using dashboard_layout::Rect;
constexpr int count=11;
constexpr std::array<Rect,count> cards{{dashboard_layout::fusion,dashboard_layout::comparison,dashboard_layout::temperatures,{20,794,440,156},dashboard_layout::depthCard,dashboard_layout::lidarCard,{960,64,420,280},{960,356,420,292},{960,660,420,134},{20,0,1360,56},{20,958,1360,102}}};
inline XrPosef initial(int i){auto r=cards.at(i);return {{0,0,0,1},{(r.x+r.w*.5f)/1400.f-.5f,(.5f-(r.y+r.h*.5f)/1060.f)*1060.f/1400.f,0}};}
// Positions are stored in workspace-width units so global resizing keeps the arrangement.
inline XrPosef world(XrPosef root,XrPosef relative,float width){relative.position.x*=width;relative.position.y*=width;relative.position.z*=width;return panel_anchor::compose(root,relative);}
inline XrPosef relative(XrPosef root,XrPosef world,float width){auto p=panel_anchor::compose(panel_anchor::inverse(root),world);p.position.x/=width;p.position.y/=width;p.position.z/=width;return p;}
inline bool valid(XrPosef& p){float q=p.orientation.x*p.orientation.x+p.orientation.y*p.orientation.y+p.orientation.z*p.orientation.z+p.orientation.w*p.orientation.w;if(!std::isfinite(q)||q<.01f)return false;for(float v:{p.position.x,p.position.y,p.position.z})if(!std::isfinite(v)||std::abs(v)>10)return false;float k=1/std::sqrt(q);p.orientation.x*=k;p.orientation.y*=k;p.orientation.z*=k;p.orientation.w*=k;return true;}
inline float distance(XrPosef ray,XrPosef panel){auto r=panel_anchor::compose(panel_anchor::inverse(panel),ray);auto d=panel_anchor::rotate(r.orientation,{0,0,-1});return d.z<-.001f?-r.position.z/d.z:std::numeric_limits<float>::infinity();}
}
