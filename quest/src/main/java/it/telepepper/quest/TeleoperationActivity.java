package it.telepepper.quest;

import android.app.NativeActivity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import java.io.*;
import org.json.JSONObject;

/** The library entry is the immersive activity, so Horizon grants VR focus. */
public class TeleoperationActivity extends NativeActivity {
    public String getAppVersion(){return it.telepepper.ui.VersionInfo.label(this);}
    private android.speech.SpeechRecognizer recognizer;
    private NaturalVoice naturalVoice;
    public String getNaturalVoiceName(){return naturalVoice==null?"Starting...":naturalVoice.name();}
    public String getNaturalVoiceLanguage(){return naturalVoice==null?"en-US":naturalVoice.language();}
    public String getNaturalVoiceStatus(){return naturalVoice==null?"":naturalVoice.status();}
    public byte[] takeNaturalVoiceAudio(){return naturalVoice==null?null:naturalVoice.take();}
    public String takeNaturalVoiceText(){return naturalVoice==null?"":naturalVoice.takeText();}
    public void speakNatural(String text,String language){runOnUiThread(()->{if(naturalVoice!=null)naturalVoice.speak(text,language);});}
    public void nextNaturalVoice(String language){runOnUiThread(()->{if(naturalVoice!=null)naturalVoice.next(language);});}
    private boolean recording=false,forceStandardSpeech=false;
    private final java.util.HashSet<String> requestedModels=new java.util.HashSet<>();
    private final java.util.ArrayList<android.speech.SpeechRecognizer> modelRecognizers=new java.util.ArrayList<>();
    private void prepareSpeechModel(String language){
        if(android.os.Build.VERSION.SDK_INT<34||requestedModels.contains(language))return;
        requestedModels.add(language);
        try{
            android.speech.SpeechRecognizer model=android.speech.SpeechRecognizer.createOnDeviceSpeechRecognizer(this);modelRecognizers.add(model);
            Intent intent=new Intent(android.speech.RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
            intent.putExtra(android.speech.RecognizerIntent.EXTRA_LANGUAGE_MODEL,android.speech.RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
            intent.putExtra(android.speech.RecognizerIntent.EXTRA_LANGUAGE,language);
            model.triggerModelDownload(intent,getMainExecutor(),new android.speech.ModelDownloadListener(){
                public void onProgress(int percent){voiceState("Speech model downloading "+percent+"%");}
                public void onSuccess(){forceStandardSpeech=false;voiceState("Speech ready / hold left grip");android.util.Log.i("TelePepper","Speech model ready: "+language);}
                public void onScheduled(){voiceState("Speech model download scheduled");android.util.Log.i("TelePepper","Speech model scheduled: "+language);}
                public void onError(int error){forceStandardSpeech=true;voiceState("Local speech model unavailable / standard engine");android.util.Log.e("TelePepper","Speech model download error "+error+" language="+language);}
            });
        }catch(Exception e){forceStandardSpeech=true;android.util.Log.e("TelePepper","Speech model setup failed",e);}
    }

    private volatile int voiceEpoch=0;
    private String voiceResult="",voiceStatus="Hold left grip to speak as Pepper";
    public synchronized String takeVoiceResult(){String value=voiceResult;voiceResult="";return value;}
    public synchronized String getVoiceStatus(){return voiceStatus;}
    private synchronized void voiceState(String value){voiceStatus=value;android.util.Log.i("TelePepper","Voice state: "+(value.startsWith("Recognized:")?"Recognized sentence":value));}
    public void cancelRecognition(){runOnUiThread(()->{voiceEpoch++;recording=false;synchronized(this){voiceResult="";}if(recognizer!=null)recognizer.cancel();voiceState("Hold left grip to speak as Pepper");});}
    public void cancelVoice(){runOnUiThread(()->{voiceEpoch++;recording=false;if(naturalVoice!=null)naturalVoice.cancel(); synchronized(this){voiceResult="";}if(recognizer!=null)recognizer.cancel();voiceState("Hold left grip to speak as Pepper");});}
    public void pushToTalk(boolean pressed,String language){runOnUiThread(()->{
        if(!pressed){if(recording&&recognizer!=null){recording=false;recognizer.stopListening();voiceState("Transcribing...");}return;}
        if(recording)return;
        final int epoch=++voiceEpoch;
        if(recognizer!=null){recognizer.destroy();recognizer=null;}
        try{
            if(!forceStandardSpeech&&android.os.Build.VERSION.SDK_INT>=31&&android.speech.SpeechRecognizer.isOnDeviceRecognitionAvailable(this))recognizer=android.speech.SpeechRecognizer.createOnDeviceSpeechRecognizer(this);
            else recognizer=android.speech.SpeechRecognizer.createSpeechRecognizer(this,new android.content.ComponentName("com.oculus.systemintelligence","com.oculus.systemintelligence.asr.service.RecognitionService"));
            recognizer.setRecognitionListener(new android.speech.RecognitionListener(){
                public void onReadyForSpeech(Bundle b){if(epoch==voiceEpoch)voiceState("MIC LIVE / release left grip");}
                public void onBeginningOfSpeech(){}
                public void onRmsChanged(float v){}
                public void onBufferReceived(byte[] b){}
                public void onEndOfSpeech(){if(epoch==voiceEpoch)voiceState("Transcribing...");}
                public void onError(int code){if(epoch!=voiceEpoch)return;recording=false;
                    if(code==12||code==13){forceStandardSpeech=true;prepareSpeechModel(language);voiceState("Speech model missing / release, retry standard engine");}
                    else voiceState("Speech recognition error "+code+" / release and retry");
                }
                public void onResults(Bundle b){if(epoch!=voiceEpoch)return;recording=false;java.util.ArrayList<String> values=b.getStringArrayList(android.speech.SpeechRecognizer.RESULTS_RECOGNITION);
                    synchronized(TeleoperationActivity.this){if(epoch!=voiceEpoch)return;if(values!=null&&!values.isEmpty()){voiceResult=values.get(0);voiceStatus="Recognized: "+voiceResult;android.util.Log.i("TelePepper","Recognized sentence length="+voiceResult.length());}else voiceStatus="No speech recognized / retry";}}
                public void onPartialResults(Bundle b){}
                public void onEvent(int type,Bundle b){}
            });
            Intent intent=new Intent(android.speech.RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
            intent.putExtra(android.speech.RecognizerIntent.EXTRA_LANGUAGE_MODEL,android.speech.RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
            intent.putExtra(android.speech.RecognizerIntent.EXTRA_LANGUAGE,language);
            intent.putExtra(android.speech.RecognizerIntent.EXTRA_MAX_RESULTS,1);
            intent.putExtra(android.speech.RecognizerIntent.EXTRA_PARTIAL_RESULTS,false);
            recording=true;voiceState("Starting microphone...");recognizer.startListening(intent);
        }catch(Exception e){recording=false;voiceState("Speech recognition unavailable: "+e.getClass().getSimpleName());}
    });}
    @Override protected void onDestroy(){if(naturalVoice!=null)naturalVoice.close();voiceEpoch++;if(recognizer!=null)recognizer.destroy();for(android.speech.SpeechRecognizer r:modelRecognizers)r.destroy();super.onDestroy();}
    private android.net.wifi.WifiManager.WifiLock wifiLock;
    @Override protected void onResume(){super.onResume();try{
        android.net.wifi.WifiManager wifi=(android.net.wifi.WifiManager)getApplicationContext().getSystemService(WIFI_SERVICE);
        wifiLock=wifi.createWifiLock(android.net.wifi.WifiManager.WIFI_MODE_FULL_LOW_LATENCY,"TelePepper-control");wifiLock.acquire();
    }catch(Exception ignored){}}
    @Override protected void onPause(){cancelVoice();if(wifiLock!=null&&wifiLock.isHeld())wifiLock.release();wifiLock=null;super.onPause();}
    @Override public void onCreate(Bundle state) {
        SharedPreferences prefs=ConnectionStore.load(this);
        String host=prefs.getString("host",""),token=prefs.getString("token","");
        getIntent().putExtra("host",host).putExtra("token",token).putExtra("panel_scale",Float.toString(prefs.getFloat("panel_scale",.9f)));
        getIntent().putExtra("depth_streaming",Boolean.toString(prefs.getBoolean("depth_streaming",false)));
        getIntent().putExtra("top_streaming",Boolean.toString(prefs.getBoolean("top_streaming",true)));
        getIntent().putExtra("bottom_streaming",Boolean.toString(prefs.getBoolean("bottom_streaming",true)));
        getIntent().putExtra("lidar_enabled",Boolean.toString(prefs.getBoolean("lidar_enabled",true)));
        getIntent().putExtra("tablet_preview",Boolean.toString(prefs.getBoolean("tablet_preview",true)));
        getIntent().putExtra("fahrenheit",Boolean.toString(prefs.getBoolean("fahrenheit",false)));
        getIntent().putExtra("torso_assist",Boolean.toString(prefs.getBoolean("torso_assist",true)));
        getIntent().putExtra("layout_state",prefs.getString("layout_state",""));
        getIntent().putExtra("room_locked",Boolean.toString(prefs.getBoolean("room_locked",false)));
        getIntent().putExtra("pose_mirror",Boolean.toString(prefs.getBoolean("pose_mirror",false)));
        getIntent().putExtra("pose_offsets",prefs.getString("pose_offsets","[]"));
        getIntent().putExtra("led_target",Integer.toString(prefs.getInt("led_target",0)));
        getIntent().putExtra("led_swatches",prefs.getString("led_swatches","[1,1,6]"));
        super.onCreate(state);
        naturalVoice=new NaturalVoice(this);
        prepareSpeechModel("en-US");
        if(host.isEmpty()||token.length()<12) {
            startActivity(new Intent(this,LauncherActivity.class));finish();
        }
    }
    public void saveAppearance(int target,String swatches){getSharedPreferences("connection",MODE_PRIVATE).edit().putInt("led_target",target).putString("led_swatches",swatches).apply();}
    public void saveRoomLocked(boolean value){getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("room_locked",value).apply();}
    public void savePoseMirror(boolean value){getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("pose_mirror",value).apply();}
    public void saveDepthStreaming(boolean value){getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("depth_streaming",value).apply();}
    public void saveCameraStreaming(int camera,boolean value){getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean(camera==0?"top_streaming":"bottom_streaming",value).apply();}
    public void saveLidarMap(boolean value){getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("lidar_enabled",value).apply();}
    public void saveTabletPreview(boolean value){getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("tablet_preview",value).apply();}
    public void saveTemperatureUnit(boolean value){getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("fahrenheit",value).apply();}
    public void saveTorsoAssist(boolean value){getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("torso_assist",value).apply();}
    public void saveOffsets(String value){getSharedPreferences("connection",MODE_PRIVATE).edit().putString("pose_offsets",value).apply();}
    public void saveLayout(String value){getSharedPreferences("connection",MODE_PRIVATE).edit().putString("layout_state",value).apply();}
    public void savePanelScale(float value){getSharedPreferences("connection",MODE_PRIVATE).edit().putFloat("panel_scale",Math.max(.65f,Math.min(1.f,value))).apply();}
    public void exitTeleoperation(){runOnUiThread(()->{
        cancelVoice();
        SharedPreferences prefs=ConnectionStore.load(this);
        it.telepepper.lifecycle.ExitSession.close(this,prefs.getString("host",""),prefs.getString("token",""));
    });}
    @Override public void onBackPressed(){exitTeleoperation();}
    public void openSettings(){runOnUiThread(()->{
        startActivity(new Intent(this,LauncherActivity.class));finish();
    });}
}
