package it.telepepper.quest;
import java.nio.*;
import java.util.*;
public class WavPcmTest {
    static byte[] wave(int rate,int channels,int frames){ByteBuffer b=ByteBuffer.allocate(44+frames*channels*2).order(ByteOrder.LITTLE_ENDIAN);b.putInt(0x46464952).putInt(b.capacity()-8).putInt(0x45564157).putInt(0x20746d66).putInt(16).putShort((short)1).putShort((short)channels).putInt(rate).putInt(rate*channels*2).putShort((short)(channels*2)).putShort((short)16).putInt(0x61746164).putInt(frames*channels*2);for(int i=0;i<frames;i++)for(int c=0;c<channels;c++)b.putShort((short)(channels==1?i:(c==0?1000:3000)));return b.array();}
    static void invalid(byte[] b)throws Exception{try{WavPcm.decode(b);throw new AssertionError("Malformed WAV accepted");}catch(java.io.IOException expected){}}
    public static void main(String[] args)throws Exception{
        byte[] direct=wave(16000,1,320);if(!Arrays.equals(WavPcm.decode(direct),Arrays.copyOfRange(direct,44,direct.length)))throw new AssertionError("PCM identity");
        byte[] stereo=WavPcm.decode(wave(24000,2,2400));if(stereo.length!=3200)throw new AssertionError("Resampling duration");ByteBuffer samples=ByteBuffer.wrap(stereo).order(ByteOrder.LITTLE_ENDIAN);while(samples.hasRemaining())if(samples.getShort()!=2000)throw new AssertionError("Stereo mix");
        if(WavPcm.decode(wave(8000,1,800)).length!=3200)throw new AssertionError("Upsampling");
        invalid(new byte[8]);invalid(Arrays.copyOf(direct,45));byte[] malformed=direct.clone();ByteBuffer.wrap(malformed).order(ByteOrder.LITTLE_ENDIAN).putInt(40,Integer.MAX_VALUE);invalid(malformed);
        malformed=direct.clone();malformed[34]=8;invalid(malformed);malformed=direct.clone();malformed[20]=3;invalid(malformed);malformed=direct.clone();ByteBuffer.wrap(malformed).order(ByteOrder.LITTLE_ENDIAN).putInt(24,0);invalid(malformed);
        invalid(wave(8000,1,8000*61));
        System.out.println("PASS mono/stereo PCM, resampling, malformed/truncated WAV, unsupported format and duration bounds");
    }
}
