#include "../quest/src/main/cpp/battery_status.h"
#include <cassert>
#include <cstdio>
int main(){
    using J=nlohmann::json;using battery_status::connected;
    assert(connected(J{{"battery_plugged",true},{"battery_charging",false}},true)==1);
    assert(connected(J{{"battery_plugged",false},{"battery_charging",true}},true)==0);
    assert(connected(J{{"battery_plugged",1.0}},true)==1);
    assert(connected(J{{"battery_plugged",0}},true)==0);
    assert(connected(J{{"battery_plugged",true}},false)==-1);
    assert(connected(J{{"battery_charging",true}},true)==1);
    assert(connected(J{{"battery_charging",false}},true)==-1);
    for(J value:{J(),J("true"),J(-1),J(2),J::array(),J::object()})assert(connected(J{{"battery_plugged",value}},true)==-1);
    assert(connected(J{{"battery",1.0},{"battery_temperature",30}},true)==-1);
    assert(connected(J::object(),true)==-1);
    std::puts("PASS plugged/full/charging fallback, unplugged precedence, stale, missing and invalid battery data");
}
