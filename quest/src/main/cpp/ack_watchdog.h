#pragma once
#include <algorithm>
#include <cmath>
namespace ack_watchdog {
// Conservatively subtract the entire round trip, not half. A stale TCP reply
// cannot make stale motion look fresh; telemetry must confirm valid armed input.
inline double receipt(double at,double remoteAge,double roundTrip,bool armed,bool tracking){
    if(!armed||!tracking||!std::isfinite(at)||!std::isfinite(remoteAge)||!std::isfinite(roundTrip)||remoteAge<0||roundTrip<0)return 0;
    return std::max(0.,at-remoteAge-roundTrip);
}
inline bool expired(double now,double udpReceipt,double tcpReceipt){return now-std::max(udpReceipt,tcpReceipt)>200.;}
}
