package it.telepepper.quest;

import android.app.Activity;
import android.app.AlertDialog;
import com.jcraft.jsch.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.Properties;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.json.JSONObject;

/** Owner-authorized setup and manual start, directly from Quest to the robot. */
final class Pepper25Bootstrap {
    private static final String ROOT="/home/nao/telepepper25";
    interface Listener {void progress(String message);void ready(String pairing);void failed(String message);}
    private static String read(InputStream in)throws IOException {
        ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] bytes=new byte[4096];int n;
        while((n=in.read(bytes))!=-1){if(out.size()+n>1048576)throw new IOException("Robot reply too large");out.write(bytes,0,n);}
        return new String(out.toByteArray(),StandardCharsets.UTF_8);
    }
    private static String exec(Session session,String command)throws Exception {
        ChannelExec channel=(ChannelExec)session.openChannel("exec");channel.setCommand(command);channel.setInputStream(null);
        ByteArrayOutputStream errors=new ByteArrayOutputStream();channel.setErrStream(errors);InputStream in=channel.getInputStream();
        channel.connect(5000);
        try{
            String text=read(in);long deadline=System.currentTimeMillis()+2000;
            while(!channel.isClosed()&&System.currentTimeMillis()<deadline)Thread.sleep(20);
            if(channel.getExitStatus()!=0){String error=errors.toString("UTF-8");if(error.contains("No module named"))throw new IOException("Pepper is missing Python qi or Pillow. Check its runtime before this trial.");throw new IOException(text.trim().isEmpty()?"Robot command failed":text.trim());}
            return text;
        }finally{channel.disconnect();}
    }
    private abstract static class Auth implements UserInfo,UIKeyboardInteractive {
        private final String secret;Auth(String value){secret=value;}
        public String getPassphrase(){return null;}public String getPassword(){return secret;}
        public boolean promptPassword(String message){return true;}public boolean promptPassphrase(String message){return false;}
        public String[] promptKeyboardInteractive(String destination,String name,String instruction,String[] prompt,boolean[] echo){return prompt.length==1&&echo.length==1&&!echo[0]?new String[]{secret}:null;}
    }
    static void connect(Activity activity,String head,String user,String password,Listener listener){
        new Thread(()->{
            Session session=null;ChannelSftp sftp=null;
            try{
                listener.progress("Connecting to Pepper 2.5...");
                JSch jsch=new JSch();File known=new File(activity.getFilesDir(),"pepper25_known_hosts");if(!known.exists())known.createNewFile();jsch.setKnownHosts(known.getAbsolutePath());
                session=jsch.getSession(user,head,22);session.setPassword(password);
                Properties options=new Properties();options.put("StrictHostKeyChecking","ask");options.put("PreferredAuthentications","keyboard-interactive,password");session.setConfig(options);
                session.setUserInfo(new Auth(password){
                    public void showMessage(String message){listener.progress("Checking robot identity...");}
                    public boolean promptYesNo(String message){
                        CountDownLatch done=new CountDownLatch(1);boolean[] yes={false};
                        activity.runOnUiThread(()->{if(activity.isFinishing()){done.countDown();return;}new AlertDialog.Builder(activity).setTitle("Pair with your Pepper")
                            .setMessage(message).setPositiveButton("Pair",(dialog,which)->{yes[0]=true;done.countDown();})
                            .setNegativeButton("Cancel",(dialog,which)->done.countDown()).setOnCancelListener(dialog->done.countDown()).show();});
                        try{done.await(60,TimeUnit.SECONDS);}catch(InterruptedException interrupted){Thread.currentThread().interrupt();}
                        return yes[0];
                    }
                });
                session.connect(10000);session.setTimeout(45000);
                listener.progress("Checking NAOqi version and camera runtime...");
                String probe="env PYTHONPATH=/opt/aldebaran/lib/python2.7/site-packages LD_LIBRARY_PATH=/opt/aldebaran/lib python -c 'import qi; from PIL import Image; s=qi.Session(); s.connect(\"tcp://127.0.0.1:9559\"); print(s.service(\"ALSystem\").systemVersion())'";
                String version=exec(session,probe).trim();
                if(!version.startsWith("2.5."))throw new IOException("This app requires Pepper NAOqi 2.5; found "+version);
                sftp=(ChannelSftp)session.openChannel("sftp");sftp.connect(5000);
                String pairing="";boolean installed=false;
                try{try(InputStream in=sftp.get(ROOT+"/.telepepper-token")){pairing=read(in).trim();}installed=pairing.matches("[0-9a-f]{32,80}");}catch(SftpException missing){}
                boolean running=false;
                if(installed){JSONObject state=new JSONObject(exec(session,"python "+ROOT+"/manage25.py status"));running=state.optBoolean("running",false);}
                if(!running){
                    listener.progress("Installing the head service / motors stay stopped...");
                    try{sftp.mkdir(ROOT);}catch(SftpException exists){sftp.stat(ROOT);}
                    for(String name:activity.getAssets().list("pepper25"))if(name.endsWith(".py")){
                        try(InputStream in=activity.getAssets().open("pepper25/"+name)){sftp.put(in,ROOT+"/"+name);}
                    }
                    if(!installed){byte[] bytes=new byte[24];new SecureRandom().nextBytes(bytes);StringBuilder text=new StringBuilder();for(byte b:bytes)text.append(String.format("%02x",b&255));pairing=text.toString();}
                    sftp.put(new ByteArrayInputStream(pairing.getBytes(StandardCharsets.UTF_8)),ROOT+"/.telepepper-token");sftp.chmod(0600,ROOT+"/.telepepper-token");
                }
                listener.progress("Starting the head service / tracking stays paused...");
                JSONObject started=new JSONObject(exec(session,"env PYTHONPATH=/opt/aldebaran/lib/python2.7/site-packages LD_LIBRARY_PATH=/opt/aldebaran/lib python "+ROOT+"/manage25.py start"));
                if(!started.optBoolean("running",false))throw new IOException("Robot service did not become ready");
                listener.ready(pairing);
            }catch(Exception error){listener.failed(error.getMessage()==null?error.getClass().getSimpleName():error.getMessage());}
            finally{if(sftp!=null)sftp.disconnect();if(session!=null)session.disconnect();}
        },"pepper25-owner-connect").start();
    }
}
