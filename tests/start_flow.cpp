#include "../quest/src/main/cpp/start_chord.h"
#include "../quest/src/main/cpp/start_flow.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
StartFlow::Input input(){StartFlow::Input i;i.connected=i.focused=i.fresh=i.tracking=i.video=i.neutral=true;i.connection=1;return i;}
void acknowledge(StartFlow& f,StartFlow::Input& i){i.stopAck++;i.stopVersion=i.ackStopVersion=1;assert(f.tick(.1,i)==StartFlow::None);}
int main(){
    {StartFlow f;auto i=input();i.ready=true;f.begin(0,i,true);acknowledge(f,i);
     assert(f.tick(.2,i)==StartFlow::None);assert(f.tick(.71,i)==StartFlow::Calibrate);
     i.calibrated=true;assert(f.tick(.72,i)==StartFlow::None);
     assert(f.tick(1.0,i)==StartFlow::Finished);assert(!f.active());assert(!i.armed);}
    StartChord chord;
    assert(!chord.update(0,true,false,true));
    assert(!chord.update(.1,true,true,true));
    assert(!chord.update(.8,true,true,true));
    assert(chord.update(.86,true,true,true));
    assert(!chord.update(2,true,true,true));
    assert(!chord.update(3,false,true,true));
    assert(!chord.update(4,true,true,true)); // both buttons must be released
    chord.update(5,false,false,true);chord.update(6,true,true,true);
    assert(chord.update(6.8,true,true,true));
    chord.update(7,false,false,true);chord.update(8,true,true,false);
    assert(!chord.update(9,true,true,true)); // no latent start after focus loss
    chord.update(10,false,false,true);chord.update(11,true,true,true);
    assert(chord.update(11.8,true,true,true));
    std::puts("PASS hold A+X duration, single toggle, release gate and focus/connection suppression");

    StartFlow f;auto i=input();f.begin(0,i);acknowledge(f,i);
    f.tick(.2,i);assert(f.tick(.71,i)==StartFlow::Prepare);
    // Old failures cannot cancel the newly requested preparation before its ACK.
    i.prepareFailed=true;assert(f.tick(.72,i)==StartFlow::None);assert(f.active());
    i.preparationRevision=1;i.prepareFailed=false;i.preparing=true;f.tick(.8,i);
    i.tracking=i.video=i.fresh=false;
    assert(f.tick(.9,i)==StartFlow::None&&f.stage==StartFlow::Preparing);
    i.tracking=i.video=i.fresh=true;
    i.preparing=false;i.ready=true;f.tick(1,i);f.tick(1.1,i);
    assert(f.tick(1.61,i)==StartFlow::Calibrate);i.calibrated=true;f.tick(1.62,i);
    assert(f.tick(1.88,i)==StartFlow::Arm);assert(f.expectedStop()==1);
    i.armed=true;assert(f.tick(1.9,i)==StartFlow::Finished);assert(!f.active());
    // Already prepared: skip wake-up but recalibrate and send fresh samples.
    i=input();i.ready=true;f.begin(0,i);acknowledge(f,i);f.tick(.2,i);
    assert(f.tick(.71,i)==StartFlow::Calibrate);i.calibrated=true;
    f.tick(.72,i);i.stopVersion++;assert(f.tick(.8,i)==StartFlow::Cancelled);
    // STOP ACK can be newer than the separately sampled telemetry snapshot.
    i=input();i.ready=true;f.begin(0,i);acknowledge(f,i);
    i.stopVersion=0;assert(f.tick(.2,i)==StartFlow::None&&f.active());
    i.stopVersion=1;assert(f.tick(.71,i)==StartFlow::Calibrate);i.calibrated=true;
    f.tick(.72,i);assert(f.tick(.99,i)==StartFlow::Arm);
    // Motor ARM RPC briefly blocks telemetry; keep waiting, never report success
    // until fresh armed telemetry arrives, and retain the bounded timeout.
    i.fresh=false;assert(f.tick(1.8,i)==StartFlow::None&&f.active());
    i.armed=true;assert(f.tick(1.9,i)==StartFlow::None&&f.active());
    i.fresh=true;assert(f.tick(2,i)==StartFlow::Finished);
    i=input();f.begin(0,i);acknowledge(f,i);f.stage=StartFlow::Arming;
    i.fresh=false;assert(f.tick(3.2,i)==StartFlow::Cancelled);
    // Losing focus or reconnecting cannot leave a latent start request.
    i=input();f.begin(0,i);i.connection=2;assert(f.tick(.1,i)==StartFlow::Cancelled);
    i=input();f.begin(0,i);i.focused=false;assert(f.tick(.1,i)==StartFlow::Cancelled);
    // Held sticks never start preparation or arm the robot.
    i=input();f.begin(0,i);acknowledge(f,i);i.neutral=false;
    for(int n=0;n<10;++n)assert(f.tick(.2+n,i)==StartFlow::None);
    assert(f.tick(13,i)==StartFlow::Cancelled);
    // STOP and command failures cancel every stage, including arming.
    for(auto stage:{StartFlow::Waiting,StartFlow::Preparing,StartFlow::Samples,StartFlow::Arming}){
        i=input();f.begin(0,i);acknowledge(f,i);f.stage=stage;i.stopVersion++;
        assert(f.tick(.3,i)==StartFlow::Cancelled);
    }
    i=input();f.begin(0,i);acknowledge(f,i);i.errors++;assert(f.tick(.3,i)==StartFlow::Cancelled);
    i=input();f.begin(0,i);f.cancel();assert(f.tick(1,i)==StartFlow::None);
    std::puts("PASS one-click start, skip unnecessary preparation, fresh calibration, STOP at every stage, disconnect/focus/error cancellation, held sticks");
}
