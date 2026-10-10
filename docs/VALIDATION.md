# Release preparation validation - 2026-10-05

## 0.3.11: connection setup and discovery (2026-10-10)

Both variants build successfully. Full Python suites pass (186 tests for 2.9, 188 for 2.5). Fourteen host C++ Exit flow cases pass. JVM tests verify endpoint-specific pairing, rotated current codes, UDP validation/deduplication, authenticated saved-endpoint fallback and rejection of invalid authentication. The production discovery code found the paired robot in a read-only PC network check.

The final 2.9 Quest APK (0.3.11-test) was installed over the existing app and its connection settings remained byte-identical; the manifest label is TelePepper. The 2.9 tablet update preserves its four private settings files byte-for-byte. An external app_process probe aborted on Quest before running discovery, so it does not establish headset discovery success. Quest setup uses compact scrollable fields and a sticky save/open action. Selecting a saved 2.5 endpoint also restores that endpoint's encrypted SSH credentials. No ARM, calibration or physical movement was initiated for this update. A fresh operator trial of headset layout, discovery, connected Exit restoration and Pepper 2.5 remains required; build and mock checks do not establish those physical behaviours.

## Operator confirmation: 0.3.8 (2026-10-09)

After START and Gestures ON, the operator confirmed that releasing push-to-talk plays arm gestures and tracking remains active on the Pepper 2.9 setup. This is physical operator confirmation of that specific regression fix, in addition to the API probes and automated checks below. Independent head/base movement during gestures, long-session stability, physical latency and Pepper 2.5 compatibility were not separately confirmed by this trial. Earlier pending-trial statements below describe the state before this confirmation.


## 0.3.8: native arm speech playback and qianim integer metadata

The real 2.9 robot still stopped on PTT release with `conversion of data to type "i" failed`. Pointing resources used Qt metadata such as mute=false and frame=19.0. Packaged manual resources now canonicalize integer flags/FPS/integral frames without changing actuator values, timestamps or tangent values; original source clips are retained. Fractional author keyframes are rejected rather than silently rounded.

Automatic speech gestures on 2.9 now use the same native ALMotion arm-only curve player used by the 2.5 variant. They no longer depend on ALAnimationPlayer parsing these speech clips. Cancellation uses only the twelve arm/hand resources and waits for acknowledgement before the measured-pose return. The official derivative curve data and gentle initial interval are retained.

On the actual 2.9 robot, the new player completed a zero-displacement interpolation at the measured arm pose and acknowledged cancellation. No expressive gesture or head/torso/wheel motion was commanded during that API probe. The physical PTT gesture and continued head/base control still require an operator trial.

186 Python tests passed for 2.9 and 188 for 2.5, including native arm ownership/cancellation and integer-metadata preservation checks. All three APKs built. The 2.5 native speech player is unchanged. Prior 0.3.6/0.3.7 repair claims were superseded by observed failures.


## 0.3.7: real-player PTT format investigation

The 0.3.6 XML forwarding repair was incorrect: ALAnimationPlayer.run accepts a registered package/path, not inline XML. On the actual 2.9 player, a temporary package with two empty (no-actuator) resources reproduced the original error for a bare Animation element; adding the XML declaration advanced to the expected null-duration rejection. No physical animation was played by this probe. The package is removed after diagnosis.

Speech package 1.0.1 now contains XML declarations on every qianim, and the player again receives package/path. Startup validation rejects malformed bundled headers. Public STOP reasons are bounded to 512 characters because SDK errors can contain entire XML clips and overflow status replies. These changes preserve native motion watchdogs and cancellation barriers.

Python suites passed 180 tests for 2.9 and 185 for 2.5; all three APKs build. Physical PTT gestures still require the operator's next trial. The 2.5 speech player is unchanged; only bounded error reporting is shared with that fork. Earlier validation text describes superseded attempts and must not be treated as proof that 0.3.6 solved PTT.


