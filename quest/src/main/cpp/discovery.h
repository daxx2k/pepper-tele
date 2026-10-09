#pragma once
#include "socket_io.h"
#include <json.hpp>
#include <vector>
#include <algorithm>
#include <chrono>
namespace transport {
inline std::vector<std::string> discoverCandidates(int port=9574,int timeoutMs=800,const char* destination="255.255.255.255"){
    std::vector<std::string> hosts;int s=socket(AF_INET,SOCK_DGRAM,0);if(s<0)return hosts;
    int yes=1;setsockopt(s,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));
    sockaddr_in target{};target.sin_family=AF_INET;target.sin_port=htons(port);
    if(inet_pton(AF_INET,destination,&target.sin_addr)!=1){close(s);return hosts;}
    const char request[]="TELEPEPPER_DISCOVER_V1";
    if(sendto(s,request,sizeof(request)-1,0,(sockaddr*)&target,sizeof(target))<0){close(s);return hosts;}
    auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeoutMs);
    while(hosts.size()<8){
        auto left=std::chrono::duration_cast<std::chrono::milliseconds>(until-std::chrono::steady_clock::now()).count();if(left<=0)break;
        pollfd p{s,POLLIN,0};if(poll(&p,1,int(left))<=0)break;
        char buffer[1024];sockaddr_in source{};socklen_t len=sizeof(source);int n=recvfrom(s,buffer,sizeof(buffer),0,(sockaddr*)&source,&len);
        if(n<=0||n>=int(sizeof(buffer)))continue;
        try{auto reply=nlohmann::json::parse(buffer,buffer+n);
            if(reply.value("kind",std::string())!="telepepper-discovery"||reply.value("version",0)!=1||reply.value("control_port",0)!=9570)continue;
            char ip[INET_ADDRSTRLEN];if(!inet_ntop(AF_INET,&source.sin_addr,ip,sizeof(ip)))continue;
            if(std::find(hosts.begin(),hosts.end(),ip)==hosts.end())hosts.emplace_back(ip);
        }catch(...){}
    }
    close(s);return hosts;
}
}
