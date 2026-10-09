package it.telepepper.quest;
import java.util.ArrayDeque;

/** Called under NaturalVoice's lock: one synthesis and one completed clip. */
final class SpeechQueue {
    static final class Request {
        final String text,language;final int id;
        Request(String text,String language,int id){this.text=text;this.language=language;this.id=id;}
    }
    static final class Clip {
        final String text;final byte[] pcm;
        Clip(String text,byte[] pcm){this.text=text;this.pcm=pcm;}
    }
    private final ArrayDeque<Request> waiting=new ArrayDeque<>();
    private Request active;private Clip completed;private int serial;
    boolean offer(String text,String language){if(waiting.size()>=8)return false;waiting.add(new Request(text,language,++serial));return true;}
    Request start(){if(active!=null||completed!=null||waiting.isEmpty())return null;active=waiting.remove();return active;}
    boolean finish(int id,byte[] pcm){if(active==null||active.id!=id)return false;completed=pcm==null?null:new Clip(active.text,pcm);active=null;return true;}
    Clip take(){Clip result=completed;completed=null;return result;}
    boolean current(int id){return active!=null&&active.id==id;}
    void clear(){waiting.clear();active=null;completed=null;++serial;}
}