## 0.3.6: animation resources, controls and tablet accent

The 2.9 robot reported `input is neither XML nor a JSON array` while starting the speech gesture after PTT release. Bundled resource names are now resolved to their actual unmodified XML before ALAnimationPlayer.run; the returned future retains the existing cancellation and STOP barrier. System-library names still pass through unchanged. Four new resource-loading/failure tests pass. Installed Quest and tablet report 0.3.6-test; deployed head assets were compared with the staged source (preserving installed numeric settings). Authenticated status confirms all twelve manual names, speech gestures available, motion disarmed and no STOP error. No automatic physical animation or speech was triggered during deployment; a PTT operator trial remains pending. This fix is for 2.9; the 2.5 speech player already uses its portable arm-only curve player.

The UI includes twelve named animations: the previous eight plus Dance (official headbang demo), Funny, Look around and Make space. Happy/Sad no longer include Reaction in their labels. The 12-button geometry test passed on Quest without commanding the robot. START/CONNECT on the 2.9 tablet uses the VR primary accent #0064E0. All three APKs built; Python suites passed 177 tests for 2.9 and 184 for 2.5.

Pepper 2.5 still exposes only animations confirmed in its own installed library; the added 2.9 resource package is not installed on 2.5. New bundled clips and the PTT correction need supervised physical trials; successful builds and mocked playback do not prove physical performance. The records below refer to previous releases.


## Current review: 0.3.5-test (2026-10-09)

The current review passed 173 Python tests for Pepper 2.9 and 184 for Pepper 2.5. Seventeen standalone native tests ran on Quest (math, panel interaction, controller chords, watchdog/start flow and loopback networking), plus the Java speech-queue and WAV tests on the PC. All three APKs built successfully. These tests do not command the real robot or measure physical latency.

Both variants contain STOP cancellation fault handling, Piper cleanup and STOP-aware normal-mode handoff fixes. Deployment preserves literal numeric base settings, validates the merged settings before replacement and stages/compiles scripts with backups. Pepper 2.9 exposes Update service on its tablet; Pepper 2.5 updates only after the current service is exited. The 2.5 supervisor confirms base cleanup before restarting a failed child; its loopback simulated Exit completed without restart.

The 0.3.5 builds have not been installed on the lab robot during this review. Physical START/STOP timing, speech gesture execution, head/base independence during gestures, pose alignment and sustained Wi-Fi stability still require a supervised trial. A real Pepper 2.5 trial remains outstanding. Sub-40 ms physical motion latency is not established. True backdrivability/compliant manual arm guidance is not implemented: engaged tracking sets joint stiffness to 1.0.

Source, APK contents, release archives and all published Git history are checked for known local credentials and identifiers before publication. The remaining sections are historical test records and may refer to earlier versions.

- Both Android debug APKs built successfully after removal of machine-specific build paths.
- 109 existing Python checks passed, plus three new publication/privacy checks covering private-file exclusion, secret/path detection and SSH parser-marker false positives.
- The privacy checker scans the shareable source allowlist and decompressed APK entries, including the native Quest library. No findings remain after the fixes.
- The Help guide now occupies a separate OpenXR panel beside the dashboard, with six titled sections and vector icons. Opening and closing it preserve motion state; pointer input can hit either panel. The two panels share centre height and depth, with an inward angle for Help. The new shader compiles and links on Quest hardware; visual/controller usability validation remains pending.
- This workspace has no Git history. Only current files and APKs were scanned; a future imported history requires its own review.
- Updated APKs were installed on the connected Quest and Pepper tablet, and the updated bridge was deployed disarmed. No robot movement or speech test was initiated by the developer.
- Existing core motion and streaming code was retained. This review does not establish mechanical latency, clinical/research compliance or general compatibility with other robots.

Original TelePepper code is licensed under Apache 2.0. Before public release, review dependency terms and choose a release-signing policy. The install package contains development/debug APKs.

## Follow-up fixes

