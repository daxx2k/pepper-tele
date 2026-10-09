#include "../quest/src/main/cpp/network.h"
#include "../quest/src/main/cpp/socket_io.h"
#include <cassert>
#include <chrono>
#include <cstdio>
using json=nlohmann::json;
static void sleepMs(int n){std::this_thread::sleep_for(std::chrono::milliseconds(n));}
// This test exercises the real network transport, without opening audio devices.
void Network::audioLoop(){while(running_)sleepMs(5);}
int main(){
    int listener=socket(AF_INET,SOCK_STREAM,0),yes=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=htons(9570);
    assert(bind(listener,(sockaddr*)&addr,sizeof(addr))==0);assert(listen(listener,4)==0);
    std::atomic<bool> quit{false};std::atomic<int> connections{0},armRequests{0};
    std::thread server([&]{while(!quit){pollfd p{listener,POLLIN,0};if(poll(&p,1,100)<=0)continue;
        int s=accept(listener,nullptr,nullptr);if(s<0)continue;int connection=++connections,count=0;std::string buffer;
        timeval timeout{2,0};setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
        while(!quit){char b[2048];int n=recv(s,b,sizeof(b),0);if(n<=0)break;buffer.append(b,n);size_t end;
            while((end=buffer.find('\n'))!=std::string::npos){auto request=json::parse(buffer.substr(0,end));buffer.erase(0,end+1);
                if(request.value("cmd",std::string())=="arm")++armRequests;
                json reply=count++==0?json{{"session",std::string(32,char('a'+connection))}}:json{{"ok",true},{"armed",false},{"telemetry",json::object()},{"fault",""}};
                if(request.value("cmd",std::string())=="led"||request.value("cmd",std::string())=="arm")reply={{"ok",false},{"armed",false},{"error","Test refusal"}};
                auto wire=reply.dump()+"\n";
                // Fragmented JSON must survive read boundaries.
                send(s,wire.data(),7,MSG_NOSIGNAL);sleepMs(2);send(s,wire.data()+7,wire.size()-7,MSG_NOSIGNAL);
            }
            if(connection==1&&count>=4)break; // Simulated Wi-Fi/control failure.
        }close(s);
    }});
    Network network("127.0.0.1","test-pairing-code");assert(!network.listenAudio&&!network.talk);network.start();
    double deadline=milliseconds()+6000;
    while(network.generation<2&&milliseconds()<deadline)sleepMs(10);
    assert(network.generation>=2&&network.connected&&!network.armed);
    unsigned generation=network.generation;network.requestReconnect();
    deadline=milliseconds()+4000;while(network.generation<=generation&&milliseconds()<deadline)sleepMs(10);
    assert(network.generation>generation&&network.connected&&!network.armed);
    sleepMs(200);assert(network.status().find("Ready")!=std::string::npos);
    network.command({{"cmd","led"}});deadline=milliseconds()+1500;while(network.commandErrors==0&&milliseconds()<deadline)sleepMs(5);
    assert(network.commandErrors==1&&network.motionCommandErrors==0);
    network.requestArm();deadline=milliseconds()+1500;while(network.motionCommandErrors==0&&milliseconds()<deadline)sleepMs(5);
    assert(network.commandErrors==2&&network.motionCommandErrors==1&&!network.armed);
    double began=milliseconds();network.stop();assert(milliseconds()-began<2500);
    quit=true;server.join();close(listener);assert(armRequests==1);
    began=milliseconds();bool rejected=false;try{int s=transport::connectTo("not-an-ip",9570);close(s);}catch(...){rejected=true;}
    assert(rejected&&milliseconds()-began<100);
    std::puts("PASS fragmented replies, dropped connection, manual reconnect, unavailable video isolation, no auto-arm, bounded shutdown");
}
