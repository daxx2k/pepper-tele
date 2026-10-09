#include "../quest/src/main/cpp/voice_gain.h"
#include <cassert>
#include <cstdio>
int main(){
    using voice_gain::piper;
    assert(piper(0)==0&&piper(1000)==4000&&piper(-1000)==-4000);
    assert(piper(5000)>=19999&&piper(5000)<=20000);
    int previous=-32768;
    for(int sample=-32768;sample<=32767;++sample){
        int value=piper(static_cast<int16_t>(sample));
        assert(value>=previous&&value>=-32767&&value<=32767);previous=value;
        if(sample>=-32767)assert(value==-piper(static_cast<int16_t>(-sample)));
    }
    assert(piper(32767)>32000&&piper(-32768)<-32000);
    puts("PASS Piper 4x gain, silence, symmetry, monotonic soft limiting and PCM16 bounds");
}