- The first-START flow completes paused motor preparation across brief tracking/video freshness interruptions. Explicit STOP, focus loss, disconnection and command errors still cancel. Fresh inputs are checked again before arming.
- Preparation skips redundant Autonomous Life disabling when already disabled. The previous live preparation status showed cancellation with motors off; physical first-START confirmation after this fix remains pending.
- 113 Python checks pass, including the new already-disabled preparation regression. The native preparation-flow regression passes on Quest.
- The tablet setup UI now uses responsive light cards, rounded buttons and a distinct red STOP control. Its layout was inspected on the tablet; participant presentation remains separate.

- The participant display has a visible bottom-right settings icon. Blue LED defaults are applied at service startup; the live robot exposes separate shoulder RGB groups, which are now addressed individually.

## Animation playback and live return

- The live ALAnimationPlayer library completed the previously requested Affirm animation; the status reported complete and then STOP. Generic greetings/Hey paths were absent on this NAOqi 2.9 robot.
- Four unmodified official greeting/pointing files were installed in a separate telepepper-anims package. PackageManager returned success and ALAnimationPlayer lists all four. Physical playback of the new clips remains an operator validation step.
- 116 Python checks pass, including measured-pose asynchronous return, a failed pose-read timeout, STOP cancellation and base-zero acknowledgement before playback. Successful completion now resumes tracking with a 0.75 second pose blend; failures remain paused.
- Help moved 22 logical pixels left to clear the rounded dashboard corner.

## Eight-gesture selection

The UI and backend allowlist now contain exactly the eight owner-selected official gestures. Happy and Sad reaction use unmodified NiceReaction_01/SadReaction_01 from Pepper Core Animations; they do not synthesize laughter. The resource package is version 1.1.0 and startup upgrades older versions. All 116 Python checks pass. Physical execution of these additional reactions remains an operator validation step.

## Tablet content expiry

- Content fades after 8 seconds over 1 second, independently of the status/settings controls. The bridge clears expired content once and marks a new revision; reconnecting cannot replay that expired content. The Quest preview uses the same opacity.
- 118 Python checks pass, including fade timing, new-caption reset, single expiry revision and rejection of expired participant responses.
- A display-only check on the Pepper tablet confirmed new text appears and subsequently disappears, while status/settings icons remain. A fresh observer connection confirmed the server content was cleared. No motion or speech was initiated for this check.

## Help size and depth colours

Help is 15% smaller in both physical dimensions and shares the main panel orientation and depth. Pointer hit testing uses the same reduced dimensions. Depth uses grayscale throughout, including black for missing measurements; a single Pillow LUT replaces the previous three channel lookups. Static effects were removed from Start, microphone listening and Pose mirror controls.

## Independent VR layout

- Dashboard cards, header and status strip use separate composition quads cropped from the existing shared atlas. Crops follow the OpenGL bottom-left origin. Rendering the UI atlas once preserves the existing feed processing. The runtime layer limit is checked before enabling the mode.
- Layout editing pauses motion, supports controller-relative grabs, individual scale, default reset, autosaved current arrangement and three persistent presets. Invalid/nonfinite poses and sizes are rejected before committing a loaded layout. Positions are relative to the workspace, not persistent room anchors.
- Android Quest build passed. The new native geometry check passed on the connected Quest: all 13 crops, rotated-root round trips, controller ray coordinates, no-jump grabs and invalid pose rejection. Updated APK installed successfully.
- Interactive placement, preset restore and cropped visual appearance still require operator verification: the Quest launch dialog currently requires waking the controllers. No robot motion was initiated for validation.
- Removed the Depth near/missing-data legend from the main panel.

## Shared style and themes

