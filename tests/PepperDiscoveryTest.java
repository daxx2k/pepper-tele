package it.telepepper.quest;
import java.net.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.atomic.AtomicReference;
import org.json.JSONObject;

public final class PepperDiscoveryTest {
    private static void require(boolean value){if(!value)throw new AssertionError();}
    private static void tcpFallback(boolean accepted)throws Exception {
        try(ServerSocket server=new ServerSocket(0,1,InetAddress.getLoopbackAddress())){
            AtomicReference<Throwable> failed=new AtomicReference<>();
            Thread worker=new Thread(()->{
                try(Socket socket=server.accept()){
                    socket.setSoTimeout(2000);
                    BufferedReader in=new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.UTF_8));
                    PrintWriter out=new PrintWriter(new OutputStreamWriter(socket.getOutputStream(),StandardCharsets.UTF_8),true);
                    JSONObject hello=new JSONObject(in.readLine());
                    require(hello.getString("role").equals("operator"));
                    require(hello.getString("token").equals("test-saved-pairing"));
                    out.println(accepted?"{\"observer\":\"test-observer\"}":"{\"error\":\"refused\"}");
                    if(accepted){
                        require(new JSONObject(in.readLine()).getString("cmd").equals("status"));
                        out.println("{\"ok\":true,\"armed\":false,\"motion_diagnostics\":{}}");
                    }
                }catch(Throwable t){failed.set(t);}
            });worker.start();
            Map<String,String> saved=Collections.singletonMap("127.0.0.1","test-saved-pairing");
            List<PepperDiscovery.Robot> robots=PepperDiscovery.find(saved,Collections.emptyList(),19874,server.getLocalPort(),30);
            worker.join(2500);require(!worker.isAlive());if(failed.get()!=null)throw new AssertionError(failed.get());
            require(robots.size()==(accepted?1:0));
            if(accepted)require(robots.get(0).host.equals("127.0.0.1"));
        }
    }
    private static void udpFiltering()throws Exception {
        try(DatagramSocket server=new DatagramSocket(0,InetAddress.getLoopbackAddress())){
            server.setSoTimeout(2000);AtomicReference<Throwable> failed=new AtomicReference<>();
            Thread worker=new Thread(()->{
                try {
                    byte[] buf=new byte[128];DatagramPacket request=new DatagramPacket(buf,buf.length);server.receive(request);
                    require(new String(buf,0,request.getLength(),StandardCharsets.UTF_8).equals("TELEPEPPER_DISCOVER_V1"));
                    String[] replies={"not JSON","{\"kind\":\"other\",\"version\":1,\"control_port\":9570}","{\"kind\":\"telepepper-discovery\",\"version\":2,\"control_port\":9570}","{\"kind\":\"telepepper-discovery\",\"version\":1,\"control_port\":22}","{\"kind\":\"telepepper-discovery\",\"version\":1,\"control_port\":9570,\"host\":\"203.0.113.1\"}"};
                    for(String reply:replies){byte[] bytes=reply.getBytes(StandardCharsets.UTF_8);server.send(new DatagramPacket(bytes,bytes.length,request.getAddress(),request.getPort()));}
                    byte[] duplicate=replies[4].getBytes(StandardCharsets.UTF_8);server.send(new DatagramPacket(duplicate,duplicate.length,request.getAddress(),request.getPort()));
                }catch(Throwable t){failed.set(t);}
            });worker.start();
            List<PepperDiscovery.Robot> robots=PepperDiscovery.find(Collections.emptyMap(),Collections.singleton(InetAddress.getLoopbackAddress()),server.getLocalPort(),9570,120);
            worker.join(2000);if(failed.get()!=null)throw new AssertionError(failed.get());require(robots.size()==1);require(robots.get(0).host.equals("127.0.0.1"));
        }
    }
    public static void main(String[] args)throws Exception {
        tcpFallback(true);tcpFallback(false);udpFiltering();
        require(!PepperDiscovery.checkSaved("127.0.0.1","test-saved-pairing",1));
        System.out.println("PASS authenticated saved-endpoint fallback with blocked UDP, rejection, UDP protocol filtering, deduplication and packet-source identity");
    }
}
