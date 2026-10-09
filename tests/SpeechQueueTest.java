package it.telepepper.quest;
public final class SpeechQueueTest {
    private static void check(boolean condition){if(!condition)throw new AssertionError();}
    public static void main(String[] args){
        SpeechQueue q=new SpeechQueue();
        check(q.offer("First","en-US")&&q.offer("Second","en-US"));
        SpeechQueue.Request first=q.start();check(first.text.equals("First")&&q.start()==null);
        check(q.finish(first.id,new byte[]{1,2}));check(q.start()==null);
        SpeechQueue.Clip clip=q.take();check(clip.text.equals("First")&&clip.pcm[0]==1&&q.take()==null);
        SpeechQueue.Request second=q.start();check(second.text.equals("Second"));
        check(!q.finish(first.id,new byte[]{9})&&q.current(second.id));
        q.clear();check(!q.finish(second.id,new byte[]{3})&&q.take()==null&&q.start()==null);
        check(q.offer("Repeat","en-US")&&q.offer("Repeat","en-US"));
        first=q.start();check(q.finish(first.id,new byte[]{4}));check(q.take().text.equals("Repeat"));
        second=q.start();check(second.id!=first.id&&q.finish(second.id,new byte[]{5}));check(q.take().pcm[0]==5);
        check(q.offer("Fail","en-US")&&q.offer("After failure","en-US"));first=q.start();check(q.finish(first.id,null)&&q.start().text.equals("After failure"));q.clear();
        for(int i=0;i<8;i++)check(q.offer("Bounded","en-US"));check(!q.offer("Overflow","en-US"));
        System.out.println("PASS FIFO presets, first/repeated requests, bounded queue, stale callbacks, cancellation and failure recovery");
    }
}
