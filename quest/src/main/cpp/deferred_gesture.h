#pragma once
#include <string>
// An animation click can prepare/calibrate first. STOP always discards the intent.
struct DeferredGesture {
    enum Action {None,Send,Stop};
    std::string name;
    bool submitted=false;
    double deadline=0;
    bool active() const {return !name.empty();}
    void begin(const std::string& value){name=value;submitted=false;deadline=0;}
    void cancel(){name.clear();submitted=false;}
    Action tick(bool finished,bool cancelled,bool armed,const std::string& observed,double now){
        if(cancelled){cancel();return None;}
        if(!active())return None;
        if(finished&&!submitted){submitted=true;deadline=now+2;return Send;}
        if(submitted){
            if(observed==name){cancel();return None;}
            if(!armed||now>deadline){cancel();return Stop;}
        }
        return None;
    }
};
