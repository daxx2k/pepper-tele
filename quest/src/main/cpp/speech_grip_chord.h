#pragma once
#include <cmath>
// A deliberate hold of both grips. Singles retain PTT and pointer use.
class SpeechGripChord {
    bool left_=false,right_=false,latched_=false,consumed_=false;
    double leftAt_=-100,rightAt_=-100,began_=-1;
public:
    bool suppress=false,entered=false;
    bool update(double now,bool left,bool right,bool enabled){
        entered=false;
        if(!enabled){left_=left;right_=right;latched_=left||right;consumed_=false;began_=-1;suppress=false;return false;}
        if(left&&!left_)leftAt_=now;
        if(right&&!right_)rightAt_=now;
        left_=left;right_=right;
        if(!left&&!right){latched_=false;consumed_=false;began_=-1;suppress=false;return false;}
        if(latched_){suppress=consumed_;return false;}
        bool together=left&&right;
        if(!together){began_=-1;suppress=false;return false;}
        entered=began_<0;
        if(entered)began_=now;
        suppress=true;
        if(now-began_<.35)return false;
        latched_=true;consumed_=true;return true;
    }
};
