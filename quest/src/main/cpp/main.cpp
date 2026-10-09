#include "head_assist.h"
#include "start_chord.h"
#include "speech_grip_chord.h"
#include <jni.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include "ui_font.h"
#include <android/asset_manager.h>
#include <meta_body_tracking_fidelity.h>
#include "network.h"
#include "arm_kinematics.h"
#include "drive_input.h"
#include "laser_map.h"
#include "start_flow.h"
#include "deferred_gesture.h"
#include "panel_layout.h"
#include "panel_anchor.h"
#include "pointer_ray.h"
#include "dashboard_layout.h"
#include "floating_layout.h"
#include "pose_mirror.h"
#include "thermal_color.h"
#include "dashboard_theme.h"
namespace ui_type {constexpr float heading=1.6f,body=1.5f,secondary=1.2f,primary=2.4f;}

static void check(XrResult result,const char* what){if(XR_FAILED(result))throw std::runtime_error(std::string(what)+" ("+std::to_string(result)+")");}
#define XR(call) check(call,#call)
static XrPosef identity(){return {{0,0,0,1},{0,0,0}};}
static XrVector3f sub(XrVector3f a,XrVector3f b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
static float dot(XrVector3f a,XrVector3f b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static XrVector3f cross(XrVector3f a,XrVector3f b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static XrVector3f norm(XrVector3f a){float l=std::sqrt(dot(a,a));return l<1e-5f?XrVector3f{0,-1,0}:XrVector3f{a.x/l,a.y/l,a.z/l};}
static XrQuaternionf inverse(XrQuaternionf q){return {-q.x,-q.y,-q.z,q.w};}
static XrQuaternionf mul(XrQuaternionf a,XrQuaternionf b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
static XrVector3f rotate(XrQuaternionf q,XrVector3f v){auto r=mul(mul(q,{v.x,v.y,v.z,0}),inverse(q));return {r.x,r.y,r.z};}
static float bounded(float v,float lo,float hi){return std::max(lo,std::min(hi,v));}
static float deadzone(float v){return std::abs(v)<.15f?0.f:std::copysign((std::abs(v)-.15f)/.85f,v);}

class App {
    android_app* app;
    XrInstance instance=XR_NULL_HANDLE;
    XrSession session=XR_NULL_HANDLE;
    XrSystemId system=XR_NULL_SYSTEM_ID;
    XrSpace local=XR_NULL_HANDLE,viewSpace=XR_NULL_HANDLE;
    XrSwapchain swapchain=XR_NULL_HANDLE,raySwapchain=XR_NULL_HANDLE;
    std::vector<XrSwapchainImageOpenGLESKHR> rayImages;
    bool rayTextureReady=false,pointerBeamValid=false;
    XrPosef pointerController{};XrVector3f pointerEnd{};
    XrActionSet actionSet=XR_NULL_HANDLE;
    XrAction sticks{},triggers{},grips{},calibrate{},arm{},stop{},menuAction{},aim{};
    XrSpace aimSpace=XR_NULL_HANDLE;
    std::array<XrPath,2> hands{};
    XrBodyTrackerFB body=XR_NULL_HANDLE;
    PFN_xrCreateBodyTrackerFB createBody=nullptr;
    PFN_xrLocateBodyJointsFB locateBody=nullptr;
    PFN_xrDestroyBodyTrackerFB destroyBody=nullptr;
    EGLDisplay display=EGL_NO_DISPLAY; EGLContext context=EGL_NO_CONTEXT; EGLSurface surface=EGL_NO_SURFACE;
    GLuint framebuffer=0,texture=0,fontTexture=0,program=0,vbo=0,vao=0;float drawAlpha=1;
    std::vector<XrSwapchainImageOpenGLESKHR> images;
    std::unique_ptr<Network> network;
    std::string appVersion="unknown";
    bool running=false,focused=false,calibrated=false,wasA=false,wasB=false,wasX=false;
    bool highFidelity=false;
    bool passthroughAvailable=false;
    XrPassthroughFB passthrough=XR_NULL_HANDLE;
    XrPassthroughLayerFB passthroughLayer=XR_NULL_HANDLE;
    PFN_xrDestroyPassthroughFB destroyPassthrough=nullptr;
    PFN_xrDestroyPassthroughLayerFB destroyPassthroughLayer=nullptr;
    bool menuOpen=true,wasY=false,wasSelect=false;
    dashboard_layout::PointerGrip pointerGrip;
    bool wasRobotGesture=false;bool helpOpen=false;bool poseMirror=false;int ledTarget=0;std::array<int,3> ledSwatches{{1,1,6}};
    bool calibrateRequested=false,pointerNavigation=true,pointerValid=false,pointerOnHelp=false;
    StartFlow startFlow;
    SpeechGripChord speechChord;
    nlohmann::json pendingRobotLimit;
    unsigned limitStopAck=0;double limitDeadline=0;
    bool pendingExitNormal=false;unsigned exitStopAck=0;double exitDeadline=0;
    XrVector2f pointer{},lastPointer{};
    const bool torsoAssist=true;
    std::array<float,2> torsoCommand{};
    float driveScale=1;int speakerVolume=50;
    bool wasTalk=false,wasVoiceEnabled=false,voiceItalian=false;unsigned voiceGeneration=0,voiceCancellation=0;std::string naturalVoiceName="Starting...";double voicePoll=0;std::string voiceStatus="Hold left grip to speak as Pepper";
    int menuIndex=0;
    double lastMenuMove=0;
    std::string inputMessage="Start prepares Pepper, calibrates and enables control. B stops motion.";
    double inputMessageUntil=0;
    std::array<XrBodyJointLocationFB,XR_BODY_JOINT_COUNT_FB> skeleton{};
    bool bodyActive=false;
    float bodyConfidence=0;
    std::string inputDiagnostic;
    XrQuaternionf neutral{0,0,0,1};
    std::array<XrQuaternionf,2> wristNeutral{{{0,0,0,1},{0,0,0,1}}};
    std::array<float,2> faceBlend{{0,0}},goalHeight{{0,0}};
    std::array<float,2> armLength{{.6f,.6f}},armError{{0,0}};
    std::array<pepper_arm::Q,2> armPrevious{};
    std::array<bool,2> armSolutionValid{{false,false}};
    std::array<pepper_arm::Pose,2> humanArms{};
    PoseCommand displayedPose,rawPose;
    std::array<float,12> offsets{};int offsetJoint=2;
    const char* offsetNames[12]={"Head yaw","Head pitch","L shoulder pitch","L shoulder roll","L elbow yaw","L elbow roll","L wrist yaw","R shoulder pitch","R shoulder roll","R elbow yaw","R elbow roll","R wrist yaw"};
    const int ledColors[7]={0xffffff,0x4388ff,0x46f0aa,0xff5050,0xffb020,0xa050ff,0};
    const char* colorNames[7]={"white","blue","green","red","amber","purple","off"};
    DriveInput driveInput;
    StartChord startChord;
    bool faceReach=true,tabletPreview=true,fahrenheit=false;
    bool showLaserMap=true;
    laser_map::Map laserMap;
    double laserStamp=-1;
    float neutralBodyYaw=0;
    double videoReceived=0;
    std::array<GLuint,3> cameraTextures{};
    std::array<double,3> cameraReceived{};
    std::array<int,3> cameraWidth{},cameraHeight{};
    std::string driveDiagnostic="Sticks: waiting";
    static constexpr int W=1400,H=1060,HELP_W=620;
    static constexpr float HELP_SCALE=.85f;
    DeferredGesture pendingGesture;
    XrPosef grabStartController=identity(),grabStartPanel=identity();
    double crtAt=-10000;std::array<float,4> crtBounds{};
    bool roomLocked=false,anchorPending=false;XrPosef roomPanel=identity();XrTime frameTime=0,anchorRetryAt=0;float lockedFit=1.35f;int windowGesture=0;XrPosef dragRelative=identity();float resizeStartWidth=0,resizeStartScale=0,resizeStartRadius=0;
    float panelScale=.9f,panelFit=1.35f;double lastResize=0;bool resized=false;
    std::string headerHover;double headerHoverAt=0;
    bool preserveColors=false;
    struct KeepColors {bool& flag;bool previous;explicit KeepColors(bool& value,bool enabled=true):flag(value),previous(value){flag=previous||enabled;}~KeepColors(){flag=previous;}};
    bool floating=false,layoutEditing=false;int layoutSlot=0,pointerCard=-1,cardDrag=-1;uint32_t maxLayers=16;
    std::array<float,13> cardScales{};std::array<XrPosef,13> cardPoses{};XrPosef cardGrab=identity();
    nlohmann::json layoutPresets=nlohmann::json::array({nullptr,nullptr,nullptr});
    void resetLayout(){for(int i=0;i<13;++i){cardPoses[i]=floating_layout::initial(i);cardScales[i]=1;}}
    nlohmann::json encodeLayout(){auto a=nlohmann::json::array();for(int i=0;i<13;++i){auto p=cardPoses[i];a.push_back({p.position.x,p.position.y,p.position.z,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w,cardScales[i]});}return a;}
    bool decodeLayout(const nlohmann::json& a){if(!a.is_array()||a.size()!=13)return false;auto next=cardPoses;auto sizes=cardScales;try{for(int i=0;i<13;++i){if(!a[i].is_array()||(a[i].size()!=7&&a[i].size()!=8))return false;auto r=a[i];XrPosef p{{r[3].get<float>(),r[4].get<float>(),r[5].get<float>(),r[6].get<float>()},{r[0].get<float>(),r[1].get<float>(),r[2].get<float>()}};if(!floating_layout::valid(p))return false;next[i]=p;if(r.size()==8){float v=r[7].get<float>();if(!std::isfinite(v)||v<.5f||v>2)return false;sizes[i]=v;}else sizes[i]=1;}}catch(...){return false;}cardPoses=next;cardScales=sizes;return true;}
    void saveLayout(){auto state=nlohmann::json{{"version",1},{"floating",floating},{"current",encodeLayout()},{"presets",layoutPresets},{"slot",layoutSlot}}.dump();JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);auto cls=env->GetObjectClass(app->activity->clazz);auto str=env->NewStringUTF(state.c_str());env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"saveLayout","(Ljava/lang/String;)V"),str);env->DeleteLocalRef(str);env->DeleteLocalRef(cls);}
    bool updateCardGesture(bool trigger,float dt){
        if(cardDrag>=0){if(!trigger||!pointerGrip.held||!focused){cardDrag=-1;saveLayout();return true;}XrPosef controller;if(controllerPose(controller)){dt=bounded(dt,0,.05f);auto axes=stick(1);cardGrab=panel_anchor::depth(cardGrab,axes.y,dt);cardPoses[cardDrag]=floating_layout::relative(roomPanel,panel_anchor::compose(controller,cardGrab),panelWidth());cardScales[cardDrag]=bounded(cardScales[cardDrag]*std::exp(deadzone(axes.x)*dt*.4f),.5f,2.f);}else {cardDrag=-1;saveLayout();}return true;}
        if(!floating||!layoutEditing||!pointerGrip.held||!pointerValid||!trigger||wasSelect||pointerCard<0)return false;
        auto r=floating_layout::cards[pointerCard];if(pointer.y>=r.y+30||menuHit(pointer.x,pointer.y)>=0)return false;XrPosef controller;if(!controllerPose(controller))return false;
        cardDrag=pointerCard;cardGrab=panel_anchor::compose(panel_anchor::inverse(controller),floating_layout::world(roomPanel,cardPoses[cardDrag],panelWidth()));return true;
    }
    float panelWidth(){return (roomLocked&&!anchorPending?lockedFit:panelFit)*panelScale;}
    void savePanelScale(){JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);
        jclass cls=env->GetObjectClass(app->activity->clazz);env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"savePanelScale","(F)V"),panelScale);env->DeleteLocalRef(cls);}
    void saveOffsets(){JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);
        jclass cls=env->GetObjectClass(app->activity->clazz);std::string value=nlohmann::json(offsets).dump();jstring str=env->NewStringUTF(value.c_str());
        env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"saveOffsets","(Ljava/lang/String;)V"),str);env->DeleteLocalRef(str);env->DeleteLocalRef(cls);
        inputMessage="Pose offsets saved on this Quest";inputMessageUntil=milliseconds()+4000;
    }
    void speakText(const std::string& text){
        if(network->voiceMode!=2){network->command({{"cmd","say"},{"text",text},{"language",voiceItalian?"Italian":"English"}});return;}
        JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);
        jstring words=env->NewStringUTF(text.c_str()),language=env->NewStringUTF(voiceItalian?"it-IT":"en-US");
        env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"speakNatural","(Ljava/lang/String;Ljava/lang/String;)V"),words,language);env->DeleteLocalRef(words);env->DeleteLocalRef(language);env->DeleteLocalRef(cls);
    }
    void updateVoice(bool held){
        JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);
        bool enabled=network->ttsVoice&&network->connected&&network->audioFocus;
        if(!enabled||voiceGeneration!=network->generation.load()||voiceCancellation!=network->voiceCancellation.load()){
            if(!enabled&&wasVoiceEnabled)network->cancelVoiceAudio();
            if(wasTalk||wasVoiceEnabled||voiceGeneration!=network->generation.load()||voiceCancellation!=network->voiceCancellation.load())env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"cancelVoice","()V"));
            voiceGeneration=network->generation;voiceCancellation=network->voiceCancellation;held=false;enabled=false;
        }
        if(enabled&&held!=wasTalk){jstring language=env->NewStringUTF(voiceItalian?"it-IT":"en-US");env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"pushToTalk","(ZLjava/lang/String;)V"),held,language);env->DeleteLocalRef(language);}
        wasTalk=held;wasVoiceEnabled=enabled;
        if(enabled&&milliseconds()-voicePoll>100){voicePoll=milliseconds();
            auto read=[&](const char* method){jstring str=(jstring)env->CallObjectMethod(app->activity->clazz,env->GetMethodID(cls,method,"()Ljava/lang/String;"));const char* raw=env->GetStringUTFChars(str,nullptr);std::string out(raw);env->ReleaseStringUTFChars(str,raw);env->DeleteLocalRef(str);return out;};
            voiceStatus=read("getVoiceStatus");naturalVoiceName=read("getNaturalVoiceName");
            if(network->voiceMode==2){std::string status=read("getNaturalVoiceStatus");if(!status.empty()&&!held)voiceStatus=status;
                // Leave completed synthesis in Java until the previous clip has
                // finished transport; never replace a sentence mid-playback.
                jbyteArray audio=network->voiceAudioPending()?nullptr:(jbyteArray)env->CallObjectMethod(app->activity->clazz,env->GetMethodID(cls,"takeNaturalVoiceAudio","()[B"));
                if(audio){jsize size=env->GetArrayLength(audio);if(size>0&&size<=1920000&&size%2==0){std::vector<int16_t> pcm(size/2);env->GetByteArrayRegion(audio,0,size,(jbyte*)pcm.data());auto caption=read("takeNaturalVoiceText");network->queueVoiceAudio(std::move(pcm),caption);}env->DeleteLocalRef(audio);}
            }
            std::string result=held?std::string():read("takeVoiceResult");if(!result.empty()){speakText(result.substr(0,400));inputMessage="Voice: "+result;inputMessageUntil=milliseconds()+8000;}
        }
        env->DeleteLocalRef(cls);
    }
    void updatePanelFit(XrTime time){
        XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;locate.displayTime=time;locate.space=viewSpace;
        XrViewState state{XR_TYPE_VIEW_STATE};std::array<XrView,2> eyes{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};uint32_t count=0;
        if(XR_SUCCEEDED(xrLocateViews(session,&locate,&state,2,&count,eyes.data()))&&count==2){
            float fit=1.65f;for(const auto& eye:eyes)fit=std::min(fit,panel_layout::fit(eye.fov.angleLeft,eye.fov.angleRight,eye.fov.angleDown,eye.fov.angleUp,1.3f,float(W)/H));
            panelFit=fit;
        }
    }

    XrPath path(const char* text){XrPath p;XR(xrStringToPath(instance,text,&p));return p;}
    void saveAppearance(){
        JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);
        std::string saved=nlohmann::json(ledSwatches).dump();jstring value=env->NewStringUTF(saved.c_str());
        env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"saveAppearance","(ILjava/lang/String;)V"),ledTarget,value);
        env->DeleteLocalRef(value);env->DeleteLocalRef(cls);
    }
    std::string extra(const char* key){
        JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);
        jobject activity=app->activity->clazz;jclass cls=env->GetObjectClass(activity);
        jobject intent=env->CallObjectMethod(activity,env->GetMethodID(cls,"getIntent","()Landroid/content/Intent;"));
        jclass ic=env->GetObjectClass(intent);jstring k=env->NewStringUTF(key);
        jstring value=(jstring)env->CallObjectMethod(intent,env->GetMethodID(ic,"getStringExtra","(Ljava/lang/String;)Ljava/lang/String;"),k);
        std::string result;if(value){const char* v=env->GetStringUTFChars(value,nullptr);result=v;env->ReleaseStringUTFChars(value,v);env->DeleteLocalRef(value);}
        env->DeleteLocalRef(k);env->DeleteLocalRef(ic);env->DeleteLocalRef(intent);env->DeleteLocalRef(cls);return result;
    }
    void initPassthrough(){
        if(!passthroughAvailable)return;
        PFN_xrCreatePassthroughFB create=nullptr;PFN_xrCreatePassthroughLayerFB createLayer=nullptr;
        try{
            XR(xrGetInstanceProcAddr(instance,"xrCreatePassthroughFB",(PFN_xrVoidFunction*)&create));
            XR(xrGetInstanceProcAddr(instance,"xrCreatePassthroughLayerFB",(PFN_xrVoidFunction*)&createLayer));
            XR(xrGetInstanceProcAddr(instance,"xrDestroyPassthroughFB",(PFN_xrVoidFunction*)&destroyPassthrough));
            XR(xrGetInstanceProcAddr(instance,"xrDestroyPassthroughLayerFB",(PFN_xrVoidFunction*)&destroyPassthroughLayer));
            XrPassthroughCreateInfoFB info{XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};info.flags=XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
            XR(create(session,&info,&passthrough));
            XrPassthroughLayerCreateInfoFB layer{XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};layer.passthrough=passthrough;
            layer.purpose=XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;layer.flags=XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
            XR(createLayer(session,&layer,&passthroughLayer));
            __android_log_print(ANDROID_LOG_INFO,"TelePepper","Room passthrough initialized");
        }catch(const std::exception& e){__android_log_print(ANDROID_LOG_WARN,"TelePepper","Passthrough unavailable: %s",e.what());}
    }
    void initGraphics(){
        display=eglGetDisplay(EGL_DEFAULT_DISPLAY);eglInitialize(display,nullptr,nullptr);
        const EGLint attrs[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
        EGLConfig config;EGLint count;eglChooseConfig(display,attrs,&config,1,&count);if(!count)throw std::runtime_error("No EGL config");
        const EGLint ctx[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};context=eglCreateContext(display,config,EGL_NO_CONTEXT,ctx);
        const EGLint surf[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};surface=eglCreatePbufferSurface(display,config,surf);
        if(!eglMakeCurrent(display,surface,surface,context))throw std::runtime_error("EGL context failed");
        PFN_xrGetOpenGLESGraphicsRequirementsKHR requirements;XR(xrGetInstanceProcAddr(instance,"xrGetOpenGLESGraphicsRequirementsKHR",(PFN_xrVoidFunction*)&requirements));
        XrGraphicsRequirementsOpenGLESKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};XR(requirements(instance,system,&req));
        XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};binding.display=display;binding.config=config;binding.context=context;
        XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};info.next=&binding;info.systemId=system;XR(xrCreateSession(instance,&info,&session));
        XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};space.poseInReferenceSpace=identity();space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;XR(xrCreateReferenceSpace(session,&space,&local));
        space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;XR(xrCreateReferenceSpace(session,&space,&viewSpace));
        uint32_t n=0;XR(xrEnumerateSwapchainFormats(session,0,&n,nullptr));std::vector<int64_t> formats(n);XR(xrEnumerateSwapchainFormats(session,n,&n,formats.data()));
        int64_t format=GL_RGBA8;if(std::find(formats.begin(),formats.end(),format)==formats.end())format=formats.at(0);
        XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};sc.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;sc.format=format;sc.sampleCount=1;sc.width=W+HELP_W;sc.height=H;sc.faceCount=1;sc.arraySize=1;sc.mipCount=1;XR(xrCreateSwapchain(session,&sc,&swapchain));
        XR(xrEnumerateSwapchainImages(swapchain,0,&n,nullptr));images.resize(n,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});XR(xrEnumerateSwapchainImages(swapchain,n,&n,(XrSwapchainImageBaseHeader*)images.data()));
        const char* vert="#version 300 es\nlayout(location=0) in vec2 p;layout(location=1) in vec2 uv;out vec2 t;void main(){gl_Position=vec4(p,0,1);t=uv;}";
        const char* frag=R"glsl(#version 300 es
