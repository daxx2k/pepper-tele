package it.telepepper.quest;

import android.content.Context;
import android.content.SharedPreferences;
import java.io.*;
import org.json.JSONObject;

final class ConnectionStore {
    static SharedPreferences load(Context context) {
        SharedPreferences prefs=context.getSharedPreferences("connection",Context.MODE_PRIVATE);
        // Deployment bootstrap is imported once. Later edits belong to the user.
        try(FileInputStream in=context.openFileInput("bootstrap.json")) {
            ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] b=new byte[1024];int n;
            while((n=in.read(b))!=-1)out.write(b,0,n);
            JSONObject value=new JSONObject(out.toString("UTF-8"));
            String host=value.getString("host"),token=value.getString("token");
            if(validHost(host)&&token.length()>=12&&prefs.edit().putString("host",host).putString("token",token).commit())context.deleteFile("bootstrap.json");
        }catch(Exception ignored){}
        return prefs;
    }
    static boolean validHost(String host){
        String[] parts=host.split("\\.",-1);if(parts.length!=4)return false;
        for(String p:parts)try{if(!p.matches("[0-9]{1,3}")||Integer.parseInt(p)>255)return false;}catch(Exception e){return false;}
        return true;
    }
}
