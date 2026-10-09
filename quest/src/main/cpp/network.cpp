#include "network.h"
#include "socket_io.h"
#include "discovery.h"
#include "voice_gain.h"
#include "ack_watchdog.h"
#include <android/log.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <chrono>
#include <time.h>
#include <stdexcept>
#include <algorithm>
#include <json.hpp>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include <stb_image.h>
using json=nlohmann::json;
double milliseconds(){
    // Android CLOCK_MONOTONIC/steady_clock excludes suspend. Pepper keeps
    // running while the headset sleeps: that offset would poison stale checks.
    timespec now{};
    if(clock_gettime(CLOCK_BOOTTIME,&now)!=0)throw std::runtime_error("Boot clock unavailable");
    return now.tv_sec*1000.0+now.tv_nsec/1000000.0;
}
static void pauseMs(int n){std::this_thread::sleep_for(std::chrono::milliseconds(n));}
using transport::connectTo;
static void sendAll(int s,const void* data,size_t size){const char* p=(const char*)data;while(size){ssize_t n=send(s,p,size,MSG_NOSIGNAL);if(n<0&&errno==EINTR)continue;if(n<=0)throw std::runtime_error("Connection lost");p+=n;size-=n;}}
static void readAll(int s,void* data,size_t size){char* p=(char*)data;while(size){ssize_t n=recv(s,p,size,0);if(n<0&&errno==EINTR)continue;if(n<=0)throw std::runtime_error("Connection timeout");p+=n;size-=n;}}
static json readJson(int s,std::string& pending){
    while(true){auto end=pending.find('\n');if(end!=std::string::npos){auto line=pending.substr(0,end);pending.erase(0,end+1);return json::parse(line);}
        if(pending.size()>=262144)throw std::runtime_error("Invalid reply");
        char block[4096];ssize_t n=recv(s,block,sizeof(block),0);if(n<0&&errno==EINTR)continue;if(n<=0)throw std::runtime_error("Connection lost: reconnecting");pending.append(block,size_t(n));}
}
static void writeJson(int s,const json& j){auto text=j.dump()+"\n";sendAll(s,text.data(),text.size());}
Network::Network(std::string h,std::string t):host_(std::move(h)),token_(std::move(t)){}
Network::~Network(){stop();}
void Network::start(){if(running_.exchange(true))return;control_=std::thread(&Network::controlLoop,this);motion_=std::thread(&Network::motionLoop,this);for(int c=0;c<3;++c)videos_[c]=std::thread(&Network::videoLoop,this,c);audio_=std::thread(&Network::audioLoop,this);voice_=std::thread(&Network::voiceLoop,this);}
void Network::stop(){running_=false;requestReconnect();talk=false;poseWake_.notify_all();if(control_.joinable())control_.join();if(motion_.joinable())motion_.join();for(auto& v:videos_)if(v.joinable())v.join();if(audio_.joinable())audio_.join();if(voice_.joinable())voice_.join();}
void Network::publish(const PoseCommand& p){{std::lock_guard<std::mutex> g(mutex_);pose_=p;}poseWake_.notify_one();}
void Network::requestArm(int64_t expectedStop){if(connected){armExpectedStop_=expectedStop;armStopEpoch_=stopEpoch_.load();stopMotion_=false;request_=1;}}
void Network::requestStop(bool returnToNeutral){++stopEpoch_;cancelVoiceAudio();armed=false;stopMotion_=true;feedbackDelayed_=false;request_=returnToNeutral?3:2;
    {std::lock_guard<std::mutex> g(mutex_);commands_.erase(std::remove_if(commands_.begin(),commands_.end(),[](const json& c){auto cmd=c.value("cmd",std::string());return cmd=="prepare_motion"||cmd=="gesture";}),commands_.end());}
    poseWake_.notify_one();}
