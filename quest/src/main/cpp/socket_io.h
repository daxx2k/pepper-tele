#pragma once
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <stdexcept>
#include <string>
namespace transport {
inline int connectTo(const std::string& host,int port,int type=SOCK_STREAM,int deadlineMs=1500){
    int s=socket(AF_INET,type,0);if(s<0)throw std::runtime_error("Socket unavailable");
    try{
        sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(port);
        if(inet_pton(AF_INET,host.c_str(),&addr.sin_addr)!=1)throw std::runtime_error("Invalid Pepper address: open Connection settings");
        int flags=fcntl(s,F_GETFL,0);
        if(flags<0||fcntl(s,F_SETFL,flags|O_NONBLOCK)<0)throw std::runtime_error("Socket setup failed");
        if(connect(s,(sockaddr*)&addr,sizeof(addr))<0){
            if(errno!=EINPROGRESS)throw std::runtime_error("Pepper unreachable: check network and robot service");
            pollfd p{s,POLLOUT,0};int ready=poll(&p,1,deadlineMs),error=0;socklen_t size=sizeof(error);
            if(ready<=0||getsockopt(s,SOL_SOCKET,SO_ERROR,&error,&size)<0||error)
                throw std::runtime_error("Pepper did not respond: retrying automatically");
        }
        if(fcntl(s,F_SETFL,flags)<0)throw std::runtime_error("Socket setup failed");
        timeval timeout{2,0};setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
        int yes=1;if(type==SOCK_STREAM){setsockopt(s,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(yes));setsockopt(s,SOL_SOCKET,SO_KEEPALIVE,&yes,sizeof(yes));}
        return s;
    }catch(...){close(s);throw;}
}
}
