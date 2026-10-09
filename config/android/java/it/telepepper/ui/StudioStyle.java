package it.telepepper.ui;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.text.InputType;
import android.view.Gravity;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

/** Shared Quest settings / Pepper tablet components. Colours match dashboard_theme.h. */
public final class StudioStyle {
    public static final int BACKGROUND=Color.rgb(26,26,26), INK=Color.rgb(235,235,235);
    public static final int MUTED=Color.rgb(180,180,180), ACCENT=Color.rgb(0,100,224);
    public static final int SURFACE=Color.rgb(51,51,51), BORDER=Color.rgb(84,87,94);
    public static final int TINT=Color.rgb(65,67,72), RED=Color.rgb(191,31,41);
    public static final int CONNECTION=Color.rgb(82,186,232), TABLET=Color.rgb(71,201,184), MOTION=Color.rgb(59,163,255);
    public final int background,ink,muted,surfaceColor,border,tint;
    private final Context context;
    public static void applyTheme(android.app.Activity activity){int id=activity.getResources().getIdentifier("TelePepperStudio","style",activity.getPackageName());if(id!=0)activity.setTheme(id);}
    public StudioStyle(Context value){
        context=value;background=BACKGROUND;surfaceColor=SURFACE;ink=INK;muted=MUTED;border=BORDER;tint=TINT;
    }
    public int dp(int value){return Math.round(value*context.getResources().getDisplayMetrics().density);}
    public GradientDrawable surface(int color,int radius){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;}
    public void window(android.app.Activity activity){activity.getWindow().setStatusBarColor(background);activity.getWindow().setNavigationBarColor(background);activity.getWindow().getDecorView().setSystemUiVisibility(0);}
    public LinearLayout column(){LinearLayout l=new LinearLayout(context);l.setOrientation(LinearLayout.VERTICAL);return l;}
    public LinearLayout row(){LinearLayout l=new LinearLayout(context);l.setOrientation(LinearLayout.HORIZONTAL);return l;}
    public LinearLayout card(){LinearLayout l=column();l.setPadding(dp(22),dp(20),dp(22),dp(20));GradientDrawable bg=surface(surfaceColor,24);bg.setStroke(dp(1),border);l.setBackground(bg);return l;}
    public TextView text(String value,int size,int color,boolean strong){TextView t=new TextView(context);t.setText(value);t.setTextSize(size);if(color==ACCENT||color==CONNECTION||color==TABLET||color==MOTION)color=ink;t.setTextColor(color);t.setTypeface(Typeface.create(strong?"sans-serif-medium":"sans-serif",Typeface.NORMAL));t.setFontFeatureSettings("kern");return t;}
    public Button button(String label,boolean primary){return coloredButton(label,primary?ACCENT:tint,primary?Color.WHITE:ink);}
    public Button stopButton(String label){return coloredButton(label,RED,Color.WHITE);}
    private Button coloredButton(String label,int background,int foreground){Button b=new Button(context);b.setText(label);b.setTextSize(16);b.setTextColor(new ColorStateList(new int[][]{new int[]{-android.R.attr.state_enabled},new int[]{}},new int[]{muted,foreground}));b.setAllCaps(false);b.setMinHeight(dp(48));b.setMinimumHeight(dp(48));b.setStateListAnimator(null);b.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));b.setGravity(Gravity.CENTER);b.setPadding(dp(14),dp(8),dp(14),dp(8));b.setBackground(new RippleDrawable(ColorStateList.valueOf(Color.argb(55,51,140,255)),surface(background,24),null));return b;}
    public LinearLayout.LayoutParams buttonSpace(){LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,dp(54));p.setMargins(0,dp(14),0,0);return p;}
    public EditText field(LinearLayout parent,String title,String value,boolean secret){TextView label=text(title,14,muted,true);LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.setMargins(0,dp(16),0,dp(6));parent.addView(label,lp);EditText e=new EditText(context);e.setId(0x03000000|(title.hashCode()&0x00ffffff));e.setSingleLine(true);e.setText(value);e.setTextSize(19);e.setTextColor(ink);e.setHintTextColor(muted);e.setPadding(dp(14),dp(10),dp(14),dp(10));e.setInputType(InputType.TYPE_CLASS_TEXT|(secret?InputType.TYPE_TEXT_VARIATION_PASSWORD:InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS));GradientDrawable bg=surface(background,12);bg.setStroke(dp(1),border);e.setBackground(bg);e.setHighlightColor(ACCENT);e.setTypeface(Typeface.create("sans-serif",Typeface.NORMAL));parent.addView(e,new LinearLayout.LayoutParams(-1,dp(52)));return e;}
}
