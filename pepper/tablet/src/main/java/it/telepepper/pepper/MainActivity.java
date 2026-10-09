package it.telepepper.pepper;
import it.telepepper.ui.StudioStyle;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.graphics.Color;
import android.text.InputType;
import android.view.WindowManager;
import android.widget.*;
import com.jcraft.jsch.*;
import org.json.JSONObject;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.Properties;
import java.util.concurrent.*;

public class MainActivity extends Activity {
    private EditText host,user,password;
    private TextView status,code,versionLabel;
    private String token;
    private Button connectButton,startButton,updateButton;
    private volatile boolean connected;
    private volatile boolean configVisible;
    private boolean allowAutoDisplay=true;
    private volatile String connectedHost;
    private Thread displayWatcher;
    private abstract static class PasswordUserInfo implements UserInfo,UIKeyboardInteractive {
        private final String secret;
        PasswordUserInfo(String secret){this.secret=secret;}
        public String[] promptKeyboardInteractive(String destination,String name,String instruction,String[] prompt,boolean[] echo){
            return prompt.length==1&&echo.length==1&&!echo[0]?new String[]{secret}:null;
        }
    }
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private String read(InputStream in) throws IOException { ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] b=new byte[4096];int n;while((n=in.read(b))!=-1)out.write(b,0,n);return new String(out.toByteArray(),StandardCharsets.UTF_8); }
    private void status(String text){runOnUiThread(()->status.setText(text));}
    private StudioStyle ui;
    private EditText field(LinearLayout parent,String label,String value,boolean secret){return ui.field(parent,label,value,secret);}
    @Override public void onCreate(Bundle state){StudioStyle.applyTheme(this);super.onCreate(state);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        JSONObject config=new JSONObject();try(FileInputStream in=openFileInput("bootstrap.json")){config=new JSONObject(read(in));}catch(Exception ignored){}
        token=config.optString("token",getPreferences(MODE_PRIVATE).getString("token",""));if(token.length()<12){byte[] bytes=new byte[16];new SecureRandom().nextBytes(bytes);StringBuilder s=new StringBuilder();for(byte b:bytes)s.append(String.format("%02x",b&255));token=s.toString();}getPreferences(MODE_PRIVATE).edit().putString("token",token).apply();
        boolean configured=getPreferences(MODE_PRIVATE).getBoolean("setup_complete",false)||config.has("token");
        ui=new StudioStyle(this);
        ui.window(this);
        allowAutoDisplay=!getIntent().getBooleanExtra("settings",false);
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setBackgroundColor(ui.background);
        LinearLayout layout=ui.column();layout.setPadding(ui.dp(18),ui.dp(10),ui.dp(18),ui.dp(12));scroll.addView(layout);
        LinearLayout heading=ui.row();heading.setGravity(android.view.Gravity.CENTER_VERTICAL);
        ImageView mark=new ImageView(this);mark.setImageResource(it.telepepper.pepper.R.drawable.ic_telepepper);heading.addView(mark,new LinearLayout.LayoutParams(ui.dp(38),ui.dp(38)));
        LinearLayout title=ui.column();title.setPadding(ui.dp(12),0,0,0);title.addView(ui.text("TelePepper",26,ui.ink,true));versionLabel=ui.text("Tablet "+it.telepepper.ui.VersionInfo.label(this)+" / Head unknown",14,ui.muted,false);title.addView(versionLabel);heading.addView(title,new LinearLayout.LayoutParams(0,-2,1));heading.setPadding(ui.dp(14),ui.dp(8),ui.dp(14),ui.dp(8));heading.setBackground(ui.surface(ui.surfaceColor,20));layout.addView(heading);
        LinearLayout cards=ui.row();boolean wide=getResources().getConfiguration().screenWidthDp>=700;if(!wide)cards.setOrientation(LinearLayout.VERTICAL);
        LinearLayout leftColumn=ui.column();LinearLayout connection=compactCard();
        LinearLayout.LayoutParams left=new LinearLayout.LayoutParams(wide?0:-1,-2,wide?1:0);left.setMargins(0,ui.dp(12),wide?ui.dp(12):0,0);cards.addView(leftColumn,left);
        LinearLayout.LayoutParams right=new LinearLayout.LayoutParams(wide?0:-1,-2,wide?1.15f:0);right.setMargins(0,ui.dp(12),0,0);cards.addView(connection,right);layout.addView(cards);
        LinearLayout pairing=compactCard();leftColumn.addView(pairing);
        pairing.addView(ui.text("PAIR YOUR QUEST",13,ui.ink,true));
        pairing.addView(ui.text("Enter this code on Quest",15,ui.muted,false));
        code=ui.text(token,17,ui.ink,true);code.setTypeface(android.graphics.Typeface.MONOSPACE);code.setTextIsSelectable(true);code.setPadding(ui.dp(10),ui.dp(14),ui.dp(10),ui.dp(14));code.setBackground(ui.surface(ui.tint,12));LinearLayout.LayoutParams codeSpace=new LinearLayout.LayoutParams(-1,-2);codeSpace.setMargins(0,ui.dp(10),0,ui.dp(8));pairing.addView(code,codeSpace);
        pairing.addView(ui.text("Quest connects to the head Wi-Fi address, not the tablet ADB address.",13,ui.muted,false));
        LinearLayout operations=compactCard();LinearLayout.LayoutParams opSpace=new LinearLayout.LayoutParams(-1,-2);opSpace.setMargins(0,ui.dp(12),0,0);leftColumn.addView(operations,opSpace);
        operations.addView(ui.text("ROBOT CONTROLS",13,ui.ink,true));
        LinearLayout controls=ui.row();
        Button displayControl=ui.button("DISPLAY",false);LinearLayout.LayoutParams displayControlSpace=new LinearLayout.LayoutParams(0,ui.dp(48),1);displayControlSpace.setMargins(0,ui.dp(8),ui.dp(8),0);controls.addView(displayControl,displayControlSpace);displayControl.setOnClickListener(v->openDisplay(host.getText().toString().trim()));
        Button stop=ui.stopButton("STOP");LinearLayout.LayoutParams stopSpace=new LinearLayout.LayoutParams(0,ui.dp(48),1);stopSpace.setMargins(0,ui.dp(8),0,0);controls.addView(stop,stopSpace);operations.addView(controls);
        stop.setOnClickListener(v->{String h=host.getText().toString().trim();new Thread(()->emergency(h)).start();});
        Button exit=ui.button("Exit • Pepper normal",false);LinearLayout.LayoutParams exitSpace=new LinearLayout.LayoutParams(-1,ui.dp(48));exitSpace.setMargins(0,ui.dp(8),0,0);operations.addView(exit,exitSpace);exit.setOnClickListener(v->exitTeleoperation());
        TextView note=ui.text("START prepares the posture. Tracking starts from Quest.",13,ui.muted,false);note.setPadding(0,ui.dp(8),0,0);operations.addView(note);
        connection.addView(ui.text("CONNECTION",13,ui.ink,true));
        host=compactField(connection,"Head address from this tablet",config.optString("host",getPreferences(MODE_PRIVATE).getString("head_host","198.18.0.1")),false);
        user=compactField(connection,"SSH username",config.optString("user",getPreferences(MODE_PRIVATE).getString("ssh_user","nao")),false);
        password=compactField(connection,"SSH password",config.optString("password",getPreferences(MODE_PRIVATE).getString("ssh_password","")),true);
        LinearLayout linkControls=ui.row();
        connectButton=ui.button("CONNECT",false);LinearLayout.LayoutParams connectSpace=new LinearLayout.LayoutParams(0,ui.dp(48),1);connectSpace.setMargins(0,ui.dp(12),ui.dp(8),0);linkControls.addView(connectButton,connectSpace);
        updateButton=ui.button("Update service",false);LinearLayout.LayoutParams updateSpace=new LinearLayout.LayoutParams(0,ui.dp(48),1);updateSpace.setMargins(0,ui.dp(12),0,0);linkControls.addView(updateButton,updateSpace);
        updateButton.setOnClickListener(v->{
            if(!connectButton.isEnabled()){status("Wait for the current operation to finish.");return;}
            String h=host.getText().toString().trim(),u=user.getText().toString().trim(),p=password.getText().toString();
            if(h.isEmpty()||u.isEmpty()){status("Enter the robot head address and SSH username.");return;}
            connected=false;connectButton.setEnabled(false);connectButton.setText("UPDATING...");updateButton.setEnabled(false);
            worker.execute(()->deploy(h,u,p,true));
        });
        connection.addView(linkControls);startButton=connectButton;
        status=ui.text("CONNECT starts the service. DISPLAY opens the participant screen.",14,ui.ink,false);status.setPadding(0,ui.dp(12),0,0);status.setMaxLines(4);status.setAccessibilityLiveRegion(android.view.View.ACCESSIBILITY_LIVE_REGION_POLITE);connection.addView(status);
        setContentView(scroll);
        connectButton.setOnClickListener(v->{
            String h=host.getText().toString().trim(),u=user.getText().toString().trim(),p=password.getText().toString();
            if(connected&&h.equals(connectedHost)){
                connectButton.setEnabled(false);connectButton.setText("STARTING...");worker.execute(()->{boolean ready=prepareMotion(h);runOnUiThread(()->{connectButton.setText(connected?"START":"CONNECT");connectButton.setEnabled(true);if(ready)openDisplay(h);});});return;
            }
            if(h.isEmpty()||u.isEmpty()){status("Enter the robot head address and SSH username.");return;}
            connected=false;connectButton.setEnabled(false);connectButton.setText("CONNECTING...");
            boolean install=!getPreferences(MODE_PRIVATE).getBoolean("setup_complete",configured)||getPreferences(MODE_PRIVATE).getInt("bridge_revision",0)!=1;
            worker.execute(()->deploy(h,u,p,install));
        });
    }
    private LinearLayout compactCard(){LinearLayout card=ui.card();card.setPadding(ui.dp(16),ui.dp(12),ui.dp(16),ui.dp(12));return card;}
    private EditText compactField(LinearLayout parent,String label,String value,boolean secret){
        EditText edit=field(parent,label,value,secret);edit.setTextSize(17);edit.setPadding(ui.dp(12),ui.dp(6),ui.dp(12),ui.dp(6));
        LinearLayout.LayoutParams ep=(LinearLayout.LayoutParams)edit.getLayoutParams();ep.height=ui.dp(42);edit.setLayoutParams(ep);
        TextView caption=(TextView)parent.getChildAt(parent.getChildCount()-2);caption.setTextSize(13);
        LinearLayout.LayoutParams lp=(LinearLayout.LayoutParams)caption.getLayoutParams();lp.setMargins(0,ui.dp(8),0,ui.dp(4));caption.setLayoutParams(lp);return edit;
    }
    private void openDisplay(String h){
        if(h.isEmpty()){status("Enter the robot head address first.");return;}
        configVisible=false;android.content.Intent intent=new android.content.Intent(this,ParticipantActivity.class);intent.putExtra("host",h);intent.putExtra("token",token);startActivity(intent);
    }
    private void watchDisplay(){
        while(configVisible&&!isFinishing()){
            if(connected&&connectedHost!=null){
                try(Socket socket=new Socket()){
                    socket.connect(new InetSocketAddress(connectedHost,9570),1500);socket.setSoTimeout(2000);
                    BufferedReader reader=new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.UTF_8));
                    OutputStream out=socket.getOutputStream();out.write((new JSONObject().put("token",token).put("role","operator").toString()+"\n").getBytes(StandardCharsets.UTF_8));
                    if(!new JSONObject(reader.readLine()).has("observer"))throw new IOException("Pairing refused");
                    out.write("{\"cmd\":\"status\"}\n".getBytes(StandardCharsets.UTF_8));JSONObject result=new JSONObject(reader.readLine());
                    JSONObject bridge=result.optJSONObject("bridge");String headVersion=bridge==null?"unknown":bridge.optString("version","unknown");
                    runOnUiThread(()->{if(versionLabel!=null)versionLabel.setText("Tablet "+it.telepepper.ui.VersionInfo.label(this)+" / Head "+headVersion);});
                    JSONObject telemetry=result.optJSONObject("telemetry"),tablet=result.optJSONObject("tablet");
                    boolean armed=result.optBoolean("armed",false)||(telemetry!=null&&telemetry.optBoolean("armed",false));
                    boolean content=tablet!=null&&tablet.optDouble("opacity",1)>0&&(!tablet.optString("text","").isEmpty()||!tablet.optString("reaction","").isEmpty());
                    if(allowAutoDisplay&&(armed||content)){runOnUiThread(()->{if(configVisible&&allowAutoDisplay&&!isFinishing())openDisplay(connectedHost);});return;}
                }catch(Exception ignored){}
            }
            try{Thread.sleep(750);}catch(InterruptedException e){return;}
        }
    }
    @Override protected void onResume(){super.onResume();configVisible=true;displayWatcher=new Thread(this::watchDisplay,"TabletDisplayWatcher");displayWatcher.start();}
    @Override protected void onPause(){configVisible=false;if(displayWatcher!=null)displayWatcher.interrupt();super.onPause();}
    private String exec(Session s,String command) throws Exception {ChannelExec c=(ChannelExec)s.openChannel("exec");c.setCommand(command);c.setInputStream(null);ByteArrayOutputStream errors=new ByteArrayOutputStream();c.setErrStream(errors);InputStream in=c.getInputStream();c.connect(5000);try{String result=read(in);long deadline=System.currentTimeMillis()+2000;while(!c.isClosed()&&System.currentTimeMillis()<deadline)Thread.sleep(20);if(c.getExitStatus()!=0)throw new IOException("Robot setup command failed: "+errors.toString("UTF-8"));return result;}finally{c.disconnect();}}
    private boolean prepareMotion(String h){try(Socket socket=new Socket()){
        socket.connect(new InetSocketAddress(h,9570),2000);socket.setSoTimeout(3000);
        BufferedReader reader=new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.UTF_8));
        OutputStream out=socket.getOutputStream();
        out.write((new JSONObject().put("token",token).put("role","operator").toString()+"\n").getBytes(StandardCharsets.UTF_8));
        if(!new JSONObject(reader.readLine()).has("observer"))throw new IOException("Pairing refused");
        JSONObject command=new JSONObject().put("cmd","prepare_motion").put("confirmed",true);
        for(int attempt=0;attempt<100;attempt++){
            out.write((command.toString()+"\n").getBytes(StandardCharsets.UTF_8));JSONObject reply=new JSONObject(reader.readLine());
            if(!reply.optBoolean("ok"))throw new IOException(reply.optString("error","Preparation refused"));
            JSONObject prep=reply.getJSONObject("preparation");status(prep.getString("message"));
            if(!prep.getBoolean("busy"))return prep.getString("message").startsWith("Posture ready");
            Thread.sleep(300);command=new JSONObject().put("cmd","status");
        }
        status("Preparation not confirmed. Check Pepper before starting.");
    }catch(Exception e){status("Preparation failed: "+e.getMessage());}return false;}
    private void backupFile(ChannelSftp sftp,String source,String destination)throws Exception{
        byte[] bytes;
        try(InputStream in=sftp.get(source)){
            ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] buffer=new byte[4096];int n;
            while((n=in.read(buffer))!=-1){if(out.size()+n>16*1024*1024)throw new IOException("Installed service file is too large to back up");out.write(buffer,0,n);}bytes=out.toByteArray();
        }catch(SftpException missing){if(missing.id==ChannelSftp.SSH_FX_NO_SUCH_FILE)return;throw missing;}
        sftp.put(new ByteArrayInputStream(bytes),destination);
    }
    private void waitForServiceStop(String h)throws Exception{
        long deadline=System.currentTimeMillis()+10000;
        while(System.currentTimeMillis()<deadline){
            try(Socket probe=new Socket()){probe.connect(new InetSocketAddress(h,9570),700);}
            catch(java.net.ConnectException stopped){return;}
            Thread.sleep(150);
        }
        throw new IOException("Previous service is still running; update was not applied");
    }
    private void requireIdleForUpdate(String h)throws Exception{
        try(Socket socket=new Socket()){
            socket.connect(new InetSocketAddress(h,9570),2000);socket.setSoTimeout(2000);
            BufferedReader in=new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.UTF_8));
            OutputStream out=socket.getOutputStream();out.write((new JSONObject().put("token",token).put("role","operator").toString()+"\n").getBytes(StandardCharsets.UTF_8));
            if(!new JSONObject(in.readLine()).has("observer"))throw new IOException("Existing service pairing refused; stop it before updating.");
            out.write("{\"cmd\":\"status\"}\n".getBytes(StandardCharsets.UTF_8));JSONObject state=new JSONObject(in.readLine());
            if(!state.has("armed")||state.getBoolean("armed")||state.optJSONObject("preparation")==null||state.getJSONObject("preparation").optBoolean("busy",true))
                throw new IOException("Stop tracking and wait for preparation to finish before Update service.");
            JSONObject d=state.optJSONObject("motion_diagnostics");
            if(d==null||d.optBoolean("stopping",true)||!d.optString("stop_error","").isEmpty()||d.optBoolean("returning_to_neutral",false))
                throw new IOException("Wait for confirmed STOP before Update service.");
        }catch(java.net.ConnectException inactive){/* No listener: a stopped service can be installed. */}
    }
    private void deploy(String h,String u,String p,boolean install){Session session=null;try{
        status("Connecting to robot head...");JSch jsch=new JSch();File known=new File(getFilesDir(),"known_hosts");if(!known.exists())known.createNewFile();jsch.setKnownHosts(known.getAbsolutePath());
        session=jsch.getSession(u,h,22);session.setPassword(p);Properties options=new Properties();options.put("StrictHostKeyChecking","ask");options.put("PreferredAuthentications","keyboard-interactive,password");session.setConfig(options);
        session.setUserInfo(new PasswordUserInfo(p){public String getPassphrase(){return null;}public String getPassword(){return p;}public boolean promptPassword(String s){return true;}public boolean promptPassphrase(String s){return false;}public void showMessage(String s){status(s);}public boolean promptYesNo(String message){CountDownLatch done=new CountDownLatch(1);boolean[] yes={false};runOnUiThread(()->new AlertDialog.Builder(MainActivity.this).setTitle("Pepper SSH identity").setMessage(message).setPositiveButton("Pair",(d,w)->{yes[0]=true;done.countDown();}).setNegativeButton("Cancel",(d,w)->done.countDown()).setOnCancelListener(d->done.countDown()).show());try{done.await(60,TimeUnit.SECONDS);}catch(InterruptedException e){Thread.currentThread().interrupt();}return yes[0];}});
        session.connect(10000);session.setTimeout(10000);status("Checking robot dependencies...");
        exec(session,"python -c 'import sys; sys.path.insert(0,\"/opt/aldebaran/lib/python2.7/site-packages\"); import qi; from PIL import Image' && systemctl --user --version >/dev/null");
        if(install){
            requireIdleForUpdate(h);
            String stage="/home/nao/telepepper-update-"+System.currentTimeMillis();
            String backup=stage+"-backup";
            status("Staging service update / keeping base settings...");
            ChannelSftp sftp=(ChannelSftp)session.openChannel("sftp");sftp.connect(5000);
            try{
                sftp.mkdir(stage);sftp.mkdir(backup);
                for(String name:getAssets().list(""))if(name.endsWith(".py")||name.endsWith(".pkg")){
                    try(InputStream script=getAssets().open(name)){sftp.put(script,stage+"/"+name);}
                }
                exec(session,"python "+stage+"/deployment_settings.py /home/nao/telepepper.py "+stage+"/telepepper.py");
                exec(session,"python -c 'import glob,py_compile; [py_compile.compile(p,doraise=True) for p in glob.glob(\""+stage+"/*.py\")]'");
                // Re-check after upload: never replace a newly armed session.
                requireIdleForUpdate(h);
                exec(session,"systemctl --user stop telepepper.service 2>/dev/null || true");
                exec(session,"if test -f /home/nao/telepepper.pid; then p=$(cat /home/nao/telepepper.pid); case $p in ''|*[!0-9]*) exit 1;; esac; if test -r /proc/$p/cmdline && tr '\\0' ' ' </proc/$p/cmdline | grep -q '^python /home/nao/telepepper.py --token-file '; then kill -TERM $p; sleep 1; fi; fi");
                waitForServiceStop(h);
                for(String name:getAssets().list(""))if(name.endsWith(".py")||name.endsWith(".pkg"))backupFile(sftp,"/home/nao/"+name,backup+"/"+name);
                backupFile(sftp,"/home/nao/.config/systemd/user/telepepper.service",backup+"/telepepper.service");
                for(String name:getAssets().list(""))if(name.endsWith(".py")||name.endsWith(".pkg"))exec(session,"mv "+stage+"/"+name+" /home/nao/"+name);
                sftp.put(new ByteArrayInputStream(token.getBytes(StandardCharsets.UTF_8)),"/home/nao/.telepepper-token");sftp.chmod(0600,"/home/nao/.telepepper-token");
                exec(session,"mkdir -p /home/nao/.config/systemd/user");
                try(InputStream unit=getAssets().open("telepepper.service")){sftp.put(unit,"/home/nao/.config/systemd/user/telepepper.service");}
            }finally{sftp.disconnect();}
        }
        exec(session,"systemctl --user daemon-reload && systemctl --user disable telepepper.service && systemctl --user start telepepper.service");
        for(int attempt=0;attempt<15;attempt++){
            try(Socket probe=new Socket()){probe.connect(new InetSocketAddress(h,9570),1000);probe.setSoTimeout(2000);JSONObject hello=new JSONObject().put("token",token).put("role","operator");probe.getOutputStream().write((hello.toString()+"\n").getBytes(StandardCharsets.UTF_8));String line=new BufferedReader(new InputStreamReader(probe.getInputStream(),StandardCharsets.UTF_8)).readLine();if(line==null||!new JSONObject(line).has("observer"))throw new IOException("Pairing refused");connectedHost=h;connected=true;break;}
            catch(IOException waiting){Thread.sleep(500);}
        }
        if(!connected)throw new IOException("Service did not become reachable");
        getPreferences(MODE_PRIVATE).edit().putBoolean("setup_complete",true).putInt("bridge_revision",1).putString("head_host",h).putString("ssh_user",u).putString("ssh_password",p).apply();
        status("Connected. Press START to prepare Pepper, then start tracking from Quest.");
    }catch(Exception e){connected=false;status("Connection failed: "+e.getMessage());}finally{if(session!=null)session.disconnect();runOnUiThread(()->{connectButton.setText(connected?"START":"CONNECT");connectButton.setEnabled(true);if(updateButton!=null)updateButton.setEnabled(true);});}}
    private void emergency(String h){try(Socket socket=new Socket()){socket.connect(new InetSocketAddress(h,9570),2000);socket.setSoTimeout(2000);JSONObject request=new JSONObject();request.put("token",token);request.put("emergency",true);socket.getOutputStream().write((request.toString()+"\n").getBytes(StandardCharsets.UTF_8));String line=new BufferedReader(new InputStreamReader(socket.getInputStream())).readLine();if(line==null)throw new IOException("No acknowledgement");status("STOP confirmed. Hold A + X in Quest to restart.");}catch(Exception e){status("STOP not confirmed: "+e.getMessage()+". Use the physical stop on Pepper.");}}
    private void exitTeleoperation(){
        if(connectButton!=null&&!connectButton.isEnabled()){status("Wait for connection setup to finish before exiting.");return;}
        it.telepepper.lifecycle.ExitSession.close(this,host.getText().toString().trim(),token);
    }
    @Override public void onBackPressed(){exitTeleoperation();}
    @Override protected void onDestroy(){worker.shutdownNow();super.onDestroy();}
}
