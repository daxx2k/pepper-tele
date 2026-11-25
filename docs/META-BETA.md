# Private Meta Beta builds

This is an opt-in distribution profile. It does not publish to Meta or change the normal sideloaded debug build.

## Build

1. Use Android SDK 34, NDK 27.2.12479018, CMake 3.22.1 and a compatible JDK as described in BUILD.md.
2. Set `ROBOTTELE_META_KEYSTORE` to your private release keystore path and `ROBOTTELE_META_KEY_PASSWORD` through your local secret store. `ROBOTTELE_META_KEY_ALIAS` defaults to `meta-release`. Keep signing files and passwords outside the repository.
3. Run `./tools/build-meta-beta.ps1` from PowerShell. The signed APK is written under `dist/meta-beta/`.
4. Check version and code in `config/meta-beta.properties` before every upload. Codes must increase beyond every earlier upload, including private channels. Preserve the same distribution signing key for updates.
5. Upload the matching APK to the existing **Beta** channel in Meta Developer Dashboard and choose **Quest 3 family** (Quest 3 and Quest 3S). Leave Production untouched.

The profile uses target SDK 34, OpenGL ES 3.0, automatic install location, a landscape/single-task launcher, release signing and disabled debugging. These packaging settings are isolated from ordinary debug builds. No tracking, motion, speech or robot bridge code is changed by the profile.

## Branding and migration

The app icon uses the approved text-free Soft 3D portrait. Android adaptive launchers apply their own outer mask; the foreground inset keeps the robot inside the safe area. The same portrait is available in `docs/assets/`.

An existing sideloaded app signed with a different debug key cannot be updated directly by the signed Meta APK. Back up/export pairing and settings before planning that migration; do not uninstall automatically. Building or uploading does not migrate the headset.

Passing package validation and Meta malware/vulnerability scans does not establish physical robot behavior or end-to-end latency. Test the signed build on a headset before expanding the private audience.