precision highp float;in vec2 t;uniform sampler2D image;uniform vec4 color;uniform int textured;uniform float canvasOffset;uniform float canvasWidth;
uniform vec4 cameraBounds;uniform float cameraRadius;uniform float cameraFade;uniform float crtTime;uniform float crtStrength;uniform sampler2D secondImage;uniform vec4 seamTop;uniform vec4 seamBottom;out vec4 o;
vec4 cameraSample(sampler2D source,vec2 uv){
 vec2 pixel=1.0/vec2(textureSize(source,0));vec4 centre=texture(source,uv);
 vec3 left=texture(source,uv-vec2(pixel.x,0.0)).rgb,right=texture(source,uv+vec2(pixel.x,0.0)).rgb;
 vec3 up=texture(source,uv-vec2(0.0,pixel.y)).rgb,down=texture(source,uv+vec2(0.0,pixel.y)).rgb;
 vec3 detail=centre.rgb-(left+right+up+down)*.25;
 float contrast=max(max(abs(detail.r),abs(detail.g)),abs(detail.b));
 // Ignore flat-field sensor noise; bound enhancement to the local colour range.
 vec3 lo=min(centre.rgb,min(min(left,right),min(up,down))),hi=max(centre.rgb,max(max(left,right),max(up,down)));
 return vec4(clamp(centre.rgb+detail*.45*smoothstep(.008,.035,contrast),lo,hi),centre.a);
}
vec4 seamSample(sampler2D source,vec2 uv,float blur){
 vec2 dx=vec2(blur,0.0),dy=vec2(0.0,blur);
 vec4 soft=texture(source,uv)*.5+(texture(source,uv+dx)+texture(source,uv-dx)+texture(source,uv+dy)+texture(source,uv-dy))*.125;
 return mix(cameraSample(source,uv),soft,smoothstep(0.0,.006,blur));
}
void main(){
 vec2 q=abs(gl_FragCoord.xy-vec2(canvasOffset+canvasWidth*.5,530.0))-vec2(canvasWidth*.5-28.0,502.0);
 float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-28.0;
 float mask=1.0-smoothstep(-1.0,0.0,d);if(mask<=0.0)discard;
 if(textured==4){
  float seed=dot(floor(gl_FragCoord.xy*.65),vec2(12.9898,78.233))+floor(crtTime*24.0)*37.1;
  float noise=fract(sin(seed)*43758.5453);float scan=.8+.2*sin(gl_FragCoord.y*1.57);
  o=vec4(vec3(.32+.30*noise)*scan,crtStrength);
 }else if(textured==5){
  vec2 p=vec2(gl_FragCoord.x,1060.0-gl_FragCoord.y);
  vec2 a=clamp((p-seamTop.xy)/seamTop.zw,0.0,1.0),b=clamp((p-seamBottom.xy)/seamBottom.zw,0.0,1.0);
  // Exact source pixels and zero blur at both boundaries: no hard edge-row jump.
  float blend=smoothstep(0.0,1.0,t.y),blur=.006*pow(sin(3.14159265*t.y),2.0);
  vec4 top=seamSample(image,a,blur),bottom=seamSample(secondImage,b,blur);
  bottom.rgb=pow(max(bottom.rgb,vec3(0.0)),vec3(.92));o=mix(top,bottom,blend);
 }else if(textured==3){vec4 c=cameraSample(image,t);o=vec4(pow(max(c.rgb,vec3(0.0)),vec3(0.92)),c.a);}
 else{o=textured==1?(cameraRadius>0.0?cameraSample(image,t):texture(image,t)):textured==2?vec4(color.rgb,color.a*texture(image,t).r):color;}
 if((textured==1||textured==3||textured==5)&&cameraRadius>0.0){
  vec2 p=vec2(gl_FragCoord.x,1060.0-gl_FragCoord.y);
  vec2 cq=abs(p-cameraBounds.xy-cameraBounds.zw*0.5)-(cameraBounds.zw*0.5-vec2(cameraRadius));
  float cd=length(max(cq,0.0))+min(max(cq.x,cq.y),0.0)-cameraRadius;
  o.a*=1.0-smoothstep(-1.0,0.0,cd);
  if(cameraFade>0.0)o.a*=smoothstep(0.0,cameraFade,t.y);
 }
 o.a*=mask;
})glsl";
        auto shader=[](GLenum type,const char* source){GLuint id=glCreateShader(type);glShaderSource(id,1,&source,nullptr);glCompileShader(id);GLint ok;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok)throw std::runtime_error("Shader compile failed");return id;};
        GLuint vs=shader(GL_VERTEX_SHADER,vert),fs=shader(GL_FRAGMENT_SHADER,frag);program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);glDeleteShader(vs);glDeleteShader(fs);
        AAsset* fontAsset=AAssetManager_open(app->activity->assetManager,"ui_font.bin",AASSET_MODE_BUFFER);
        if(!fontAsset||AAsset_getLength(fontAsset)!=UI_FONT_WIDTH*UI_FONT_HEIGHT)throw std::runtime_error("UI font asset missing");
        glGenTextures(1,&fontTexture);glBindTexture(GL_TEXTURE_2D,fontTexture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R8,UI_FONT_WIDTH,UI_FONT_HEIGHT,0,GL_RED,GL_UNSIGNED_BYTE,AAsset_getBuffer(fontAsset));AAsset_close(fontAsset);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1,&framebuffer);glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenTextures(3,cameraTextures.data());for(GLuint t:cameraTextures){glBindTexture(GL_TEXTURE_2D,t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
        XrSwapchainCreateInfo ray{XR_TYPE_SWAPCHAIN_CREATE_INFO};ray.createFlags=XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;ray.usageFlags=XR_SWAPCHAIN_USAGE_SAMPLED_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;ray.format=format;ray.sampleCount=1;ray.width=8;ray.height=64;ray.faceCount=1;ray.arraySize=1;ray.mipCount=1;
        if(XR_SUCCEEDED(xrCreateSwapchain(session,&ray,&raySwapchain))){uint32_t n=0;XR(xrEnumerateSwapchainImages(raySwapchain,0,&n,nullptr));rayImages.resize(n,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});XR(xrEnumerateSwapchainImages(raySwapchain,n,&n,(XrSwapchainImageBaseHeader*)rayImages.data()));}
        glBindTexture(GL_TEXTURE_2D,texture);const unsigned char pixel[]={10,20,30};glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,1,1,0,GL_RGB,GL_UNSIGNED_BYTE,pixel);
    }
    XrAction action(const char* name,XrActionType type,bool both){XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};std::strncpy(info.actionName,name,sizeof(info.actionName)-1);std::strncpy(info.localizedActionName,name,sizeof(info.localizedActionName)-1);info.actionType=type;if(both){info.countSubactionPaths=2;info.subactionPaths=hands.data();}XrAction a;XR(xrCreateAction(actionSet,&info,&a));return a;}
    void initInput(){
        hands={path("/user/hand/left"),path("/user/hand/right")};XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};std::strcpy(set.actionSetName,"pilot");std::strcpy(set.localizedActionSetName,"TelePepper");XR(xrCreateActionSet(instance,&set,&actionSet));
        sticks=action("sticks",XR_ACTION_TYPE_VECTOR2F_INPUT,true);triggers=action("triggers",XR_ACTION_TYPE_FLOAT_INPUT,true);grips=action("grips",XR_ACTION_TYPE_FLOAT_INPUT,true);
        calibrate=action("calibrate",XR_ACTION_TYPE_BOOLEAN_INPUT,false);arm=action("arm",XR_ACTION_TYPE_BOOLEAN_INPUT,false);stop=action("stop",XR_ACTION_TYPE_BOOLEAN_INPUT,false);
        menuAction=action("menu",XR_ACTION_TYPE_BOOLEAN_INPUT,false);aim=action("aim",XR_ACTION_TYPE_POSE_INPUT,true);
        std::vector<XrActionSuggestedBinding> bindings;
        for(int i=0;i<2;++i){std::string base=i==0?"/user/hand/left/input/":"/user/hand/right/input/";bindings.push_back({sticks,path((base+"thumbstick").c_str())});bindings.push_back({triggers,path((base+"trigger/value").c_str())});bindings.push_back({grips,path((base+"squeeze/value").c_str())});}
        bindings.push_back({aim,path("/user/hand/right/input/aim/pose")});
        bindings.push_back({calibrate,path("/user/hand/left/input/x/click")});bindings.push_back({arm,path("/user/hand/right/input/a/click")});bindings.push_back({stop,path("/user/hand/right/input/b/click")});
        bindings.push_back({menuAction,path("/user/hand/left/input/y/click")});
        XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};suggested.interactionProfile=path("/interaction_profiles/oculus/touch_controller");suggested.countSuggestedBindings=(uint32_t)bindings.size();suggested.suggestedBindings=bindings.data();XR(xrSuggestInteractionProfileBindings(instance,&suggested));
        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&actionSet;XR(xrAttachSessionActionSets(session,&attach));
        XrActionSpaceCreateInfo aimInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};aimInfo.action=aim;aimInfo.subactionPath=hands[1];aimInfo.poseInActionSpace=identity();XR(xrCreateActionSpace(session,&aimInfo,&aimSpace));
        __android_log_print(ANDROID_LOG_INFO,"TelePepper","Controller contact detection OFF / manual START and STOP");
        XR(xrGetInstanceProcAddr(instance,"xrCreateBodyTrackerFB",(PFN_xrVoidFunction*)&createBody));XR(xrGetInstanceProcAddr(instance,"xrLocateBodyJointsFB",(PFN_xrVoidFunction*)&locateBody));XR(xrGetInstanceProcAddr(instance,"xrDestroyBodyTrackerFB",(PFN_xrVoidFunction*)&destroyBody));
        XrBodyTrackerCreateInfoFB info{XR_TYPE_BODY_TRACKER_CREATE_INFO_FB};info.bodyJointSet=XR_BODY_JOINT_SET_DEFAULT_FB;XR(createBody(session,&info,&body));
        if(highFidelity){PFN_xrRequestBodyTrackingFidelityMETA request=nullptr;
            if(XR_SUCCEEDED(xrGetInstanceProcAddr(instance,"xrRequestBodyTrackingFidelityMETA",(PFN_xrVoidFunction*)&request)))
                if(request && XR_FAILED(request(body,XR_BODY_TRACKING_FIDELITY_HIGH_META)))
                    __android_log_print(ANDROID_LOG_WARN,"TelePepper","High fidelity unavailable; using default body tracking");}
    }
    bool button(XrAction a){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=a;XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};XR(xrGetActionStateBoolean(session,&get,&state));return state.isActive&&state.currentState;}
    float axis(XrAction a,int hand){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=a;get.subactionPath=hands[hand];XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};XR(xrGetActionStateFloat(session,&get,&state));return state.isActive?state.currentState:0;}
    XrVector2f stick(int hand){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=sticks;get.subactionPath=hands[hand];XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};XR(xrGetActionStateVector2f(session,&get,&state));return state.isActive?state.currentState:XrVector2f{};}
    struct MenuItem { std::string label,detail; nlohmann::json action; };
    std::vector<MenuItem> menuActions(){
        using J=nlohmann::json;std::vector<MenuItem> rows;auto state=network->snapshot();
        rows.push_back({helpOpen?"Close help":"Help","Quick guide beside the dashboard",{{"cmd","local_help"}}});
        rows.push_back({layoutEditing?"Done":"Layout","Arrange independent panels in the room",{{"cmd","local_layout"}}});
        if(layoutEditing){rows.push_back({"Preset "+std::to_string(layoutSlot+1),"Choose one of three saved layouts",{{"cmd","local_layout_slot"}}});rows.push_back({"Save","Save the current panel arrangement",{{"cmd","local_layout_save"}}});rows.push_back({"Load","Restore the selected preset",{{"cmd","local_layout_load"}}});rows.push_back({"Reset","Restore the original arrangement",{{"cmd","local_layout_reset"}}});}
        bool motionRunning=network->armed||startFlow.active();
        rows.push_back({startFlow.active()?"STARTING...":motionRunning?"STOP":"START","Start with calibration / stop motion and recovery",{{"cmd",motionRunning?"local_stop":"local_start"}}});
        rows.push_back({network->listenAudio?"Listen: ON":"Listen: OFF","Pepper microphones / OFF at startup",{{"cmd","local_listen"}}});
        rows.push_back({network->voiceMode==0?"Voice: Pepper TTS":network->voiceMode==1?"Voice: puppeteer":"Voice: Piper / Cori","Hold left grip / release to speak",{{"cmd","local_voice_mode"}}});
        auto speechGestures=state.value("speech_gestures",J::object());
        rows.push_back({speechGestures.value("enabled",false)?"Gestures: ON":"Gestures: OFF","Release left grip to gesture / both grips toggle",{{"cmd","speech_gestures"},{"enabled",!speechGestures.value("enabled",false)}}});
        rows.push_back({"Voice demo","Hear the selected voice on Pepper",{{"cmd","say"},{"text",voiceItalian?"Ciao, sono Pepper. Come posso aiutarti?":"Hi, I am Pepper. How can I help you?"}}});
        rows.push_back({state.value("speech_on_tablet",true)?"Speech on tablet: ON":"Speech on tablet: OFF","Show Pepper TTS and Cori phrases automatically",{{"cmd","speech_on_tablet"},{"enabled",!state.value("speech_on_tablet",true)}}});
        rows.push_back({"Stop speech","Interrupt Pepper's current phrase",{{"cmd","speech_stop"}}});
        rows.push_back({poseMirror?"Pose mirror: ON":"Pose mirror: OFF","Facing Pepper: reflect arms, hands, head and torso",{{"cmd","local_pose_mirror"}}});
        rows.push_back({"Welcome","Display a welcome on Pepper tablet / no speech",{{"cmd","tablet"},{"text","Welcome! I am Pepper."},{"choices",J::array()}}});
        const char* reactions[]={"smile","laugh","love","surprise","sad","wink","angry"};
        const char* reactionLabels[]={"Smile","Laugh","Love","Surprise","Sad","Wink","Angry"};
        for(int i=0;i<7;++i)rows.push_back({reactionLabels[i],"Show a big reaction emoji on Pepper tablet",{{"cmd","tablet"},{"reaction",reactions[i]}}});
        rows.push_back({"Recalibrate","Recalibrate head, arms, wrists, hands, torso and base reference",{{"cmd","local_calibrate"}}});
        auto limits=state.value("robot_limits",J::object());
        const char* limitKeys[]={"range","speed"};const char* limitLabels[]={"Joint limits","Speed limits"};
        for(int i=0;i<2;++i)rows.push_back({std::string(limitLabels[i])+(limits.value(limitKeys[i],true)?": ON":": OFF"),"Pause then change this software limit / START resumes",{{"cmd","local_robot_limit"},{"key",limitKeys[i]},{"slot",i},{"enabled",!limits.value(limitKeys[i],true)}}});
        const char* gestureNames[]={"wave_left","wave_right","point_left","point_right","yes","no","happy","sad","dance","funny","look_around","make_space"};
        const char* gestureLabels[]={"Wave left","Wave right","Point left","Point right","Affirm","Refuse","Happy","Sad","Dance","Funny","Look around","Make space"};
        for(int i=0;i<12;++i)rows.push_back({gestureLabels[i],"Official Pepper animation / base stopped / B cancels",{{"cmd","gesture"},{"name",gestureNames[i]}}});
        for(int c=0;c<2;++c)rows.push_back({c==0?"Top camera":"Bottom camera","Click the feed to toggle streaming",{{"cmd","local_camera"},{"camera",c}}});
        rows.push_back({network->depthStreaming?"Depth: ON":"Depth: OFF","Click the depth panel to toggle streaming",{{"cmd","local_depth"}}});
        rows.push_back({"Volume: "+std::to_string(speakerVolume)+"%","Cycle speaker volume 0 to 100% / 20% steps",{{"cmd","local_volume"}}});
        rows.push_back({"Clear","Clear participant message and choices",{{"cmd","tablet"},{"text",""},{"choices",J::array()}}});
        rows.push_back({tabletPreview?"Preview: ON":"Preview: OFF","Toggle the local preview / Pepper display stays active",{{"cmd","local_tablet_preview"}}});
        rows.push_back({fahrenheit?"F":"C","Switch Celsius / Fahrenheit",{{"cmd","local_temperature_unit"}}});
        rows.push_back({"Reconnect","Reconnect control, video and audio",{{"cmd","local_reconnect"}}});
        rows.push_back({"Exit TelePepper","Stop control and return Pepper to normal mode",{{"cmd","local_exit_normal"}}});
        rows.push_back({"Connection settings","Edit robot address and pairing",{{"cmd","local_settings"}}});
        rows.push_back({"Move window","Grip + trigger: move freely / right stick scales",{{"cmd","local_panel_move"}}});
        rows.push_back({roomLocked?"Unpin window":"Pin window","Toggle room lock / head follow",{{"cmd","local_panel_lock"}}});
        rows.push_back({showLaserMap?"Lidar: ON":"Lidar: OFF","Click the map to toggle its display",{{"cmd","local_map"}}});
        rows.push_back({"Clear","Reset laser history",{{"cmd","local_map_clear"}}});
        if(state.contains("phrases"))for(const auto& phrase:state["phrases"])rows.push_back({phrase.get<std::string>(),"Speak this phrase in English",{{"cmd","say"},{"text",phrase},{"language","English"},{"phrase",true}}});
        const char* targets[]={"Eyes","Shoulders","Ears"};
        for(int i=0;i<3;++i)rows.push_back({std::string(ledTarget==i?"> ":"")+targets[i],"Select LEDs for the presets below",{{"cmd","local_led_target"},{"target",i}}});
        const int earPresets[]={0,15,30,50,70,85,100};
        for(int i=0;i<7;++i)rows.push_back({ledTarget==2?std::to_string(earPresets[i])+"% blue":colorNames[i],ledTarget==2?"Set ear brightness / blue LEDs only":"Apply this colour to selected LEDs",{{"cmd","local_led_preset"},{"swatch",i},{"color",ledColors[i]},{"intensity",earPresets[i]/100.f}}});
        std::stable_sort(rows.begin(),rows.end(),[&](const MenuItem& a,const MenuItem& b){return dashboard_layout::x[menuGroup(a)]<dashboard_layout::x[menuGroup(b)];});
        return rows;
    }
    int menuGroup(const MenuItem& item){
        auto c=item.action.value("cmd",std::string());
        if(item.action.value("tablet_preset",false))return 2;
        if(c=="local_robot_limit"||c=="local_pose_mirror"||c=="local_torso_assist"||c=="local_start"||c=="local_stop"||c=="local_calibrate"||c=="gesture")return 0;
        if(c=="speech_gestures"||c=="speech_on_tablet"||c=="local_voice_mode"||c=="local_natural_voice"||c=="say"||c=="speech_stop"||c=="local_listen"||c=="local_volume")return 1;
        if(c=="tablet")return 2;
        if(c=="local_exit_normal"||c=="local_settings"||c=="local_reconnect"||c=="base_protection"||c=="local_panel_move"||c=="local_panel_resize"||c=="local_panel_lock"||c=="local_panel_here")return 4;
        if(c.find("local_offset_")==0)return 5;
        if(c=="led"||c=="local_led_target"||c=="local_led_preset")return 6;
        return 2;
    }
    struct Card {float x,y,w,h;};
    Card menuRect(int index,const std::vector<MenuItem>& items){
        auto command=items[index].action.value("cmd",std::string());
        if(command.find("local_layout")==0){if(command=="local_layout"){auto r=dashboard_layout::headerLayout;return layoutEditing?Card{814,6,48,48}:Card{r.x,r.y,r.w,r.h};}if(command=="local_layout_slot")return {876,14,100,32};if(command=="local_layout_save")return {982,14,80,32};if(command=="local_layout_load")return {1068,14,80,32};return {1154,14,92,32};}
        if(command=="local_help"){auto r=dashboard_layout::headerHelp;return {r.x,r.y,r.w,r.h};}
        if(command=="local_tablet_preview"){auto r=dashboard_layout::tabletScreen;return {r.x,r.y,r.w,r.h};}
        if(command=="local_temperature_unit"){auto r=dashboard_layout::temperatureUnit;return {r.x,r.y,r.w,r.h};}
        if(command=="local_panel_move"){auto r=dashboard_layout::windowMove;return {r.x,r.y,r.w,r.h};}
        if(command=="local_panel_lock"){auto r=dashboard_layout::windowPin;return {r.x,r.y,r.w,r.h};}
        if(command=="local_camera"){auto r=dashboard_layout::cameras[items[index].action.value("camera",0)];return {r.x,r.y,r.w,r.h};}
        if(items[index].action.value("cmd",std::string())=="local_depth"){auto r=dashboard_layout::cameras[2];return {r.x,r.y,r.w,r.h};}
        if(command=="local_map"){auto r=dashboard_layout::lidarCard;return {r.x,r.y,r.w,r.h};}
        if(items[index].action.value("cmd",std::string())=="local_map_clear"){auto r=dashboard_layout::clearLaser;return {r.x,r.y,r.w,r.h};}
        if(command=="local_robot_limit"){float w=(dashboard_layout::width[0]-22)/2;return {dashboard_layout::x[0]+8+items[index].action.value("slot",0)*(w+6),dashboard_layout::groupY[0]+140,w,30};}
        int group=menuGroup(items[index]),count=0,ordinal=0;
        if(group==1){bool phrase=items[index].action.value("phrase",false);for(int i=0;i<(int)items.size();++i)if(menuGroup(items[i])==1&&items[i].action.value("phrase",false)==phrase){if(i<index)++ordinal;++count;}auto r=dashboard_layout::voiceButton(phrase,ordinal,count);return {r.x,r.y,r.w,r.h};}
        for(int i=0;i<(int)items.size();++i)if(menuGroup(items[i])==group&&items[i].action.value("cmd",std::string())!="local_robot_limit"&&items[i].action.value("cmd",std::string())!="local_help"&&items[i].action.value("cmd",std::string()).find("local_layout")!=0&&items[i].action.value("cmd",std::string())!="local_temperature_unit"&&items[i].action.value("cmd",std::string())!="local_tablet_preview"&&items[i].action.value("cmd",std::string())!="local_map_clear"&&items[i].action.value("cmd",std::string())!="local_map"&&items[i].action.value("cmd",std::string())!="local_depth"&&items[i].action.value("cmd",std::string())!="local_camera"&&items[i].action.value("cmd",std::string()).find("local_panel_")!=0){if(i<index)++ordinal;++count;}
        auto r=dashboard_layout::button(group,ordinal,count);return {r.x,r.y,r.w,r.h};
    }
    StartFlow::Input startInput(bool tracked){
        StartFlow::Input in;auto state=network->snapshot();auto t=state.value("telemetry",nlohmann::json::object());
        auto prep=state.value("preparation",nlohmann::json::object());
        in.connected=network->connected;in.focused=focused&&app->activityState==APP_CMD_RESUME;
        in.fresh=milliseconds()-network->lastSnapshot<750&&t.value("available",false)&&state.value("robot_mono",0.)-t.value("robot_mono",-100.)<.75;
        in.tracking=tracked;in.video=!network->anyVideoEnabled()||(network->lastVideo>0&&milliseconds()-network->lastVideo<300);
        auto l=stick(0),r=stick(1);in.neutral=DriveInput::deadzone(l.x)==0&&DriveInput::deadzone(l.y)==0&&DriveInput::deadzone(r.x)==0;
        in.ready=t.value("motion_ready",false);in.preparing=prep.value("busy",false);in.preparationRevision=prep.value("revision",0u);
        in.prepareFailed=prep.value("message",std::string()).find("Preparation failed")==0;
        in.calibrated=calibrated;in.armed=network->armed;in.connection=network->generation;
        in.stopAck=network->stopAcknowledged;in.ackStopVersion=network->acknowledgedStopVersion;in.errors=network->motionCommandErrors;
        in.stopVersion=state.value("motion_diagnostics",nlohmann::json::object()).value("stop_generation",int64_t(0));
        return in;
    }
    void beginStart(bool animation=false,bool recalibrate=false){
        if(pendingExitNormal){inputMessage="Closing teleoperation / wait for STOP";inputMessageUntil=milliseconds()+3000;return;}
        if(!pendingRobotLimit.is_null()){inputMessage="Changing limits / wait before START";inputMessageUntil=milliseconds()+3000;return;}
        if(windowGesture||layoutEditing){inputMessage="Release the window before starting motion";inputMessageUntil=milliseconds()+3000;return;}
        if(startFlow.active())return;
        if(!network->connected){inputMessage="Connecting: waiting for Pepper";inputMessageUntil=milliseconds()+4000;return;}
        if((network->armed&&!animation&&!recalibrate)||startFlow.active()){inputMessage="Already running / B stops, X recalibrates";inputMessageUntil=milliseconds()+3000;return;}
        startFlow.begin(milliseconds()/1000.,startInput(false),recalibrate&&!network->armed);network->requestStop();menuOpen=false;
    }
    void openSettings(){network->requestStop();JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);
        jclass cls=env->GetObjectClass(app->activity->clazz);env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"openSettings","()V"));env->DeleteLocalRef(cls);}
    void selectMenu(){auto items=menuActions();if(menuIndex<0||menuIndex>=(int)items.size())return;
        const auto state=network->snapshot();const auto item=items[menuIndex];std::string cmd=item.action.value("cmd",std::string());
        inputMessage=item.label;inputMessageUntil=milliseconds()+4000;
        if(cmd=="local_map"||cmd=="local_camera"||cmd=="local_depth"||cmd=="local_panel_lock"||(item.action.contains("enabled")&&cmd!="local_robot_limit")||cmd=="local_led_preset"){auto r=menuRect(menuIndex,items);crtBounds={r.x,r.y,r.w,r.h};crtAt=milliseconds();}
        if(cmd.find("local_layout")==0){
            if(cmd=="local_layout"){if(!layoutEditing){if(maxLayers<15){inputMessage="Independent panels unavailable on this headset";return;}if(!roomLocked&&!placePanel())return;roomLocked=true;floating=true;saveWindowMode();startFlow.cancel();pendingGesture.cancel();network->requestStop();}layoutEditing=!layoutEditing;cardDrag=-1;saveLayout();inputMessage=layoutEditing?"Grip + trigger title / stick up-down depth, left-right size":"Layout ready / START resumes motion";}
            else if(cmd=="local_layout_slot")layoutSlot=(layoutSlot+1)%3;
            else if(cmd=="local_layout_reset"){resetLayout();saveLayout();}
            else if(cmd=="local_layout_save"){layoutPresets[layoutSlot]=encodeLayout();saveLayout();inputMessage="Preset saved";}
            else if(cmd=="local_layout_load"){inputMessage=decodeLayout(layoutPresets[layoutSlot])?"Preset loaded":"This preset is empty";saveLayout();}
            return;
        }
        if(layoutEditing&&cmd!="local_help")return;
        if(cmd=="local_offset_face")faceReach=!faceReach;
        else if(cmd=="local_offset_joint")offsetJoint=(offsetJoint+1)%12;
        else if(cmd=="local_offset_minus"||cmd=="local_offset_plus")offsets[offsetJoint]=bounded(offsets[offsetJoint]+(cmd=="local_offset_plus"?2:-2),-30,30);
        else if(cmd=="local_offset_save")saveOffsets();
        else if(cmd=="local_offset_reset")offsets.fill(0);
        else if(cmd=="local_help"){helpOpen=!helpOpen;pointerOnHelp=false;}
        else if(cmd=="local_led_target"){ledTarget=std::max(0,std::min(2,item.action.value("target",0)));saveAppearance();}
        else if(cmd=="local_led_preset"){ledSwatches[ledTarget]=item.action.value("swatch",-1);saveAppearance();const char* groups[]={"FaceLeds","ChestLeds","EarLeds"};network->command({{"cmd","led"},{"group",groups[ledTarget]},{"color",item.action["color"]},{"intensity",item.action["intensity"]}});}
        else if(cmd=="local_pose_mirror"){startFlow.cancel();pendingGesture.cancel();network->requestStop();poseMirror=!poseMirror;driveInput.ready=false;
            JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"savePoseMirror","(Z)V"),poseMirror);env->DeleteLocalRef(cls);inputMessage="Pose mirror changed / hold A + X to resume";}
        else if(cmd=="local_voice_mode"){network->cancelVoiceAudio();network->voiceMode=(network->voiceMode+1)%3;network->ttsVoice=network->voiceMode!=1;network->command({{"cmd","speech_stop"}});updateVoice(false);if(network->voiceMode==2){JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);jstring lang=(jstring)env->CallObjectMethod(app->activity->clazz,env->GetMethodID(cls,"getNaturalVoiceLanguage","()Ljava/lang/String;"));const char* raw=env->GetStringUTFChars(lang,nullptr);voiceItalian=std::string(raw).find("it")==0;env->ReleaseStringUTFChars(lang,raw);env->DeleteLocalRef(lang);env->DeleteLocalRef(cls);}}
        else if(cmd=="local_natural_voice"){network->cancelVoiceAudio();updateVoice(false);JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);jstring lang=env->NewStringUTF(voiceItalian?"it-IT":"en-US");env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"nextNaturalVoice","(Ljava/lang/String;)V"),lang);env->DeleteLocalRef(lang);env->DeleteLocalRef(cls);}
        else if(cmd=="local_listen")network->listenAudio=!network->listenAudio;
        else if(cmd=="local_volume"){speakerVolume=dashboard_layout::nextVolume(speakerVolume);network->command({{"cmd","volume"},{"value",speakerVolume}});}
        else if(cmd=="local_panel_move")beginWindowGesture(3);
        else if(cmd=="local_panel_lock"){
            windowGesture=0;cardDrag=-1;layoutEditing=false;
            if(roomLocked){roomLocked=false;anchorPending=false;}
            else if(placePanel())roomLocked=true;
            saveWindowMode();saveLayout();
            inputMessage=roomLocked?"Window pinned in room":"Window follows your head";
            __android_log_print(ANDROID_LOG_INFO,"TelePepper","Window pin: locked=%d floating=%d",roomLocked,floating);
        }
        else if(cmd=="local_map"){showLaserMap=!showLaserMap;JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"saveLidarMap","(Z)V"),showLaserMap);env->DeleteLocalRef(cls);}
        else if(cmd=="local_tablet_preview"||cmd=="local_temperature_unit"){bool preview=cmd=="local_tablet_preview";if(preview)tabletPreview=!tabletPreview;else fahrenheit=!fahrenheit;JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,preview?"saveTabletPreview":"saveTemperatureUnit","(Z)V"),preview?tabletPreview:fahrenheit);env->DeleteLocalRef(cls);}
        else if(cmd=="local_map_clear"){laserMap.clear();laserStamp=-1;}
        else if(cmd=="local_camera"){int c=item.action.value("camera",0);bool enabled=!network->cameraStreaming(c);network->setCameraStreaming(c,enabled);cameraReceived[c]=0;
            JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"saveCameraStreaming","(IZ)V"),c,enabled);env->DeleteLocalRef(cls);}
        else if(cmd=="local_depth"){bool enabled=!network->depthStreaming;network->setDepthStreaming(enabled);JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"saveDepthStreaming","(Z)V"),enabled);env->DeleteLocalRef(cls);}

        else if(cmd=="local_stop"){pendingExitNormal=false;pendingRobotLimit=nullptr;startFlow.cancel();pendingGesture.cancel();network->requestStop(true);}
        else if(cmd=="local_reconnect")network->requestReconnect();
        else if(cmd=="local_settings")openSettings();
        else if(cmd=="local_exit_normal"){
            startFlow.cancel();pendingGesture.cancel();pendingRobotLimit=nullptr;
            pendingExitNormal=true;exitStopAck=network->stopAcknowledged;exitDeadline=milliseconds()+5000;
            network->requestStop();inputMessage="Stopping before returning Pepper to normal";inputMessageUntil=exitDeadline;
        }
        else if(cmd=="local_robot_limit"){
            startFlow.cancel();pendingGesture.cancel();limitStopAck=network->stopAcknowledged;
            pendingRobotLimit={{"cmd","motion_limits"},{"key",item.action.value("key",std::string())},{"enabled",item.action.value("enabled",true)}};
            limitDeadline=milliseconds()+5000;network->requestStop();inputMessage="Pausing before changing robot limits";
        }
        else if(cmd=="local_calibrate"){startFlow.cancel();pendingGesture.cancel();calibrateRequested=true;}
        else if(cmd=="local_start")beginStart();
        else if(cmd=="gesture"){
            if(!network->connected)inputMessage="Connect Pepper before playing an animation";
            else if(state.contains("official_animations")&&std::find(state["official_animations"].begin(),state["official_animations"].end(),item.action["name"])==state["official_animations"].end())inputMessage="This official animation is not installed on your Pepper";
            else if(!network->snapshot().value("gesture",std::string()).empty())inputMessage="Animation already playing / B stops";
            else if(state.value("speech_gestures",nlohmann::json::object()).value("active",false))inputMessage="Auto gestures are playing / switch gestures OFF first";
            else if(startFlow.active())inputMessage="Starting: wait for calibration / B cancels";
            else if(network->armed&&!menuOpen){network->command(item.action);inputMessage="Animation: "+item.label+" / B stops";}
            else {beginStart(true);if(startFlow.active()){pendingGesture.begin(item.action.value("name",std::string()));inputMessage="Preparing animation: centre sticks and look forward / B cancels";}}
        }
        else if(network->connected&&cmd=="say")speakText(item.action.value("text",std::string()));
        else if(network->connected&&cmd=="speech_stop"){network->cancelVoiceAudio();updateVoice(false);network->command(item.action);}
        else if(network->connected)network->command(item.action);
        else inputMessage="Pepper disconnected: reconnecting";
    }
    int menuHit(float x,float y){
        if(pointerOnHelp){auto items=menuActions();for(int i=0;i<(int)items.size();++i)if(items[i].action.value("cmd",std::string())=="local_help"){Card r{HELP_W-114.f,24,78,34};return x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h?i:-1;}return -1;}

        auto items=menuActions();
        if(layoutEditing){for(int i=0;i<(int)items.size();++i){auto c=items[i].action.value("cmd",std::string());if(c.find("local_layout")!=0&&c!="local_help")continue;auto r=menuRect(i,items);if(x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h)return i;}return -1;}
        // Keep the small clear control clickable inside the map toggle target.
        for(int i=0;i<(int)items.size();++i)if(items[i].action.value("cmd",std::string())=="local_map_clear"){auto r=menuRect(i,items);if(x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h)return i;}
        const auto playing=network->snapshot().value("gesture",std::string());
        for(int i=0;i<(int)items.size();++i){
            if(items[i].action.value("cmd",std::string())=="gesture"&&!playing.empty()&&items[i].action.value("name",std::string())!=playing)continue;
            auto r=menuRect(i,items);if(x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h)return i;}
        return -1;
    }
    bool controllerPose(XrPosef& pose){XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        if(XR_FAILED(xrLocateSpace(aimSpace,local,frameTime,&location)))return false;
        auto flags=XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
        if((location.locationFlags&flags)!=flags)return false;pose=location.pose;return true;}
    void saveWindowMode(){JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"saveRoomLocked","(Z)V"),roomLocked);env->DeleteLocalRef(cls);}
    void beginWindowGesture(int gesture){if(!pointerGrip.held||!pointerValid||pointerOnHelp)return;XrPosef controller;if(!controllerPose(controller))return;
        if((!roomLocked||anchorPending)&&!placePanel())return;
        float u=0,v=0;if(!panel_anchor::hit(controller,roomPanel,panelWidth(),panelWidth()*H/W,u,v,false))return;
        roomLocked=true;saveWindowMode();windowGesture=gesture;grabStartController=controller;grabStartPanel=roomPanel;dragRelative=panel_anchor::compose(panel_anchor::inverse(controller),roomPanel);
        resizeStartWidth=panelWidth();resizeStartScale=panelScale;resizeStartRadius=std::hypot((u-.5f)*resizeStartWidth,(v-.5f)*resizeStartWidth*H/W);
        startFlow.cancel();pendingGesture.cancel();network->requestStop();menuOpen=true;inputMessage="Move freely / stick up-away, down-near / left-right size";inputMessageUntil=milliseconds()+4000;}
    bool updateWindowGesture(bool trigger){if(!windowGesture)return false;
        if(!trigger||!pointerGrip.held||!focused){if(windowGesture==2||windowGesture==3)savePanelScale();windowGesture=0;return true;}
        XrPosef controller;if(!controllerPose(controller))return true;
        if(windowGesture==3){
            float dt=lastResize>0?bounded(float((milliseconds()-lastResize)/1000.),0,.05f):0;
            auto axes=stick(1);dragRelative=panel_anchor::depth(dragRelative,axes.y,dt);
            roomPanel=panel_anchor::compose(controller,dragRelative);
            panelScale=panel_layout::resize(panelScale,axes.x,dt);
        }else if(windowGesture==1)roomPanel=panel_anchor::compose(controller,dragRelative);
        else {float u=0,v=0;if(panel_anchor::hit(controller,roomPanel,resizeStartWidth,resizeStartWidth*H/W,u,v,false)&&resizeStartRadius>.01f){float radius=std::hypot((u-.5f)*resizeStartWidth,(v-.5f)*resizeStartWidth*H/W);panelScale=bounded(resizeStartScale*radius/resizeStartRadius,.65f,1.f);}}
        return true;}
    bool placePanel(){XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};if(XR_FAILED(xrLocateSpace(viewSpace,local,frameTime,&head)))return false;
        auto required=XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;if((head.locationFlags&required)!=required){inputMessage="Window lock needs headset position tracking";return false;}
        roomPanel=panel_anchor::front(head.pose,1.3f);lockedFit=panelFit;anchorPending=false;return true;
    }
    void updateMenuPointer(XrTime time){
        pointerValid=false;pointerOnHelp=false;pointerBeamValid=false;XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=aim;get.subactionPath=hands[1];
        XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};if(XR_FAILED(xrGetActionStatePose(session,&get,&state))||!state.isActive)return;
        XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};if(XR_FAILED(xrLocateSpace(aimSpace,roomLocked&&!anchorPending?local:viewSpace,time,&loc)))return;
        auto required=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;if((loc.locationFlags&required)!=required)return;
        pointerController=loc.pose;pointerBeamValid=true;
        auto direction=rotate(loc.pose.orientation,{0,0,-1});pointerEnd={loc.pose.position.x+direction.x*2,loc.pose.position.y+direction.y*2,loc.pose.position.z+direction.z*2};
        auto target=panel_anchor::workspace(roomLocked,anchorPending,roomPanel);
        float u=0,v=0;pointerCard=-1;
        if(floating&&!anchorPending){float nearest=1e10f;for(int i=0;i<13;++i){auto r=floating_layout::cards[i];auto p=floating_layout::world(target,cardPoses[i],panelWidth());float a,b;if(panel_anchor::hit(loc.pose,p,panelWidth()*r.w/W*cardScales[i],panelWidth()*r.h/W*cardScales[i],a,b)){float d=floating_layout::distance(loc.pose,p);if(d<nearest){nearest=d;pointerValid=true;pointerCard=i;pointer={r.x+a*r.w,r.y+b*r.h};}}}
            auto hp=helpPose(target);if(helpOpen&&panel_anchor::hit(loc.pose,hp,panelWidth()*HELP_W/W*HELP_SCALE,panelWidth()*H/W*HELP_SCALE,u,v)&&floating_layout::distance(loc.pose,hp)<nearest){pointerValid=true;pointerOnHelp=true;pointerCard=-1;pointer={u*HELP_W,v*H};}
        }else{pointerValid=panel_anchor::hit(loc.pose,target,panelWidth(),panelWidth()*H/W,u,v);
            if(!pointerValid&&helpOpen){pointerValid=panel_anchor::hit(loc.pose,helpPose(target),panelWidth()*HELP_W/W*HELP_SCALE,panelWidth()*H/W*HELP_SCALE,u,v);pointerOnHelp=pointerValid;}
            pointer={u*(pointerOnHelp?HELP_W:W),v*H};}
        if(pointerValid){
            if(pointerOnHelp)pointerEnd=pointer_ray::endpoint(helpPose(target),panelWidth()*HELP_W/W*HELP_SCALE,panelWidth()*H/W*HELP_SCALE,pointer.x/HELP_W,pointer.y/H);
            else if(floating&&!anchorPending&&pointerCard>=0){auto r=floating_layout::cards[pointerCard];pointerEnd=pointer_ray::endpoint(floating_layout::world(target,cardPoses[pointerCard],panelWidth()),panelWidth()*r.w/W*cardScales[pointerCard],panelWidth()*r.h/W*cardScales[pointerCard],(pointer.x-r.x)/r.w,(pointer.y-r.y)/r.h);}
            else pointerEnd=pointer_ray::endpoint(target,panelWidth(),panelWidth()*H/W,pointer.x/W,pointer.y/H);
        }
        if(std::hypot(pointer.x-lastPointer.x,pointer.y-lastPointer.y)>10){pointerNavigation=true;lastPointer=pointer;}
        int hit=menuHit(pointer.x,pointer.y);if(pointerValid&&pointerNavigation&&hit>=0)menuIndex=hit;
    }
    void mapArm(PoseCommand& pose,const std::array<XrBodyJointLocationFB,XR_BODY_JOINT_COUNT_FB>& joints,bool left,const XrSpaceLocation& head){
        int s=left?XR_BODY_JOINT_LEFT_ARM_UPPER_FB:XR_BODY_JOINT_RIGHT_ARM_UPPER_FB;
        int e=left?XR_BODY_JOINT_LEFT_ARM_LOWER_FB:XR_BODY_JOINT_RIGHT_ARM_LOWER_FB;
        int w=left?XR_BODY_JOINT_LEFT_HAND_WRIST_FB:XR_BODY_JOINT_RIGHT_HAND_WRIST_FB;
        // Build anatomical axes from joint positions, not the runtime's bone-local
        // quaternion conventions: right = shoulder span, up = spine, back = cross.
        auto right=norm(sub(joints[XR_BODY_JOINT_RIGHT_ARM_UPPER_FB].pose.position,joints[XR_BODY_JOINT_LEFT_ARM_UPPER_FB].pose.position));
        auto up=norm(sub(joints[XR_BODY_JOINT_SPINE_UPPER_FB].pose.position,joints[XR_BODY_JOINT_HIPS_FB].pose.position));
        auto back=norm(cross(right,up));up=norm(cross(back,right));
        auto torso=[&](XrVector3f v){return XrVector3f{dot(v,right),dot(v,up),dot(v,back)};};
        auto upperRaw=torso(sub(joints[e].pose.position,joints[s].pose.position));
        auto foreRaw=torso(sub(joints[w].pose.position,joints[e].pose.position));
        int offset=left?2:7;
        const int side=left?0:1;
        pepper_arm::V upper{-upperRaw.z,-upperRaw.x,upperRaw.y};
        pepper_arm::V fore{-foreRaw.z,-foreRaw.x,foreRaw.y};
        humanArms[side]=pepper_arm::retarget(upper,fore);
        upper=pepper_arm::ry(pepper_arm::rx(upper,-torsoCommand[0]),-torsoCommand[1]);
        fore=pepper_arm::ry(pepper_arm::rx(fore,-torsoCommand[0]),-torsoCommand[1]);
        auto mapped=pepper_arm::retarget(upper,fore);
        auto elbow=mapped.elbow,wrist=mapped.wrist;
        float elbowWeight=1;faceBlend[side]=0;
        if(faceReach&&(head.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT)){
            auto delta=torso(sub(joints[w].pose.position,head.pose.position));
            pepper_arm::V relative{-delta.z,-delta.x,delta.y};
            relative=pepper_arm::ry(pepper_arm::rx(relative,-torsoCommand[0]),-torsoCommand[1]);
            auto goal=pepper_arm::faceGoal(mapped,relative,pepper_arm::length(fore),left,pose.angles[0],pose.angles[1]-torsoCommand[1]);
            wrist=goal.wrist;elbowWeight=goal.elbowWeight;faceBlend[side]=goal.blend;
        }
        goalHeight[side]=wrist.z;
        auto seed=pepper_arm::directionSeed(upper,fore,left);
        auto solution=elbowWeight<.99f?pepper_arm::solveFace(wrist,elbow,left,seed,elbowWeight):pepper_arm::solve(wrist,elbow,left,seed,elbowWeight);
        if(armSolutionValid[side]){
            auto continuous=pepper_arm::solve(wrist,elbow,left,armPrevious[side],elbowWeight);
            if(pepper_arm::cost(continuous,left,wrist,elbow,elbowWeight)<=pepper_arm::cost(solution,left,wrist,elbow,elbowWeight)+.000025f)solution=continuous;
        }
        armPrevious[side]=solution;armSolutionValid[side]=true;
        armError[side]=pepper_arm::length(pepper_arm::sub(pepper_arm::forward(solution,left).wrist,wrist));
        for(int k=0;k<4;++k)pose.angles[offset+k]=solution[k];
        // Twist relative to the calibrated hand/forearm orientation. Project the
        // rotation onto the anatomical forearm axis to exclude wrist flexion.
        auto relative=mul(inverse(joints[e].pose.orientation),joints[w].pose.orientation);
        auto delta=mul(relative,inverse(wristNeutral[left?0:1]));
        auto axis=norm(rotate(inverse(joints[e].pose.orientation),sub(joints[w].pose.position,joints[e].pose.position)));
        if(delta.w<0)delta={-delta.x,-delta.y,-delta.z,-delta.w};
        pose.angles[offset+4]=2*std::atan2(delta.x*axis.x+delta.y*axis.y+delta.z*axis.z,delta.w);
    }
    bool updatePose(XrTime time){
        XrActiveActionSet active{actionSet,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;XR(xrSyncActions(session,&sync));
        XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};XR(xrLocateSpace(viewSpace,local,time,&head));
        std::array<XrBodyJointLocationFB,XR_BODY_JOINT_COUNT_FB> joints{};XrBodyJointsLocateInfoFB locate{XR_TYPE_BODY_JOINTS_LOCATE_INFO_FB};locate.baseSpace=local;locate.time=time;
        XrBodyJointLocationsFB locations{XR_TYPE_BODY_JOINT_LOCATIONS_FB};locations.jointCount=XR_BODY_JOINT_COUNT_FB;locations.jointLocations=joints.data();
        bool tracked=XR_SUCCEEDED(locateBody(body,&locate,&locations))&&locations.isActive&&locations.confidence>.5f&&focused&&(head.locationFlags&XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT);
        skeleton=joints;bodyActive=locations.isActive;bodyConfidence=locations.confidence;
        for(int index:{XR_BODY_JOINT_HIPS_FB,XR_BODY_JOINT_SPINE_UPPER_FB,XR_BODY_JOINT_LEFT_ARM_UPPER_FB,XR_BODY_JOINT_LEFT_ARM_LOWER_FB,XR_BODY_JOINT_LEFT_HAND_WRIST_FB,XR_BODY_JOINT_RIGHT_ARM_UPPER_FB,XR_BODY_JOINT_RIGHT_ARM_LOWER_FB,XR_BODY_JOINT_RIGHT_HAND_WRIST_FB})
            tracked=tracked&&((joints[index].locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT)!=0);
        tracked=tracked&&((joints[XR_BODY_JOINT_SPINE_UPPER_FB].locationFlags&XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)!=0);
        for(int index:{XR_BODY_JOINT_LEFT_ARM_LOWER_FB,XR_BODY_JOINT_LEFT_HAND_WRIST_FB,XR_BODY_JOINT_RIGHT_ARM_LOWER_FB,XR_BODY_JOINT_RIGHT_HAND_WRIST_FB})
            tracked=tracked&&((joints[index].locationFlags&XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)!=0);
        bool manualCalibration=calibrateRequested;
        bool x=button(calibrate),a=button(arm),b=button(stop),y=button(menuAction);
        calibrateRequested=false;
        auto shoulderRight=norm(sub(joints[XR_BODY_JOINT_RIGHT_ARM_UPPER_FB].pose.position,joints[XR_BODY_JOINT_LEFT_ARM_UPPER_FB].pose.position));
        float bodyYaw=std::atan2(-shoulderRight.z,shoulderRight.x);
        char inputs[128];std::snprintf(inputs,sizeof(inputs),"INPUT X:%d Y:%d A:%d B:%d | L/R trigger %.1f %.1f",x,y,a,b,axis(triggers,0),axis(triggers,1));inputDiagnostic=inputs;
        if((x&&!wasX)||(a&&!wasA)||(b&&!wasB)||(y&&!wasY)){
            __android_log_print(ANDROID_LOG_INFO,"TelePepper","Buttons X=%d A=%d B=%d Y=%d body_active=%d confidence=%.2f tracking=%d calibrated=%d",x,a,b,y,locations.isActive,locations.confidence,tracked,calibrated);
            inputMessageUntil=milliseconds()+4500;
            if(x||a)inputMessage="Hold A + X together to start / pause";
            else if(b)inputMessage="B received: STOP";
            else inputMessage="Y received: menu toggled";
        }
        bool select=axis(triggers,1)>(wasSelect?.3f:.7f);
        if(y&&!wasY){pendingRobotLimit=nullptr;windowGesture=0;startFlow.cancel();pendingGesture.cancel();menuOpen=!menuOpen;network->requestStop();menuIndex=0;}
        wasY=y;
        bool gestureToggle=speechChord.update(milliseconds()/1000.,axis(grips,0)>.6f,axis(grips,1)>.6f,
            focused&&network->connected&&!b&&!windowGesture&&cardDrag<0&&!layoutEditing);
        if(speechChord.entered){
            JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);jclass cls=env->GetObjectClass(app->activity->clazz);
            env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"cancelRecognition","()V"));env->DeleteLocalRef(cls);wasTalk=false;
        }
        if(gestureToggle){auto gestures=network->snapshot().value("speech_gestures",nlohmann::json::object());
            bool enabled=!gestures.value("enabled",false);network->command({{"cmd","speech_gestures"},{"enabled",enabled}});
            inputMessage=enabled?"Auto speech gestures requested / head and base stay live":"Auto speech gestures OFF requested / arms return to tracking";inputMessageUntil=milliseconds()+4500;
        }
        pointerGrip.update(speechChord.suppress?0.f:axis(grips,1),axis(triggers,1),menuOpen);
        bool manipulating=updateWindowGesture(select);
        double resizeNow=milliseconds();float resizeDt=lastResize>0?float((resizeNow-lastResize)/1000.):0;lastResize=resizeNow;
        float resizeAxis=menuOpen?stick(1).y:0;
        if(menuOpen&&!layoutEditing&&!manipulating&&std::abs(resizeAxis)>.2f){panelScale=panel_layout::resize(panelScale,resizeAxis,resizeDt);resized=true;
            inputMessage="Panel "+std::to_string(int(panelScale*100))+"% / right stick up-down to resize";inputMessageUntil=resizeNow+1500;}
        else if(resized){savePanelScale();resized=false;}
        if(menuOpen||pointerGrip.held){
            if(pointerGrip.held){pointerNavigation=true;updateMenuPointer(time);}else {pointerValid=false;pointerNavigation=false;}auto move=menuOpen&&cardDrag<0?stick(0):XrVector2f{};auto items=menuActions();
            if(milliseconds()-lastMenuMove>240){
                if(std::abs(move.y)>.6f||std::abs(move.x)>.6f){int delta=std::abs(move.y)>std::abs(move.x)?(move.y>0?-1:1):(move.x>0?1:-1);
                    menuIndex=std::max(0,std::min((int)items.size()-1,menuIndex+delta));lastMenuMove=milliseconds();pointerNavigation=false;}
            }
            manipulating=updateCardGesture(select,resizeDt)||manipulating;
            if(!speechChord.suppress&&!manipulating&&!b&&!x&&!a&&(select&&!wasSelect)){
                int hit=pointerGrip.held?(pointerValid?menuHit(pointer.x,pointer.y):-1):menuIndex;
                if(hit>=0){menuIndex=hit;selectMenu();}
            }
        }
        if(!menuOpen&&!pointerGrip.held){pointerValid=false;if(cardDrag>=0){cardDrag=-1;saveLayout();}}
        wasSelect=select;
        network->audioFocus=focused&&app->activityState==APP_CMD_RESUME;
        bool wasPushToTalk=network->talk;
        network->talk=network->audioFocus&&!speechChord.suppress&&axis(grips,0)>.6f;
        if(wasPushToTalk&&!network->talk&&!speechChord.suppress&&network->audioFocus&&network->connected&&axis(grips,0)<.3f){
            auto gestures=network->snapshot().value("speech_gestures",nlohmann::json::object());
            if(gestures.value("enabled",false))network->command({{"cmd","speech_gesture_trigger"}});
        }
        updateVoice(network->talk);
        if(b&&!wasB){pendingExitNormal=false;pendingRobotLimit=nullptr;if(cardDrag>=0){cardDrag=-1;saveLayout();}windowGesture=0;startFlow.cancel();pendingGesture.cancel();network->requestStop();}
        if(manualCalibration){startFlow.cancel();pendingGesture.cancel();beginStart(false,true);}
        if(startChord.update(milliseconds()/1000.,a,x,!b&&!windowGesture&&!layoutEditing&&focused&&network->connected&&app->activityState==APP_CMD_RESUME)){
            if(network->armed||startFlow.active()){startFlow.cancel();pendingGesture.cancel();network->requestStop(true);inputMessage="Returning to neutral / hold A + X to resume";inputMessageUntil=milliseconds()+4000;}
            else beginStart();
        }else if(startChord.progress>0){inputMessage="Hold A + X: "+std::to_string(int(startChord.progress*100))+"%";inputMessageUntil=milliseconds()+1500;}

        if(!pendingRobotLimit.is_null()){
            auto state=network->snapshot();auto d=state.value("motion_diagnostics",nlohmann::json::object());
            if(!network->connected||milliseconds()>limitDeadline){pendingRobotLimit=nullptr;inputMessage="Limit change cancelled / check connection and STOP";inputMessageUntil=milliseconds()+4000;}
            else if(network->stopAcknowledged>limitStopAck&&!network->armed&&milliseconds()-network->lastSnapshot<750&&!d.value("stopping",true)&&d.value("stop_error",std::string()).empty()){
                network->command(pendingRobotLimit);pendingRobotLimit=nullptr;inputMessage="Limit change requested / START resumes";inputMessageUntil=milliseconds()+4000;
            }
        }
        if(pendingExitNormal){
            auto d=network->snapshot();
            if(!network->connected||milliseconds()>exitDeadline){pendingExitNormal=false;inputMessage="Exit cancelled / STOP was not confirmed";inputMessageUntil=milliseconds()+5000;}
            else if(network->stopAcknowledged>exitStopAck&&!network->armed&&milliseconds()-network->lastSnapshot<750&&!d.value("stopping",true)&&d.value("stop_error",std::string()).empty()){
                pendingExitNormal=false;
                JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);
                jclass cls=env->GetObjectClass(app->activity->clazz);
                env->CallVoidMethod(app->activity->clazz,env->GetMethodID(cls,"exitTeleoperation","()V"));env->DeleteLocalRef(cls);
            }
        }
        auto startAction=startFlow.tick(milliseconds()/1000.,startInput(tracked));
        if(startFlow.active()||startAction==StartFlow::Cancelled||startAction==StartFlow::Finished){inputMessage=startFlow.message;inputMessageUntil=milliseconds()+4000;}
        if(startAction==StartFlow::Prepare)network->command({{"cmd","prepare_motion"},{"confirmed",true}});
        if(startAction==StartFlow::Cancelled){
            if(std::string(startFlow.message)=="Start refused: check Pepper status")inputMessage="Start refused: "+network->status();
            __android_log_print(ANDROID_LOG_WARN,"TelePepper","Start flow cancelled: %s",inputMessage.c_str());
            pendingGesture.cancel();network->requestStop();}
        auto gestureAction=pendingGesture.tick(startAction==StartFlow::Finished,startAction==StartFlow::Cancelled,network->armed,network->snapshot().value("gesture",std::string()),milliseconds()/1000.);
        if(gestureAction==DeferredGesture::Send){network->command({{"cmd","gesture"},{"name",pendingGesture.name}});inputMessage="Animation playing / B stops / live tracking resumes";inputMessageUntil=milliseconds()+7000;}
        else if(gestureAction==DeferredGesture::Stop){network->requestStop();inputMessage="Animation not acknowledged: check Pepper status";inputMessageUntil=milliseconds()+7000;}
        bool autoCalibration=startAction==StartFlow::Calibrate;
        if(autoCalibration&&tracked){
            offsets.fill(0);saveOffsets();torsoCommand={};faceBlend={};goalHeight={};armError={};
            driveInput=DriveInput{};
            neutral=head.pose.orientation;calibrated=true;
            neutralBodyYaw=bodyYaw;
            for(int side=0;side<2;++side){
                int s=side==0?XR_BODY_JOINT_LEFT_ARM_UPPER_FB:XR_BODY_JOINT_RIGHT_ARM_UPPER_FB;
                int e=side==0?XR_BODY_JOINT_LEFT_ARM_LOWER_FB:XR_BODY_JOINT_RIGHT_ARM_LOWER_FB;
                int w=side==0?XR_BODY_JOINT_LEFT_HAND_WRIST_FB:XR_BODY_JOINT_RIGHT_HAND_WRIST_FB;
                auto u=sub(joints[e].pose.position,joints[s].pose.position),f=sub(joints[w].pose.position,joints[e].pose.position);
                float length=std::sqrt(dot(u,u))+std::sqrt(dot(f,f));
                if(!std::isfinite(length)||length<.2f||length>1.2f)calibrated=false;
                else armLength[side]=length;
                armSolutionValid[side]=false;
            }
            inputMessage=calibrated?"Full pose calibrated: head, arms, wrists, hands, torso and base":"Invalid arm tracking: retry Start";
            wristNeutral[0]=mul(inverse(joints[XR_BODY_JOINT_LEFT_ARM_LOWER_FB].pose.orientation),joints[XR_BODY_JOINT_LEFT_HAND_WRIST_FB].pose.orientation);
            wristNeutral[1]=mul(inverse(joints[XR_BODY_JOINT_RIGHT_ARM_LOWER_FB].pose.orientation),joints[XR_BODY_JOINT_RIGHT_HAND_WRIST_FB].pose.orientation);}
        if(startAction==StartFlow::Arm)network->requestArm(startFlow.expectedStop());
        wasX=x;wasA=a;wasB=b;
        PoseCommand pose;pose.sampled=milliseconds();pose.active=tracked&&calibrated&&!menuOpen&&!layoutEditing&&app->activityState==APP_CMD_RESUME&&(!network->anyVideoEnabled()||(network->lastVideo>0&&milliseconds()-network->lastVideo<1000));
        pose.pauseReason=app->activityState!=APP_CMD_RESUME||!focused?"focus":menuOpen?"controls":!calibrated?"calibration":!tracked?"tracking":(network->anyVideoEnabled()&&(network->lastVideo<=0||milliseconds()-network->lastVideo>=1000))?"video":"";
        if(tracked&&calibrated){auto forward=rotate(mul(inverse(neutral),head.pose.orientation),{0,0,-1});pose.angles[0]=std::atan2(-forward.x,-forward.z);pose.angles[1]=std::asin(bounded(-forward.y,-1,1));
            auto headUp=rotate(mul(inverse(neutral),head.pose.orientation),{0,1,0});float headRoll=std::atan2(headUp.x,headUp.y);
            torsoCommand=torsoAssist?headAssistance(pose.angles[1]+offsets[1]*.01745329252f,headRoll):std::array<float,2>{};
            mapArm(pose,joints,true,head);mapArm(pose,joints,false,head);
            pose.angles[12]=1-axis(triggers,0);pose.angles[13]=1-pointerGrip.handClosure;
        }
        if(poseMirror&&tracked&&calibrated)mirrorPose(pose.angles,torsoCommand);
        rawPose=pose;for(int i=0;i<12;++i)pose.angles[i]+=offsets[i]*.01745329252f;
        pose.torso=torsoCommand;pose.torsoAssist=torsoAssist;pose.angles[1]-=torsoCommand[1];
        auto l=stick(0),r=stick(1);pose.base=driveInput.update(network->armed,pose.active,l.x,l.y,r.x);pose.drive=driveInput.ready;if(!network->snapshot().value("robot_limits",nlohmann::json::object()).value("speed",true)){pose.base[0]/=.50f;pose.base[1]/=.40f;pose.base[2]/=.90f;}
        for(auto& speed:pose.base)speed*=driveScale;
        char driveText[160];std::snprintf(driveText,sizeof(driveText),"L %.2f %.2f | R %.2f | %s",l.x,l.y,r.x,menuOpen?"MENU":!pose.active?"PAUSED":!network->armed?"START":!driveInput.ready?"CENTER STICKS":"DRIVE READY");driveDiagnostic=driveText;
        static double lastDriveLog=0;if(milliseconds()-lastDriveLog>500){lastDriveLog=milliseconds();__android_log_print(ANDROID_LOG_INFO,"TelePepper","DriveDiag %s armed=%d active=%d base=%.3f/%.3f/%.3f video_age=%.0f",driveText,(int)network->armed.load(),pose.active,pose.base[0],pose.base[1],pose.base[2],milliseconds()-network->lastVideo);}
        pose.sticksNeutral=DriveInput::deadzone(l.x)==0&&DriveInput::deadzone(l.y)==0&&DriveInput::deadzone(r.x)==0;
        pose.autoTurn=pose.active;
        pose.bodyYaw=(poseMirror?-1.f:1.f)*std::atan2(std::sin(bodyYaw-neutralBodyYaw),std::cos(bodyYaw-neutralBodyYaw));
        static double lastArmLog=0;
        if(tracked&&calibrated&&milliseconds()-lastArmLog>2000){lastArmLog=milliseconds();
            auto status=network->snapshot();auto telemetry=status.value("telemetry",nlohmann::json::object());
            __android_log_print(ANDROID_LOG_INFO,"TelePepper","PoseDiag armed=%d headflags=%llu face=%.2f/%.2f goalz=%.3f/%.3f ik_mm=%.0f/%.0f requested=%s measured=%s",(int)network->armed.load(),(unsigned long long)head.locationFlags,faceBlend[0],faceBlend[1],goalHeight[0],goalHeight[1],armError[0]*1000,armError[1]*1000,nlohmann::json(pose.angles).dump().c_str(),telemetry.value("joint_measured",nlohmann::json::array()).dump().c_str());
        }
        bool robotGesture=!network->snapshot().value("gesture",std::string()).empty();
        bool returningFromGesture=wasRobotGesture&&!robotGesture;
        if(returningFromGesture){driveInput.ready=false;}
        wasRobotGesture=robotGesture;
        if(pendingGesture.active()||robotGesture||returningFromGesture){pose.base={};pose.autoTurn=false;}
        displayedPose=pose;network->publish(pose);return tracked;
    }
    float drawScaleX=1,drawOffsetX=0,drawScaleY=1,drawOffsetY=0;
    void vertices(const std::vector<float>& data,int textured,float r=1,float g=1,float b=1){
        if(!preserveColors&&(textured==0||textured==2)){auto c=dashboard_theme::uiColor({r,g,b},textured==2);r=c[0];g=c[1];b=c[2];}
        glUseProgram(program);glUniform1i(glGetUniformLocation(program,"textured"),textured);glUniform4f(glGetUniformLocation(program,"color"),r,g,b,drawAlpha);glUniform1i(glGetUniformLocation(program,"image"),0);
        std::vector<float> transformed;const float* source=data.data();if(drawScaleX!=1||drawOffsetX!=0||drawScaleY!=1||drawOffsetY!=0){transformed=data;for(size_t i=0;i<transformed.size();i+=4){transformed[i]=((transformed[i]+1)*W*.5f*drawScaleX+drawOffsetX)*2/W-1;transformed[i+1]=1-((1-transformed[i+1])*H*.5f*drawScaleY+drawOffsetY)*2/H;}source=transformed.data();}
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(float),source,GL_STREAM_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));glDrawArrays(GL_TRIANGLES,0,(GLsizei)data.size()/4);
    }
    void text(const std::string& value,float x,float y,float r,float g,float b,float size=2){
        std::vector<float> data;data.reserve(value.size()*24);float pen=x,scale=size*.25f;
        for(size_t i=0;i<value.size();){unsigned cp=(unsigned char)value[i++];
            if(cp>=192&&cp<224&&i<value.size()){cp=((cp&31)<<6)|((unsigned char)value[i++]&63);}
            else if(cp>=224){while(i<value.size()&&((unsigned char)value[i]&192)==128)++i;cp='?';}
            if(cp=='\n'){pen=x;y+=size*14;continue;}if(cp<32||cp>255)cp='?';
            const auto& glyph=UI_FONT_GLYPHS[cp-32];float gx=pen+glyph.x*scale,gy=y+glyph.y*scale,w=glyph.w*scale,h=glyph.h*scale;
            data.insert(data.end(),{2*gx/W-1,1-2*(gy+h)/H,glyph.u0,glyph.v1,2*(gx+w)/W-1,1-2*(gy+h)/H,glyph.u1,glyph.v1,2*(gx+w)/W-1,1-2*gy/H,glyph.u1,glyph.v0,2*gx/W-1,1-2*(gy+h)/H,glyph.u0,glyph.v1,2*(gx+w)/W-1,1-2*gy/H,glyph.u1,glyph.v0,2*gx/W-1,1-2*gy/H,glyph.u0,glyph.v0});pen+=glyph.advance*scale;
        }
        glBindTexture(GL_TEXTURE_2D,fontTexture);vertices(data,2,r,g,b);
    }
    void fitText(const std::string& value,float x,float y,float width,float r,float g,float b,float size){
        float advance=0;
        for(unsigned char c:value)advance+=UI_FONT_GLYPHS[(c>=32&&c<=255?c:'?')-32].advance;
        if(advance>0)size=std::min(size,std::max(0.f,width-4)*4/advance);
        text(value,x,y,r,g,b,size);
    }
    void centeredText(const std::string& value,Card bounds,float r,float g,float b,float size){
        float advance=0;for(unsigned char c:value)advance+=UI_FONT_GLYPHS[(c>=32?c:'?')-32].advance;
        if(advance>0)size=std::min(size,(bounds.w-20)*4/advance);
        float pen=0,left=1e9f,right=-1e9f,top=1e9f,bottom=-1e9f;
        for(unsigned char c:value){const auto& glyph=UI_FONT_GLYPHS[(c>=32?c:'?')-32];left=std::min(left,pen+glyph.x);right=std::max(right,pen+glyph.x+glyph.w);top=std::min(top,glyph.y);bottom=std::max(bottom,glyph.y+glyph.h);pen+=glyph.advance;}
        if(value.empty())return;float scale=size*.25f;
        text(value,bounds.x+(bounds.w-(right-left)*scale)*.5f-left*scale,bounds.y+(bounds.h-(bottom-top)*scale)*.5f-top*scale,r,g,b,size);
    }
    void pixelLine(float x1,float y1,float x2,float y2,float width,float r,float g,float b){
        float dx=x2-x1,dy=y2-y1,length=std::sqrt(dx*dx+dy*dy);
        if(length<.01f)return;float nx=-dy/length*width*.5f,ny=dx/length*width*.5f;
        std::vector<float> data;
        for(auto p:std::array<XrVector2f,6>{{{x1+nx,y1+ny},{x2+nx,y2+ny},{x2-nx,y2-ny},{x1+nx,y1+ny},{x2-nx,y2-ny},{x1-nx,y1-ny}}})
            data.insert(data.end(),{2*p.x/W-1,1-2*p.y/H,0,0});
        vertices(data,false,r,g,b);
    }
    void panel(float x,float y,float width,float height,float r,float g,float b,float radius=14){
        radius=std::max(0.f,std::min(radius,std::min(width,height)*.5f));
        std::vector<XrVector2f> edge;
        for(int corner=0;corner<4;++corner){float cx=corner==0||corner==3?x+radius:x+width-radius,cy=corner<2?y+radius:y+height-radius;
            for(int j=0;j<=6;++j){float angle=(-180+corner*90+j*15)*.01745329252f;edge.push_back({cx+radius*std::cos(angle),cy+radius*std::sin(angle)});}}
        std::vector<float> data;for(size_t i=0;i<edge.size();++i)for(auto p:std::array<XrVector2f,3>{{{x+width*.5f,y+height*.5f},edge[i],edge[(i+1)%edge.size()]}})data.insert(data.end(),{2*p.x/W-1,1-2*p.y/H,0,0});
        vertices(data,false,r,g,b);
    }
    void themedPanel(float x,float y,float width,float height,dashboard_theme::Color value,float radius){KeepColors original(preserveColors);panel(x,y,width,height,value[0],value[1],value[2],radius);}
    void surfaceCard(float x,float y,float width,float height,dashboard_theme::Color =dashboard_theme::accent){
        auto p=dashboard_theme::palette();themedPanel(x,y,width,height,p.border,20);themedPanel(x+1,y+1,width-2,height-2,p.surface,19);
    }
    void headerIcon(const std::string& command,Card r,bool hover){
        KeepColors original(preserveColors);auto p=dashboard_theme::palette();
        bool active=command=="local_help"?helpOpen:command=="local_layout"?layoutEditing:false;
        auto bg=active?p.selected:hover?p.button:p.surface,ink=active?dashboard_theme::rgb(0xffffff):p.primary;
        float cx=r.x+r.w*.5f,cy=r.y+r.h*.5f;
        themedPanel(cx-20,cy-20,40,40,bg,20);
        auto box=[&](float x,float y,float w,float h,float radius=2.f){panel(cx+x,cy+y,w,h,ink[0],ink[1],ink[2],radius);};
        auto line=[&](float x,float y,float xx,float yy,float width=3.f){pixelLine(cx+x,cy+y,cx+xx,cy+yy,width,ink[0],ink[1],ink[2]);};
        if(command=="local_help"){box(-11,-11,22,22,11);centeredText("?",{cx-22,cy-15,44,30},bg[0],bg[1],bg[2],2.4f);}
        else if(command=="local_layout"){
            if(layoutEditing){line(-9,0,-3,6,3.5f);line(-3,6,10,-7,3.5f);}
            else {box(-10,-10,8,20);box(1,-10,9,8);box(1,1,9,9);}
        }
    }
    void drawCrt(float x,float y,float w,float h,float strength){glUseProgram(program);glUniform1f(glGetUniformLocation(program,"crtTime"),std::fmod(milliseconds()/1000.,60.));glUniform1f(glGetUniformLocation(program,"crtStrength"),strength);vertices({2*x/W-1,1-2*(y+h)/H,0,1,2*(x+w)/W-1,1-2*(y+h)/H,1,1,2*(x+w)/W-1,1-2*y/H,1,0,2*x/W-1,1-2*(y+h)/H,0,1,2*(x+w)/W-1,1-2*y/H,1,0,2*x/W-1,1-2*y/H,0,0},4);}
    void drawReaction(const std::string& name,dashboard_layout::Rect screen){KeepColors original(preserveColors);
        float size=screen.h*.95f,scale=size/400,x=screen.x+(screen.w-size)*.5f,y=screen.y+(screen.h-size)*.5f;
        using Point=std::array<float,2>;using Color=std::array<float,3>;
        const Color ink{71/255.f,43/255.f,23/255.f},blue{61/255.f,165/255.f,242/255.f};
        auto fill=[&](const std::vector<Point>& points,Point centre,Color color){std::vector<float> data;for(size_t i=0;i<points.size();++i)for(auto p:std::array<Point,3>{centre,points[i],points[(i+1)%points.size()]})data.insert(data.end(),{2*(x+p[0]*scale)/W-1,1-2*(y+p[1]*scale)/H,0,0});vertices(data,0,color[0],color[1],color[2]);};
        auto oval=[&](float cx,float cy,float rx,float ry,Color color){std::vector<Point> points;for(int i=0;i<48;++i){float a=i*6.2831853f/48;points.push_back({cx+rx*std::cos(a),cy+ry*std::sin(a)});}fill(points,{cx,cy},color);};
        auto curve=[&](Point start,Point control,Point end){Point previous=start;for(int i=1;i<=20;++i){float t=i/20.f,u=1-t;Point next{u*u*start[0]+2*u*t*control[0]+t*t*end[0],u*u*start[1]+2*u*t*control[1]+t*t*end[1]};float dx=next[0]-previous[0],dy=next[1]-previous[1],length=std::max(.001f,std::sqrt(dx*dx+dy*dy)),nx=-dy*6/length,ny=dx*6/length;fill({{previous[0]+nx,previous[1]+ny},{next[0]+nx,next[1]+ny},{next[0]-nx,next[1]-ny},{previous[0]-nx,previous[1]-ny}},{(previous[0]+next[0])*.5f,(previous[1]+next[1])*.5f},ink);oval(next[0],next[1],6,6,ink);previous=next;}oval(start[0],start[1],6,6,ink);};
        auto heart=[&](float cx,float cy){std::vector<Point> points;for(int i=0;i<64;++i){float a=i*6.2831853f/64,s=std::sin(a);points.push_back({cx+3*16*s*s*s,cy-3*(13*std::cos(a)-5*std::cos(2*a)-2*std::cos(3*a)-std::cos(4*a))});}fill(points,{cx,cy},Color{241/255.f,59/255.f,83/255.f});};
        auto tear=[&](float cx,float cy){oval(cx,cy+40,16,23,blue);fill({{cx,cy},{cx-15,cy+32},{cx+15,cy+32}},{cx,cy+22},blue);};
        oval(200,206,164,164,{223/255.f,148/255.f,28/255.f});oval(200,200,160,160,name=="angry"?Color{250/255.f,119/255.f,65/255.f}:Color{1,206/255.f,61/255.f});
        if(name=="love"){heart(130,155);heart(270,155);}
        else if(name=="laugh"){curve({104,157},{130,124},{156,157});curve({244,157},{270,124},{296,157});tear(83,168);tear(317,168);}
        else{oval(130,152,12,21,ink);if(name=="wink")curve({244,154},{270,137},{296,154});else oval(270,152,12,21,ink);}
        if(name=="angry"){curve({101,105},{126,119},{153,127});curve({247,127},{274,119},{299,105});curve({145,284},{200,253},{255,284});}
        else if(name=="surprise")oval(200,264,31,42,ink);
        else if(name=="sad"){curve({145,282},{200,242},{255,282});curve({105,113},{126,117},{149,103});curve({251,103},{274,117},{295,113});tear(280,176);}
        else{std::vector<Point> mouth;for(int i=0;i<=24;++i){float t=i/24.f,u=1-t;mouth.push_back({u*u*111+2*u*t*200+t*t*289,u*u*223+2*u*t*259+t*t*223});}for(int i=1;i<=24;++i){float t=i/24.f,u=1-t;mouth.push_back({u*u*289+2*u*t*200+t*t*111,u*u*223+2*u*t*370+t*t*223});}fill(mouth,{200,270},ink);oval(200,303,35,14,{244/255.f,96/255.f,111/255.f});}
    }
    XrPosef helpPose(XrPosef main){
        XrPosef offset=identity();offset.position={panelWidth()*(.5f+.5f*HELP_W/W*HELP_SCALE)+.035f,0,0};
        return panel_anchor::compose(main,offset);
    }
    void drawHelp(){
        panel(12,8,HELP_W-24,H-16,.055f,.09f,.14f,24);
        text("Quick help",36,32,.92f,.97f,1,2.6f);
        auto bridge=network->snapshot().value("bridge",nlohmann::json::object());
        fitText("Quest "+appVersion+" / Head "+bridge.value("version",std::string("unknown")),36,70,HELP_W-160,.65f,.75f,.85f,1.1f);
        fitText("Your controls at a glance",36,75,HELP_W-72,.59f,.72f,.85f,ui_type::body);
        const char* titles[]={"Start & stop","Move Pepper","Point & speak","Cameras & sensors","Window & layout","Animations & recovery"};
        const char* first[]={"Hold A + X for 0.75 seconds to start/pause.","Left stick: move. Right stick: turn.","Right grip + trigger: point and click.","Tap Top, Bottom or Depth to toggle.","Layout: grip + trigger on panel titles.","Official clips return to live tracking."};
        const char* second[]={"Centre sticks, look forward. B stops.","Head, arms and triggers mirror your pose.","Left grip: release to speak + auto gestures.","Depth: light near; black far or no data.","Right stick scales. Save / Load / Reset.","Both grips toggle gestures. B cancels."};
        for(int i=0;i<6;++i){float y=125+i*137;panel(28,y,HELP_W-56,123,.075f,.13f,.20f,18);
            float cx=57,cy=y+29;panel(cx-16,cy-16,32,32,.12f,.30f,.40f,11);
            // Compact geometric icons remain legible without emoji font support.
            if(i==0){panel(cx-6,cy-7,4,14,.45f,1,.78f,1);panel(cx+2,cy-7,4,14,.45f,1,.78f,1);}
            else if(i==1){pixelLine(cx-9,cy,cx+9,cy,2,.45f,.85f,1);pixelLine(cx,cy-9,cx,cy+9,2,.45f,.85f,1);}
            else if(i==2){pixelLine(cx-6,cy-8,cx+8,cy+5,2,.45f,.85f,1);pixelLine(cx-6,cy-8,cx-4,cy+9,2,.45f,.85f,1);}
            else if(i==3){panel(cx-10,cy-7,20,14,.45f,.85f,1,3);panel(cx-4,cy-4,8,8,.075f,.13f,.20f,4);}
            else if(i==4){pixelLine(cx-10,cy-8,cx+10,cy-8,2,.45f,.85f,1);pixelLine(cx-10,cy+8,cx+10,cy+8,2,.45f,.85f,1);pixelLine(cx-10,cy-8,cx-10,cy+8,2,.45f,.85f,1);pixelLine(cx+10,cy-8,cx+10,cy+8,2,.45f,.85f,1);}
            else{pixelLine(cx-6,cy-8,cx+8,cy,2,.45f,.85f,1);pixelLine(cx+8,cy,cx-6,cy+8,2,.45f,.85f,1);pixelLine(cx-6,cy+8,cx-6,cy-8,2,.45f,.85f,1);}
            text(titles[i],85,y+18,.9f,.96f,1,ui_type::heading);
            fitText(first[i],46,y+61,HELP_W-92,.81f,.88f,.95f,ui_type::body);
            fitText(second[i],46,y+87,HELP_W-92,.61f,.74f,.86f,ui_type::body);
        }
        fitText("Keep this guide open while using TelePepper.",36,980,HELP_W-72,.97f,.76f,.48f,ui_type::secondary);
        Card close{HELP_W-114.f,24,78,34};panel(close.x,close.y,close.w,close.h,.13f,.25f,.36f,12);
        centeredText("Close",close,.9f,.96f,1,ui_type::body);
        if(pointerOnHelp&&pointerGrip.held&&pointerValid)panel(pointer.x-6,pointer.y-6,12,12,.4f,1,.83f,6);
    }
    void drawStudio(bool tracking){
        using J=nlohmann::json;auto state=network->snapshot();auto t=state.value("telemetry",J::object());
        bool fresh=network->connected&&milliseconds()-network->lastSnapshot<750&&t.value("available",false)&&state.value("robot_mono",0.)-t.value("robot_mono",-100.)<.75;
        themedPanel(20,8,W-40,48,dashboard_theme::palette().surface,24);
        text("TelePepper",36,14,.93f,.97f,1,2.3f);
        fitText(appVersion,36,38,222,.65f,.75f,.85f,1.f);
        auto readings=t.value("readings",J::object());bool batteryValid=fresh&&readings.contains("battery")&&readings["battery"].is_number();
        float battery=batteryValid?bounded(readings["battery"].get<float>(),0,1):0;
        std::string stats="Battery "+(batteryValid?std::to_string(int(std::lround(battery*100)))+"%":"--");
        if(fresh&&readings.contains("battery_temperature")&&readings["battery_temperature"].is_number())stats+=" / "+std::to_string(int(std::lround(dashboard_layout::displayTemperature(readings["battery_temperature"].get<float>(),fahrenheit))))+(fahrenheit?" F":" C");
        stats+="   |   Robot ";stats+=fresh&&t.contains("awake")&&t["awake"].is_boolean()?(t["awake"].get<bool>()?"AWAKE":"REST"):"--";
        fitText(stats,390,22,layoutEditing?365:790,.75f,.85f,.9f,ui_type::body);
        panel(270,22,100,22,.13f,.145f,.17f,7);if(batteryValid&&battery>0){auto color=dashboard_theme::batteryColor(battery);panel(273,25,94*battery,16,color[0],color[1],color[2],4);}

        const auto fused=dashboard_layout::fusion;surfaceCard(fused.x,fused.y,fused.w,fused.h);
        text("Pepper vision / Top + Bottom",fused.x+23,fused.y+10,.85f,.88f,.93f,ui_type::heading);
        const bool blendCameras=network->connected&&network->topStreaming&&network->bottomStreaming&&cameraReceived[0]>0&&cameraReceived[1]>0&&milliseconds()-cameraReceived[0]<1000&&milliseconds()-cameraReceived[1]<1000;
        const auto topContent=dashboard_layout::contain(dashboard_layout::cameras[0],cameraWidth[0],cameraHeight[0]);
        const auto bottomContent=dashboard_layout::contain(dashboard_layout::cameras[1],cameraWidth[1],cameraHeight[1]);
        for(int c=0;c<2;++c){KeepColors original(preserveColors);auto rect=dashboard_layout::cameras[c];bool freshCamera=network->connected&&network->cameraStreaming(c)&&cameraReceived[c]>0&&milliseconds()-cameraReceived[c]<1000;
            
            if(freshCamera){auto content=dashboard_layout::contain(rect,cameraWidth[c],cameraHeight[c]);float x=content.x,y=content.y,w=content.w,h=content.h;
                glUseProgram(program);glUniform4f(glGetUniformLocation(program,"cameraBounds"),x,y,w,h);glUniform1f(glGetUniformLocation(program,"cameraRadius"),dashboard_layout::cameraCornerRadius);glUniform1f(glGetUniformLocation(program,"cameraFade"),0.f);
                // One outer rounded silhouette while blended; inner corners stay square.
                if(blendCameras)glUniform4f(glGetUniformLocation(program,"cameraBounds"),x,topContent.y,w,bottomContent.y+bottomContent.h-topContent.y);
                glBindTexture(GL_TEXTURE_2D,cameraTextures[c]);vertices({2*x/W-1,1-2*(y+h)/H,0,1,2*(x+w)/W-1,1-2*(y+h)/H,1,1,2*(x+w)/W-1,1-2*y/H,1,0,2*x/W-1,1-2*(y+h)/H,0,1,2*(x+w)/W-1,1-2*y/H,1,0,2*x/W-1,1-2*y/H,0,0},c==1?3:1);glUniform1f(glGetUniformLocation(program,"cameraRadius"),0);glUniform1f(glGetUniformLocation(program,"cameraFade"),0);
            }else {drawCrt(rect.x,rect.y,rect.w,rect.h,.55f);fitText(!network->connected?"ROBOT OFFLINE":network->cameraStreaming(c)?"Waiting / stale feed":c==0?"TOP OFF / click to enable":"BOTTOM OFF / click to enable",rect.x+12,rect.y+rect.h*.4f,rect.w-24,1,.7f,.35f,ui_type::body);}
            fitText(c==0?"Top":"Bottom",rect.x+10,c==0?rect.y+9:rect.y+rect.h-22,rect.w-20,.85f,.94f,1,ui_type::secondary);
        }
        // Feather into both frames, matching their actual UVs exactly at each boundary.
        if(blendCameras){
            constexpr float feather=36;
            float x=topContent.x,y=topContent.y+topContent.h-feather,w=topContent.w,h=bottomContent.y-(topContent.y+topContent.h)+2*feather;
            glUseProgram(program);glUniform4f(glGetUniformLocation(program,"cameraBounds"),x,topContent.y,w,bottomContent.y+bottomContent.h-topContent.y);glUniform1f(glGetUniformLocation(program,"cameraRadius"),dashboard_layout::cameraCornerRadius);
            glUniform4f(glGetUniformLocation(program,"seamTop"),topContent.x,topContent.y,topContent.w,topContent.h);glUniform4f(glGetUniformLocation(program,"seamBottom"),bottomContent.x,bottomContent.y,bottomContent.w,bottomContent.h);
            glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,cameraTextures[1]);glUniform1i(glGetUniformLocation(program,"secondImage"),1);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,cameraTextures[0]);
            vertices({2*x/W-1,1-2*(y+h)/H,0,1,2*(x+w)/W-1,1-2*(y+h)/H,1,1,2*(x+w)/W-1,1-2*y/H,1,0,2*x/W-1,1-2*(y+h)/H,0,1,2*(x+w)/W-1,1-2*y/H,1,0,2*x/W-1,1-2*y/H,0,0},5);
            glUniform1f(glGetUniformLocation(program,"cameraRadius"),0);
        }
        {
            auto card=dashboard_layout::depthCard;surfaceCard(card.x,card.y,card.w,card.h);
            const auto rect=dashboard_layout::cameras[2];float x=rect.x,y=rect.y,w=rect.w,h=rect.h;
            std::string label="Depth";
            fitText(label,card.x+23,card.y+10,card.w-32,.6f,.85f,.9f,ui_type::heading);
            if(network->connected&&network->depthStreaming&&cameraReceived[2]>0&&milliseconds()-cameraReceived[2]<1000){auto content=dashboard_layout::contain(rect,cameraWidth[2],cameraHeight[2]);x=content.x;y=content.y;w=content.w;h=content.h;
                glBindTexture(GL_TEXTURE_2D,cameraTextures[2]);vertices({2*x/W-1,1-2*(y+h)/H,0,1,2*(x+w)/W-1,1-2*(y+h)/H,1,1,2*(x+w)/W-1,1-2*y/H,1,0,2*x/W-1,1-2*(y+h)/H,0,1,2*(x+w)/W-1,1-2*y/H,1,0,2*x/W-1,1-2*y/H,0,0},1);
            }else{drawCrt(x,y,w,h,.55f);fitText(!network->connected?"ROBOT OFFLINE":network->depthStreaming?"Waiting / stale feed":"DEPTH OFF",x+10,y+30,w-20,1,.7f,.35f,ui_type::body);if(!network->depthStreaming)fitText("Click to enable",x+10,y+54,w-20,.65f,.8f,.86f,ui_type::secondary);}
        }
        const auto map=dashboard_layout::lidar;auto mapCard=dashboard_layout::lidarCard;surfaceCard(mapCard.x,mapCard.y,mapCard.w,mapCard.h);fitText("Lidar",mapCard.x+23,mapCard.y+10,mapCard.w-32,.6f,.85f,.9f,ui_type::heading);
        drawScaleX=map.w/630;drawOffsetX=map.x-490*drawScaleX;drawScaleY=map.h/473;drawOffsetY=map.y-137*drawScaleY;if(showLaserMap&&fresh){KeepColors original(preserveColors);drawLaserMap();}drawScaleX=drawScaleY=1;drawOffsetX=drawOffsetY=0;
        if(!showLaserMap||!fresh){drawCrt(map.x,map.y,map.w,map.h,.55f);fitText(!network->connected?"ROBOT OFFLINE":!showLaserMap?"LIDAR OFF":"Waiting / stale feed",map.x+10,map.y+30,map.w-20,1,.7f,.35f,ui_type::body);fitText("Click to enable",map.x+10,map.y+54,map.w-20,.65f,.8f,.86f,ui_type::secondary);}
        {
            const auto preview=dashboard_layout::tablet;float px=preview.x+23,py=preview.y;
            surfaceCard(preview.x,py,preview.w,preview.h);fitText("Tablet",px,py+9,preview.w-35,.85f,.88f,.93f,ui_type::heading);
            auto content=state.value("tablet",J::object());auto delivery=state.value("tablet_display",J::object());
            bool shown=fresh&&delivery.value("connected",false)&&delivery.value("revision",-1)==content.value("revision",0);
            // A small confirmation dot replaces the redundant delivery wording.
            panel(preview.x+preview.w-18,py+14,6,6,shown?.3f:1.f,shown?.8f:.65f,.55f,3);
            KeepColors original(preserveColors);const auto screen=dashboard_layout::tabletScreen;
            panel(screen.x,screen.y,screen.w,screen.h,26/255.f,26/255.f,26/255.f,3);
            drawAlpha=tabletPreview&&fresh?content.value("opacity",1.f):1.f;
            std::string reaction=content.value("reaction",std::string());
            if(!tabletPreview||!fresh){drawCrt(screen.x,screen.y,screen.w,screen.h,.55f);centeredText(!network->connected?"ROBOT OFFLINE":!tabletPreview?"Preview off":"Waiting / stale feed",{screen.x,screen.y,screen.w,screen.h},.8f,.9f,1,ui_type::body);}
            else if(!reaction.empty())drawReaction(reaction,screen);
            else{
                std::string message=content.value("text",std::string());
                std::vector<std::string> lines;size_t pos=0;
                while(pos<message.size()&&lines.size()<4){size_t n=std::min(size_t(24),message.size()-pos);if(pos+n<message.size()){size_t space=message.rfind(' ',pos+n);if(space>pos&&space!=std::string::npos)n=space-pos;}lines.push_back(message.substr(pos,n));pos+=n;while(pos<message.size()&&message[pos]==' ')++pos;}
                if(pos<message.size()&&!lines.empty())lines.back()+="...";
                float step=13,y=screen.y+(screen.h-lines.size()*step)*.5f;
                for(const auto& line:lines){centeredText(line,{screen.x+6,y,screen.w-12,step},.95f,.96f,1,ui_type::secondary);y+=step;}
                if(content.contains("choices")&&content["choices"].is_array()){int n=content["choices"].size();float step=n?(screen.w-12)/n:32;for(int i=0;i<n;++i){float cy=screen.y+screen.h-16;panel(screen.x+6+i*step,cy,step-2,12,.17f,.28f,.34f,2);fitText(content["choices"][i].get<std::string>(),screen.x+8+i*step,cy+2,step-6,.85f,.96f,1,.8f);}}
            }
            drawAlpha=1;
        }
        drawComparison(tracking);
        const auto thermal=dashboard_layout::temperatures;surfaceCard(thermal.x,thermal.y,thermal.w,thermal.h);text("Joint temperatures",thermal.x+23,thermal.y+7,.85f,.88f,.93f,ui_type::heading);
        const char* names[]={"HeadYaw","HeadPitch","HipRoll","HipPitch","KneePitch","LShoulderPitch","LShoulderRoll","LElbowYaw","LElbowRoll","LWristYaw","RShoulderPitch","RShoulderRoll","RElbowYaw","RElbowRoll","RWristYaw","LHand","RHand","WheelFL","WheelFR","WheelB"};
        const char* shortNames[]={"HY","HP","HipR","HipP","Knee","LSP","LSR","LEY","LER","LWY","RSP","RSR","REY","RER","RWY","LH","RH","WFL","WFR","WB"};
        auto temps=t.value("joint_temperature",J::object()),statuses=t.value("joint_temperature_status",J::object());
        for(int i=0;i<20;++i){auto n=names[i];bool available=fresh&&temps.contains(n)&&temps[n].is_number();bool statusValid=fresh&&statuses.contains(n)&&statuses[n].is_number();bool warning=statusValid&&statuses[n].get<float>()!=0;
            std::string value=available?std::to_string(int(std::lround(dashboard_layout::displayTemperature(temps[n].get<float>(),fahrenheit)))):"--";
            KeepColors original(preserveColors);auto tile=dashboard_layout::temperatureTile(i);float x=tile.x,y=tile.y;
            auto color=thermalColor(available?temps[n].get<float>():NAN,warning);float shade=warning?.48f:.22f;
            panel(x,y,tile.w,tile.h,color[0]*shade,color[1]*shade,color[2]*shade,4);
            for(int step=0;step<8;++step){float brightness=.3f+.7f*step/7;panel(x+4+step*9,y+tile.h-4,9,2,color[0]*brightness,color[1]*brightness,color[2]*brightness,0);}
            fitText(shortNames[i],x+5,y+3,66,.8f,.85f,.9f,ui_type::secondary);
            centeredText(value,{x+4,y+18,tile.w-8,23},color[0],color[1],color[2],2.f);
            if(warning||!statusValid)text(warning?"!":"?",x+73,y+5,warning?1.f:.8f,warning?.15f:.7f,warning?.12f:.4f,.8f);
        }
        int thermalHover=!pointerOnHelp&&pointerGrip.held&&pointerValid?dashboard_layout::temperatureAt(pointer.x,pointer.y):-1;
        std::string temperatureLegend=fresh?"Temperature colour key / red = robot thermal alert":"STALE / waiting for robot sensors";
        if(thermalHover>=0)temperatureLegend=std::string(names[thermalHover])+(fahrenheit?" / temperature in Fahrenheit":" / temperature in Celsius");
        fitText(temperatureLegend,thermal.x+12,thermal.y+thermal.h-18,thermal.w-24,.75f,.8f,.85f,ui_type::secondary);
        auto tablet=state.value("tablet_display",J::object());bool delivered=fresh&&tablet.value("connected",false)&&tablet.value("revision",-1)==state.value("tablet",J::object()).value("revision",0);
        std::string tabletState=delivered?"Tablet: displayed":tablet.value("connected",false)?"Tablet: updating":"Tablet: disconnected";
        if(state.contains("participant_response")&&state["participant_response"].is_object()&&state["participant_response"].value("revision",-1)==state.value("tablet",J::object()).value("revision",0))tabletState+=" / answer: "+state["participant_response"].value("value",std::string());
        
        const char* groups[]={"Motion","Voice & phrases","Tablet","View","Connection","Pose offsets","Appearance"};
        const auto& xs=dashboard_layout::x;const auto& ws=dashboard_layout::width;
        for(int g=0;g<7;++g){if(g==3||g==5)continue;auto tint=dashboard_theme::section[g];surfaceCard(xs[g],dashboard_layout::groupY[g],ws[g],dashboard_layout::groupHeight[g],tint);fitText(groups[g],xs[g]+23,dashboard_layout::groupY[g]+8,ws[g]-35,.85f,.88f,.93f,ui_type::heading);}
        fitText("OFFICIAL ANIMATIONS / B cancels",xs[0]+10,dashboard_layout::groupY[0]+174,ws[0]-20,.55f,.85f,.75f,ui_type::secondary);
        pixelLine(xs[1]+12,dashboard_layout::groupY[1]+136,xs[1]+ws[1]-12,dashboard_layout::groupY[1]+136,1,.21f,.25f,.34f);
        fitText("PHRASES",xs[1]+12,dashboard_layout::groupY[1]+139,ws[1]-24,.7f,.75f,.9f,ui_type::secondary);
        if(layoutEditing){for(int i=0;i<13;++i){auto r=floating_layout::cards[i];pixelLine(r.x+5,r.y+2,r.x+r.w-5,r.y+2,3,.25f,.65f,1);if(i<11)panel(r.x+r.w*.5f-20,r.y+4,40,3,.4f,.75f,1,1.5f);}}
        auto items=menuActions();menuIndex=std::max(0,std::min(menuIndex,int(items.size())-1));
        const std::string playingAnimation=state.value("gesture",std::string());
        for(int i=0;i<(int)items.size();++i){auto r=menuRect(i,items);bool selected=(menuOpen||pointerGrip.held)&&i==menuIndex&&(!pointerGrip.held||(pointerValid&&menuHit(pointer.x,pointer.y)==i));int g=menuGroup(items[i]);bool stop=items[i].action.value("cmd",std::string())=="local_stop";
            const auto command=items[i].action.value("cmd",std::string());
            bool animationButton=command=="gesture";
            bool playingButton=animationButton&&!playingAnimation.empty()&&items[i].action.value("name",std::string())==playingAnimation;
            bool disabledAnimation=animationButton&&((!playingAnimation.empty()&&!playingButton)||state.value("speech_gestures",J::object()).value("active",false));
            if(disabledAnimation)selected=false;else if(playingButton)selected=true;
            
            if(command=="local_layout"||command=="local_help"){headerIcon(command,r,selected);continue;}
            if(command.find("local_panel_")==0){if(!(windowGesture||pointerGrip.held&&pointerValid&&dashboard_layout::windowHover(pointer.x,pointer.y)))continue;float cx=r.x+r.w*.5f,cy=r.y+r.h*.5f;panel(r.x,r.y,r.w,r.h,selected?.15f:.07f,selected?.32f:.12f,selected?.4f:.17f,r.h*.5f);
                if(command=="local_panel_move")panel(cx-70,cy-3,140,6,.7f,.78f,.86f,3);
                else if(command=="local_panel_resize"){pixelLine(cx-9,cy+8,cx+9,cy-8,2,.85f,.9f,1);pixelLine(cx-9,cy+1,cx-9,cy+8,2,.85f,.9f,1);pixelLine(cx-9,cy+8,cx-2,cy+8,2,.85f,.9f,1);pixelLine(cx+9,cy-8,cx+9,cy-1,2,.85f,.9f,1);pixelLine(cx+2,cy-8,cx+9,cy-8,2,.85f,.9f,1);}
                else if(command=="local_panel_lock"){float green=roomLocked?.95f:.75f;pixelLine(cx-7,cy-9,cx+7,cy-9,2,.65f,green,1);pixelLine(cx-5,cy-9,cx-5,cy+2,2,.65f,green,1);pixelLine(cx+5,cy-9,cx+5,cy+2,2,.65f,green,1);pixelLine(cx-8,cy+2,cx+8,cy+2,2,.65f,green,1);pixelLine(cx,cy+2,cx,cy+11,2,.65f,green,1);if(!roomLocked)pixelLine(cx-10,cy+11,cx+10,cy-12,2,.8f,.85f,.9f);}
                else {pixelLine(cx-10,cy,cx+10,cy,2,.85f,.9f,1);pixelLine(cx,cy-10,cx,cy+10,2,.85f,.9f,1);panel(cx-3,cy-3,6,6,.5f,.9f,1,3);}continue;}
            if(command=="local_map"||command=="local_depth"||command=="local_camera"||command=="local_tablet_preview"){if(selected){pixelLine(r.x,r.y,r.x+r.w,r.y,3,.4f,1,.83f);pixelLine(r.x,r.y+r.h,r.x+r.w,r.y+r.h,3,.4f,1,.83f);}continue;}
            if(command=="local_temperature_unit"){KeepColors original(preserveColors);auto p=dashboard_theme::palette();themedPanel(r.x,r.y,r.w,r.h,p.button,12);themedPanel(r.x+(fahrenheit?r.w*.5f:0),r.y,r.w*.5f,r.h,p.selected,12);auto c=fahrenheit?p.primary:dashboard_theme::rgb(0xffffff),f=fahrenheit?dashboard_theme::rgb(0xffffff):p.primary;centeredText("C",{r.x,r.y,r.w*.5f,r.h},c[0],c[1],c[2],1.2f);centeredText("F",{r.x+r.w*.5f,r.y,r.w*.5f,r.h},f[0],f[1],f[2],1.2f);continue;}
            auto buttonPalette=dashboard_theme::palette();auto fill=disabledAnimation?buttonPalette.surface:selected?buttonPalette.selected:buttonPalette.button;
            if(command=="local_start")fill=dashboard_theme::palette().accent;
            if(stop)fill={.75f,.12f,.16f};
            themedPanel(r.x,r.y,r.w,r.h,fill,std::min(16.f,r.h*.5f));
            if(items[i].label.find(": OFF")!=std::string::npos&&command!="local_listen"&&command!="local_pose_mirror"&&command!="local_robot_limit")drawCrt(r.x+3,r.y+3,r.w-6,r.h-6,.20f);
            if(items[i].action.value("cmd",std::string())=="local_led_preset"){KeepColors original(preserveColors);int color=ledTarget==2?0x4388ff:items[i].action.value("color",0);float level=ledTarget==2?items[i].action.value("intensity",1.f):1.f;
                float diameter=std::min(r.w,r.h)-10,cx=r.x+r.w*.5f,cy=r.y+r.h*.5f;
                if(ledSwatches[ledTarget]==items[i].action.value("swatch",-2)){panel(cx-diameter*.5f-4,cy-diameter*.5f-4,diameter+8,diameter+8,.45f,.85f,1.f,diameter*.5f+4);panel(cx-diameter*.5f-2,cy-diameter*.5f-2,diameter+4,diameter+4,.06f,.10f,.15f,diameter*.5f+2);}
                panel(cx-diameter*.5f,cy-diameter*.5f,diameter,diameter,((color>>16)&255)/255.f*level,((color>>8)&255)/255.f*level,(color&255)/255.f*level,diameter*.5f);if((color==0&&ledTarget!=2)||level==0)pixelLine(cx-9,cy+9,cx+9,cy-9,2,.8f,.85f,.9f);continue;}
            if(command=="local_map_clear"){KeepColors selectedIcon(preserveColors,selected);float cx=r.x+r.w*.5f,cy=r.y+r.h*.5f;
                pixelLine(cx-8,cy-6,cx+8,cy-6,2,.9f,.96f,1);pixelLine(cx-4,cy-9,cx+4,cy-9,2,.9f,.96f,1);
                pixelLine(cx-6,cy-4,cx-5,cy+8,2,.9f,.96f,1);pixelLine(cx+6,cy-4,cx+5,cy+8,2,.9f,.96f,1);pixelLine(cx-5,cy+8,cx+5,cy+8,2,.9f,.96f,1);continue;}
            if(stop||command=="local_start"){KeepColors original(preserveColors);centeredText(items[i].label,r,1.f,1.f,1,ui_type::primary);continue;}
            KeepColors selectedText(preserveColors,selected||disabledAnimation);auto caption=disabledAnimation?dashboard_theme::rgb(0x777777):selected?dashboard_theme::rgb(0xffffff):dashboard_theme::Color{.9f,.96f,1};
            const auto& title=items[i].label;float size=(command=="local_start"||command=="local_stop")?ui_type::primary:ui_type::body;
            if(items[i].action.value("phrase",false)&&r.h>=32){float advance=0;for(unsigned char c:title)advance+=UI_FONT_GLYPHS[(c>=32?c:'?')-32].advance;
                if(advance*size*.25f>r.w-20){size_t split=title.rfind(' ',title.size()/2);if(split==std::string::npos)split=title.find(' ',title.size()/2);if(split!=std::string::npos){centeredText(title.substr(0,split),{r.x,r.y,r.w,r.h*.5f},caption[0],caption[1],caption[2],size);centeredText(title.substr(split+1),{r.x,r.y+r.h*.5f,r.w,r.h*.5f},caption[0],caption[1],caption[2],size);continue;}}
            }
            centeredText(title,r,caption[0],caption[1],caption[2],size);
        }
        themedPanel(20,958,W-40,48,dashboard_theme::palette().surface,24);
        auto animation=state.value("animation",J::object());
        std::string motion=network->armed?"LIVE / B STOP":startFlow.active()?"STARTING / B CANCEL":"PAUSED / HOLD A + X";
        if(network->armed&&!state.value("gesture",std::string()).empty())motion="ANIMATION "+animation.value("name",std::string())+" / "+animation.value("stage",std::string())+" "+std::to_string(int(animation.value("elapsed",0.)))+"s";
        char status[240];bool recentAck=network->armed&&milliseconds()-network->lastAck<300;
        std::snprintf(status,sizeof(status),"%s  |  %s  |  Tracking %s  |  RTT %.0f ms / Queue %.0f ms / NAOqi %.1f ms  |  Recoveries %d",
            motion.c_str(),network->connected?"Connected":"Reconnecting",tracking?"ON":"OFF",recentAck?network->rtt.load():-1.,recentAck?network->queueMs.load():-1.,recentAck?network->applyMs.load():-1.,state.value("motion_diagnostics",J::object()).value("recovery_count",0));
        fitText(status,36,964,W-72,.75f,.88f,.9f,ui_type::body);
        std::string message=milliseconds()<inputMessageUntil?inputMessage:network->status();
        if(menuOpen&&milliseconds()>=inputMessageUntil)message=items[menuIndex].detail+" | "+network->status();
        fitText(message,36,986,816,.95f,.8f,.55f,ui_type::secondary);
        std::string audio=network->ttsVoice?voiceStatus:network->talk?"MIC LIVE":network->audioStatus();
        fitText(audio+" | "+tabletState,870,986,W-902,.6f,.85f,.86f,ui_type::secondary);
        if(milliseconds()-crtAt<320){float strength=.18f*std::max(0.f,1.f-float((milliseconds()-crtAt)/320.));drawCrt(crtBounds[0],crtBounds[1],crtBounds[2],crtBounds[3],strength);}
        std::string hovered;
        if(!pointerOnHelp&&pointerGrip.held&&pointerValid){int hit=menuHit(pointer.x,pointer.y);if(hit>=0){auto cmd=items[hit].action.value("cmd",std::string());if(cmd=="local_layout"||cmd=="local_help")hovered=items[hit].label;}}
        if(hovered!=headerHover){headerHover=hovered;headerHoverAt=milliseconds();}
        if(!layoutEditing&&!headerHover.empty()&&milliseconds()-headerHoverAt>180){KeepColors original(preserveColors);auto p=dashboard_theme::palette();themedPanel(864,10,314,40,p.button,20);centeredText(headerHover,{864,10,314,40},p.primary[0],p.primary[1],p.primary[2],ui_type::body);}
        if(!pointerOnHelp&&pointerGrip.held&&pointerValid){auto p=dashboard_theme::palette();themedPanel(pointer.x-6,pointer.y-6,12,12,p.accent,6);themedPanel(pointer.x-2,pointer.y-2,4,4,p.primary,2);}
    }
    void drawComparison(bool tracking){
        using namespace pepper_arm;using J=nlohmann::json;
        const auto comparison=dashboard_layout::comparison;surfaceCard(comparison.x,comparison.y,comparison.w,comparison.h);drawOffsetY=comparison.y-100;
        text("Pose comparison",43,112,.85f,.88f,.93f,ui_type::heading);
        text("Quest",32,145,.2f,1,.3f,ui_type::body);text("Target",166,145,1,.65f,.1f,ui_type::body);text("Robot",310,145,.2f,.75f,1,ui_type::body);
        {KeepColors original(preserveColors);panel(32,173,196,243,26/255.f,26/255.f,26/255.f,12);panel(240,173,208,243,26/255.f,26/255.f,26/255.f,12);text("FRONT",99,181,.6f,.7f,.8f,ui_type::secondary);text("SIDE",316,181,.6f,.7f,.8f,ui_type::secondary);}
        for(int i=0;i<2;++i){float x=i==0?32:240;pixelLine(x+8,290,x+(i==0?188:200),290,1,.13f,.22f,.28f);}
        struct Bone{V a,b;float width,r,g,blue;};struct Marker{V p;float radius,width,r,g,b;bool outline;};
        std::vector<Bone> bones;std::vector<Marker> markers;
        auto bone=[&](V a,V b,float width,float r,float g,float blue){bones.push_back({a,b,width,r,g,blue});};
        auto drawArm=[&](Pose p,bool left,float twist,float grip,float width,float r,float g,float b,float hipRoll=0,float hipPitch=0){
            auto tilt=[&](V p){V hip{0,0,-.26f};return add(hip,rx(ry(sub(p,hip),hipPitch),hipRoll));};
            V shoulder{0,left?.14974f:-.14974f,0};auto elbow=add(shoulder,p.elbow),wrist=add(shoulder,p.wrist);
            shoulder=tilt(shoulder);elbow=tilt(elbow);wrist=tilt(wrist);
            bone(shoulder,elbow,width,r,g,b);bone(elbow,wrist,width,r,g,b);
            V direction=scale(sub(wrist,elbow),.04f/.15f),tip=add(wrist,direction);
            bone(wrist,tip,width,r,g,b);
            markers.push_back({wrist,.013f,width,r,g,b,false});
            // Two finger marks show aperture; small crossbar shows wrist twist.
            V span{0,.012f+.02f*grip,0};span=rx(span,twist);
            bone(tip,add(tip,span),width,r,g,b);bone(tip,sub(tip,span),width,r,g,b);
        };
        auto headAndTorso=[&](float yaw,float pitch,float width,float r,float g,float b,float hipRoll,float hipPitch){
            V hip{0,0,-.26f};auto tilt=[&](V p){return add(hip,rx(ry(sub(p,hip),hipPitch),hipRoll));};V neck=tilt({0,0,.09f}),head=tilt({0,0,.17f});
            bone(hip,{.04f*std::sin(hipPitch),.04f*std::sin(hipRoll),-.09f},width,r,g,b);
            bone({.04f*std::sin(hipPitch),.04f*std::sin(hipRoll),-.09f},neck,width,r,g,b);
            bone(tilt({0,.14974f,0}),tilt({0,-.14974f,0}),width,r,g,b);bone(neck,head,width,r,g,b);
            markers.push_back({head,.065f,width,r,g,b,true});
            bone(head,add(head,rx(ry(rz(ry({.09f,0,0},pitch),yaw),hipPitch),hipRoll)),width,r,g,b);
        };
        const bool ready=tracking&&calibrated;
        if(ready){
            headAndTorso(rawPose.angles[0],rawPose.angles[1],6,.2f,1,.3f,0,0);
            headAndTorso(bounded(displayedPose.angles[0],-1,1),bounded(displayedPose.angles[1],-.68f,.43f),4,1,.65f,.1f,displayedPose.torso[0],displayedPose.torso[1]);
            for(int side=0;side<2;++side){int o=side==0?2:7;Q q{};for(int k=0;k<4;++k)q[k]=displayedPose.angles[o+k];
                auto human=humanArms[poseMirror?1-side:side];if(poseMirror){human.elbow.y=-human.elbow.y;human.wrist.y=-human.wrist.y;}
                drawArm(human,side==0,displayedPose.angles[o+4],displayedPose.angles[12+side],7,.2f,1,.3f);
                drawArm(forward(q,side==0),side==0,displayedPose.angles[o+4],displayedPose.angles[12+side],4,1,.65f,.1f,displayedPose.torso[0],displayedPose.torso[1]);
            }
        }
        auto s=network->snapshot();auto t=s.value("telemetry",J::object());bool fresh=network->connected&&milliseconds()-network->lastSnapshot<750&&t.value("available",false)&&s.value("robot_mono",0.)-t.value("robot_mono",-100.)<.75&&t.contains("joint_measured")&&t["joint_measured"].is_array()&&t["joint_measured"].size()==14;
        float error=0;std::array<float,3> gaps{};char label[120];
        if(fresh){const auto& angles=t["joint_measured"];float roll=0,pitch=0;
            if(ready)for(int i=0;i<12;++i){int group=i<2?0:i<7?1:2;gaps[group]=std::max(gaps[group],std::abs(angles[i].get<float>()-displayedPose.angles[i]));}
            if(t.contains("lower_body_measured")&&t["lower_body_measured"].is_array()&&t["lower_body_measured"].size()==3){roll=-t["lower_body_measured"][0].get<float>();pitch=-t["lower_body_measured"][1].get<float>();}
            headAndTorso(angles[0].get<float>(),angles[1].get<float>(),2,.2f,.75f,1,roll,pitch);
            for(int side=0;side<2;++side){int o=side==0?2:7;Q q{};for(int k=0;k<4;++k){q[k]=angles[o+k].get<float>();if(ready)error=std::max(error,std::abs(q[k]-displayedPose.angles[o+k]));}
                drawArm(forward(q,side==0),side==0,angles[o+4].get<float>(),angles[12+side].get<float>(),2,.2f,.75f,1,roll,pitch);
            }
            std::snprintf(label,sizeof(label),"Joint gap: Head %.0f / Left %.0f / Right %.0f deg",gaps[0]*57.29578f,gaps[1]*57.29578f,gaps[2]*57.29578f);
            fitText(ready?label:"Hold A + X to start",32,435,416,.65f,.82f,.9f,ui_type::body);
        }else fitText("Robot feedback unavailable / stale",32,435,416,1,.65f,.3f,ui_type::body);
        if(ready){std::snprintf(label,sizeof(label),"Mapping fit: Left %.0f / Right %.0f mm",armError[0]*1000,armError[1]*1000);fitText(label,32,462,416,.85f,.78f,.4f,ui_type::body);}
        float acceptedGap=0;bool haveAccepted=fresh&&t.contains("joint_accepted")&&t["joint_accepted"].is_array()&&t["joint_accepted"].size()==14&&t.contains("joint_target")&&t["joint_target"].is_array()&&t["joint_target"].size()==14;
        if(haveAccepted){for(int i=0;i<12;++i)acceptedGap=std::max(acceptedGap,std::abs(t["joint_accepted"][i].get<float>()-t["joint_target"][i].get<float>()));
            std::snprintf(label,sizeof(label),"Sent vs accepted command: %.0f deg",acceptedGap*57.29578f);fitText(label,32,489,416,.75f,.68f,.8f,ui_type::body);
        }else fitText("Accepted command feedback unavailable",32,489,416,.55f,.68f,.78f,ui_type::body);
        fitText("Robot sensors: 4 Hz / model normalized to Pepper",32,515,416,.55f,.68f,.78f,ui_type::secondary);
        // Fit the complete pose to the existing boxes; magnify without clipping
        // raised hands or changing either panel's footprint.
        for(int view=0;view<2;++view){KeepColors original(preserveColors);
            if(bones.empty())continue;
            float minX=1e9f,maxX=-1e9f,minY=1e9f,maxY=-1e9f;
            auto coordinate=[&](V p){return XrVector2f{view==0?-p.y:p.x,-p.z*1.26316f};};
            auto extend=[&](V p,float radius){auto q=coordinate(p);minX=std::min(minX,q.x-radius);maxX=std::max(maxX,q.x+radius);minY=std::min(minY,q.y-radius);maxY=std::max(maxY,q.y+radius);};
            for(const auto& b:bones){extend(b.a,.015f);extend(b.b,.015f);}for(const auto& m:markers)extend(m.p,m.radius+.01f);
            float left=view==0?40:248,width=view==0?180:192,top=202,height=205;
            float zoom=std::min(285.f,std::min(width/std::max(.01f,maxX-minX),height/std::max(.01f,maxY-minY)));
            auto project=[&](V p){auto q=coordinate(p);return XrVector2f{left+width*.5f+(q.x-(minX+maxX)*.5f)*zoom,top+height*.5f+(q.y-(minY+maxY)*.5f)*zoom};};
            for(const auto& b:bones){auto a=project(b.a),end=project(b.b);pixelLine(a.x,a.y,end.x,end.y,b.width,b.r,b.g,b.blue);}
            for(const auto& m:markers){auto q=project(m.p);float radius=m.radius*zoom;if(!m.outline)panel(q.x-radius,q.y-radius,radius*2,radius*2,m.r,m.g,m.b,radius);else for(int i=0;i<20;++i){float a=i*6.283185f/20,b=(i+1)*6.283185f/20;pixelLine(q.x+radius*std::cos(a),q.y+radius*std::sin(a),q.x+radius*std::cos(b),q.y+radius*std::sin(b),m.width,m.r,m.g,m.b);}}
        }
        drawOffsetY=0;
    }
    void drawLaserMap(){
        auto state=network->snapshot();const double now=milliseconds()/1000.;
        bool valid=false;
        if(network->connected&&milliseconds()-network->lastSnapshot<750&&state.contains("telemetry")){
            const auto& t=state["telemetry"];
            const double stamp=t.value("robot_mono",-1.);
            valid=t.value("available",false)&&state.value("robot_mono",0.)-stamp<.75&&t.contains("odometry")&&t["odometry"].is_array()&&t["odometry"].size()==3;
            if(valid&&stamp!=laserStamp){
                if(stamp<laserStamp)laserMap.clear();
                laserStamp=stamp;std::vector<laser_map::Hit> hits;
                if(t.contains("lasers"))for(const auto& h:t["lasers"]){std::string b=h.value("bank",std::string());
                    hits.push_back({b=="Front"?0:b=="Left"?1:b=="Right"?2:-1,h.value("x",0.),h.value("y",0.)});}
                const auto& p=t["odometry"];laserMap.update({p[0].get<double>(),p[1].get<double>(),p[2].get<double>()},hits,now);
            }
        }
        // Render directly in final panel pixels so both map axes use the same
        // scale. The shorter map panel must not squash square metre cells.
        const float sx=drawScaleX,sy=drawScaleY,ox=drawOffsetX,oy=drawOffsetY;
        const float mx=490*sx+ox,my=137*sy+oy,mw=630*sx,mh=473*sy;
        drawScaleX=drawScaleY=1;drawOffsetX=drawOffsetY=0;
        glEnable(GL_SCISSOR_TEST);glScissor(int(mx),int(H-(my+mh)),int(mw),int(mh));
        // Fixed odometry axes; viewport follows the robot. One metre grid,
        // 60 final pixels per metre on each axis, with identical line thickness.
        auto pixel=[&](double x,double y){return XrVector2f{float(mx+mw*.5-(y-laserMap.pose.y)*60),float(my+mh*.5-(x-laserMap.pose.x)*60)};};
        for(int i=-6;i<=6;++i){
            double x=std::floor(laserMap.pose.x)+i,y=std::floor(laserMap.pose.y)+i;
            auto a=pixel(x,laserMap.pose.y-5),b=pixel(x,laserMap.pose.y+5);pixelLine(a.x,a.y,b.x,b.y,1,.12f,.22f,.25f);
            a=pixel(laserMap.pose.x-5,y);b=pixel(laserMap.pose.x+5,y);pixelLine(a.x,a.y,b.x,b.y,1,.12f,.22f,.25f);
        }
        bool fresh=valid&&laserMap.fresh(now);
        for(const auto& item:laserMap.points){const auto& q=item.second;if(now-q.at>60)continue;auto p=pixel(q.x,q.y);bool live=fresh&&now-q.at<.75;
            pixelLine(p.x-2,p.y,p.x+2,p.y,4,live?.2f:.4f,live?1.f:.45f,live?.65f:.5f);}
        auto center=pixel(laserMap.pose.x,laserMap.pose.y);double yaw=laserMap.pose.yaw;
        auto tip=pixel(laserMap.pose.x+.38*std::cos(yaw),laserMap.pose.y+.38*std::sin(yaw));
        auto l=pixel(laserMap.pose.x+.2*std::cos(yaw+2.5),laserMap.pose.y+.2*std::sin(yaw+2.5));
        auto r=pixel(laserMap.pose.x+.2*std::cos(yaw-2.5),laserMap.pose.y+.2*std::sin(yaw-2.5));
        pixelLine(tip.x,tip.y,l.x,l.y,3,1,.65f,.1f);pixelLine(tip.x,tip.y,r.x,r.y,3,1,.65f,.1f);pixelLine(l.x,l.y,r.x,r.y,3,1,.65f,.1f);
        glDisable(GL_SCISSOR_TEST);
        drawScaleX=sx;drawScaleY=sy;drawOffsetX=ox;drawOffsetY=oy;
    }
    void render(uint32_t index,bool tracking){
        for(int c=0;c<3;++c){VideoFrame video;if(network->takeFrame(video,c)){glBindTexture(GL_TEXTURE_2D,cameraTextures[c]);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,video.width,video.height,0,GL_RGB,GL_UNSIGNED_BYTE,video.pixels.data());cameraWidth[c]=video.width;cameraHeight[c]=video.height;cameraReceived[c]=video.received;if(c==0)videoReceived=video.received;}}
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,images[index].image,0);glViewport(0,0,W,H);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glBindTexture(GL_TEXTURE_2D,texture);
        glUseProgram(program);glUniform1f(glGetUniformLocation(program,"canvasOffset"),0);glUniform1f(glGetUniformLocation(program,"canvasWidth"),W);
        drawStudio(tracking);
        if(helpOpen){glViewport(W,0,HELP_W,H);glUniform1f(glGetUniformLocation(program,"canvasOffset"),W);glUniform1f(glGetUniformLocation(program,"canvasWidth"),HELP_W);drawScaleX=float(W)/HELP_W;drawHelp();drawScaleX=1;}
        glFlush();
    }

    void frame(){XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState state{XR_TYPE_FRAME_STATE};XR(xrWaitFrame(session,&wait,&state));XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};XR(xrBeginFrame(session,&begin));
        frameTime=state.predictedDisplayTime;updatePanelFit(frameTime);if(anchorPending&&focused&&frameTime>=anchorRetryAt)placePanel();
        bool tracking=updatePose(state.predictedDisplayTime);XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};const XrCompositionLayerBaseHeader* layer=nullptr;
        static double lastDiagnostic=0;
        if(milliseconds()-lastDiagnostic>5000){lastDiagnostic=milliseconds();
            __android_log_print(ANDROID_LOG_INFO,"TelePepper","focus=%d render=%d tracking=%d connected=%d video_age_ms=%.0f command_rtt=%.1f sample_age=%.1f queue=%.1f apply=%.1f audio=%s",focused,state.shouldRender,tracking,(int)network->connected.load(),network->lastVideo>0?milliseconds()-network->lastVideo:-1,network->rtt.load(),network->sampleAgeMs.load(),network->queueMs.load(),network->applyMs.load(),network->audioStatus().c_str());}
        if(state.shouldRender){uint32_t index;XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};XR(xrAcquireSwapchainImage(swapchain,&acquire,&index));XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;XR(xrWaitSwapchainImage(swapchain,&wi));render(index,tracking);XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};XR(xrReleaseSwapchainImage(swapchain,&release));
            quad.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;quad.space=roomLocked&&!anchorPending?local:viewSpace;quad.eyeVisibility=XR_EYE_VISIBILITY_BOTH;quad.subImage.swapchain=swapchain;quad.subImage.imageRect.extent={W,H};quad.pose=panel_anchor::workspace(roomLocked,anchorPending,roomPanel);quad.size={panelWidth(),panelWidth()*H/W};layer=(XrCompositionLayerBaseHeader*)&quad;}
        XrCompositionLayerPassthroughFB room{XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};room.layerHandle=passthroughLayer;room.space=XR_NULL_HANDLE;
        XrCompositionLayerQuad help{XR_TYPE_COMPOSITION_LAYER_QUAD};
        std::array<XrCompositionLayerQuad,13> panels{};std::array<const XrCompositionLayerBaseHeader*,17> layers{};uint32_t count=0;
        if(state.shouldRender&&passthroughLayer)layers[count++]=(XrCompositionLayerBaseHeader*)&room;
        if(layer){if(floating&&!anchorPending){for(int i=0;i<13;++i){auto r=floating_layout::cards[i];auto& p=panels[i];p=quad;p.pose=floating_layout::world(quad.pose,cardPoses[i],panelWidth());p.size={panelWidth()*r.w/W*cardScales[i],panelWidth()*r.h/W*cardScales[i]};p.subImage.imageRect.offset={int(r.x),H-int(r.y+r.h)};p.subImage.imageRect.extent={int(r.w),int(r.h)};layers[count++]=(XrCompositionLayerBaseHeader*)&p;}}else layers[count++]=layer;}
        if(layer&&helpOpen){help=quad;help.pose=helpPose(quad.pose);help.size={panelWidth()*HELP_W/W*HELP_SCALE,panelWidth()*H/W*HELP_SCALE};help.subImage.imageRect.offset={W,0};help.subImage.imageRect.extent={HELP_W,H};layers[count++]=(XrCompositionLayerBaseHeader*)&help;}
        if(layer&&floating&&!anchorPending){XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};if(XR_SUCCEEDED(xrLocateSpace(viewSpace,quad.space,frameTime,&head))&&(head.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT)){auto start=layers.begin()+(state.shouldRender&&passthroughLayer?1:0);std::stable_sort(start,layers.begin()+count,[&](const XrCompositionLayerBaseHeader* a,const XrCompositionLayerBaseHeader* b){auto pa=((const XrCompositionLayerQuad*)a)->pose.position,pb=((const XrCompositionLayerQuad*)b)->pose.position;return dot(sub(pa,head.pose.position),sub(pa,head.pose.position))>dot(sub(pb,head.pose.position),sub(pb,head.pose.position));});}}
        XrCompositionLayerQuad beam{XR_TYPE_COMPOSITION_LAYER_QUAD};
        if(layer&&raySwapchain&&focused&&pointerGrip.held&&pointerBeamValid&&count<maxLayers){
            if(!rayTextureReady){uint32_t image;XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};XR(xrAcquireSwapchainImage(raySwapchain,&acquire,&image));XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=XR_INFINITE_DURATION;XR(xrWaitSwapchainImage(raySwapchain,&wait));
                std::array<unsigned char,8*64*4> pixels{};for(int y=0;y<64;++y)for(int x=0;x<8;++x){float a=(1-std::abs((x+.5f)/4-1))*(.35f+.6f*y/63);int o=(y*8+x)*4;pixels[o]=63*a;pixels[o+1]=166*a;pixels[o+2]=255*a;pixels[o+3]=255*a;}
                glBindTexture(GL_TEXTURE_2D,rayImages[image].image);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,8,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};XR(xrReleaseSwapchainImage(raySwapchain,&release));rayTextureReady=true;
            }
            XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};if(XR_SUCCEEDED(xrLocateSpace(viewSpace,quad.space,frameTime,&head))&&(head.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT)){
                float length;if(pointer_ray::ribbon(pointerController.position,pointerEnd,head.pose.position,beam.pose,length)){beam.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;beam.space=quad.space;beam.eyeVisibility=XR_EYE_VISIBILITY_BOTH;beam.subImage.swapchain=raySwapchain;beam.subImage.imageRect.extent={8,64};beam.size={.005f,length};layers[count++]=(XrCompositionLayerBaseHeader*)&beam;}
            }
        }
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=state.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;end.layerCount=count;end.layers=count?layers.data():nullptr;XR(xrEndFrame(session,&end));
    }
