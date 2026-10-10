package it.telepepper.quest;

import java.io.IOException;

/** Parse protocol replies without mistaking qi startup diagnostics for payload. */
final class RobotRuntimeReply {
    private static final String VERSION_TAG = "TELEPEPPER_NAOQI=";
    private RobotRuntimeReply() {}

    static String pepper25Version(String output) throws IOException {
        String version = naoqiVersion(output);
        if (!version.startsWith("2.5."))
            throw new IOException("This app requires Pepper NAOqi 2.5; found " + version);
        return version;
    }

    static String naoqiVersion(String output) throws IOException {
        String version = null;
        for (String line : output.split("\\r?\\n")) {
            String value = line.trim();
            boolean tagged = value.startsWith(VERSION_TAG);
            if (tagged) value = value.substring(VERSION_TAG.length()).trim();
            if (!value.matches("\\d{1,3}(?:\\.\\d{1,9}){2,5}")) {
                if (tagged) throw new IOException("Pepper returned an invalid NAOqi version.");
                continue;
            }
            if (version != null && !version.equals(value))
                throw new IOException("Pepper returned conflicting NAOqi versions.");
            version = value;
        }
        if (version == null) throw new IOException("Pepper did not return its NAOqi version.");
        return version;
    }

    static String jsonObject(String output) throws IOException {
        String payload = null;
        for (String line : output.split("\\r?\\n")) {
            String value = line.trim();
            if (!value.startsWith("{") || !value.endsWith("}")) continue;
            if (payload != null) throw new IOException("Pepper returned multiple service replies.");
            payload = value;
        }
        if (payload == null) throw new IOException("Pepper did not return a service reply.");
        return payload;
    }
}
