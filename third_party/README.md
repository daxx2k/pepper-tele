# Vendored dependencies

- `json.hpp`: nlohmann/json v3.11.3, MIT license (embedded in header).
- `stb_image.h`, `stb_easy_font.h`: nothings/stb commit `f0569113c93ad095470c54bf34a17b36646bbbb5`, dual MIT/public-domain (embedded in headers).
- OpenXR loader: Maven `org.khronos.openxr:openxr_loader_for_android:1.1.54`, Apache-2.0/MIT. Resolved by Gradle; not copied here.
- `meta_body_tracking_fidelity.h`: Meta-OpenXR-SDK commit `bbed2f20e38a5df7113630771c83cb8279e4fc26`, Oculus SDK License Agreement. Optional high-fidelity tracking extension, requested only when the runtime advertises it. License: `META-LICENSE.txt`.

The application does not use Unity or Meta's Unity SDK.