public:
    explicit App(android_app* a):app(a){
        JNIEnv* env=nullptr;app->activity->vm->AttachCurrentThread(&env,nullptr);
        auto cls=env->GetObjectClass(app->activity->clazz);
        auto label=(jstring)env->CallObjectMethod(app->activity->clazz,env->GetMethodID(cls,"getAppVersion","()Ljava/lang/String;"));
        const char* raw=env->GetStringUTFChars(label,nullptr);appVersion=raw;
        env->ReleaseStringUTFChars(label,raw);env->DeleteLocalRef(label);env->DeleteLocalRef(cls);
    }
    void run(){
        resetLayout();try{auto saved=nlohmann::json::parse(extra("layout_state"));if(saved.value("version",0)==1&&decodeLayout(saved.at("current"))){floating=saved.value("floating",false);if(saved.contains("presets")&&saved["presets"].is_array()&&saved["presets"].size()==3)layoutPresets=saved["presets"];layoutSlot=std::max(0,std::min(2,saved.value("slot",0)));}}catch(...){}
        roomLocked=extra("room_locked")=="true";anchorPending=roomLocked;poseMirror=extra("pose_mirror")=="true";showLaserMap=extra("view")=="map";tabletPreview=extra("tablet_preview")!="false";fahrenheit=extra("fahrenheit")=="true";
        try{ledTarget=std::max(0,std::min(2,std::stoi(extra("led_target"))));auto saved=nlohmann::json::parse(extra("led_swatches"));if(saved.is_array()&&saved.size()==3)for(int i=0;i<3;++i)if(saved[i].is_number_integer())ledSwatches[i]=saved[i].get<int>()<0?(i==2?6:1):std::min(6,saved[i].get<int>());}catch(...){}
        try{auto saved=nlohmann::json::parse(extra("pose_offsets"));if(saved.is_array()&&saved.size()==12)for(int i=0;i<12;++i){float v=saved[i].get<float>();if(std::isfinite(v))offsets[i]=bounded(v,-30,30);}}catch(...){}

        std::string savedScale=extra("panel_scale");if(!savedScale.empty())panelScale=bounded(std::strtof(savedScale.c_str(),nullptr),.65f,1.f);
        PFN_xrInitializeLoaderKHR initLoader=nullptr;xrGetInstanceProcAddr(XR_NULL_HANDLE,"xrInitializeLoaderKHR",(PFN_xrVoidFunction*)&initLoader);
        if(initLoader){XrLoaderInitInfoAndroidKHR loader{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};loader.applicationVM=app->activity->vm;loader.applicationContext=app->activity->clazz;XR(initLoader((XrLoaderInitInfoBaseHeaderKHR*)&loader));}
        uint32_t extensionCount=0;XR(xrEnumerateInstanceExtensionProperties(nullptr,0,&extensionCount,nullptr));
        std::vector<XrExtensionProperties> available(extensionCount,{XR_TYPE_EXTENSION_PROPERTIES});XR(xrEnumerateInstanceExtensionProperties(nullptr,extensionCount,&extensionCount,available.data()));
        std::vector<const char*> extensions={XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,XR_FB_BODY_TRACKING_EXTENSION_NAME};
        for(const auto& extension:available)if(std::strcmp(extension.extensionName,XR_META_BODY_TRACKING_FIDELITY_EXTENSION_NAME)==0){extensions.push_back(XR_META_BODY_TRACKING_FIDELITY_EXTENSION_NAME);highFidelity=true;}
        for(const auto& extension:available)if(std::strcmp(extension.extensionName,XR_FB_PASSTHROUGH_EXTENSION_NAME)==0){extensions.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);passthroughAvailable=true;}
        XrInstanceCreateInfoAndroidKHR android{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};android.applicationVM=app->activity->vm;android.applicationActivity=app->activity->clazz;
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};info.next=&android;std::strcpy(info.applicationInfo.applicationName,"TelePepper");info.applicationInfo.applicationVersion=1;info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,34);info.enabledExtensionCount=(uint32_t)extensions.size();info.enabledExtensionNames=extensions.data();XR(xrCreateInstance(&info,&instance));
        XrSystemGetInfo get{XR_TYPE_SYSTEM_GET_INFO};get.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;XR(xrGetSystem(instance,&get,&system));XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};XR(xrGetSystemProperties(instance,system,&properties));maxLayers=properties.graphicsProperties.maxLayerCount;if(maxLayers<15)floating=false;initGraphics();initPassthrough();initInput();
        __android_log_print(ANDROID_LOG_INFO,"TelePepper","OpenXR graphics, controller actions and body tracker initialized");
        showLaserMap=extra("lidar_enabled")!="false";network.reset(new Network(extra("host"),extra("token")));network->setDepthStreaming(extra("depth_streaming")=="true");network->setCameraStreaming(0,extra("top_streaming")!="false");network->setCameraStreaming(1,extra("bottom_streaming")!="false");network->start();
        while(!app->destroyRequested){android_poll_source* source=nullptr;int events;while(ALooper_pollOnce(running?0:50,nullptr,&events,(void**)&source)>=0){if(source)source->process(app,source);if(app->destroyRequested)break;}
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};while(xrPollEvent(instance,&event)==XR_SUCCESS){if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){auto* changed=(XrEventDataSessionStateChanged*)&event;focused=changed->state==XR_SESSION_STATE_FOCUSED;
                if(!focused&&network){if(cardDrag>=0){cardDrag=-1;saveLayout();}windowGesture=0;startFlow.cancel();pendingGesture.cancel();network->requestStop();network->audioFocus=false;network->talk=false;}
                if(changed->state==XR_SESSION_STATE_READY){XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;XR(xrBeginSession(session,&begin));running=true;}
                if(changed->state==XR_SESSION_STATE_STOPPING){running=false;XR(xrEndSession(session));}
                if(changed->state==XR_SESSION_STATE_EXITING||changed->state==XR_SESSION_STATE_LOSS_PENDING)return;
            }else if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING){if(cardDrag>=0){cardDrag=-1;saveLayout();}windowGesture=0;calibrated=false;startFlow.cancel();pendingGesture.cancel();network->requestStop();if(roomLocked){anchorPending=true;anchorRetryAt=((XrEventDataReferenceSpaceChangePending*)&event)->changeTime;}}event={XR_TYPE_EVENT_DATA_BUFFER};}
            if(running&&!app->destroyRequested)frame();
        }
    }
    ~App(){if(network)network->stop();if(body&&destroyBody)destroyBody(body);if(passthroughLayer&&destroyPassthroughLayer)destroyPassthroughLayer(passthroughLayer);if(passthrough&&destroyPassthrough)destroyPassthrough(passthrough);if(raySwapchain)xrDestroySwapchain(raySwapchain);if(swapchain)xrDestroySwapchain(swapchain);if(aimSpace)xrDestroySpace(aimSpace);if(local)xrDestroySpace(local);if(viewSpace)xrDestroySpace(viewSpace);if(session)xrDestroySession(session);if(actionSet)xrDestroyActionSet(actionSet);if(instance)xrDestroyInstance(instance);if(display!=EGL_NO_DISPLAY){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);eglTerminate(display);}}
};
void android_main(android_app* app){try{App program(app);program.run();}catch(const std::exception& e){__android_log_print(ANDROID_LOG_ERROR,"TelePepper","%s",e.what());}ANativeActivity_finish(app->activity);}
