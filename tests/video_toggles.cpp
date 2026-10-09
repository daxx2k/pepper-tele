#include "../quest/src/main/cpp/network.h"
#include "../quest/src/main/cpp/socket_io.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <functional>
using json=nlohmann::json;
static void sleepMs(int ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));}
void Network::audioLoop(){while(running_)sleepMs(5);}
static int listenAt(int port){int s=socket(AF_INET,SOCK_STREAM,0),yes=1;setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(port);assert(bind(s,(sockaddr*)&a,sizeof(a))==0);assert(listen(s,8)==0);return s;}
static bool until(std::function<bool()> f){auto end=milliseconds()+2500;while(milliseconds()<end){if(f())return true;sleepMs(10);}return f();}
int main(){
    int control=listenAt(9570),video=listenAt(9572);std::atomic<bool> quit{false};std::atomic<int> controls{0};std::array<std::atomic<int>,3> opened{},closed{};
    std::thread ctl([&]{while(!quit){pollfd p{control,POLLIN,0};if(poll(&p,1,50)<=0)continue;int s=accept(control,nullptr,nullptr);++controls;std::string buffer;bool first=true;
        while(!quit){pollfd ready{s,POLLIN,0};if(poll(&ready,1,50)<=0)continue;char bytes[4096];int n=recv(s,bytes,sizeof(bytes),0);if(n<=0)break;buffer.append(bytes,n);size_t end;
            while((end=buffer.find('\n'))!=std::string::npos){buffer.erase(0,end+1);json r=first?json{{"session",std::string(32,'a')}}:json{{"ok",true},{"armed",false},{"fault",""},{"telemetry",json::object()}};first=false;auto line=r.dump()+"\n";send(s,line.data(),line.size(),MSG_NOSIGNAL);}}
        close(s);}});
    std::vector<std::thread> handlers;
    std::thread media([&]{while(!quit){pollfd p{video,POLLIN,0};if(poll(&p,1,50)<=0)continue;int s=accept(video,nullptr,nullptr);handlers.emplace_back([&,s]{std::string hello;int camera=-1;
        while(!quit){pollfd p{s,POLLIN,0};if(poll(&p,1,50)<=0)continue;char bytes[2048];int n=recv(s,bytes,sizeof(bytes),0);if(n<=0)break;hello.append(bytes,n);if(camera<0&&hello.find('\n')!=std::string::npos){camera=json::parse(hello.substr(0,hello.find('\n'))).value("camera",-1);assert(camera>=0&&camera<3);++opened[camera];}}
        if(camera>=0)++closed[camera];close(s);});}});
    Network net("127.0.0.1","test-pairing-code");assert(net.cameraStreaming(0)&&net.cameraStreaming(1)&&!net.cameraStreaming(2));net.start();
    assert(until([&]{return opened[0]>=1&&opened[1]>=1&&net.connected.load();}));unsigned gen=net.generation;
    net.setCameraStreaming(0,false);assert(until([&]{return closed[0]>=1;}));assert(net.cameraStreaming(1)&&!net.cameraStreaming(0));
    net.setCameraStreaming(1,false);assert(until([&]{return closed[1]>=1;}));assert(!net.anyVideoEnabled());
    net.setDepthStreaming(true);assert(until([&]{return opened[2]>=1;}));assert(net.anyVideoEnabled());
    net.setDepthStreaming(false);assert(until([&]{return closed[2]>=1;}));
    net.setCameraStreaming(0,true);assert(until([&]{return opened[0]>=2;}));
    assert(net.connected&&net.generation==gen&&!net.armed&&controls==1);net.stop();quit=true;ctl.join();media.join();for(auto& h:handlers)h.join();close(control);close(video);
    std::puts("PASS actual transport: camera toggles close/reopen media sockets independently without reconnecting or arming control");
}
