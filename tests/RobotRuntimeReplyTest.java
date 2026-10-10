package it.telepepper.quest;

import java.io.IOException;

public final class RobotRuntimeReplyTest {
    private interface Call { void run() throws IOException; }
    private static void equal(String actual, String expected) {
        if (!actual.equals(expected)) throw new AssertionError(actual);
    }
    private static void rejects(Call call, String message) throws IOException {
        try { call.run(); } catch (IOException expected) {
            if (!expected.getMessage().contains(message)) throw new AssertionError(expected);
            return;
        }
        throw new AssertionError("Expected failure: " + message);
    }
    public static void main(String[] args) throws Exception {
        String warning = "[W] 123 qi.path.sdklayout: No Application was created, trying to deduce paths\n";
        equal(RobotRuntimeReply.pepper25Version(warning + "2.5.10.7\n"), "2.5.10.7");
        equal(RobotRuntimeReply.pepper25Version(warning + "TELEPEPPER_NAOQI=2.5.10.7\r\n"), "2.5.10.7");
        equal(RobotRuntimeReply.pepper25Version("  TELEPEPPER_NAOQI=2.5.11.14  \n" + warning), "2.5.11.14");
        rejects(() -> RobotRuntimeReply.pepper25Version(warning + "TELEPEPPER_NAOQI=2.9.5.172\n"), "requires Pepper NAOqi 2.5; found 2.9.5.172");
        rejects(() -> RobotRuntimeReply.pepper25Version("TELEPEPPER_NAOQI=2.50.10.7"), "requires Pepper NAOqi 2.5");
        rejects(() -> RobotRuntimeReply.pepper25Version(warning + "cannot connect, expected 2.5.10.7"), "did not return");
        rejects(() -> RobotRuntimeReply.pepper25Version("TELEPEPPER_NAOQI=unknown\n2.5.10.7"), "invalid");
        rejects(() -> RobotRuntimeReply.pepper25Version("2.5.10.7\nTELEPEPPER_NAOQI=2.9.5.172"), "conflicting");
        equal(RobotRuntimeReply.jsonObject(warning + "{\"running\":true,\"version\":\"2.5.10.7\"}\n"),
                "{\"running\":true,\"version\":\"2.5.10.7\"}");
        rejects(() -> RobotRuntimeReply.jsonObject(warning), "did not return");
        rejects(() -> RobotRuntimeReply.jsonObject("{\"running\":false}\n{\"running\":true}"), "multiple");
        System.out.println("PASS 11 cases: video warning regression, tagged replies, incompatible versions, missing/ambiguous payloads and service JSON");
    }
}
