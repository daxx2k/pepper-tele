package it.telepepper.quest;

import android.os.SystemClock;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import org.json.JSONObject;

final class PepperDiscovery {
    static final class Robot {
        final String host,name;
        Robot(String h,String n){host=h;name=n;}
    }
    static List<Robot> find() throws Exception {
        Map<String,Robot> found=new LinkedHashMap<>();
        try(DatagramSocket socket=new DatagramSocket()){
            socket.setBroadcast(true);
            byte[] request="TELEPEPPER_DISCOVER_V1".getBytes(StandardCharsets.UTF_8);
            socket.send(new DatagramPacket(request,request.length,InetAddress.getByName("255.255.255.255"),9574));
            long deadline=SystemClock.elapsedRealtime()+1500;
            while(found.size()<8&&SystemClock.elapsedRealtime()<deadline){
                socket.setSoTimeout((int)Math.max(1,deadline-SystemClock.elapsedRealtime()));
                byte[] data=new byte[1024];DatagramPacket packet=new DatagramPacket(data,data.length);
                try{socket.receive(packet);}catch(SocketTimeoutException e){break;}
                if(packet.getLength()>=data.length)continue;
                try{JSONObject reply=new JSONObject(new String(data,0,packet.getLength(),StandardCharsets.UTF_8));
                    if(!reply.optString("kind").equals("telepepper-discovery")||reply.optInt("version")!=1||reply.optInt("control_port")!=9570)continue;
                    String host=packet.getAddress().getHostAddress();if(!ConnectionStore.validHost(host))continue;
                    String name=reply.optString("name","Pepper").replaceAll("[\\p{Cntrl}]","");if(name.length()>40)name=name.substring(0,40);
                    found.put(host,new Robot(host,name));
                }catch(Exception ignored){}
            }
        }
        return new ArrayList<>(found.values());
    }
}
