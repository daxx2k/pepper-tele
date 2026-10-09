#include "../quest/src/main/cpp/pointer_ray.h"
#include <cassert>
#include <cstdio>
bool close(float a,float b){return std::abs(a-b)<1e-4f;}
int main(){
 XrPosef panel{{0,0,0,1},{0,0,-1.3f}},beam{};float length;
 auto end=pointer_ray::endpoint(panel,1,.8f,.8f,.25f);assert(close(end.x,.3f)&&close(end.y,.2f)&&close(end.z,-1.3f));
 XrVector3f start{.2f,-.2f,-.3f};assert(pointer_ray::ribbon(start,end,{0,0,0},beam,length));
 auto up=panel_anchor::rotate(beam.orientation,{0,length*.5f,0});assert(close(beam.position.x+up.x,end.x)&&close(beam.position.y+up.y,end.y)&&close(beam.position.z+up.z,end.z));
 auto down=panel_anchor::rotate(beam.orientation,{0,-length*.5f,0});assert(close(beam.position.x+down.x,start.x)&&close(beam.position.y+down.y,start.y)&&close(beam.position.z+down.z,start.z));
 assert(pointer_ray::ribbon({0,0,0},{0,0,-1},{0,0,0},beam,length));assert(std::isfinite(beam.orientation.w));
 assert(!pointer_ray::ribbon(start,start,{0,0,0},beam,length));
 std::puts("PASS controller ray endpoints, camera-facing strip and degenerate poses");
}