- Quest settings and Pepper tablet screens compile the same StudioStyle Java source and Android themes from config/android. The existing VR palette is retained as Default, with saved Dark and Light variants; UI theme changes leave media and semantic swatches unchanged.
- Both Android builds passed and both APKs installed successfully. Native theme checks run on Quest verify Default preservation, Dark/Light separation, label colours and retained START/STOP accents.
- Pepper settings were opened and all three theme selections were exercised, captured and visually reviewed. Private pairing/address fields were masked in review images. Settings were returned to Default without issuing robot motion commands.
- Interactive VR theme appearance and Quest settings still need operator review; system VR/controller launch state prevented a useful Quest UI hierarchy capture.

## Blender-inspired theme refinement

- Replaced near-black Dark and almost-white Light with dedicated gray palette roles for cards, buttons, borders, text and selection. Main text/secondary text against card surfaces, normal button captions and white captions against blue selections pass 4.5:1 contrast checks in the native theme test. These are palette checks, not measurements of every rendered VR pixel. Default remains unchanged.
- Camera/depth feeds, emoji, LED swatches and thermal indicators retain their colours. Pose and lidar plots retain dark backgrounds for coloured sensor lines. Selected phrase captions still wrap; temperature-unit selection uses explicit foreground/background pairs.
- Palette references: https://raw.githubusercontent.com/blender/blender/main/scripts/presets/interface_theme/Blender_Light.xml and https://raw.githubusercontent.com/blender/blender/main/release/datafiles/userdef/userdef_default_theme.c . Colours are adapted to TelePepper; full Blender theme/source files are not bundled.
- Both APKs built and installed. Native theme checks passed on Quest. Interactive VR appearance remains operator validation.

## Current single Horizon-inspired style (supersedes theme experiments above)

- Removed Default/Dark/Light selection and persistence paths from VR and both Android apps. Old theme preferences are ignored. One palette now applies to all screens.
- Header uses filled Layout and Help icons, hover labels and 48-pixel targets. Shared Android cards/buttons use rounded neutral surfaces and blue selection.
- Both APK builds passed and were installed on the connected Quest and Pepper tablet. Native palette contrast and dashboard geometry/input checks passed on Quest. Palette role checks meet 4.5:1 text contrast; they do not measure every rendered pixel.
- Pepper settings screenshot was reviewed with private pairing/address values masked: readable text, coherent cards and no theme selector. Quest screenshot capture returned no pixels, so headset appearance and hover interaction still require operator review.
- Publishable source/APK privacy checks passed. No robot movement or speech was initiated during these style checks.

## Full pose recalibration and upright torso reference

- Read-only robot telemetry showed HipPitch about +0.238 rad (13.6 degrees) while paused. The previous first-arm code saved whatever measured hip pose existed as neutral; this could retain a backward lean. Torso engagement now fixes the neutral hip reference at zero while retaining measured angles as the starting point for acceleration-limited following. No new knee or wheel posture commands are introduced.
- Recalibrate now waits for STOP acknowledgement and fresh stable tracking through the existing Start flow. It refreshes head/body heading, arm proportions/IK seeds, both wrist references, hands through fresh trigger samples, torso assistance and base input state, and clears persisted pose offsets. Already running sessions re-engage with measured-pose blending; paused sessions remain paused.
- Python bridge regression checks cover tilted engagement, smooth upright return, repeat engagement, all controlled motor readings and no instantaneous setAngles during arm. Native Start flow tests cover calibration-only completion without arming. Quest build passed. Physical posture match remains operator verification.

## START cancellation and control session robustness (2026-10-08)

Operator reported repeated reconnects and START immediately returning to STOP. Device logs could not be collected because no ADB devices were connected during this check.

