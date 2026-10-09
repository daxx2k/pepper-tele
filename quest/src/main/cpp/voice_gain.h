#pragma once
#include <cmath>
#include <cstdint>

namespace voice_gain {
// Piper-only +12 dB. A memoryless soft knee keeps boosted peaks within PCM16
// without wrapping, buffering, or changing the robot's shared speaker volume.
inline int16_t piper(int16_t sample){
    float value=4.f*sample/32768.f,magnitude=std::abs(value);
    if(magnitude>.8f)magnitude=.8f+.2f*std::tanh((magnitude-.8f)/.2f);
    float limited=std::copysign(magnitude,value)*32767.f;
    return static_cast<int16_t>(std::lround(limited));
}
}
