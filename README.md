# TelePepper

[![Buy me a coffee](https://img.shields.io/badge/Buy_me_a_coffee-Support_my_projects-FFDD00?logo=buymeacoffee&logoColor=000000)](https://buymeacoffee.com/daxx2k)

<img src="docs/assets/telepepper-icon.png" alt="TelePepper app icon: a Pepper robot with blue accents" width="140" />

[Download the 0.3.18 experimental release](https://github.com/daxx2k/pepper-tele/releases/tag/v0.3.18-test) Â· [Pepper 2.5 branch](https://github.com/daxx2k/pepper-tele/tree/pepper25)

TelePepper lets you control **Pepper, the wheeled humanoid robot developed by SoftBank Robotics**, using a **Meta Quest virtual reality headset**. The robot follows your head and arm movements; the headset controllers let you drive it, and live camera feeds show what the robot sees.

For **Wizard of Oz research**, an operator controls the robot's speech and behaviour while participants interact with it. TelePepper provides speech controls, gestures and lights for these interactions.

**Version 0.3.18-test â€” experimental prerelease.** This is a development project for operator-supervised evaluation. Automated checks do not prove physical latency, pose matching or compatibility on another robot.

The 0.3.18 Quest update aligns Depth, LiDAR and Tablet preview titles and content areas, with consistent internal margins that keep the previews inside the rounded cards.

The 0.3.18 Quest update removes the LiDAR clear-history button, leaving the map preview unobstructed.

The 0.3.18 Quest update shows built-in phrases even before connecting, keeps the latest robot phrase list through disconnects, and refreshes it after reconnecting.

## Download apps

**Ready-to-install compiled apps are available in [GitHub Releases](https://github.com/daxx2k/pepper-tele/releases/tag/v0.3.18-test). You do not need to build the source code.** An APK is an Android app installation file. On the release page, expand **Assets** if the downloads are not visible.

| Your robot software | App and device | Download |
| --- | --- | --- |
| Pepper NAOqi 2.9 | Meta Quest VR headset | [TelePepper-Quest.apk](https://github.com/daxx2k/pepper-tele/releases/download/v0.3.18-test/TelePepper-Quest.apk) |
| Pepper NAOqi 2.9 | Android tablet on the Pepper robot | [TelePepper-Pepper.apk](https://github.com/daxx2k/pepper-tele/releases/download/v0.3.18-test/TelePepper-Pepper.apk) |
| Pepper NAOqi 2.5 | Meta Quest VR headset | [TelePepper-Quest25.apk](https://github.com/daxx2k/pepper-tele/releases/download/v0.3.18-test/TelePepper-Quest25.apk) |

For first setup, download the complete package with English instructions and installation tools:

- [Pepper 2.9 install ZIP](https://github.com/daxx2k/pepper-tele/releases/download/v0.3.18-test/TelePepper-Pepper29-install-0.3.18.zip) â€” includes both Quest and Pepper tablet apps.
- [Pepper 2.5 install ZIP](https://github.com/daxx2k/pepper-tele/releases/download/v0.3.18-test/TelePepper-Pepper25-install-0.3.18.zip) â€” includes the Quest25 app; no Pepper Android tablet app is required.

Use the package matching your robot's NAOqi version. Source ZIPs are for developers and are not required to install the apps. [View all releases](https://github.com/daxx2k/pepper-tele/releases).

**Pepper NAOqi 2.5 users:** use the [0.3.18 setup/Exit update](https://github.com/daxx2k/pepper-tele/releases/tag/v0.3.18-test). It fixes warning-prefixed firmware replies and opens setup in the headset Home environment. The Pepper 2.9 downloads above use the separate 0.3.18 update.

The 0.3.18 update fixes **Exit TelePepper** on both variants. Exit closes the VR studio directly if it never connected to a robot. After a lost connection or an unconfirmed stop/normal-mode return, controls offer **Close VR studio only**; this closes the headset scene without claiming the robot has returned to normal. With a connected robot, normal mode still requires a fresh confirmed STOP and a successful robot acknowledgement.

## Choose your robot software version

The numbers **2.9 and 2.5 refer to NAOqi, Pepper's robot software**, rather than different robot models. This repository contains two separate app variants. Use the `main` branch for Pepper NAOqi 2.9 with an Android tablet, and `pepper25` for Pepper NAOqi 2.5. Download the matching install ZIP from Releases; each ZIP includes English setup instructions, help, checksums and third-party notices.

| Variant | Apps to install | Head-service setup | Validation |
| --- | --- | --- | --- |
| Pepper 2.9 (`main`) | Quest APK and Pepper tablet APK | CONNECT on the tablet installs/starts the bridge | Evaluated on Quest 3 and Pepper 2.9.5.172; operator confirmed PTT arm gestures with tracking remaining active |
| Pepper 2.5 (`pepper25`) | Quest25 APK only | CONNECT on Quest installs/starts the bridge via owner SSH | Built and tested with simulation/mocks; a real Pepper 2.5 trial is still required |

A computer is only needed for initial APK installation. During operation, Quest connects directly to the robot head. No PC relay, cloud TTS API or firmware upgrade is required. Quest Pro has not been validated. Sub-40 ms physical motion latency has not been established.

The 0.3.18 Quest update opens connection settings as a panel in the headset Home environment before entering the immersive VR studio. The setup background follows your Home environment/passthrough setting. It requires Meta Horizon OS v69 or newer. The Pepper tablet app remains 0.3.11; robot control, calibration and explicit arming are unchanged.

## Install and operate

- [Pepper 2.9 quick start](docs/QUICKSTART.md)
- [Controls, Help and troubleshooting](docs/HELP.md)
- [Build from source](docs/BUILD.md)
- [Validation record](docs/VALIDATION.md)
- [Privacy guidance](docs/PRIVACY.md)

Install the Quest APK using Meta Quest Developer Hub (MDH). Install the tablet APK through ADB. On Pepper's tablet, CONNECT becomes START once connected. START prepares the robot; Quest START or holding A + X calibrates and engages tracking. B stops motion. Exit TelePepper confirms STOP, restores normal autonomy and ends the head service. The bridge does not start at robot boot.

Piper/Cori is the 2.9 default voice; Pepper TTS and live microphone are also available. Automatic speech gestures start OFF. Enable Gestures, then release the left push-to-talk grip to request an arm gesture while tracking is active. Head and base remain controlled independently. Both grips toggle the mode. Help is a companion panel, and app/head-service versions are visible for diagnosis.

## Architecture

- `quest/`: native OpenXR C++ dashboard/body tracking and Android speech integration.
- `pepper/tablet/`: Android configuration, pairing, deployment and participant display (2.9 only).
- `pepper/bridge/`: Python 2.7-compatible NAOqi motion, media and Wizard of Oz services.
- `config/android/`: shared Android UI and lifecycle components.
- `console/`: optional browser console.
- `tests/`: backend and standalone native regression checks.
- `tools/`: build, private setup, privacy checks and release packaging.

Control, pose, video and audio use separate connections. Motion consumes the latest pose rather than queueing older targets. Measured-pose engagement, tracking validity, STOP, watchdogs and body collision protection remain part of control. Lidar is a local sensor/odometry trace, not SLAM or a clearance guarantee. Recording and research-session export are deferred.

## Private configuration

Use your own robot addresses, SSH credentials and pairing code. Keep local settings, device captures, recordings and signing keys outside Git. Optional developer setup uses ignored `.local/pepper.properties`; see `config/pepper.properties.example`. Release APKs contain no lab pairing code or SSH password.

Run `python tools/privacy_check.py` before sharing source. GitHub APKs are debug-signed evaluation builds; rebuilding with another signing key may prevent in-place updates. No private signing keys are published.

## License

Original TelePepper code is Apache 2.0: see [LICENSE](LICENSE) and [NOTICE](NOTICE). Third-party libraries, fonts and official animation derivatives retain their own terms; see [dependency notices](docs/THIRD-PARTY.md).

Connection setup uses a compact scrollable form with **Find Pepper on Wi-Fi** above the fields and **Save and open VR studio** always visible at the bottom. Selecting a saved endpoint restores its own pairing code. Discovery retries local-subnet broadcasts and saved addresses, then checks previously paired endpoints with an authenticated read-only connection if Wi-Fi blocks broadcast. A running TelePepper head service is required for discovery; use manual setup for the first installation.



The 0.3.18 Quest update separates the Quick Help version line and subtitle in both the NAOqi 2.9 and 2.5 apps. The bottom move bar and pin stay visible. The Pepper 2.9 tablet APK remains 0.3.11.

Local test build **0.3.19** corrects arm mapping beyond vertical and uses native Pepper position bounds for shoulders, elbows and wrists. Existing speed limits and other controls are retained. Update the Quest app and the head service together; for 2.9, install the updated tablet APK and use Update service while stopped. For 2.5, cleanly exit the old service and CONNECT with the updated Quest25 app. Physical operator confirmation is pending; published release links above refer to the previous public build.
