# Privacy and publication checklist

The shareable source excludes `.local/`, `.reference/`, `dist/`, build caches, local SDK settings, signing keys and recordings. Keep the full working folder private: those excluded directories may contain credentials, device identifiers, journal logs, diagnostic audio and screenshots.

`tools/privacy_check.py` checks publishable files for local secrets, user paths, development device identifiers, personal-network values and common key/token formats. It reports file names and categories without printing secret values. `tools/package_release.py` uses an explicit source allowlist and also checks decompressed APK entries. This reduces accidental disclosure; it is not proof that every possible personal datum is detectable.

Before GitHub publication:

1. Review the generated source ZIP and the privacy scan result.
2. Keep LICENSE and NOTICE in the release; review third-party redistribution terms.
3. Decide whether public APK releases should use a private release-signing key. Never include the key in the repository.
4. If Git history is later created or imported, scan the entire history separately. The publication checkout starts from the audited source snapshot; inspect any imported history separately.
5. Do not upload the full Desktop or Drive working directory. Publish only the sanitized source package.

No cloud account, telemetry service or API key is required for robot control. Pairing secrets and SSH details are stored locally. Diagnostic tools can collect identifiers or spoken/displayed text from logs; their outputs stay in ignored diagnostic folders. Local control protocols are authenticated but not encrypted with TLS; keep them on a trusted local network.

Publish only audited files and review the staged Git diff. Repository visibility is selected separately from package generation.
