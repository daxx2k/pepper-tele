# TelePepper for Pepper 2.9 - 0.3.7-test

This package contains the Pepper Android tablet app and the native Quest app. During use, Quest communicates directly with the robot head; a PC is needed only for initial installation. Use your own robot credentials and pairing code.

## Requirements

- Pepper with an Android tablet and NAOqi 2.9. The development robot runs 2.9.5.172, Python 2.7 with qi/Pillow and user systemd.
- Quest 3, developer mode, USB debugging and authorized controllers.
- A PC with Android platform-tools (ADB) for sideloading.
- A shared Wi-Fi network that permits device-to-device traffic; SSH access to your robot head for first setup.

## Install the apps

Extract the ZIP before running the installer. Identify Quest with `adb devices`. Enable ADB on the Pepper tablet and connect over Wi-Fi with `adb connect TABLET_IP:5555`. The tablet ADB address and robot head Wi-Fi address can be different.

For Quest, Meta Quest Developer Hub (MDH) provides graphical installation: enable developer mode, connect by USB, authorize debugging, then use Device Manager > Apps > Add Build or drag TelePepper-Quest.apk. The following ADB/PowerShell installer is an alternative and also installs the Cori engine. Replace the placeholders with your own serials:

```powershell
.\Install.ps1 -Device Pepper -Serial TABLET_ADB_SERIAL -Adb C:\Android\platform-tools\adb.exe
.\Install.ps1 -Device Quest -Serial QUEST_USB_SERIAL -Adb C:\Android\platform-tools\adb.exe
```

The Quest installer downloads and installs the pinned offline British English Cori voice engine (about 86 MB), then installs TelePepper. Internet is needed for this initial download; Piper synthesis then runs on Quest. The Cori APK replaces other model variants of the same sherpa-onnx engine package. Installation preserves app data and never uninstalls automatically.

## Connect your Pepper

1. Open TelePepper on Pepper's tablet. In Connection, enter your robot's own SSH username and password. The tablet-to-head address defaults to its internal address, `198.18.0.1`.
2. Press CONNECT (the button becomes START after connection) and confirm the SSH identity for your robot. First setup checks dependencies and installs the bundled head bridge. CONNECT starts the service disarmed; it does not wake or move Pepper. The service does not start at robot boot.
3. Copy the private Quest pairing code shown on the tablet. Press START on the tablet to prepare Pepper and open the participant display; leave room for its operating posture.
4. Open TelePepper on Quest from the sideloaded/Unknown Sources library. In Connection settings, select Find Pepper on Wi-Fi, choose your robot, enter its pairing code, then save and open VR. If discovery is unavailable, enter the robot HEAD Wi-Fi address shown under For Robot Browser on Pepper's tablet.
5. Centre the sticks and look forward. Hold A + X for 0.75 seconds, or select START, to calibrate and engage tracking.

Discovery finds a running TelePepper service. Wi-Fi client isolation can block it. A subsequent CONNECT starts an already installed bridge without overwriting manual head updates; updating the Android APK alone does not replace a previously configured head bridge. Keep a backup of custom head scripts when upgrading an existing installation.

## Controls

| Control | Action |
| --- | --- |
| Hold A + X | Start / Pause; release both before repeating |
| B | Immediate STOP and animation cancellation |
| Left stick | Move forward/backward and sideways |
| Right stick | Rotate the base |
| Front triggers | Close hands; right trigger clicks when using the pointer |
| Right grip | Show the controller pointer |
| Left grip | Push to talk; release for recognized speech, or stream directly in Puppeteer mode |
| Both grips together, hold 0.35 seconds | Toggle automatic speech gestures; release both before repeating |
| Recalibrate | Refresh head, arms, wrists, hands, torso and base references |

Piper/Cori is the default voice, volume starts at 50%, and listening to Pepper's microphone starts OFF. Voice settings also offer Pepper TTS and the live puppeteer microphone. Speech captions can be shown on the tablet. Presets and queued Piper speech share the same playback path; Stop speech cancels pending phrases.

Automatic gestures start OFF. When enabled with live tracking armed, official arm-only gesture derivatives accompany Pepper TTS, Piper and a left-grip release in live microphone mode. Head, torso assistance and joystick control continue. At speech end, arms return gradually from their measured pose to tracking. Turning the gesture mode OFF leaves speech playing. See HELP.md for details.

Help opens beside the dashboard and leaves tracking and controls available. Pin locks the window in the room; Layout lets you arrange independent panels and save presets. Clicking Top, Bottom, Depth or Lidar toggles the corresponding feature. Depth is monochrome: bright near, dark far; invalid measurements are black. The top/bottom camera transition is a visual blend, not calibrated panoramic stitching.

## Finish and troubleshoot

Use Exit TelePepper to stop control, restore normal autonomous mode and stop the head service. Use tablet CONNECT again for the next session. Disconnects and watchdog stops do not restore autonomy automatically.

If connection fails, check the running service, head address, pairing code and Wi-Fi isolation. Ports: TCP 9570 control, UDP 9571 motion, TCP 9572 media, UDP 9573 audio, UDP 9574 discovery. Keep these services on your trusted local network.

Before use, check STOP, tracking-loss behaviour and movement on your robot. The new speech gesture mode still needs a physical operator trial; automated tests do not establish mechanical latency. See VALIDATION.md for the exact checks completed, HELP.md for controls and THIRD-PARTY.md for notices. Recording and session export are deferred.

Tablet configuration is compact on one screen: CONNECT becomes START once connected, and DISPLAY opens the participant screen directly. The participant screen shows only the settings icon and small status indicators.

## Update an existing 2.9 installation

Install the new tablet and Quest APKs with the same signing key. Stop tracking, wait for neutral return/STOP to complete, then use **Update service** in the tablet Connection card. This uploads and checks the bundle before replacement, backs up the prior scripts on the robot and preserves numeric base-security/test-cap settings. Other custom source edits are superseded by the bundled update; retain the backup if needed. The update does not arm tracking or enable boot autostart. A plain CONNECT continues to start the existing installed bridge.

Installing only the Quest APK with MDH does not install the separate Cori voice engine. Use Install.ps1 for Quest to install both, or install the Cori engine APK separately with MDH. Pepper TTS remains available without Cori.
