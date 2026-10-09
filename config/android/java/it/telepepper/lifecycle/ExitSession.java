package it.telepepper.lifecycle;

import android.app.Activity;
import android.app.AlertDialog;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import org.json.JSONObject;

/** A deliberate close is the only client action that resumes autonomous behavior. */
public final class ExitSession {
    private ExitSession() {}
    public static void close(Activity activity, String host, String token) {
        new Thread(() -> {
            try (Socket socket = new Socket()) {
                socket.connect(new InetSocketAddress(host, 9570), 2500);
                socket.setSoTimeout(20000);
                BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
                OutputStream out = socket.getOutputStream();
                out.write((new JSONObject().put("token", token).put("role", "operator").toString()+"\n").getBytes(StandardCharsets.UTF_8));
                String hello = in.readLine();
                if (hello == null || !new JSONObject(hello).has("observer")) throw new IOException("Connection refused");
                out.write((new JSONObject().put("cmd", "return_to_normal").put("confirmed", true).toString()+"\n").getBytes(StandardCharsets.UTF_8));
                String line = in.readLine();
                if (line == null) throw new IOException("Exit was not confirmed");
                JSONObject reply = new JSONObject(line);
                if (!reply.optBoolean("ok") || !reply.optBoolean("normal_mode"))
                    throw new IOException(reply.optString("error", "Normal mode was not confirmed"));
                activity.runOnUiThread(activity::finishAndRemoveTask);
            } catch (Exception error) {
                activity.runOnUiThread(() -> {
                    if (!activity.isFinishing()) new AlertDialog.Builder(activity)
                        .setTitle("Exit not confirmed")
                        .setMessage("Pepper could not confirm normal mode. "+error.getMessage())
                        .setPositiveButton("OK", null).show();
                });
            }
        }, "TelePepper-exit").start();
    }
}
