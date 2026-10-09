#include "../quest/src/main/cpp/ack_watchdog.h"
#include <cassert>
#include <cstdio>
#include <limits>
int main(){using namespace ack_watchdog;
 assert(receipt(1000,16,40,true,true)==944);
 assert(!expired(1050,800,944)); // Lost UDP ACK, fresh confirmed commands on TCP.
 assert(expired(1150,800,944)); // Both confirmations expire at the same bound.
 assert(expired(1050,800,receipt(1000,250,40,true,true)));
 assert(expired(1050,800,receipt(1000,16,300,true,true)));
 assert(receipt(1000,16,40,false,true)==0);assert(receipt(1000,16,40,true,false)==0);
 assert(receipt(1000,std::numeric_limits<double>::quiet_NaN(),40,true,true)==0);
 assert(!expired(1000,990,0));
 std::puts("PASS fresh TCP confirmation covers UDP ACK loss; stale, delayed and invalid replies still expire");
}
