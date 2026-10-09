package it.telepepper.pepper;
import it.telepepper.ui.StudioStyle;
import android.app.Activity;
import android.os.Bundle;
import android.graphics.Color;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.text.StaticLayout;
import android.text.Layout;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ConcurrentLinkedQueue;
import org.json.*;

public class ParticipantActivity extends Activity {
    private StudioStyle ui;
    private volatile boolean running=true;
    private final ConcurrentLinkedQueue<JSONObject> outgoing=new ConcurrentLinkedQueue<>();
    private LinearLayout layout;private ScrollView participantScroll;private StatusIcons statusIcons;private volatile int revision=-1;
    private volatile Socket activeSocket;
    private Runnable fadeContent;
    @Override public void onCreate(Bundle state){StudioStyle.applyTheme(this);super.onCreate(state);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        ui=new StudioStyle(this);
        FrameLayout root=new FrameLayout(this);root.setBackgroundColor(ui.background);
        participantScroll=new ScrollView(this);participantScroll.setFillViewport(true);root.addView(participantScroll,new FrameLayout.LayoutParams(-1,-1));
        layout=new LinearLayout(this);layout.setOrientation(LinearLayout.VERTICAL);layout.setGravity(Gravity.CENTER);layout.setPadding(dp(36),dp(36),dp(36),dp(36));participantScroll.addView(layout,new ScrollView.LayoutParams(-1,-2));
        statusIcons=new StatusIcons();FrameLayout.LayoutParams indicators=new FrameLayout.LayoutParams(dp(68),dp(36),Gravity.TOP|Gravity.RIGHT);indicators.setMargins(dp(12),dp(12),dp(12),0);root.addView(statusIcons,indicators);statusIcons.setOnLongClickListener(v->{openSettings();return true;});
        ImageButton settings=new ImageButton(this);settings.setImageResource(android.R.drawable.ic_menu_preferences);settings.setColorFilter(ui.ink);settings.setContentDescription("Connection settings");
        GradientDrawable settingsBackground=new GradientDrawable();settingsBackground.setColor(ui.surfaceColor);settingsBackground.setCornerRadius(dp(18));settings.setBackground(settingsBackground);settings.setPadding(dp(10),dp(10),dp(10),dp(10));
        FrameLayout.LayoutParams settingsPosition=new FrameLayout.LayoutParams(dp(40),dp(40),Gravity.BOTTOM|Gravity.RIGHT);settingsPosition.setMargins(0,0,dp(14),dp(14));root.addView(settings,settingsPosition);settings.setOnClickListener(v->openSettings());
        setContentView(root);new Thread(this::poll).start();}
    private int dp(int value){return Math.round(value*getResources().getDisplayMetrics().density);}
    private class StatusIcons extends View {
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);private boolean connected,active;
        StatusIcons(){super(ParticipantActivity.this);setContentDescription("TelePepper connecting. Hold for settings.");}
        void update(boolean linked,boolean moving){connected=linked;active=moving;setContentDescription("TelePepper "+(linked?"connected":"reconnecting")+", motion "+(moving?"active":"paused")+". Hold for settings.");invalidate();}
        @Override protected void onDraw(Canvas canvas){super.onDraw(canvas);canvas.save();canvas.scale(getWidth()/68f,getHeight()/36f);paint.setStrokeWidth(2);paint.setStyle(Paint.Style.STROKE);paint.setColor(connected?Color.rgb(75,210,160):Color.rgb(220,155,80));
            canvas.drawRoundRect(7,12,23,22,5,5,paint);canvas.drawRoundRect(17,12,33,22,5,5,paint);if(!connected)canvas.drawLine(10,8,30,27,paint);
            paint.setStyle(Paint.Style.FILL);paint.setColor(active?Color.rgb(75,210,160):StudioStyle.MUTED);if(active){android.graphics.Path play=new android.graphics.Path();play.moveTo(48,10);play.lineTo(48,26);play.lineTo(60,18);play.close();canvas.drawPath(play,paint);}else{canvas.drawRect(47,11,51,25,paint);canvas.drawRect(56,11,60,25,paint);}canvas.restore();}
    }
    // Draw classic reactions directly: the tablet's older emoji font can render
    // large colour glyphs as an empty texture, even when accessibility sees them.
    private class ReactionFace extends View {
        private final Paint brush=new Paint(Paint.ANTI_ALIAS_FLAG);private final String reaction;
        ReactionFace(String name){super(ParticipantActivity.this);reaction=name;setContentDescription(name+" reaction");}
        private void fill(int color){brush.setColor(color);brush.setStyle(Paint.Style.FILL);}
        private void stroke(int color,float width){brush.setColor(color);brush.setStyle(Paint.Style.STROKE);brush.setStrokeWidth(width);brush.setStrokeCap(Paint.Cap.ROUND);}
        private void curve(Canvas c,float x,float y,float mx,float my,float ex,float ey){android.graphics.Path p=new android.graphics.Path();p.moveTo(x,y);p.quadTo(mx,my,ex,ey);c.drawPath(p,brush);}
        private void heart(Canvas c,float x,float y){android.graphics.Path p=new android.graphics.Path();p.moveTo(x,y+30);p.cubicTo(x-70,y-8,x-30,y-57,x,y-24);p.cubicTo(x+30,y-57,x+70,y-8,x,y+30);p.close();c.drawPath(p,brush);}
        private void tear(Canvas c,float x,float y){android.graphics.Path p=new android.graphics.Path();p.moveTo(x,y);p.cubicTo(x-35,y+32,x-22,y+65,x,y+65);p.cubicTo(x+22,y+65,x+35,y+32,x,y);p.close();c.drawPath(p,brush);}
        @Override protected void onDraw(Canvas c){super.onDraw(c);float scale=Math.min(getWidth(),getHeight())/400f;c.save();c.translate((getWidth()-400*scale)/2,(getHeight()-400*scale)/2);c.scale(scale,scale);
            int ink=Color.rgb(71,43,23);fill(Color.rgb(223,148,28));c.drawCircle(200,206,164,brush);fill(reaction.equals("angry")?Color.rgb(250,119,65):Color.rgb(255,206,61));c.drawCircle(200,200,160,brush);
            if(reaction.equals("love")){fill(Color.rgb(241,59,83));heart(c,130,155);heart(c,270,155);}
            else if(reaction.equals("laugh")){stroke(ink,12);curve(c,104,157,130,124,156,157);curve(c,244,157,270,124,296,157);fill(Color.rgb(61,165,242));tear(c,83,168);tear(c,317,168);}
            else{fill(ink);c.drawOval(118,131,142,173,brush);if(reaction.equals("wink")){stroke(ink,12);curve(c,244,154,270,137,296,154);}else c.drawOval(258,131,282,173,brush);}
            if(reaction.equals("angry")){stroke(ink,12);curve(c,101,105,126,119,153,127);curve(c,247,127,274,119,299,105);curve(c,145,284,200,253,255,284);}
            else if(reaction.equals("surprise")){fill(ink);c.drawOval(169,222,231,306,brush);}
            else if(reaction.equals("sad")){stroke(ink,12);curve(c,145,282,200,242,255,282);curve(c,105,113,126,117,149,103);curve(c,251,103,274,117,295,113);fill(Color.rgb(61,165,242));tear(c,280,176);}
            else{android.graphics.Path mouth=new android.graphics.Path();mouth.moveTo(111,223);mouth.quadTo(200,259,289,223);mouth.quadTo(200,370,111,223);mouth.close();fill(ink);c.drawPath(mouth,brush);c.save();c.clipPath(mouth);fill(Color.WHITE);c.drawRoundRect(114,222,286,248,5,5,brush);fill(Color.rgb(244,96,111));c.drawOval(160,285,240,331,brush);c.restore();}
            c.restore();}
    }
    private void write(Socket socket,JSONObject object)throws Exception{socket.getOutputStream().write((object.toString()+"\n").getBytes(StandardCharsets.UTF_8));}
    private void poll(){while(running){try(Socket socket=new Socket()){activeSocket=socket;socket.connect(new InetSocketAddress(getIntent().getStringExtra("host"),9570),2000);socket.setSoTimeout(3000);BufferedReader reader=new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.UTF_8));JSONObject hello=new JSONObject();hello.put("token",getIntent().getStringExtra("token"));hello.put("role","operator");write(socket,hello);JSONObject ack=new JSONObject(reader.readLine());if(!ack.has("observer"))throw new IOException("Pairing failed");
            while(running){JSONObject request=outgoing.poll();if(request==null){request=new JSONObject();request.put("cmd","tablet_poll");request.put("displayed_revision",revision);}write(socket,request);JSONObject result=new JSONObject(reader.readLine());runOnUiThread(()->updateStatus(result));JSONObject content=result.optJSONObject("tablet");if(content!=null&&content.optInt("revision")!=revision){final int rev=content.optInt("revision");runOnUiThread(()->display(content,rev));}Thread.sleep(300);}
        }catch(Exception e){revision=-1;runOnUiThread(()->{if(isFinishing())return;statusIcons.update(false,false);cancelContentFade();layout.removeAllViews();});try{Thread.sleep(1000);}catch(InterruptedException ignored){}}}}
    private void updateStatus(JSONObject result){if(isFinishing())return;statusIcons.update(true,result.optBoolean("armed",false));}
    @SuppressWarnings("deprecation")
    private void fitMessage(TextView message,int choiceCount){
        if(message.getParent()==null||isFinishing())return;
        int width=Math.max(dp(100),participantScroll.getWidth()-layout.getPaddingLeft()-layout.getPaddingRight());
        int room=Math.max(dp(120),participantScroll.getHeight()-layout.getPaddingTop()-layout.getPaddingBottom()-choiceCount*dp(80));
        float size=64;
        while(size>36){message.setTextSize(size);StaticLayout lines=new StaticLayout(message.getText(),message.getPaint(),width,Layout.Alignment.ALIGN_CENTER,1.1f,0,false);if(lines.getHeight()<=room)break;size-=2;}
        message.setTextSize(size);
    }
    private void display(JSONObject content,int rev){
        if(isFinishing())return;cancelContentFade();layout.removeAllViews();participantScroll.scrollTo(0,0);
        String reaction=content.optString("reaction","");
        String[] names={"smile","laugh","love","surprise","sad","wink","angry"};
        for(int i=0;i<names.length;i++)if(names[i].equals(reaction)){
            ReactionFace face=new ReactionFace(reaction);layout.addView(face,new LinearLayout.LayoutParams(-1,dp(440)));face.setOnLongClickListener(v->{openSettings();return true;});revision=rev;scheduleContentFade(content,rev);return;
        }
        TextView message=new TextView(this);String text=content.optString("text");JSONArray choices=content.optJSONArray("choices");
        message.setText(text);message.setTextColor(ui.ink);message.setTextSize(64);message.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));message.setGravity(Gravity.CENTER);message.setLineSpacing(0,1.1f);message.setPadding(0,0,0,dp(16));
        if(!text.trim().isEmpty())layout.addView(message,new LinearLayout.LayoutParams(-1,-2));
        message.setOnLongClickListener(v->{openSettings();return true;});revision=rev;
        int choiceCount=choices==null?0:choices.length();
        for(int i=0;i<choiceCount;i++){
            final int index=i;Button b=new StudioStyle(this).button(choices.optString(i),false);b.setTextSize(32);b.setAllCaps(false);b.setTextColor(ui.ink);b.setMinHeight(dp(64));
            LinearLayout.LayoutParams params=new LinearLayout.LayoutParams(-1,-2);params.topMargin=dp(8);params.bottomMargin=dp(8);layout.addView(b,params);
            b.setOnClickListener(v->{try{JSONObject response=new JSONObject();response.put("cmd","participant_response");response.put("revision",rev);response.put("choice",index);outgoing.add(response);for(int j=0;j<layout.getChildCount();j++)if(layout.getChildAt(j) instanceof Button)layout.getChildAt(j).setEnabled(false);}catch(JSONException ignored){}});
        }
        participantScroll.post(()->{if(revision==rev)fitMessage(message,choiceCount);});
        scheduleContentFade(content,rev);
    }
    private void cancelContentFade(){
        if(fadeContent!=null)layout.removeCallbacks(fadeContent);
        fadeContent=null;layout.animate().cancel();layout.setAlpha(1f);
    }
    private void scheduleContentFade(JSONObject content,int rev){
        if(layout.getChildCount()==0)return;
        float opacity=(float)Math.max(0,Math.min(1,content.optDouble("opacity",1)));
        layout.setAlpha(opacity);
        long delay=Math.max(0,Math.min(8000,content.optLong("fade_after_ms",8000)));
        long duration=Math.max(1,Math.round(content.optLong("fade_ms",1000)*opacity));
        fadeContent=()->{
            if(isFinishing()||revision!=rev)return;
            layout.animate().alpha(0f).setDuration(duration).setInterpolator(new android.view.animation.LinearInterpolator()).withEndAction(()->{
                if(revision==rev)layout.removeAllViews();
            }).start();
        };
        layout.postDelayed(fadeContent,delay);
    }
    private void openSettings(){startActivity(new android.content.Intent(this,MainActivity.class).putExtra("settings",true));finish();}
    @Override public void onBackPressed(){openSettings();}
    @Override protected void onDestroy(){running=false;cancelContentFade();try{if(activeSocket!=null)activeSocket.close();}catch(IOException ignored){}super.onDestroy();}
}
