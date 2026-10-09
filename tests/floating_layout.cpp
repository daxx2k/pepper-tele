#include "../quest/src/main/cpp/floating_layout.h"
#include <cassert>
#include <cstdio>
bool near(float a,float b){return std::abs(a-b)<1e-5f;}
int main(){
    using namespace floating_layout;
    XrPosef root{{0,.70710678f,0,.70710678f},{2,1.6f,3}};
    XrPosef grab{{0,0,0,1},{.1f,.2f,-1}};
    auto away=panel_anchor::depth(grab,1,.05f),closer=panel_anchor::depth(grab,-1,.05f);
    assert(away.position.z<grab.position.z&&closer.position.z>grab.position.z);
    assert(near(away.position.x,grab.position.x)&&near(closer.position.y,grab.position.y));
    for(int n=0;n<1000;++n)grab=panel_anchor::depth(grab,1,.05f);assert(near(grab.position.z,-4));
    for(int n=0;n<1000;++n)grab=panel_anchor::depth(grab,-1,.05f);assert(near(grab.position.z,-.35f));
    auto follow=panel_anchor::workspace(false,false,root);assert(near(follow.position.z,-1.3f)&&near(follow.position.x,0));
    auto pinned=panel_anchor::workspace(true,false,root);assert(near(pinned.position.x,root.position.x));
    auto pending=panel_anchor::workspace(true,true,root);assert(near(pending.position.z,-1.3f));
    auto card=initial(4),inView=world(follow,card,1.25f),inRoom=world(root,card,1.25f);
    auto viewRoundTrip=relative(follow,inView,1.25f),roomRoundTrip=relative(root,inRoom,1.25f);
    assert(near(viewRoundTrip.position.x,roomRoundTrip.position.x)&&near(viewRoundTrip.position.z,roomRoundTrip.position.z));
    for(int i=0;i<count;++i){
        auto r=cards[i];assert(r.x>=0&&r.y>=0&&r.x+r.w<=1400&&r.y+r.h<=1060);
        auto p=initial(i),w=world(root,p,1.25f),back=relative(root,w,1.25f);
        assert(near(p.position.x,back.position.x)&&near(p.position.y,back.position.y)&&near(p.position.z,back.position.z));
        auto ray=w;auto dz=panel_anchor::rotate(w.orientation,{0,0,.7f});ray.position.x+=dz.x;ray.position.y+=dz.y;ray.position.z+=dz.z;
        float u,v;assert(panel_anchor::hit(ray,w,1.25f*r.w/1400,1.25f*r.h/1400,u,v));assert(near(u,.5f)&&near(v,.5f)&&near(distance(ray,w),.7f));
        auto grab=panel_anchor::compose(panel_anchor::inverse(ray),w);ray.position.y+=.35f;auto dragged=panel_anchor::compose(ray,grab);
        assert(near(dragged.position.y,w.position.y+.35f));
        auto saved=relative(root,dragged,1.25f);auto restored=world(root,saved,1.25f);assert(near(restored.position.y,dragged.position.y));
    }
    XrPosef bad{{0,0,0,0},{0,0,0}};assert(!valid(bad));bad={{0,0,0,2},{0,0,0}};assert(valid(bad)&&near(bad.orientation.w,1));bad.position.x=INFINITY;assert(!valid(bad));bad.position.x=11;assert(!valid(bad));
    std::puts("PASS: tablet-free atlas crops, rotated workspace round trips, ray mapping, no-jump 3D grabs and invalid preset poses");
}
