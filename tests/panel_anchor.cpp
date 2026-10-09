#include "../quest/src/main/cpp/panel_anchor.h"
#include <cassert>
#include <cstdio>
int main(){using namespace panel_anchor;
    XrPosef head{{0,.707106781f,0,.707106781f},{2,1.6f,3}};
    auto panel=front(head,1.3f);assert(std::abs(panel.position.x-.7f)<1e-5&&std::abs(panel.position.y-1.6f)<1e-5);
    float u=0,v=0;assert(hit(head,panel,1.2f,.75f,u,v));assert(std::abs(u-.5f)<1e-5&&std::abs(v-.5f)<1e-5);
    auto moved=head;moved.position.y+=.2f;assert(hit(moved,panel,1.2f,.75f,u,v));assert(v<.5f);
    moved.position.y+=1;assert(!hit(moved,panel,1.2f,.75f,u,v));
    moved=head;moved.orientation={0,-.707106781f,0,.707106781f};assert(!hit(moved,panel,1.2f,.75f,u,v));
    moved=head;auto behind=front(head,-1.f);assert(!hit(moved,behind,1.2f,.75f,u,v));
    auto pinned=panel;head.position={6,2,8};assert(panel.position.x==pinned.position.x&&panel.position.z==pinned.position.z);
    // Attaching to the controller must preserve the current panel pose without
    // a jump, then follow controller displacement in room coordinates.
    auto relative=compose(inverse(moved),panel);auto unchanged=compose(moved,relative);
    assert(std::abs(unchanged.position.x-panel.position.x)<1e-5&&std::abs(unchanged.position.z-panel.position.z)<1e-5);
    moved.position.x+=.3f;moved.position.y+=.2f;moved.position.z+=.6f;auto dragged=compose(moved,relative);
    assert(std::abs(dragged.position.x-panel.position.x-.3f)<1e-5&&std::abs(dragged.position.y-panel.position.y-.2f)<1e-5&&std::abs(dragged.position.z-panel.position.z-.6f)<1e-5);
    auto outside=moved;outside.position.y+=2;
    assert(!hit(outside,dragged,1.2f,.75f,u,v));assert(hit(outside,dragged,1.2f,.75f,u,v,false));assert(v<0);
    std::puts("PASS rotated/transformed room anchor, controller ray hits, misses, behind-ray rejection and pinned position");
}
