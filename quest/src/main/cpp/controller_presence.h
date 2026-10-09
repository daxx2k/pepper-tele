#pragma once
// Never automatically resume robot motion after a controller was put down.
// Capacitive transitions have a short grace period; controller loss has none.
struct ControllerPresence {
    bool bothHeld=false,stopSent=false;
    double missingSince=-1;
    bool update(double now,bool left,bool right,bool controllersActive=true){
        if(!controllersActive){bothHeld=false;missingSince=now;if(!stopSent){stopSent=true;return true;}return false;}
        if(left&&right){bothHeld=true;missingSince=-1;stopSent=false;return false;}
        if(missingSince<0)missingSince=now;
        if(now-missingSince<.8)return false;
        bothHeld=false;
        if(!stopSent){stopSent=true;return true;}
        return false;
    }
};
