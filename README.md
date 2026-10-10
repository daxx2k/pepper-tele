# TelePepper for the Pepper humanoid robot (NAOqi 2.5)

<img src="docs/assets/telepepper-icon.png" alt="TelePepper app icon: a Pepper robot with blue accents" width="140" />

[Download the 0.3.11 Pepper 2.5 experimental release](https://github.com/daxx2k/pepper-tele/releases/tag/v0.3.11-test) · [Pepper 2.9 branch](https://github.com/daxx2k/pepper-tele/tree/main)

TelePepper lets you control **Pepper, the wheeled humanoid robot developed by SoftBank Robotics**, using a **Meta Quest virtual reality headset**. The robot follows your head and arm movements; the headset controllers let you drive it, and live camera feeds show what the robot sees.

For **Wizard of Oz research**, an operator controls the robot's speech and behaviour while participants interact with it. TelePepper provides speech controls, gestures and lights for these interactions.

**Version 0.3.11-pepper25-test — experimental prerelease.**

This update fixes the false NAOqi rejection caused by qi startup warnings (including the `2.5.10.7` case). Connection settings now open as a Home panel before entering the VR studio, avoiding the previous launch into an empty immersive background. The Home background follows your headset environment/passthrough setting. Automatic tests and the Android build pass; a fresh headset and Pepper 2.5 trial is still required.

## Download apps

**Ready-to-install compiled apps are available in [GitHub Releases](https://github.com/daxx2k/pepper-tele/releases/tag/v0.3.11-test). You do not need to build the source code.** An APK is an Android app installation file. On the release page, expand **Assets** if the downloads are not visible.

| Your robot software | App and device | Download |
| --- | --- | --- |
| Pepper NAOqi 2.9 | Meta Quest VR headset | [TelePepper-Quest.apk](https://github.com/daxx2k/pepper-tele/releases/download/v0.3.11-test/TelePepper-Quest.apk) |
| Pepper NAOqi 2.9 | Android tablet on the Pepper robot | [TelePepper-Pepper.apk](https://github.com/daxx2k/pepper-tele/releases/download/v0.3.11-test/TelePepper-Pepper.apk) |
| Pepper NAOqi 2.5 | Meta Quest VR headset | [TelePepper-Quest25.apk](https://github.com/daxx2k/pepper-tele/releases/download/v0.3.11-test/TelePepper-Quest25.apk) |

For first setup, download the complete package with English instructions and installation tools:

- [Pepper 2.9 install ZIP](https://github.com/daxx2k/pepper-tele/releases/download/v0.3.11-test/TelePepper-Pepper29-install-0.3.11.zip) — includes both Quest and Pepper tablet apps.
- [Pepper 2.5 install ZIP](https://github.com/daxx2k/pepper-tele/releases/download/v0.3.11-test/TelePepper-Pepper25-install-0.3.11.zip) — includes the Quest25 app; no Pepper Android tablet app is required.

Use the package matching your robot's NAOqi version. Source ZIPs are for developers and are not required to install the apps. [View all releases](https://github.com/daxx2k/pepper-tele/releases).

The 0.3.11 update fixes **Exit TelePepper** on both variants. Exit closes the VR studio directly if it never connected to a robot. After a lost connection or an unconfirmed stop/normal-mode return, controls offer **Close VR studio only**; this closes the headset scene without claiming the robot has returned to normal. With a connected robot, normal mode still requires a fresh confirmed STOP and a successful robot acknowledgement.

## Choose your robot software version

The numbers **2.9 and 2.5 refer to NAOqi, Pepper's robot software**, rather than different robot models. This repository contains two separate app variants. Use the `main` branch for Pepper NAOqi 2.9 with an Android tablet, and `pepper25` for Pepper NAOqi 2.5. Download the matching install ZIP from Releases; each ZIP includes English setup instructions, help, checksums and third-party notices.

| Variant | Apps to install | Head-service setup | Validation |
| --- | --- | --- | --- |
| Pepper 2.9 (`main`) | Quest APK and Pepper tablet APK | CONNECT on the tablet installs/starts the bridge | Evaluated on Quest 3 and Pepper 2.9.5.172; operator confirmed PTT arm gestures with tracking remaining active |
| Pepper 2.5 (`pepper25`) | Quest25 APK only | CONNECT on Quest installs/starts the bridge via owner SSH | Built and tested with simulation/mocks; a real Pepper 2.5 trial is still required |

A computer is only needed for initial APK installation. During operation, Quest connects directly to the robot head. No PC relay, cloud TTS API or firmware upgrade is required. Quest Pro has not been validated. Sub-40 ms physical motion latency has not been established.



Quest 3/3S connects directly to the Pepper head. There is no runtime PC, Pepper Android APK, tablet preview, caption or emoji panel. This is a separate fork, with a distinct Quest application ID: it.telepepper.quest25.

Start with [QUICKSTART](docs/QUICKSTART.md). Physical compatibility with a NAOqi 2.5 robot is not yet verified. This package is for a supervised trial, not a production compatibility claim.

CONNECT in Quest settings checks NAOqi 2.5 and qi/Pillow, installs head scripts in /home/nao/telepepper25 when needed, creates pairing and starts disarmed. SSH passwords stay in private Quest storage, encrypted using Android Keystore. No robot boot hook or systemd dependency is added.

Hold A + X in VR to calibrate, prepare and engage tracking. STOP pauses and returns gently to neutral; B is immediate stop. Exit TelePepper confirms stopping, restores normal autonomy and ends the service. A clean Exit is not restarted. Wi-Fi loss stops motion without automatically restoring autonomy.

Defaults: obstacle margins 0.40 m orthogonal / 0.10 m tangential, joint/speed limits ON, measured-pose engagement, 180 ms watchdog. Physical latency/alignment on Pepper 2.5 remain unmeasured.

Only matching official animations confirmed in the installed library are selectable; the 2.9 animation package is not installed. Some robots may offer no matching clips. Default voice: Pepper TTS, volume 50, Listen OFF. Live microphone and optional Piper/Cori remain available.

Build with JDK 17, SDK 34, NDK 27.2.12479018 and CMake 3.22.1: tools/build.ps1 -Target Quest. Output: dist/TelePepper-Quest25.apk. Development tests use requirements-dev.txt on the PC, never on the robot.

Original code is Apache 2.0; see LICENSE, NOTICE and third-party notices. Public releases are marked experimental.

Automatic speech gestures use arms-only keyframe derivatives of the attributed official Pepper Core Animations through ALMotion. They do not require the 2.9 package manager. App and head-service versions appear in the Quest dashboard/Help.

Connection setup uses a compact scrollable form with **Find Pepper on Wi-Fi** above the fields and **Save and open VR studio** always visible at the bottom. Selecting a saved endpoint restores its own pairing code and encrypted SSH credentials. Discovery retries local-subnet broadcasts and saved addresses, then checks previously paired endpoints with an authenticated read-only connection if Wi-Fi blocks broadcast. A running TelePepper head service is required for discovery; use manual setup for the first installation.
