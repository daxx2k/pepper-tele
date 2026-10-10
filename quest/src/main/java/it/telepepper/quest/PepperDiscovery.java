package it.telepepper.quest;

import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import org.json.JSONObject;

final class PepperDiscovery {
    static final class Robot {
        final String host,name;
        Robot(String h,String n){host=h;name=n;}
    }
    private static long now(){return System.nanoTime()/1000000L;}
    static List<Robot> find(Map<String,String> saved) throws Exception {
        Set<InetAddress> targets=new LinkedHashSet<>();
        targets.add(InetAddress.getByName("255.255.255.255"));
        try {
            Enumeration<NetworkInterface> interfaces=NetworkInterface.getNetworkInterfaces();
            while(interfaces!=null&&interfaces.hasMoreElements()) {
                NetworkInterface nic=interfaces.nextElement();
                if(!nic.isUp()||nic.isLoopback())continue;
                for(InterfaceAddress address:nic.getInterfaceAddresses())
                    if(address.getBroadcast() instanceof Inet4Address)targets.add(address.getBroadcast());
            }
        }catch(SocketException ignored){}
        for(String host:saved.keySet())if(ConnectionStore.validHost(host))targets.add(InetAddress.getByName(host));
        return find(saved,targets,9574,9570,1800);
    }
    static List<Robot> find(Map<String,String> saved,Collection<InetAddress> targets,int udpPort,int controlPort,int duration) throws Exception {
        Map<String,Robot> found=new LinkedHashMap<>();
        try(DatagramSocket socket=new DatagramSocket()){
            socket.setBroadcast(true);
            byte[] request="TELEPEPPER_DISCOVER_V1".getBytes(StandardCharsets.UTF_8);
            long deadline=now()+duration,next=0;
            while(found.size()<8&&now()<deadline){
                if(now()>=next){
                    for(InetAddress address:targets)try{socket.send(new DatagramPacket(request,request.length,address,udpPort));}catch(IOException ignored){}
                    next=now()+500;
                }
                socket.setSoTimeout((int)Math.max(1,Math.min(200,deadline-now())));
                byte[] data=new byte[1024];DatagramPacket packet=new DatagramPacket(data,data.length);
                try{socket.receive(packet);}catch(SocketTimeoutException e){continue;}
                if(packet.getLength()>=data.length)continue;
                try{JSONObject reply=new JSONObject(new String(data,0,packet.getLength(),StandardCharsets.UTF_8));
                    if(!reply.optString("kind").equals("telepepper-discovery")||reply.optInt("version")!=1||reply.optInt("control_port")!=controlPort)continue;
                    String host=packet.getAddress().getHostAddress();if(!ConnectionStore.validHost(host))continue;
                    String name=reply.optString("name","Pepper").replaceAll("[\\p{Cntrl}]","");if(name.length()>40)name=name.substring(0,40);
                    found.put(host,new Robot(host,name));
                }catch(Exception ignored){}
            }
        }
        // Some access points suppress broadcasts. Authenticate only previously
        // paired endpoints via the same observer/status path as Check connection.
        int checked=0;
        for(Map.Entry<String,String> entry:saved.entrySet()){
            if(checked>=4||found.size()>=8)break;
            if(found.containsKey(entry.getKey())||!ConnectionStore.validHost(entry.getKey())||entry.getValue().length()<12)continue;
            checked++;
            if(checkSaved(entry.getKey(),entry.getValue(),controlPort))found.put(entry.getKey(),new Robot(entry.getKey(),"Saved Pepper"));
        }
        return new ArrayList<>(found.values());
    }
    private static String line(BufferedReader in,int limit)throws IOException {
        StringBuilder value=new StringBuilder();int c;
        while((c=in.read())!=-1){if(c=='\n')return value.toString();if(value.length()>=limit)throw new IOException("Reply too large");value.append((char)c);}
        throw new EOFException("Robot disconnected");
    }
    static boolean checkSaved(String host,String token,int port){
        try(Socket socket=new Socket()){
            socket.connect(new InetSocketAddress(host,port),400);socket.setSoTimeout(500);
            BufferedReader in=new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.UTF_8));
            OutputStream out=socket.getOutputStream();
            out.write((new JSONObject().put("token",token).put("role","operator").toString()+"\n").getBytes(StandardCharsets.UTF_8));
            if(!new JSONObject(line(in,2048)).has("observer"))return false;
            out.write("{\"cmd\":\"status\"}\n".getBytes(StandardCharsets.UTF_8));
            JSONObject status=new JSONObject(line(in,65536));
            return status.optBoolean("ok",false)&&status.has("armed")&&status.optJSONObject("motion_diagnostics")!=null;
        }catch(Exception ignored){return false;}
    }
}
