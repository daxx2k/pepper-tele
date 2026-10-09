#include "network.h"
#include "socket_io.h"
#include <aaudio/AAudio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <algorithm>

namespace {
struct Playback {
    // SPSC ring. Producer never changes the consumer cursor.
    std::array<int16_t,8192> samples{};
    std::atomic<uint32_t> read{0},write{0};
    std::atomic<bool> muted{true};
    void push(const int16_t* pcm,size_t count){uint32_t w=write.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);if(w-r+count>samples.size())return;for(size_t i=0;i<count;i++)samples[(w+i)%samples.size()]=pcm[i];write.store(w+count,std::memory_order_release);}
    static aaudio_data_callback_result_t callback(AAudioStream*,void* user,void* data,int32_t count){
        auto& q=*static_cast<Playback*>(user);auto* out=(int16_t*)data;uint32_t r=q.read.load(std::memory_order_relaxed),w=q.write.load(std::memory_order_acquire);
        // Pepper delivers microphone audio in blocks larger than 40ms.
        // Keep those blocks intact; only trim abnormal accumulated backlog.
        if(w-r>4000)r=w-3200;
        for(int i=0;i<count;i++){out[i]=(r<w&&!q.muted)?q.samples[(r++)%q.samples.size()]:0;}
        if(q.muted)r=w;q.read.store(r,std::memory_order_release);return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }
};
AAudioStream* openStream(bool input,Playback* playback){
    AAudioStreamBuilder* builder=nullptr;if(AAudio_createStreamBuilder(&builder)!=AAUDIO_OK)return nullptr;
    AAudioStreamBuilder_setDirection(builder,input?AAUDIO_DIRECTION_INPUT:AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder,AAUDIO_FORMAT_PCM_I16);AAudioStreamBuilder_setSampleRate(builder,16000);AAudioStreamBuilder_setChannelCount(builder,1);
    AAudioStreamBuilder_setPerformanceMode(builder,AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);AAudioStreamBuilder_setSharingMode(builder,AAUDIO_SHARING_MODE_SHARED);
    if(input)AAudioStreamBuilder_setInputPreset(builder,AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
    else {AAudioStreamBuilder_setUsage(builder,AAUDIO_USAGE_VOICE_COMMUNICATION);AAudioStreamBuilder_setDataCallback(builder,&Playback::callback,playback);}
    AAudioStream* stream=nullptr;aaudio_result_t result=AAudioStreamBuilder_openStream(builder,&stream);AAudioStreamBuilder_delete(builder);
    if(result!=AAUDIO_OK)return nullptr;
    if(AAudioStream_getSampleRate(stream)!=16000||AAudioStream_getChannelCount(stream)!=1){AAudioStream_close(stream);return nullptr;}
    AAudioStream_setBufferSizeInFrames(stream,AAudioStream_getFramesPerBurst(stream)*2);
    if(AAudioStream_requestStart(stream)!=AAUDIO_OK){AAudioStream_close(stream);return nullptr;}return stream;
}
void putDoubleBE(unsigned char* dest,double value){uint64_t bits;std::memcpy(&bits,&value,8);for(int i=0;i<8;i++)dest[i]=(bits>>(56-i*8))&255;}
}
void Network::audioLoop(){
    Playback playback;AAudioStream* output=openStream(false,&playback),*input=nullptr;
    {std::lock_guard<std::mutex> g(mutex_);audioStatus_=ttsVoice?"Pepper TTS / hold left grip":input?(output?"Audio ready / left grip to talk":"Headphones unavailable"):(output?"Microphone unavailable / check permission":"Audio unavailable");}
    int sock=-1;double audioRetry=0,socketRetry=0;
    uint32_t sequence=0,lastReceived=0;double lastHello=0;std::string previousSession;
    std::array<int16_t,320> capture{};int captured=0;
    while(running_){double now=milliseconds();std::string session;bool fresh;
        {std::lock_guard<std::mutex> g(mutex_);session=session_;fresh=now-pose_.sampled<100;}
        if(session!=previousSession){lastReceived=0;lastHello=0;previousSession=session;if(sock>=0)close(sock);sock=-1;socketRetry=0;captured=0;}
        if(connected&&sock<0&&now>=socketRetry){try{sock=transport::connectTo(currentHost(),9573,SOCK_DGRAM);}catch(...){socketRetry=now+1000;}}
        if(ttsVoice&&input){AAudioStream_requestStop(input);AAudioStream_close(input);input=nullptr;captured=0;}
        if(now>=audioRetry){
            audioRetry=now+2000;
            if(input&&AAudioStream_getState(input)==AAUDIO_STREAM_STATE_DISCONNECTED){AAudioStream_close(input);input=nullptr;captured=0;}
            if(output&&AAudioStream_getState(output)==AAUDIO_STREAM_STATE_DISCONNECTED){AAudioStream_close(output);output=nullptr;}
            if(audioFocus){if(!input&&!ttsVoice)input=openStream(true,nullptr);if(!output)output=openStream(false,&playback);}
            std::lock_guard<std::mutex> g(mutex_);audioStatus_=ttsVoice?"Pepper TTS / hold left grip":input?(output?"Audio ready / left grip to talk":"Headphones unavailable: retrying"):(output?"Microphone unavailable: check permission":"Audio reconnecting");
        }
        bool sending=talk&&audioFocus&&fresh&&connected&&!ttsVoice;
        playback.muted=!listenAudio||!audioFocus||!connected||((sending||voiceAudioPending())&&!fullDuplex);
        if(connected&&sock>=0&&session.size()==32){
            if(now-lastHello>800){auto hello=nlohmann::json({{"session",session}}).dump();send(sock,hello.data(),hello.size(),MSG_NOSIGNAL);lastHello=now;}
            unsigned char packet[1500];ssize_t size;
            while((size=recv(sock,packet,sizeof(packet),MSG_DONTWAIT))>0){if(size!=688||std::memcmp(packet,"TPAD",4)!=0||std::memcmp(packet+4,session.data(),32)!=0)continue;
                uint32_t seq;std::memcpy(&seq,packet+36,4);seq=ntohl(seq);if(seq<=lastReceived)continue;lastReceived=seq;
                std::array<int16_t,320> pcm;std::memcpy(pcm.data(),packet+48,640);if(output)playback.push(pcm.data(),pcm.size());}
        }
        if(input){int got=AAudioStream_read(input,capture.data()+captured,320-captured,5000000);
            if(got>0)captured+=got;
            if(captured==320){if(sending&&session.size()==32){unsigned char packet[688];std::memcpy(packet,"TPAU",4);std::memcpy(packet+4,session.data(),32);uint32_t seq=htonl(++sequence);std::memcpy(packet+36,&seq,4);putDoubleBE(packet+40,now/1000.);std::memcpy(packet+48,capture.data(),640);send(sock,packet,sizeof(packet),MSG_NOSIGNAL);}captured=0;}
            if(got<0){AAudioStream_close(input);input=nullptr;captured=0;std::lock_guard<std::mutex> g(mutex_);audioStatus_="Microphone reconnecting";}
        }else std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if(input){AAudioStream_requestStop(input);AAudioStream_close(input);}if(output){AAudioStream_requestStop(output);AAudioStream_close(output);}close(sock);
}
