package it.telepepper.ui;
import android.content.Context;
import android.content.pm.PackageInfo;
public final class VersionInfo {
    private VersionInfo(){}
    public static String label(Context context){
        try{PackageInfo info=context.getPackageManager().getPackageInfo(context.getPackageName(),0);
            return info.versionName+" ("+info.versionCode+")";
        }catch(Exception e){return "unknown";}
    }
}
