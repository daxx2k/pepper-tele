package it.telepepper.quest;

import android.content.SharedPreferences;
import java.lang.reflect.Proxy;
import java.util.HashMap;
import java.util.Map;

public final class ConnectionStoreTest {
    private static void equal(String actual,String expected){if(!actual.equals(expected))throw new AssertionError(actual);}
    public static void main(String[] args){
        Map<String,String> values=new HashMap<>();
        SharedPreferences.Editor editor=(SharedPreferences.Editor)Proxy.newProxyInstance(
                SharedPreferences.Editor.class.getClassLoader(),new Class[]{SharedPreferences.Editor.class},(proxy,method,arguments)->{
                    if(method.getName().equals("putString")){values.put((String)arguments[0],(String)arguments[1]);return proxy;}
                    if(method.getName().equals("apply"))return null;
                    throw new AssertionError(method.getName());
                });
        SharedPreferences prefs=(SharedPreferences)Proxy.newProxyInstance(
                SharedPreferences.class.getClassLoader(),new Class[]{SharedPreferences.class},(proxy,method,arguments)->{
                    if(method.getName().equals("getString"))return values.getOrDefault((String)arguments[0],(String)arguments[1]);
                    if(method.getName().equals("edit"))return editor;
                    if(method.getName().equals("getAll"))return new HashMap<>(values);
                    throw new AssertionError(method.getName());
                });
        String first="198.51.100.11",second="198.51.100.12";
        values.put("host",first);values.put("token","test-pairing-first");
        equal(ConnectionStore.pairingFor(prefs,first),"test-pairing-first");
        equal(ConnectionStore.pairingFor(prefs,second),"");
        ConnectionStore.save(prefs,second,"test-pairing-second");
        equal(ConnectionStore.pairingFor(prefs,first),"test-pairing-first");
        equal(ConnectionStore.pairingFor(prefs,second),"test-pairing-second");
        equal(ConnectionStore.pairingFor(prefs,"198.51.100.13"),"");
        values.put("token","test-rotated-pairing");
        equal(ConnectionStore.pairingFor(prefs,second),"test-rotated-pairing");
        equal(ConnectionStore.knownPairings(prefs).get(second),"test-rotated-pairing");
        equal(ConnectionStore.knownPairings(prefs).get(first),"test-pairing-first");
        System.out.println("PASS saved pairing recovery, legacy migration, multiple endpoints and unknown-endpoint isolation");
    }
}
