# Validation - Pepper 2.5 trial build

## 0.3.11: connection setup and discovery (2026-10-10)

Both variants build successfully. Full Python suites pass (186 tests for 2.9, 188 for 2.5). Fourteen host C++ Exit flow cases pass. JVM tests verify endpoint-specific pairing, rotated current codes, UDP validation/deduplication, authenticated saved-endpoint fallback and rejection of invalid authentication. The production discovery code found the paired robot in a read-only PC network check.

The final 2.9 Quest APK (0.3.11-test) was installed over the existing app and its connection settings remained byte-identical; the manifest label is TelePepper. The 2.9 tablet update preserves its four private settings files byte-for-byte. An external app_process probe aborted on Quest before running discovery, so it does not establish headset discovery success. Quest setup uses compact scrollable fields and a sticky save/open action. Selecting a saved 2.5 endpoint also restores that endpoint's encrypted SSH credentials. No ARM, calibration or physical movement was initiated for this update. A fresh operator trial of headset layout, discovery, connected Exit restoration and Pepper 2.5 remains required; build and mock checks do not establish those physical behaviours.

## Operator confirmation: 0.3.8 (2026-10-09)

After START and Gestures ON, the operator confirmed that releasing push-to-talk plays arm gestures and tracking remains active on the Pepper 2.9 setup. This is physical operator confirmation of that specific regression fix, in addition to the API probes and automated checks below. Independent head/base movement during gestures, long-session stability, physical latency and Pepper 2.5 compatibility were not separately confirmed by this trial. Earlier pending-trial statements below describe the state before this confirmation.


## 0.3.8: native arm speech playback and qianim integer metadata

The real 2.9 robot still stopped on PTT release with `conversion of data to type "i" failed`. Pointing resources used Qt metadata such as mute=false and frame=19.0. Packaged manual resources now canonicalize integer flags/FPS/integral frames without changing actuator values, timestamps or tangent values; original source clips are retained. Fractional author keyframes are rejected rather than silently rounded.

Automatic speech gestures on 2.9 now use the same native ALMotion arm-only curve player used by the 2.5 variant. They no longer depend on ALAnimationPlayer parsing these speech clips. Cancellation uses only the twelve arm/hand resources and waits for acknowledgement before the measured-pose return. The official derivative curve data and gentle initial interval are retained.

On the actual 2.9 robot, the new player completed a zero-displacement interpolation at the measured arm pose and acknowledged cancellation. No expressive gesture or head/torso/wheel motion was commanded during that API probe. The physical PTT gesture and continued head/base control still require an operator trial.

186 Python tests passed for 2.9 and 188 for 2.5, including native arm ownership/cancellation and integer-metadata preservation checks. All three APKs built. The 2.5 native speech player is unchanged. Prior 0.3.6/0.3.7 repair claims were superseded by observed failures.


## 0.3.7: real-player PTT format investigation

The 0.3.6 XML forwarding repair was incorrect: ALAnimationPlayer.run accepts a registered package/path, not inline XML. On the actual 2.9 player, a temporary package with two empty (no-actuator) resources reproduced the original error for a bare Animation element; adding the XML declaration advanced to the expected null-duration rejection. No physical animation was played by this probe. The package is removed after diagnosis.

Speech package 1.0.1 now contains XML declarations on every qianim, and the player again receives package/path. Startup validation rejects malformed bundled headers. Public STOP reasons are bounded to 512 characters because SDK errors can contain entire XML clips and overflow status replies. These changes preserve native motion watchdogs and cancellation barriers.

Python suites passed 180 tests for 2.9 and 185 for 2.5; all three APKs build. Physical PTT gestures still require the operator's next trial. The 2.5 speech player is unchanged; only bounded error reporting is shared with that fork. Earlier validation text describes superseded attempts and must not be treated as proof that 0.3.6 solved PTT.


## 0.3.6: animation resources, controls and tablet accent

The 2.9 robot reported `input is neither XML nor a JSON array` while starting the speech gesture after PTT release. Bundled resource names are now resolved to their actual unmodified XML before ALAnimationPlayer.run; the returned future retains the existing cancellation and STOP barrier. System-library names still pass through unchanged. Four new resource-loading/failure tests pass. This fix is for 2.9; the 2.5 speech player already uses its portable arm-only curve player.

The UI includes twelve named animations: the previous eight plus Dance (official headbang demo), Funny, Look around and Make space. Happy/Sad no longer include Reaction in their labels. The 12-button geometry test passed on Quest without commanding the robot. START/CONNECT on the 2.9 tablet uses the VR primary accent #0064E0. All three APKs built; Python suites passed 177 tests for 2.9 and 184 for 2.5.

Pepper 2.5 still exposes only animations confirmed in its own installed library; the added 2.9 resource package is not installed on 2.5. New bundled clips and the PTT correction need supervised physical trials; successful builds and mocked playback do not prove physical performance. The records below refer to previous releases.


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
