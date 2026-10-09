#include "../quest/src/main/cpp/panel_layout.h"
#include <cassert>
#include <cstdio>
int main(){
    // Narrow vertical FOV, asymmetric horizontal FOV, both constrain corners.
    for(float left:{-.45f,-.9f})for(float right:{.5f,1.f})for(float down:{-.3f,-.8f})for(float up:{.4f,.95f}){
        float w=panel_layout::fit(left,right,down,up,1.3f,1.6f),h=w/1.6f;
        assert(w/2<1.3f*std::tan(std::min(-left,right)));
        assert(h/2<1.3f*std::tan(std::min(-down,up)));
    }
    float s=.9f;
    assert(panel_layout::resize(s,.1f,1)==s);
    for(int i=0;i<200;++i)s=panel_layout::resize(s,1,.01f);
    assert(s==1.f);
    for(int i=0;i<200;++i)s=panel_layout::resize(s,-1,.01f);
    assert(s==.65f);
    assert(panel_layout::fit(NAN,.7f,-.7f,.7f,1.3f,1.6f)==1.25f);
    std::puts("PASS panel corners inside asymmetric FOV; continuous resize bounds and dead zone");
}
