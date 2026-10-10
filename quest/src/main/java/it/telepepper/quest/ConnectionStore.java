package it.telepepper.quest;

import android.content.Context;
import android.content.SharedPreferences;
import java.io.*;
import java.util.*;
import org.json.JSONObject;

final class ConnectionStore {
    static String pairingFor(SharedPreferences prefs,String host) {
        String current=prefs.getString("token","");
        if(host.equals(prefs.getString("host",""))&&current.length()>=12)return current;
        String saved=prefs.getString("known_pairing_"+host,"");
        if(saved.length()>=12)return saved;
        return host.equals(prefs.getString("host",""))?prefs.getString("token",""):"";
    }
    static Map<String,String> knownPairings(SharedPreferences prefs) {
        Map<String,String> result=new LinkedHashMap<>();
        String current=prefs.getString("host",""),token=pairingFor(prefs,current);
        if(validHost(current)&&token.length()>=12)result.put(current,token);
        for(String key:new TreeSet<>(prefs.getAll().keySet()))if(key.startsWith("known_pairing_")){
            String host=key.substring("known_pairing_".length()),saved=pairingFor(prefs,host);
            if(validHost(host)&&saved.length()>=12)result.put(host,saved);
        }
        return result;
    }
    static void save(SharedPreferences prefs,String host,String token) {
        SharedPreferences.Editor edit=prefs.edit();
        String previous=prefs.getString("host",""),old=prefs.getString("token","");
        if(validHost(previous)&&old.length()>=12)edit.putString("known_pairing_"+previous,old);
        edit.putString("host",host).putString("token",token).putString("known_pairing_"+host,token).apply();
    }
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
