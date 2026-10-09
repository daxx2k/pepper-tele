package it.telepepper.quest;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
/** Bounded PCM16 WAV decoder and mono 16 kHz resampler for robot audio transport. */
final class WavPcm {
    static byte[] decode(byte[] bytes)throws IOException{
        if(bytes.length<12||bytes.length>12000000)throw new IOException("Invalid WAV size");ByteBuffer b=ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        if(b.getInt(0)!=0x46464952||b.getInt(8)!=0x45564157)throw new IOException("Expected WAV audio");
        int channels=0,rate=0,bits=0,format=0,start=-1,length=0;
        for(int at=12;at+8<=bytes.length;){int id=b.getInt(at),len=b.getInt(at+4);if(len<0||len>bytes.length-at-8)throw new IOException("Truncated WAV");int data=at+8;
            if(id==0x20746d66){if(len<16)throw new IOException("Invalid WAV format");format=b.getShort(data)&65535;channels=b.getShort(data+2)&65535;rate=b.getInt(data+4);bits=b.getShort(data+14)&65535;}
            if(id==0x61746164){start=data;length=len;}at=data+len+(len&1);
        }
        if(format!=1||bits!=16||channels<1||channels>2||rate<8000||rate>96000||start<0||length%(channels*2)!=0)throw new IOException("Unsupported WAV format");
        int frames=length/(channels*2);if(frames<1||frames>(long)rate*60)throw new IOException("Invalid voice duration");
        int count=(int)((long)frames*16000/rate);byte[] out=new byte[count*2];ByteBuffer dest=ByteBuffer.wrap(out).order(ByteOrder.LITTLE_ENDIAN);
        for(int i=0;i<count;i++){double pos=i*rate/16000.0;int a=(int)pos,z=Math.min(a+1,frames-1);double mix=pos-a,first=0,second=0;
            for(int c=0;c<channels;c++){first+=b.getShort(start+(a*channels+c)*2);second+=b.getShort(start+(z*channels+c)*2);}dest.putShort((short)Math.round((first+(second-first)*mix)/channels));}
        return out;
    }
}
