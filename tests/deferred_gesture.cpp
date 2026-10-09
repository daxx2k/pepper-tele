#include "../quest/src/main/cpp/deferred_gesture.h"
#include <cassert>
#include <cstdio>
int main(){DeferredGesture g;g.begin("nod");
    assert(g.tick(false,false,false,"",0)==g.None&&g.active());
    assert(g.tick(true,false,true,"",1)==g.Send);
    assert(g.tick(true,false,true,"",1.1)==g.None); // never queue twice
    assert(g.tick(false,false,true,"nod",1.2)==g.None&&!g.active());
    g.begin("wave_left");assert(g.tick(false,true,false,"",2)==g.None&&!g.active());
    assert(g.tick(true,false,true,"",3)==g.None); // STOP cannot resurrect it
    g.begin("nod");g.tick(true,false,true,"",4);assert(g.tick(false,false,true,"",6.1)==g.Stop&&!g.active());
    g.begin("nod");g.tick(true,false,true,"",7);assert(g.tick(false,false,false,"",7.1)==g.Stop);
    g.begin("nod");g.cancel();assert(g.tick(true,false,true,"",8)==g.None);
    std::puts("PASS deferred animation: one dispatch after arm, acknowledgement, STOP, failure and timeout cancellation");
}
