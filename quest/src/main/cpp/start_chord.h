#pragma once
#include <algorithm>
class StartChord {
    double began_=-1;
    bool latched_=false;
public:
    float progress=0;
    bool update(double now,bool a,bool x,bool enabled){
        if(!enabled){began_=-1;progress=0;latched_=true;return false;}
        if(!a&&!x){latched_=false;began_=-1;progress=0;return false;}
        if(latched_||!a||!x){began_=-1;progress=0;return false;}
        if(began_<0)began_=now;
        progress=float(std::max(0.,std::min(1.,(now-began_)/.75)));
        if(now-began_<.75)return false;
        latched_=true;began_=-1;progress=0;return true;
    }
};
