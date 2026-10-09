#pragma once
#include <cmath>
#include <map>
#include <utility>
#include <vector>

// Device/SubDeviceList laser values use each projected laser frame.
// Transform to base axes exactly once, then apply wheel odometry.
// Frames verified against ros-naoqi/naoqi_driver src/converters/laser.cpp
// and pepper1.0_generated_urdf/pepper_sensors.xacro (older prose is ambiguous).
namespace laser_map {
struct Pose { double x,y,yaw; };
struct Hit { int bank; double x,y; };
struct Point { double x,y,at; };
class Map {
public:
    std::map<std::pair<int,int>,Point> points;
    Pose pose{};
    double last=-1;
    void clear(){points.clear();last=-1;pose={};}
    bool fresh(double now)const{return last>=0&&now>=last&&now-last<.75;}
    bool update(Pose p,const std::vector<Hit>& hits,double at){
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.yaw)||!std::isfinite(at))return false;
        if(last>=0&&at<=last)return false;
        // Reconnect, robot reboot or odometry reset invalidates the old trace.
        if(last>=0&&(at-last>2||std::hypot(p.x-pose.x,p.y-pose.y)>.75))points.clear();
        pose=p;last=at;
        for(auto i=points.begin();i!=points.end();)if(at-i->second.at>60)i=points.erase(i);else ++i;
        const double c=std::cos(p.yaw),s=std::sin(p.yaw);
        for(const auto& h:hits){
            if(h.bank<0||h.bank>2||!std::isfinite(h.x)||!std::isfinite(h.y))continue;
            const double r=std::hypot(h.x,h.y);
            if(r<.05||r>3)continue; // invalid/no-return values are never free-space evidence
            const double angle=h.bank==1?1.75728:h.bank==2?-1.75728:0;
            const double x=h.x*std::cos(angle)-h.y*std::sin(angle)+(h.bank==0?.0562:-.018);
            const double y=h.x*std::sin(angle)+h.y*std::cos(angle)+(h.bank==1?.0899:h.bank==2?-.0899:0);
            Point q{p.x+c*x-s*y,p.y+s*x+c*y,at};
            points[{int(std::floor(q.x/.04)),int(std::floor(q.y/.04))}]=q;
        }
        while(points.size()>4096){auto oldest=points.begin();for(auto i=points.begin();i!=points.end();++i)if(i->second.at<oldest->second.at)oldest=i;points.erase(oldest);}
        return true;
    }
};
}
