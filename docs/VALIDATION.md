# Validation - Pepper 2.5 trial build

## Current review: 0.3.5-test (2026-10-09)

The current review passed 173 Python tests for Pepper 2.9 and 184 for Pepper 2.5. Seventeen standalone native tests ran on Quest (math, panel interaction, controller chords, watchdog/start flow and loopback networking), plus the Java speech-queue and WAV tests on the PC. All three APKs built successfully. These tests do not command the real robot or measure physical latency.

Both variants contain STOP cancellation fault handling, Piper cleanup and STOP-aware normal-mode handoff fixes. Deployment preserves literal numeric base settings, validates the merged settings before replacement and stages/compiles scripts with backups. Pepper 2.9 exposes Update service on its tablet; Pepper 2.5 updates only after the current service is exited. The 2.5 supervisor confirms base cleanup before restarting a failed child; its loopback simulated Exit completed without restart.

The 0.3.5 builds have not been installed on the lab robot during this review. Physical START/STOP timing, speech gesture execution, head/base independence during gestures, pose alignment and sustained Wi-Fi stability still require a supervised trial. A real Pepper 2.5 trial remains outstanding. Sub-40 ms physical motion latency is not established. True backdrivability/compliant manual arm guidance is not implemented: engaged tracking sets joint stiffness to 1.0.

Source, APK contents, release archives and all published Git history are checked for known local credentials and identifiers before publication. The remaining sections are historical test records and may refer to earlier versions.

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

## 0.3.5 review fixes

Animation cancellation failures no longer skip base STOP. Piper temporary files and clip locks are cleaned even if gesture cancellation fails. A newer STOP during normal-mode handoff cancels Exit and requests disabled autonomy again. These paths have fault-injection tests; physical acknowledgement timing still needs operator verification.