- Code inspection found that StartFlow treated any STOP version mismatch as a newer STOP, including an older telemetry snapshot sampled separately from the STOP acknowledgement. Older snapshots now wait; a genuinely newer STOP still cancels.
- While ARM awaits its bounded reply, stale telemetry no longer causes immediate cancellation. Fresh armed telemetry is still required before reporting successful start. Tracking/video loss, non-centred sticks, STOP and the existing three-second ARM deadline still cancel.
- START displays STARTING... during its preparation flow. Control action/reply timings and disconnection diagnostics are logged without pairing tokens.
- Pilot TCP receive timeout is five seconds instead of two; successful replies are not delayed or buffered. The independent UDP watchdog and START deadline are unchanged. Interrupted send/receive system calls retry EINTR.
- All 120 Python backend tests passed. Extended native StartFlow regression checks compiled for ARM64. Quest APK build passed using a local build cache outside the synced Drive folder; source/APK privacy checks passed. Execution on Quest, installation and physical start/connection stability validation remain pending device availability. These changes are not proof of the cause of every reported disconnect or a confirmed hardware fix. Hold public release until the operator validates this build.

Follow-up: updated Quest APK installed successfully over USB. Extended native StartFlow regressions executed on Quest and passed, including old-snapshot handling and delayed ARM telemetry. Connection logs captured privately. Hardware START/motion validation awaits Pepper boot; no motion or speech initiated by the diagnostic checks.

## Live regression investigation and controller ray (2026-10-08)

- Live logs confirmed ARM success followed by an old queued UDP STOP, cancelling a new start. TCP ARM and UDP STOP now carry a client STOP epoch. Already acknowledged STOP copies are ignored without refreshing watchdog freshness; newer and legacy STOP packets still stop immediately. Explicit TCP STOP re-establishes the clock offset while disarmed. All 123 backend tests passed, including duplicate/new/legacy STOP cases. Both APKs and the current head service were installed; bundled Python assets match source.
- START was accepted in live logs. Repeated command timeouts and automatic recoveries remain visible during all-stream operation. This regression is not resolved; publication remains on hold. A comparison with Top/Bottom/Depth disabled is pending operator input. Quest RSSI was approximately -70 dBm in the diagnostic sample; this does not by itself prove the cause.
- Added a lightweight blue camera-facing controller ray quad, shown while right grip is held with valid aim tracking. It ends at the selected main/help/floating panel, or two metres ahead when no panel is hit. Uses an 8x64 static premultiplied texture and respects the runtime layer limit. Ray endpoint/pose/degeneracy tests passed on Quest; native APK build, installation and privacy scan passed. In-headset appearance still requires operator review.

## Window modes and return-feedback regression

- Pin now toggles independent layouts between LOCAL room lock and VIEW head follow; loading a floating layout no longer forces room lock. Rendering, pointer hits, ray endpoints and panel sorting use the same workspace space. Relative layout arrangements are retained.
- While grabbing the footer or a card title, right stick up moves the panel away, down brings it closer; left/right changes size. Depth uses the controller-relative -Z direction with bounded frame steps and range. Native geometry checks passed on Quest.
- Live diagnostics also showed client acknowledgement pauses while robot commands were only 16 ms old. UDP acknowledgements now inform feedback diagnostics instead of triggering a redundant client motion pause. TCP receipt estimates remain conservative (subtracting the entire round trip) for this diagnostic. Pepper's 180 ms valid-command watchdog, sample freshness/order checks, tracking-loss STOP, explicit STOP, disconnect behavior and motor RPC stall checks remain unchanged. This does not establish overall stability: a new live comparison is required.

## Animation buttons and robot limit controls

- Active official animation buttons now retain blue highlight, with other animation buttons greyed out and excluded from pointer selection. STOP stays available; ordinary buttons return when the reported gesture ends. This presentation change needs in-headset review during a real animation.
- Three Motion controls expose Base guard, Joint limits and Speed limits. They request STOP first, wait for acknowledgement/stop completion, apply the change and leave control paused until START. Base guard uses official Move protection with owner-consent refusals propagated; arm/body protection remains enabled. Joint range changes select conservative versus actual queried hardware bounds. Speed controls normal versus full following fractions and normalized stick caps, preserving initial measured-pose engagement and torso stability caps.
- All 125 Python backend checks passed. Both APKs built and installed, and service assets inside the tablet APK match source. Updated service reports all three limits ON and motion disarmed on initial verification. No limits were disabled automatically for validation. Native window/ray/feedback checks passed on Quest. Overall motion stability remains under live evaluation; keep publication on hold.


