#pragma once
#include <atomic>
#include <array>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>
#include <deque>
#include <condition_variable>
#include <json.hpp>

double milliseconds();
struct PoseCommand {
    std::array<float,14> angles{0,0,1.3f,.15f,0,-.5f,0,1.3f,-.15f,0,.5f,0,1,1};
    std::array<float,3> base{};
    bool active=false, drive=false;
    std::string pauseReason;
    bool autoTurn=false;
    bool sticksNeutral=false;
    float bodyYaw=0;
    std::array<float,2> torso{};
    bool torsoAssist=false;
    double sampled=0;
};
struct VideoFrame { std::vector<unsigned char> pixels; int width=0,height=0; double received=0; };
class Network {
public:
    Network(std::string host,std::string token);
    ~Network();
    void start();
    void stop();
    void publish(const PoseCommand& pose);
    void requestArm(int64_t expectedStop=-1);
    void requestStop(bool returnToNeutral=false);
    void requestReconnect();
    void queueVoiceAudio(std::vector<int16_t> pcm,std::string text="");
    bool voiceAudioPending();
    void cancelVoiceAudio();
    void setDepthStreaming(bool enabled);
    void setCameraStreaming(int camera,bool enabled);
    bool cameraStreaming(int camera) const {return camera==2?depthStreaming.load():camera==0?topStreaming.load():bottomStreaming.load();}
    bool anyVideoEnabled() const {return topStreaming||bottomStreaming||depthStreaming;}
    bool takeFrame(VideoFrame& out,int camera=0);
    std::string status();
    nlohmann::json snapshot();
    void command(const nlohmann::json& request);
    std::string audioStatus();
    std::atomic<bool> connected{false}, armed{false};
    std::atomic<bool> ttsVoice{true},depthStreaming{false};
    std::atomic<bool> topStreaming{true},bottomStreaming{true};
    std::atomic<int> voiceMode{0}; // Default Piper / Cori; 0 Pepper TTS, 1 live microphone
    std::atomic<unsigned> voiceCancellation{0};
    std::atomic<bool> listenAudio{false}, talk{false}, audioFocus{false}, fullDuplex{false};
    std::atomic<int> camera{0}, view{0};
    std::atomic<double> rtt{0}, applyMs{0}, lastAck{0}, lastVideo{0}, lastSnapshot{0};
    std::atomic<double> robotCommandAgeMs{0};
    std::atomic<double> sampleAgeMs{0},queueMs{0};
    std::atomic<double> lastReceipt{0};
    std::atomic<double> controlRtt{0};
    std::atomic<unsigned> reconnectAttempts{0}, generation{0};
    std::atomic<unsigned> stopAcknowledged{0},commandErrors{0},motionCommandErrors{0};
    std::atomic<int64_t> acknowledgedStopVersion{0};
private:
    std::string host_,token_,session_,status_="Connecting...";
    std::atomic<bool> running_{false};
    std::atomic<int> request_{0};
    std::atomic<int64_t> armExpectedStop_{-1};
    std::atomic<bool> stopMotion_{true};
    std::atomic<uint64_t> stopEpoch_{0},armStopEpoch_{0};
    std::atomic<bool> feedbackDelayed_{false};
    std::atomic<double> tcpMotionReceipt_{0};
    std::atomic<bool> reconnect_{false};
    std::mutex socketsMutex_;
    int controlSocket_=-1;
    int voiceSocket_=-1;
    std::array<int,3> videoSockets_{{-1,-1,-1}};
    std::thread control_,motion_,audio_,voice_;
    std::array<std::thread,3> videos_;
    std::mutex mutex_,frameMutex_;
    std::condition_variable poseWake_;
    PoseCommand pose_;
    std::array<VideoFrame,3> frames_;
    nlohmann::json snapshot_;
    std::deque<nlohmann::json> commands_;
    std::string audioStatus_="Audio starting";
    std::vector<int16_t> voicePcm_;
    std::string voiceText_;
    size_t voicePosition_=0;
    unsigned voiceSessionGeneration_=0;
    bool voiceBusy_=false;
    void controlLoop(); void motionLoop(); void videoLoop(int camera);
    void audioLoop();
    void voiceLoop();
    void setStatus(const std::string& value);
    std::string currentHost();
};
