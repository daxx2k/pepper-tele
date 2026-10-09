# TelePepper for Pepper NAOqi 2.5

[Download the 0.3.5 experimental release](https://github.com/daxx2k/pepper-tele/releases/tag/v0.3.5-test) · [Pepper 2.9 branch](https://github.com/daxx2k/pepper-tele/tree/main)

**Version 0.3.5-pepper25-test — experimental prerelease.**

## Versions and downloads

This repository contains two separate variants. Use the `main` branch for Pepper NAOqi 2.9 with an Android tablet, and `pepper25` for Pepper NAOqi 2.5. Download the matching install ZIP from Releases; each ZIP includes English setup instructions, help, checksums and third-party notices.

| Variant | Apps to install | Head-service setup | Validation |
| --- | --- | --- | --- |
| Pepper 2.9 (`main`) | Quest APK and Pepper tablet APK | CONNECT on the tablet installs/starts the bridge | Evaluated on Quest 3 and Pepper 2.9.5.172; latest gestures need a physical trial |
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
