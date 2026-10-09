package it.telepepper.quest;

import android.content.Context;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.speech.tts.Voice;
import java.io.File;
import java.nio.file.Files;
import java.util.*;

/** Uses the installed synthesis engine; never substitutes Pepper TTS silently. */
final class NaturalVoice {
    private final Context context;
    private TextToSpeech engine;
    private boolean ready;
    private int epoch;
    private File pending;
    private final SpeechQueue speech=new SpeechQueue();
    private String takenText="";
    private String modelName="Natural",language="en-US",name="Starting voice engine...",status="";
    NaturalVoice(Context context){this.context=context;
        try{android.content.pm.ApplicationInfo info=context.getPackageManager().getApplicationInfo("com.k2fsa.sherpa.onnx.tts.engine",0);try(java.util.zip.ZipFile apk=new java.util.zip.ZipFile(info.sourceDir)){java.util.Enumeration<? extends java.util.zip.ZipEntry> entries=apk.entries();while(entries.hasMoreElements())if(entries.nextElement().getName().contains("en_GB-cori-medium")){modelName="Cori medium";break;}}catch(java.io.IOException ignored){}}catch(android.content.pm.PackageManager.NameNotFoundException e){name="Install natural voice engine";return;}
        engine=new TextToSpeech(context,result->{synchronized(this){ready=result==TextToSpeech.SUCCESS;
            if(!ready){name="Natural voice unavailable";return;}
            Set<Voice> voices=engine.getVoices();if(voices!=null)for(Voice v:voices)android.util.Log.i("TelePepper","TTS voice: "+v.getName()+" locale="+v.getLocale()+" network="+v.isNetworkConnectionRequired());
            boolean english=false;if(voices!=null)for(Voice v:voices)if(v.getLocale().getLanguage().equals("en"))english=true;select(english?"en-US":"it-IT",false);startNext();
        }},"com.k2fsa.sherpa.onnx.tts.engine");
        engine.setOnUtteranceProgressListener(new UtteranceProgressListener(){
            public void onStart(String id){}
            public void onError(String id){finish(id,false);}
            public void onDone(String id){finish(id,true);}
        });
    }
    private synchronized void select(String tag,boolean next){
        language=tag;if(!ready)return;String lang=Locale.forLanguageTag(tag).getLanguage();List<Voice> voices=new ArrayList<>();
        Set<Voice> available=engine.getVoices();if(available!=null)for(Voice v:available)if(v.getLocale().getLanguage().equals(lang)&&!v.isNetworkConnectionRequired()&&!v.getFeatures().contains(TextToSpeech.Engine.KEY_FEATURE_NOT_INSTALLED))voices.add(v);
        voices.sort(Comparator.comparing(Voice::getName));
        if(voices.isEmpty()){name="No local voice for "+tag;return;}
        String saved=context.getSharedPreferences("connection",Context.MODE_PRIVATE).getString("natural_voice_"+lang,"");int index=-1;
        for(int i=0;i<voices.size();i++)if(voices.get(i).getName().equals(saved))index=i;
        if(index<0){index=0;for(int i=0;i<voices.size();i++){String n=voices.get(i).getName().toLowerCase(Locale.ROOT);if(n.contains("female")||n.contains("woman")){index=i;break;}}}
        if(next)index=(index+1)%voices.size();Voice chosen=voices.get(index);
        if(engine.setVoice(chosen)!=TextToSpeech.SUCCESS){name="Voice selection failed";return;}
        engine.setPitch(1f);engine.setSpeechRate(1f);name=chosen.getName();
        context.getSharedPreferences("connection",Context.MODE_PRIVATE).edit().putString("natural_voice_"+lang,name).apply();
    }
    synchronized String name(){return ready&&!name.startsWith("No local")&&!name.startsWith("Voice selection")?modelName+" / "+name:name;}
    synchronized String language(){return language;}
    synchronized String status(){return status;}
    synchronized byte[] take(){SpeechQueue.Clip result=speech.take();takenText=result==null?"":result.text;startNext();return result==null?null:result.pcm;}
    synchronized String takeText(){String result=takenText;takenText="";return result;}
    synchronized void next(String tag){cancel();select(tag,true);}
    synchronized void speak(String text,String tag){
        if(text.isEmpty()||text.length()>400){status="Speech text must contain 1-400 characters";return;}
        if(engine==null){status=name;return;}
        if(!speech.offer(text,tag)){status="Speech queue full / wait or Stop speech";return;}
        status=ready?"Phrase queued":"Starting voice engine / phrase queued";startNext();
    }
    private synchronized void startNext(){
        if(!ready)return;SpeechQueue.Request request=speech.start();if(request==null)return;
        if(!language.equals(request.language)||name.startsWith("No local")||name.startsWith("Voice selection"))select(request.language,false);
        if(name.startsWith("No local")||name.startsWith("Voice selection")){speech.finish(request.id,null);status=name;return;}
        epoch=request.id;pending=new File(context.getCacheDir(),"telepepper-voice-"+epoch+".wav");status="Synthesizing natural voice...";
        android.util.Log.i("TelePepper","Piper synthesis start id="+epoch+" chars="+request.text.length());
        if(engine.synthesizeToFile(request.text,new android.os.Bundle(),pending,Integer.toString(epoch))!=TextToSpeech.SUCCESS){speech.finish(epoch,null);status="Natural voice synthesis refused";clearFile();}
    }
    private void finish(String id,boolean success){File file;int expected;
        synchronized(this){if(!id.equals(Integer.toString(epoch))||!speech.current(epoch))return;file=pending;expected=epoch;}
        byte[] result=null;String error="Natural voice synthesis failed";
        if(success&&file!=null)try{if(file.length()>12000000)throw new java.io.IOException("Voice file too large");result=WavPcm.decode(Files.readAllBytes(file.toPath()));}catch(Exception e){error="Natural voice audio unavailable: "+e.getMessage();}
        synchronized(this){if(!speech.finish(expected,result)){if(file!=null)file.delete();return;}status=result!=null?"Natural voice ready":error;clearFile();android.util.Log.i("TelePepper","Piper synthesis finish id="+expected+" bytes="+(result==null?0:result.length));if(result==null)startNext();}
    }
    private void clearFile(){if(pending!=null)pending.delete();pending=null;}
    synchronized void cancel(){speech.clear();takenText="";if(engine!=null)engine.stop();clearFile();status="";}
    synchronized void close(){cancel();if(engine!=null)engine.shutdown();ready=false;}
}