## 2026-10-08: tablet CONNECT / START and offline panels

- Tablet launcher now stays on the connection screen. CONNECT is bottom left inside Connection; START remains disabled until the service is reachable and pairing succeeds. CONNECT does not prepare posture. START explicitly prepares posture and opens the participant display; live tracking remains a separate Quest action.
- Boot autostart was disabled and confirmed on the lab robot. Opening the tablet app left the service inactive. Actual CONNECT on the tablet started it and enabled START; service remained disabled for boot. START was not pressed during this validation.
- Fixed SSH negotiation to use library defaults and enabled PAM keyboard-interactive password authentication, matching the lab robot. Host identity was verified over the already trusted SSH connection before accepting it in the tablet app.
- Removed Base guard from Quest UI. Camera, depth, lidar and tablet preview show continuous CRT static on disconnected or stale inputs; cached camera images are not drawn after control disconnect.
- 134 Python checks passed; both Android APKs built and were installed. Motion stability and physical START/STOP behavior still require live operator validation.


## 2026-10-08: Exit TelePepper simulation under systemd

- Re-ran all 141 Python checks: passed.
- Started a separate transient user service with --simulate, a temporary pairing token and a separate loopback control port. Confirmed capabilities.simulation before sending return_to_normal with confirmed=true. No real qi services, motors or autonomy were used.
- Exit acknowledged normal_mode=true, service_stopping=true and armed=false. After two seconds systemd reported ExecMainStatus=0, ActiveState=inactive, SubState=dead and NRestarts=0 with Restart=on-failure.
- Removed the simulation service and token. The real TelePepper service remained stopped; its failed state was unchanged. Real UI and physical autonomy handover still require a supervised test after PepperGPT work is complete.

## Automatic speech gestures, 2026-10-08

159 Python tests pass, including all 141 prior tests plus optional package failure isolation, arm ownership, TTS/Piper/live activity, cancellation barriers, stale tracking, measured-pose return, source isolation and unchanged audio on gesture OFF. Eleven native tests ran on Quest: speech grip chord, start flow, deferred gestures, controller ray, panel anchor, dashboard and floating layouts, pose mirror, voice gain, Piper clip transport and network recovery. These tests use mocks or local test servers and do not command the real robot.

Both Android APKs compile. Physical gesture transitions and continued head/base movement while an actual clip runs still require a supervised operator trial; no physical latency or motion claim follows from these automated checks.

## PTT gestures and tablet fix, 2026-10-08

Both-grip toggle no longer requires presses within 0.2 seconds. Left-grip release requests a bounded gesture only with the mode enabled, fresh armed tracking and completed initial engagement. Voice playback continues to control gesture duration for TTS/Piper. Live capture does not initiate gestures. The release command never arms the robot.
The tablet has a compact single-screen configuration, CONNECT changes to START, DISPLAY opens without moving the robot, and Quest tracking or participant content can open the display automatically. Explicit settings remain open. Participant display retains only its settings icon and status icons. Layout bounds were checked on the actual tablet; physical speech gesture execution still requires an operator trial.

Final release check: 161 Python tests passed. Both variant builds completed; 2.9 apps installed. Physical 2.5 compatibility and speech gesture performance are not established by the mock tests.

The camera cadence test now isolates its fake clock/sleep from background test threads; production camera code was unchanged. Final full suites pass: 161 for 2.9 and 169 for 2.5.

## 0.3.5 review fixes

Animation cancellation failures no longer skip base STOP. Piper temporary files and clip locks are cleaned even if gesture cancellation fails. A newer STOP during normal-mode handoff cancels Exit and requests disabled autonomy again. These paths have fault-injection tests; physical acknowledgement timing still needs operator verification.
