# Build from source

Install Android Studio / Android SDK, JDK 17, SDK platform 34, NDK 27.2.12479018 and CMake 3.22.1. Gradle resolves AGP 8.1.0 and the OpenXR loader from Google/Maven Central.

On Windows, set `ANDROID_HOME` and `JAVA_HOME`. `tools/build.ps1` can also use Android Studio's standard SDK/runtime locations. An optional ignored `.local/build.env.ps1` can set machine-specific environment variables. No shared Robotics folder is required.

```powershell
.\tools\build.ps1 -Target All
# Or use the included wrapper directly:
.\gradlew.bat :quest:assembleDebug :pepper:tablet:assembleDebug
```

The included wrapper downloads Gradle 8.5 on its first run; Internet access is also needed to resolve dependencies. On Linux/macOS use `sh ./gradlew` with the same Gradle tasks. APKs are copied into ignored `dist/` by the PowerShell helper. Debug signing is for evaluation; configure release signing privately before a public APK release. Rebuilding with another key can prevent in-place upgrades.

Python tools require Python 3, Pillow and Paramiko (`python -m pip install -r requirements-dev.txt`). The robot itself uses its installed Python 2.7 qi/Pillow runtime; do not upgrade the robot runtime with these developer requirements.

```powershell
python -m unittest discover -s tests -p "test_*.py"
python tools/privacy_check.py
python tools/package_release.py
```

C++/Java test files are standalone checks, including native tests run on an ARM64 Quest. The Python test command does not execute those native tests.

Optional lab setup: copy `config/pepper.properties.example` to `.local/pepper.properties`. Fill in your own values, then pass explicit device addresses/serials to `tools/configure.py`, `tools/robot_remote.py`, and `tools/install.ps1`. Runtime credentials travel into private app storage rather than source/build assets. Normal recipients can pair through the apps; developer setup is optional.

The official animation resource package is included in source. After changing its clips or version, rebuild it with `python tools/package_animations.py` before building the APKs. The head service installs or upgrades this separate resource package at startup.
