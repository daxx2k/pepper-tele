# Third-party notices

The applications include OpenXR/Meta headers (see META-LICENSE.txt), Roboto font assets (Apache 2.0, see UI-FONT-NOTICE.txt), nlohmann/json (MIT notice embedded in json.hpp), stb_image/stb_image_write (public domain or MIT notices embedded in their headers), and JSch 0.1.72 (BSD-style license). Android debug APKs are supplied for evaluation. The installer downloads the unmodified Cori TTS engine from the release link listed by the sherpa-onnx project: https://k2-fsa.github.io/sherpa/onnx/tts/apk-engine.html . Engine source: https://github.com/k2-fsa/sherpa-onnx/tree/v1.13.8/android/SherpaOnnxTtsEngine . Cori model card states UK English female, one speaker, medium quality, 22,050 Hz, trained on public-domain LibriVox recordings. The engine APK is downloaded from upstream and is not embedded in this ZIP. Original TelePepper code is licensed under Apache 2.0; see LICENSE and NOTICE.

Affirm/Refuse refer to files already installed on Pepper; those system-library files are not bundled. The six additional clips below are distributed under their own license.

The Gradle 8.5 wrapper is included under Apache 2.0; see third_party/GRADLE-LICENSE.txt in the source package.

## Pepper Core Animations

Six unmodified clips (Hello_01, Hello_09, PointFrontL_01, PointFrontR_01, NiceReaction_01, SadReaction_01) from https://github.com/softbankrobotics-labs/pepper-core-anims are bundled in the telepepper-anims package. Copyright 2011-2019 SoftBank Robotics Europe; BSD 3-Clause terms are reproduced in third_party/PEPPER-CORE-ANIMS-LICENSE.txt and the package COPYING file. The package is installed through Pepper PackageManager; the system animation library is not modified.

The separate `telepepper-speaking.pkg` contains arm-only derivatives of the attributed Pepper Core Animations happy and pointing clips. Head and lower-body curves are removed; the pointing clips receive a 0.76-second entry interval. The original manual animation package is unchanged. Both packages include the original COPYING notice.

Automatic speech gestures use arm-only keyframe derivatives of the same Pepper Core Animations, under the same BSD 3-Clause terms. The 2.9 runtime packages those derivatives separately; the 2.5 runtime uses the attributed curves in speech_clip_data.py through ALMotion. Head, hips and wheel curves are excluded.
