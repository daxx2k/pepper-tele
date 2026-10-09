#pragma once
#include <cstdint>
// One explicit start gesture; all subsequent steps are bounded and cancellable.
class StartFlow {
public:
    enum Action {None,Prepare,Calibrate,Arm,Finished,Cancelled};
    enum Stage {Idle,Stopping,Waiting,Preparing,Samples,Arming};
    struct Input {
        bool connected=false,focused=false,fresh=false,tracking=false,video=false,neutral=false;
        bool ready=false,preparing=false,prepareFailed=false,calibrated=false,armed=false;
        unsigned connection=0,stopAck=0,errors=0,preparationRevision=0;
        int64_t stopVersion=0,ackStopVersion=0;
    };
    Stage stage=Idle;
    const char* message="";
    bool active()const{return stage!=Idle;}
    int64_t expectedStop()const{return stopVersion_;}
    void begin(double now,const Input& in,bool calibrationOnly=false){
        calibrationOnly_=calibrationOnly;
        connection_=in.connection;stopAck_=in.stopAck;errors_=in.errors;
        stage=Stopping;since_=now;stable_=-1;message="Starting: preparing connection";
    }
    void cancel(const char* why="Start cancelled"){stage=Idle;stable_=-1;message=why;}
    Action tick(double now,const Input& in){
        if(!active())return None;
        if(!in.connected||!in.focused||in.connection!=connection_)return fail("Start cancelled: connection or headset lost");
        if(in.errors!=errors_)return fail("Start refused: check Pepper status");
        if(stage==Stopping){
            if(now-since_>3)return fail("STOP not acknowledged: retry Start");
            if(in.stopAck>stopAck_){stopVersion_=in.ackStopVersion;stage=Waiting;since_=now;}
            return None;
        }
        // The snapshot and STOP acknowledgement are sampled across threads.
        // An older snapshot is not a newer STOP and must not cancel this start.
        if(in.fresh&&in.stopVersion>stopVersion_)return fail("Start cancelled by STOP");
        if(stage==Arming){
            if(!in.tracking||!in.video||!in.neutral)return fail("Start interrupted: tracking, video or sticks");
            // ARM shares the TCP request stream with telemetry. Await its reply
            // within the existing deadline instead of cancelling while that reply
            // is in flight. Tracking, sticks and explicit STOP still cancel immediately.
            if(in.armed&&in.fresh){stage=Idle;message="Puppeteering active";return Finished;}
            if(now-since_>3)return fail("Start not acknowledged: retry Start");
            return None;
        }
        if(stage==Preparing){
            message="Pepper is preparing / B cancels";
            if(in.preparationRevision>preparationRevision_&&!in.preparing&&in.prepareFailed)return fail("Preparation failed: retry Start");
            if(now-since_>45)return fail("Preparation timed out: retry Start");
            // Changing Life state can briefly interrupt camera/sensor telemetry.
            // Finish the explicitly requested wake-up while control stays paused;
            // require fresh tracking/video again before calibration and arming.
            // Explicit STOP, focus loss and disconnect still cancel preparation.
            if(in.preparationRevision>preparationRevision_&&in.ready&&!in.preparing){stage=Waiting;since_=now;stable_=-1;}
            return None;
        }
        if(now-since_>12)return fail("Start timed out: check tracking and centre sticks");
        if(!in.fresh||!in.tracking||!in.video||!in.neutral){stable_=-1;message="Look forward and centre sticks / B cancels";return None;}
        if(stable_<0)stable_=now;
        if(stage==Samples){
            if(!in.calibrated)return fail("Invalid calibration: retry Start");
            if(now-stable_>=.25){if(calibrationOnly_){stage=Idle;message="Full pose calibrated / START resumes motion";return Finished;}stage=Arming;since_=now;message="Engaging control gently";return Arm;}
            return None;
        }
        message="Look forward: automatic calibration";
        if(now-stable_<.5)return None;
        stable_=-1;since_=now;
        if(!in.ready){stage=Preparing;preparationRevision_=in.preparationRevision;message="Pepper is changing posture / B cancels";return Prepare;}
        stage=Samples;message="Calibration saved: starting control";return Calibrate;
    }
private:
    bool calibrationOnly_=false;
    double since_=0,stable_=-1;
    unsigned connection_=0,stopAck_=0,errors_=0,preparationRevision_=0;
    int64_t stopVersion_=0;
    Action fail(const char* why){cancel(why);return Cancelled;}
};
