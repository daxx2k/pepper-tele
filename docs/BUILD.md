# Build the Pepper 2.5 variant

Install JDK 17, SDK platform 34, NDK 27.2.12479018 and CMake 3.22.1. Set ANDROID_HOME and JAVA_HOME or use Android Studio's SDK/runtime.

```powershell
.\tools\build.ps1 -Target Quest
.\gradlew.bat :quest:assembleDebug
```

On Linux/macOS use `sh ./gradlew :quest:assembleDebug`. The helper copies the APK to ignored `dist/TelePepper-Quest25.apk`. Debug signing is for evaluation; keep release signing keys private. Rebuilding with a different key may prevent in-place upgrades.

The Quest build bundles the Python head bridge. Do not install the 2.9 tablet APK on this variant. Runtime is Quest to Pepper with owner SSH bootstrap, no PC relay and no boot hook.

Install PC test dependencies from requirements-dev.txt, then run:

```powershell
python -m unittest discover -s tests -p "test_*.py"
python tools/privacy_check.py
```

The robot uses its existing Python 2.7 qi/Pillow environment. Do not install PC developer dependencies on the robot or upgrade firmware. Native tests are separate from the Python suite. Physical Pepper 2.5 compatibility remains unverified.
