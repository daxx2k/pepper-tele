# Validation - Pepper 2.5 trial build

Physical operation on NAOqi 2.5 is not yet verified. This is a supervised trial build.

- Separate sanitized fork; distinct Quest application ID; Pepper tablet module/UI removed.
- Version guard tested with mocks and the actual 2.9 lab robot: rejects 2.9 before installation or movement.
- All bridge scripts compiled using Python 2.7 on the lab robot.
- Launcher and bounded supervisor tested end-to-end with --simulate in a temporary folder. Authenticated Exit stopped child and supervisor without restart. The real service/robot mode were unchanged.
- 148 Python checks passed. Native dashboard/floating-layout geometry tests passed on Quest without robot control or OpenXR tracking.
- APK builds with bundled head scripts, verified-host SSH, password/PAM authentication, private pairing and encrypted credentials. Real first setup on 2.5 still needs the friend's trial.

Verify calibration, gentle engagement, B/STOP, head/arms/hands, base release, tracking/Wi-Fi loss and explicit Exit before other people are present. Check optional cameras/audio/LEDs/sensors and confirmed animations individually.

## PTT gestures and tablet fix, 2026-10-08

Both-grip toggle no longer requires presses within 0.2 seconds. Left-grip release requests a bounded gesture only with the mode enabled, fresh armed tracking and completed initial engagement. Voice playback continues to control gesture duration for TTS/Piper. Live capture does not initiate gestures. The release command never arms the robot.
167 Python tests passed. The portable player uses official arm-only keyframe derivatives and waits for acknowledgement of cancellation of only its arm resources before returning to measured-pose tracking. One timing-sensitive camera polling test failed on the first run and passed on recheck; camera code was unchanged. The 2.5 APK builds; physical Pepper 2.5 remains untested.

Final release check: 169 Python tests passed. Both variant builds completed; 2.9 apps installed. Physical 2.5 compatibility and speech gesture performance are not established by the mock tests.

The camera cadence test now isolates its fake clock/sleep from background test threads; production camera code was unchanged. Final full suites pass: 161 for 2.9 and 169 for 2.5.