void Network::requestReconnect(){requestStop();reconnect_=true;lastVideo=0;
    std::lock_guard<std::mutex> g(socketsMutex_);if(controlSocket_>=0)shutdown(controlSocket_,SHUT_RDWR);for(int v:videoSockets_)if(v>=0)shutdown(v,SHUT_RDWR);
}
void Network::setDepthStreaming(bool enabled){setCameraStreaming(2,enabled);}
void Network::setCameraStreaming(int camera,bool enabled){
    if(camera<0||camera>2)return;
    if(camera==0)topStreaming=enabled;else if(camera==1)bottomStreaming=enabled;else depthStreaming=enabled;
    if(!enabled){std::lock_guard<std::mutex> g(socketsMutex_);if(videoSockets_[camera]>=0)shutdown(videoSockets_[camera],SHUT_RDWR);}
}
void Network::setStatus(const std::string& s){std::lock_guard<std::mutex> g(mutex_);status_=s;}
std::string Network::currentHost(){std::lock_guard<std::mutex> g(mutex_);return host_;}
std::string Network::status(){std::lock_guard<std::mutex> g(mutex_);return status_;}
std::string Network::audioStatus(){std::lock_guard<std::mutex> g(mutex_);return std::string(listenAudio?"Listen ON | ":"Listen OFF | ")+audioStatus_;}
json Network::snapshot(){std::lock_guard<std::mutex> g(mutex_);return snapshot_.is_object()?snapshot_:json::object();}
void Network::cancelVoiceAudio(){voiceCancellation++;{std::lock_guard<std::mutex> g(mutex_);voicePcm_.clear();voicePosition_=0;}{std::lock_guard<std::mutex> g(socketsMutex_);if(voiceSocket_>=0)shutdown(voiceSocket_,SHUT_RDWR);}}
bool Network::voiceAudioPending(){std::lock_guard<std::mutex> g(mutex_);return voiceBusy_||!voicePcm_.empty();}
void Network::queueVoiceAudio(std::vector<int16_t> pcm,std::string text){if(pcm.size()>960000)return;for(auto& sample:pcm)sample=voice_gain::piper(sample);std::lock_guard<std::mutex> g(mutex_);if(!connected||!audioFocus||voiceMode!=2)return;voicePcm_=std::move(pcm);voiceText_=std::move(text);voicePosition_=0;voiceSessionGeneration_=generation;}
void Network::command(const json& request){std::lock_guard<std::mutex> g(mutex_);if(connected&&commands_.size()<16)commands_.push_back(request);}
bool Network::takeFrame(VideoFrame& out,int camera){std::lock_guard<std::mutex> g(frameMutex_);if(camera<0||camera>2||frames_[camera].pixels.empty())return false;out=std::move(frames_[camera]);return true;}
void Network::controlLoop(){
    double messageUntil=0,lastDiscovery=-10000;std::string actionMessage;
    while(running_){int s=-1;reconnect_=false;++reconnectAttempts;try{
        std::string pending;
        s=connectTo(currentHost(),9570);
        // A slow motor RPC must not tear down the pilot TCP session at 2 seconds.
        // This is a receive deadline, not buffering: UDP tracking/watchdog and
        // the three-second START deadline remain independent and unchanged.
        timeval controlTimeout{5,0};setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&controlTimeout,sizeof(controlTimeout));
        {std::lock_guard<std::mutex> g(socketsMutex_);controlSocket_=s;}writeJson(s,{{"token",token_}});auto hello=readJson(s,pending);
        if(!hello.contains("session"))throw std::runtime_error(hello.value("error","Pairing failed"));
        {std::lock_guard<std::mutex> g(mutex_);session_=hello["session"].get<std::string>();}
        stopMotion_=true;request_=0;feedbackDelayed_=false;tcpMotionReceipt_=0;lastReceipt=milliseconds();lastVideo=0;lastAck=0;rtt=0;messageUntil=0;++generation;connected=true;armed=false;setStatus(hello.value("simulation",false)?"SIMULATION / press Start":"Connected / press Start");
        while(running_&&!reconnect_){int req=request_.exchange(0);json request={{"cmd",req==1?"arm":(req==2||req==3)?"stop":"status"}};
            if(req==3)request["return_to_neutral"]=true;
            if(req==1){request["stop_epoch"]=armStopEpoch_.load();if(armExpectedStop_>=0)request["expected_stop_generation"]=armExpectedStop_.load();}
            if(req==0){std::lock_guard<std::mutex> g(mutex_);if(!commands_.empty()){request=commands_.front();commands_.pop_front();}}
            double began=milliseconds();
            if(request.value("cmd",std::string())!="status")__android_log_print(ANDROID_LOG_INFO,"TelePepper","Control request %s generation=%u",request.value("cmd",std::string()).c_str(),generation.load());
            writeJson(s,request);auto reply=readJson(s,pending);controlRtt=milliseconds()-began;
            if(req||controlRtt>500)__android_log_print(ANDROID_LOG_INFO,"TelePepper","Control reply %s ok=%d armed=%d duration_ms=%.0f stop_version=%lld",request.value("cmd",std::string()).c_str(),reply.value("ok",true),reply.value("armed",false),controlRtt.load(),(long long)reply.value("motion_diagnostics",json::object()).value("stop_generation",int64_t(0)));
            auto diagnostics=reply.value("motion_diagnostics",json::object());
            if(diagnostics.contains("command_age_ms")&&diagnostics["command_age_ms"].is_number()){
                robotCommandAgeMs=diagnostics["command_age_ms"].get<double>();
                double confirmed=ack_watchdog::receipt(milliseconds(),diagnostics["command_age_ms"].get<double>(),controlRtt.load(),reply.value("armed",false),diagnostics.value("tracking_valid",false));
                if(confirmed>tcpMotionReceipt_)tcpMotionReceipt_=confirmed;
            }
            if(reply.contains("telemetry")){std::lock_guard<std::mutex> g(mutex_);snapshot_=reply;lastSnapshot=milliseconds();camera=reply.value("camera",0);view=reply.value("view",std::string("panel"))=="wide"?1:0;}
            if(req==2&&reply.value("ok",false)){
                acknowledgedStopVersion=reply.value("motion_diagnostics",json::object()).value("stop_generation",int64_t(0));++stopAcknowledged;
            }
            bool nowArmed=reply.value("armed",false);bool previouslyArmed=armed.exchange(nowArmed);
            if(nowArmed&&!previouslyArmed){lastAck=milliseconds();lastReceipt=milliseconds();feedbackDelayed_=false;}
            if(!reply.value("ok",true)){
                ++commandErrors;
                const auto failedCommand=request.value("cmd",std::string());
                if(failedCommand=="arm"||failedCommand=="stop"||failedCommand=="prepare_motion")++motionCommandErrors;
                if(failedCommand=="arm"||failedCommand=="stop"||failedCommand=="prepare_motion"||failedCommand=="gesture")
                    __android_log_print(ANDROID_LOG_WARN,"TelePepper","Motion action %s refused: %s",failedCommand.c_str(),reply.value("error",std::string("Action failed")).c_str());
                actionMessage=reply.value("error",std::string("Action failed"));messageUntil=milliseconds()+8000;setStatus(actionMessage);
            }
            else if(milliseconds()<messageUntil)setStatus(actionMessage);
            else if(!reply.value("fault",std::string()).empty())setStatus(reply["fault"].get<std::string>());
            else setStatus(armed?(feedbackDelayed_?"ACTIVE / feedback delayed":"ACTIVE / B stop"):"Ready / press Start");
            for(int i=0;i<10 && running_ && request_==0;++i)pauseMs(10);
        }
        writeJson(s,{{"cmd","stop"}});
    }catch(const std::exception& e){
        __android_log_print(ANDROID_LOG_WARN,"TelePepper","Control disconnected: %s generation=%u last_reply_age_ms=%.0f udp_receipt_age_ms=%.0f",e.what(),generation.load(),milliseconds()-lastSnapshot.load(),milliseconds()-lastReceipt.load());
        setStatus(e.what());
    }
    connected=false;cancelVoiceAudio();armed=false;stopMotion_=true;lastVideo=0;{std::lock_guard<std::mutex> g(mutex_);session_.clear();commands_.clear();snapshot_=json();}
    {std::lock_guard<std::mutex> g(socketsMutex_);controlSocket_=-1;if(s>=0)close(s);}
    // Discovery only runs while disconnected. A broadcast reply is a hint;
    // verify the saved pairing code through an observer connection before
    // changing the robot address. Reconnection always remains disarmed.
    if(running_&&milliseconds()-lastDiscovery>5000){
        lastDiscovery=milliseconds();
        for(const auto& candidate:transport::discoverCandidates()){
            if(!running_)break;int check=-1;
            try{check=connectTo(candidate,9570,SOCK_STREAM,300);timeval shortWait{0,300000};setsockopt(check,SOL_SOCKET,SO_RCVTIMEO,&shortWait,sizeof(shortWait));setsockopt(check,SOL_SOCKET,SO_SNDTIMEO,&shortWait,sizeof(shortWait));
                writeJson(check,{{"token",token_},{"role","operator"}});std::string pending;auto answer=readJson(check,pending);close(check);check=-1;
                if(answer.contains("observer")){{std::lock_guard<std::mutex> guard(mutex_);host_=candidate;}setStatus("Paired Pepper found: reconnecting");break;}
            }catch(...){if(check>=0)close(check);}
        }
    }
    for(int i=0;i<10&&running_&&!reconnect_;++i)pauseMs(100);
    }
}
void Network::motionLoop(){int s=-1;uint64_t seq=0;double lastSample=-1,lastSent=0;bool lastStop=true;unsigned socketGeneration=0;double nextConnect=0,lastRepair=0;
    while(running_){double now=milliseconds();
        if(connected){
            // Rebind only while disarmed and TCP confirms the robot is not
            // receiving commands. Missing return ACKs alone do not pause motion.
            if(s>=0&&!armed&&now-lastSnapshot<750&&robotCommandAgeMs>500&&now-lastRepair>1000){
                __android_log_print(ANDROID_LOG_WARN,"TelePepper","Rebinding stale UDP motion channel robot_age_ms=%.0f",robotCommandAgeMs.load());
                close(s);s=-1;lastRepair=now;
            }
            if(s<0||socketGeneration!=generation){if(s>=0){close(s);s=-1;}if(now<nextConnect){pauseMs(10);continue;}
                try{s=connectTo(currentHost(),9571,SOCK_DGRAM);socketGeneration=generation;lastSample=-1;lastSent=0;}catch(...){nextConnect=now+500;continue;}}
            PoseCommand p;std::string session;{std::lock_guard<std::mutex> g(mutex_);p=pose_;session=session_;}
            now=milliseconds();
            bool fresh=now-p.sampled<100;
            // Send tracking validity independently of arm status. No auto-arm is possible.
            if(p.sampled!=lastSample||now-lastSent>=20||lastStop!=stopMotion_){
                json packet={{"session",session},{"seq",++seq},{"sent",now},{"sample_age_ms",std::max(0.,now-p.sampled)},{"angles",p.angles},{"base",p.base},{"active",p.active&&fresh},{"drive",p.drive&&fresh},{"auto_turn",p.autoTurn&&fresh},{"body_yaw",p.bodyYaw},{"torso",p.torso},{"torso_assist",p.torsoAssist&&fresh}};
                lastStop=stopMotion_;packet["stop"]=lastStop;packet["stop_epoch"]=stopEpoch_.load();
                packet["sticks_neutral"]=p.sticksNeutral;packet["pause_reason"]=p.pauseReason;
                auto text=packet.dump();ssize_t sent=send(s,text.data(),text.size(),MSG_NOSIGNAL);
                if(sent!=ssize_t(text.size())){
                    __android_log_print(ANDROID_LOG_WARN,"TelePepper","UDP motion send failed errno=%d; reconnecting channel",errno);
                    close(s);s=-1;nextConnect=now+100;continue;
                }
                lastSample=p.sampled;lastSent=now;
            }
            char buffer[1024];ssize_t n;
            while((n=recv(s,buffer,sizeof(buffer),MSG_DONTWAIT))>0){try{auto a=json::parse(buffer,buffer+n);lastReceipt=milliseconds();
                if(a.value("kind",std::string())!="received"){rtt=milliseconds()-a.at("sent").get<double>();applyMs=a.at("apply_ms").get<double>();queueMs=a.value("queue_ms",0.);sampleAgeMs=a.value("sample_age_ms",0.);lastAck=milliseconds();}
            }catch(...){}}
            // ACKs are diagnostic, not a second motion watchdog. Pepper validates
            // fresh tracking and stops locally after 180 ms without valid commands.
            // Lost return packets must not pause an otherwise fresh input stream.
            bool delayed=armed&&lastReceipt>0&&ack_watchdog::expired(now,lastReceipt.load(),tcpMotionReceipt_.load())&&!stopMotion_;
            if(delayed&&!feedbackDelayed_.exchange(true))__android_log_print(ANDROID_LOG_WARN,"TelePepper","Feedback delayed; robot input watchdog remains authoritative udp_age_ms=%.0f tcp_age_ms=%.0f",now-lastReceipt.load(),now-tcpMotionReceipt_.load());
            if(!delayed)feedbackDelayed_=false;
        }
        std::unique_lock<std::mutex> g(mutex_);poseWake_.wait_for(g,std::chrono::milliseconds(2));
    }close(s);
}
void Network::voiceLoop(){while(running_){
    std::vector<int16_t> pcm;std::string session,text;unsigned cancellation=0;
    {std::lock_guard<std::mutex> g(mutex_);if(connected&&audioFocus&&voiceMode==2&&!voicePcm_.empty()){pcm=std::move(voicePcm_);text=std::move(voiceText_);voiceBusy_=true;session=session_;cancellation=voiceCancellation;}else if(!connected||!audioFocus||voiceMode!=2)voicePcm_.clear();}
    if(pcm.empty()){pauseMs(10);continue;}int s=-1;
    try{
        s=connectTo(currentHost(),9572);{std::lock_guard<std::mutex> g(socketsMutex_);voiceSocket_=s;if(cancellation!=voiceCancellation)shutdown(s,SHUT_RDWR);}
        std::string reply;writeJson(s,{{"session",session},{"piper_bytes",pcm.size()*2},{"text",text}});auto ready=readJson(s,reply);if(!ready.value("ready",false))throw std::runtime_error(ready.value("error",std::string("Piper playback unavailable")));
        sendAll(s,pcm.data(),pcm.size()*2);timeval timeout{65,0};setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));auto result=readJson(s,reply);
        if(!result.value("played",false))throw std::runtime_error(result.value("error",std::string("Piper playback interrupted")));
        __android_log_print(ANDROID_LOG_INFO,"TelePepper","Piper robot playback complete bytes=%zu",pcm.size()*2);
    }catch(const std::exception& e){if(cancellation==voiceCancellation){std::lock_guard<std::mutex> g(mutex_);audioStatus_=e.what();__android_log_print(ANDROID_LOG_WARN,"TelePepper","Piper playback: %s",e.what());}}
    {std::lock_guard<std::mutex> g(socketsMutex_);voiceSocket_=-1;if(s>=0)close(s);}
    {std::lock_guard<std::mutex> g(mutex_);voiceBusy_=false;}
}}
void Network::videoLoop(int camera){while(running_){if(!connected||!cameraStreaming(camera)){pauseMs(100);continue;}int s=-1;try{
        std::string session;{std::lock_guard<std::mutex> g(mutex_);session=session_;}
        s=connectTo(currentHost(),9572);{std::lock_guard<std::mutex> g(socketsMutex_);videoSockets_[camera]=s;}writeJson(s,{{"session",session},{"camera",camera},{"fps",camera==0?30:camera==1?10:5}});
        unsigned videoGeneration=generation;
        while(running_&&connected&&!reconnect_&&videoGeneration==generation&&cameraStreaming(camera)){uint32_t length;readAll(s,&length,4);length=ntohl(length);if(length==0||length>1024*1024)throw std::runtime_error("Invalid video frame");
            std::vector<unsigned char> jpeg(length);readAll(s,jpeg.data(),length);double received=milliseconds();
            // Grant the next capture immediately; decoding cannot delay robot commands.
            const char next='N';sendAll(s,&next,1);
            int w,h,components;auto pixels=stbi_load_from_memory(jpeg.data(),(int)length,&w,&h,&components,3);
            if(!pixels)throw std::runtime_error("JPEG decode failed");
            VideoFrame f;f.width=w;f.height=h;f.received=received;f.pixels.assign(pixels,pixels+(size_t)w*h*3);stbi_image_free(pixels);
            {std::lock_guard<std::mutex> g(frameMutex_);frames_[camera]=std::move(f);}if(cameraStreaming(camera))lastVideo=received;
        }
    }catch(const std::exception&){/* Keep receipt time so a reconnect uses the bounded video grace period. */}
    {std::lock_guard<std::mutex> g(socketsMutex_);videoSockets_[camera]=-1;if(s>=0)close(s);}pauseMs(300);}}
