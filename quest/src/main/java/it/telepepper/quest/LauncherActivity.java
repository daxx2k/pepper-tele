package it.telepepper.quest;
import it.telepepper.ui.StudioStyle;
import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.widget.*;
import android.text.InputType;
import android.Manifest;
import android.content.pm.PackageManager;
import java.io.*;
import java.net.*;
import org.json.JSONObject;

public class LauncherActivity extends Activity {
    private NaturalVoice voiceProbe;
    private boolean diagnosticStarted;
    private StudioStyle ui;
    private int ink,muted,accent;
    private TextView label(String value,int size,int color){return ConnectionForm.label(ui,value,size,color);}
    private EditText field(LinearLayout box,String title,String value,boolean secret){return ConnectionForm.field(ui,box,title,value,secret);}
    @Override public void onCreate(Bundle state){
        StudioStyle.applyTheme(this);super.onCreate(state);
        if(checkSelfPermission(Manifest.permission.RECORD_AUDIO)!=PackageManager.PERMISSION_GRANTED)requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO},42);
        SharedPreferences prefs=ConnectionStore.load(this);
        ui=new StudioStyle(this);ui.window(this);ink=ui.ink;muted=ui.muted;accent=StudioStyle.CONNECTION;
        voiceProbe=new NaturalVoice(this);
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setBackgroundColor(ui.background);
        LinearLayout root=new LinearLayout(this);root.setOrientation(1);root.setPadding(ui.dp(28),ui.dp(24),ui.dp(28),ui.dp(28));android.widget.FrameLayout.LayoutParams bounds=new android.widget.FrameLayout.LayoutParams(-1,-2,android.view.Gravity.TOP|android.view.Gravity.CENTER_HORIZONTAL);scroll.addView(root,bounds);root.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,orr,ob)->{int target=Math.min(scroll.getWidth(),ui.dp(860));if(root.getLayoutParams().width!=target){root.getLayoutParams().width=target;root.requestLayout();}});
        LinearLayout heading=ui.row();heading.setGravity(android.view.Gravity.CENTER_VERTICAL);heading.setPadding(ui.dp(12),ui.dp(10),ui.dp(12),ui.dp(10));heading.setBackground(ui.surface(ui.surfaceColor,22));ImageView mark=new ImageView(this);mark.setImageResource(R.drawable.ic_telepepper);heading.addView(mark,new LinearLayout.LayoutParams(ui.dp(36),ui.dp(36)));LinearLayout titles=ui.column();titles.setPadding(ui.dp(16),0,0,0);titles.addView(label("TelePepper "+it.telepepper.ui.VersionInfo.label(this),30,ink));titles.addView(label("Connection settings",16,muted));heading.addView(titles,new LinearLayout.LayoutParams(0,-2,1));root.addView(heading);
        root.addView(label("Connect Quest to the Pepper head. Start motion from the headset.",17,muted));
        LinearLayout card=ui.card();card.setPadding(ui.dp(14),ui.dp(12),ui.dp(14),ui.dp(12));card.addView(ui.text("ROBOT CONNECTION",13,accent,true));
        LinearLayout.LayoutParams cp=new LinearLayout.LayoutParams(-1,-2);cp.setMargins(0,ui.dp(10),0,ui.dp(10));root.addView(card,cp);
        EditText host=field(card,"ROBOT HEAD ADDRESS",prefs.getString("host",""),false);
        EditText token=field(card,"PAIRING CODE",prefs.getString("token",""),true);
        TextView status=label("Check connection and pairing before entering.",16,muted);card.addView(status);
        TextView voiceInfo=label("Checking local natural voice...",14,muted);card.addView(voiceInfo);
        android.os.Handler voiceHandler=new android.os.Handler(getMainLooper());
        if(getIntent().getBooleanExtra("voice_sequence_self_test",false)&&(getApplicationInfo().flags&android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE)!=0){
            diagnosticStarted=true;final String[] phrases={"Hello, I am Pepper.","How are you?","How are you?"};
            for(String phrase:phrases)voiceProbe.speak(phrase,"en-US");
            final long deadline=android.os.SystemClock.elapsedRealtime()+60000;
            voiceHandler.postDelayed(new Runnable(){int completed;public void run(){if(isFinishing()||isDestroyed())return;byte[] audio=voiceProbe.take();if(audio!=null){String caption=voiceProbe.takeText();if(completed>=phrases.length||!caption.equals(phrases[completed])||audio.length==0){android.util.Log.e("TelePepper","Piper sequence self-test FAIL: output/order");return;}completed++;android.util.Log.i("TelePepper","Piper sequence clip="+completed+" bytes="+audio.length);}
                if(completed==phrases.length){if(voiceProbe.take()!=null){android.util.Log.e("TelePepper","Piper sequence self-test FAIL: duplicate");return;}android.util.Log.i("TelePepper","Piper sequence self-test PASS: startup queue, three clips, repeated preset, no robot playback");return;}
                if(android.os.SystemClock.elapsedRealtime()<deadline)voiceHandler.postDelayed(this,100);else android.util.Log.e("TelePepper","Piper sequence self-test FAIL: timeout "+voiceProbe.status());}},100);
        }
        voiceHandler.postDelayed(new Runnable(){public void run(){if(isFinishing()||isDestroyed())return;voiceInfo.setText("Natural voice: "+voiceProbe.name());
            if(voiceProbe.name().startsWith("Starting")){voiceHandler.postDelayed(this,300);return;}
            if(!diagnosticStarted&&getIntent().getBooleanExtra("voice_self_test",false)&&(getApplicationInfo().flags&android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE)!=0){diagnosticStarted=true;voiceProbe.speak("Hello, I am Pepper. How can I help you today?","en-GB");final long deadline=android.os.SystemClock.elapsedRealtime()+30000;
                voiceHandler.postDelayed(new Runnable(){public void run(){if(isFinishing()||isDestroyed())return;byte[] pcm=voiceProbe.take();if(pcm!=null){try(java.io.FileOutputStream out=openFileOutput("voice-self-test.pcm",MODE_PRIVATE)){out.write(pcm);android.util.Log.i("TelePepper","Cori self-test PASS: PCM16 16000 Hz mono, bytes="+pcm.length);}catch(Exception e){android.util.Log.e("TelePepper","Voice self-test output failed",e);}return;}if(android.os.SystemClock.elapsedRealtime()<deadline)voiceHandler.postDelayed(this,100);else android.util.Log.e("TelePepper","Cori self-test failed: "+voiceProbe.status());}},100);
            }}},300);
        Button find=ui.button("Find Pepper on Wi-Fi",false);root.addView(find,2,ui.buttonSpace());
        find.setOnClickListener(v->{find.setEnabled(false);status.setText("Searching for TelePepper on this network...");new Thread(()->{
            try{java.util.List<PepperDiscovery.Robot> robots=PepperDiscovery.find(ConnectionStore.knownPairings(prefs));runOnUiThread(()->{if(isFinishing())return;find.setEnabled(true);
                if(robots.isEmpty()){status.setText("No TelePepper found. Start the robot service and check Wi-Fi. You can also enter its head address.");return;}
                if(robots.size()==1){host.setText(robots.get(0).host);token.setText(ConnectionStore.pairingFor(prefs,robots.get(0).host));status.setText(token.getText().length()>=12?"Pepper found. Saved pairing restored. Check connection.":"Pepper found. Enter the pairing code shown on her tablet.");return;}
                String[] names=new String[robots.size()];for(int i=0;i<names.length;i++)names[i]=robots.get(i).name+"  /  "+robots.get(i).host;
                new android.app.AlertDialog.Builder(this).setTitle("Choose Pepper").setItems(names,(d,i)->{host.setText(robots.get(i).host);token.setText(ConnectionStore.pairingFor(prefs,robots.get(i).host));status.setText(token.getText().length()>=12?"Pepper selected. Saved pairing restored. Check connection.":"Pepper selected. Enter the pairing code shown on her tablet.");}).show();
            });}catch(Exception e){runOnUiThread(()->{if(!isFinishing()){find.setEnabled(true);status.setText("Network search unavailable. Enter the robot head address.");}});}
        },"pepper-discovery").start();});
        Button test=ui.button("Check connection",false);card.addView(test,ui.buttonSpace());
        test.setOnClickListener(v->{String h=host.getText().toString().trim(),t=token.getText().toString().trim();
            if(!ConnectionStore.validHost(h)||t.length()<12){status.setText("Check address and pairing code.");return;}
            test.setEnabled(false);status.setText("Checking...");new Thread(()->{
                String message;
                try(Socket s=new Socket()){s.connect(new InetSocketAddress(h,9570),1500);s.setSoTimeout(2000);
                    s.getOutputStream().write((new JSONObject().put("token",t).put("role","operator").toString()+"\n").getBytes("UTF-8"));
                    String line=new BufferedReader(new InputStreamReader(s.getInputStream(),"UTF-8")).readLine();
                    JSONObject reply=new JSONObject(line);message=reply.has("observer")?"Pepper connected. Pairing confirmed.":"Pairing refused: check the code.";
                }catch(Exception e){message="Pepper unreachable. Check Wi-Fi, address and robot service.";}
                final String result=message;runOnUiThread(()->{if(!isFinishing()){status.setText(result);test.setEnabled(true);}});
            },"connection-check").start();
        });
        Button enter=ui.button("Save and open VR studio",true);
        enter.setOnClickListener(v->{String h=host.getText().toString().trim(),t=token.getText().toString().trim();
            if(!ConnectionStore.validHost(h)||t.length()<12){status.setText("Enter a valid IPv4 address and a pairing code of at least 12 characters.");return;}
            ConnectionStore.save(prefs,h,t);HeadsetNavigation.openStudio(this);
        });
        root.addView(label("Hold A + X: Start / Pause     Y: Controls     B: STOP\nLeft stick: translate   |   Right stick: turn\nTriggers: hands   |   Left grip: talk",16,muted));
        ConnectionForm.show(this,ui,root,scroll,enter);if(host.getText().toString().isEmpty())find.performClick();if(getIntent().getBooleanExtra("enter_vr",false))enter.performClick();
    }
    @Override protected void onDestroy(){if(voiceProbe!=null)voiceProbe.close();super.onDestroy();}
}
