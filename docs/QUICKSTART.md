# Quick start: Meta Quest 3/3S and the Pepper humanoid robot (NAOqi 2.5)

Pepper is a wheeled humanoid robot developed by SoftBank Robotics. Meta Quest is the virtual reality headset used to control it. NAOqi is the robot's software; install the TelePepper variant that matches its version.

## Requirements

Pepper must run NAOqi 2.5.x and allow owner SSH access. Its existing Python 2.7 environment must include qi and Pillow. CONNECT checks these before installation; it does not install system dependencies or change firmware. Both devices must share Wi-Fi permitting peer traffic. The Quest setup flow requires Meta Horizon OS v69 or newer.

## Install Quest once

Extract the package. Install TelePepper-Quest25.apk with Meta Quest Developer Hub (MDH): connect Quest by USB, authorize debugging, select Device Manager, then under Apps drag the APK or click Add Build. The following ADB command is an optional alternative:

```powershell
adb -s QUEST_USB_SERIAL install -r TelePepper-Quest25.apk
```

Replace the placeholder using adb devices. Launch TelePepper 2.5 from the Quest library. Connection settings open as a panel in Home, using your headset's current environment/passthrough choice. The immersive VR studio starts only when you select Save and open VR studio. Its distinct application ID lets it coexist with the original app. The PC is only needed for this initial sideload; it is never a relay during use.

## Connect directly from Quest

1. Enter the robot head IPv4 address used by Choregraphe/SSH, your SSH user (normally nao) and password in Connection settings.
2. Press CONNECT Pepper 2.5. Confirm the fingerprint only for your own robot. The app checks the firmware, installs its separate head service, creates pairing automatically and starts disarmed.
3. Press Save and open VR studio.
4. Centre sticks, look forward and hold A + X for 0.75 seconds. This can disable autonomy and wake Pepper into its operating posture before tracking begins. Keep clear of its arms.

There is no Pepper tablet app. After Exit, use CONNECT in Quest settings to start another session.

## First physical trial

Keep default protections. With the base still, test small head/arm motions and triggers, then STOP and B. Then try a small translation/rotation in clear space. Verify tracking/Wi-Fi loss stops movement before involving other people. Physical compatibility and latency on this robot remain unverified until this trial.

## End the session

STOP pauses tracking and gently returns to neutral. B immediately cancels motion, animations and neutral return. Neither ends the service.

Exit TelePepper in Connection controls acknowledges STOP, cancels speech, restores normal Autonomous Life and ends the service. Failed stop confirmation never restores autonomy. Force-closing the app or losing Wi-Fi cannot guarantee Exit reaches the robot: reconnect and explicitly exit, or use the robot's own controls.

## Voice and optional features

Pepper TTS is the default and needs no extra voice APK. Optional Piper/Cori requires an English engine installed on Quest; Install-Cori.ps1 can install the pinned model with internet access during initial setup. Transcription depends on Quest speech services/language models. Listen defaults OFF to avoid feedback.

Camera, Depth, audio, sensor and animation availability vary with hardware/runtime. Stale/missing feeds show static. Only confirmed animation paths appear; the fork does not install 2.9 clips or invent substitutes.

## Troubleshooting

Version check: qi startup warnings no longer cause a false firmware rejection; NAOqi 2.5.10.7 is accepted, while 2.9 is still rejected before installation. Black setup background: update to 0.3.9 or newer, launch the app from the library, and check that passthrough is enabled in Home. Authentication: use the robot SSH password, not a pairing token. Missing qi/Pillow: check the existing runtime, do not upgrade firmware for this trial. Ports used: end the other teleoperation service first; this fork never kills unrelated processes.

Report firmware, the exact error and failing feature without passwords/pairing data. Do not install a Pepper Android APK on this robot.

With Gestures ON, release the left push-to-talk grip to trigger arm gestures. Hold both grips for 0.35 seconds to toggle the mode. See HELP.md for timing and safety behaviour.

## Update an existing 2.5 installation

Install the new Quest25 APK using the same signing key. Use Exit TelePepper to end the existing service, then CONNECT again in Quest Connection settings. A running service is reused without replacing it; an inactive service is updated from the APK. Updates stage and compile the scripts first, keep backups in the robot's telepepper25 folder, preserve numeric base-security/test-cap settings and retain pairing. Other custom source edits are superseded by the bundle. No firmware, boot hooks or system dependencies are changed.
