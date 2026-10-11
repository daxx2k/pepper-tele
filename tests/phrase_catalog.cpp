#include "../quest/src/main/cpp/phrase_catalog.h"
#include <cassert>
#include <cstdio>
int main(){
    using J=nlohmann::json;
    PhraseCatalog catalog;
    auto defaults=catalog.update(J::object());
    assert(defaults.size()==6&&defaults.front()=="Hello, I am Pepper.");
    assert(catalog.update(J::object())==defaults);
    auto custom=catalog.update(J{{"phrases",{"Welcome back.","See you soon."}}});
    assert(custom.size()==2&&custom.front()=="Welcome back.");
    assert(catalog.update(J::object())==custom);
    assert(catalog.update(J{{"phrases",J::array()}})==custom);
    assert(catalog.update(J{{"phrases",{"Partial",42}}})==custom);
    auto reconnected=catalog.update(J{{"phrases",{"Connected again."}}});
    assert(reconnected.size()==1&&reconnected.front()=="Connected again.");
    std::puts("PASS offline startup, disconnect retention, malformed catalogue and reconnect refresh");
}
