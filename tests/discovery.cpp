#include "../quest/src/main/cpp/discovery.h"
#include <cassert>
#include <thread>
#include <cstdio>
int main(){
    int s=socket(AF_INET,SOCK_DGRAM,0);sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_LOOPBACK);local.sin_port=htons(19874);
    assert(bind(s,(sockaddr*)&local,sizeof(local))==0);
    std::thread server([&]{char request[128];sockaddr_in peer{};socklen_t len=sizeof(peer);int n=recvfrom(s,request,sizeof(request),0,(sockaddr*)&peer,&len);
        assert(std::string(request,request+n)=="TELEPEPPER_DISCOVER_V1");
        const char* replies[]={"not json","{\"kind\":\"other\",\"version\":1,\"control_port\":9570}","{\"kind\":\"telepepper-discovery\",\"version\":2,\"control_port\":9570}","{\"kind\":\"telepepper-discovery\",\"version\":1,\"control_port\":22}","{\"kind\":\"telepepper-discovery\",\"version\":1,\"control_port\":9570,\"host\":\"203.0.113.1\"}"};
        for(const char* reply:replies)sendto(s,reply,std::strlen(reply),0,(sockaddr*)&peer,len);
        sendto(s,replies[4],std::strlen(replies[4]),0,(sockaddr*)&peer,len);
    });
    auto hosts=transport::discoverCandidates(19874,200,"127.0.0.1");server.join();close(s);
    assert(hosts.size()==1&&hosts[0]=="127.0.0.1");
    std::puts("PASS discovery protocol, invalid replies, duplicate filtering, source address validation");
}
