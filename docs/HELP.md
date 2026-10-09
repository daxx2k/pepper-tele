# Controls and troubleshooting

Open TelePepper on Pepper's tablet. Press **CONNECT** at bottom left of Connection to start
the head service, then **START** to prepare Pepper and open the participant display.
START can move Pepper into its operating posture; keep clear of its arms.
Engage live tracking separately from Quest with A + X. The head service no longer
starts at robot boot. CONNECT is idempotent and does not restart an already active
session. First-time setup installs the bundled bridge; subsequent connections start
the installed service without overwriting manual head updates.

| Control | Action |
| --- | --- |
| Hold A + X for 0.75 seconds | Start or stop; voluntary stop gently returns to neutral after the wheels stop. Centre sticks and look forward for calibration. |
| B | Immediate motion STOP / cancel animation |
| Recalibrate | Pause, look forward with relaxed arms and neutral wrists; refresh all controlled pose references and clear old offsets. If already running, live control resumes gently; if paused, it stays paused. |
| Left stick | Forward/backward and sideways movement |
| Right stick | Turn the robot |
| Front triggers | Close Pepper hands; right trigger clicks when pointing |
| Right grip + front trigger | Blue controller ray aims at the dashboard; front trigger clicks |
| Left grip | Push to talk; release to synthesize with selected voice |
| Bottom window handle, grip + trigger | Move/rotate window; right stick up/down moves farther/nearer, left/right changes size |
| Pin | Switch room lock / head-follow for the window or independent panels |
| Layout, in the header | Arrange independent VR panels; motion pauses while editing |
| Panel title, grip + trigger in Layout | Move/rotate that panel; right stick up/down moves farther/nearer, left/right changes size |
| Preset 1/2/3, Save / Load | Store and restore three arrangements |
| Reset / Done | Restore default positions / finish editing; START resumes motion |
| Help, top right | Toggle a smaller guide beside the dashboard on the same plane; tracking and dashboard remain usable |

Dashboard STOP and the A + X stop gesture request a gentle neutral pose. B interrupts
that transition immediately. Connection failures, timeouts and internal setup stops
do not request a neutral pose. The return waits for completed braking and measured
joint readings; failed stop acknowledgements prevent it. Tracking-loss neutral return
continues to use its existing separate behavior.

For supervised base tests, the editable `BASE_TEST_CAP` constant in
`pepper/bridge/telepepper.py` limits the normalized translation magnitude and rotation,
even with Speed limits OFF or automatic base rotation active. It accepts values in
`(0, 0.1]` and is `None` in normal operation. This is a fraction of the SDK maximum,
not a measured physical speed. Restore normal obstacle margins before removing it.

Piper/Cori is the default synthesized voice. The live puppeteer mode forwards microphone audio; Pepper TTS uses transcription and Pepper's voice. Speech transcription depends on the Quest speech service/model. Listen is OFF by default to avoid feedback.

Tap a camera feed to toggle its stream. Tap Lidar to toggle display; this does not disable robot obstacle sensing. Tablet preview toggles locally; Welcome displays a greeting without speech. Text, speech captions and emoji stay visible for 8 seconds, then fade out over 1 second. New content restarts the timer. The Quest preview follows the same fade; connection/settings icons stay visible. Expired content is not replayed after reconnection. LED swatches retain separate last-selected preferences for eyes, shoulders and ears. Ear LEDs support blue intensity presets.

The panel contains eight official gestures: Wave left/right, Point left/right, Affirm, Refuse, Happy reaction and Sad reaction. Affirm/Refuse are expressive library clips, not necessarily head-only nods/shakes. Greeting, pointing and reaction clips come from SoftBank Robotics Pepper Core Animations; Affirm/Refuse use the installed library. The playing animation stays highlighted; other animation buttons are greyed out and cannot be selected until it finishes. STOP remains available. Playback owns the joints exclusively. Successful completion resumes live tracking from the measured robot pose with a short smooth transition. STOP, tracking loss, timeout or playback errors cancel this return. Centre the sticks before driving again.

## Connection fails

Start the tablet app and wait for its service status. Pair the Quest using the same code. Use the Pepper HEAD Wi-Fi address, not its tablet ADB address. Both devices must share a network that allows peer communication. Discovery helps find the paired robot; it does not replace authentication. Reconnect starts paused.

## Start refused / motion paused

Read the exact bottom status message. Check tracking, live camera freshness and centred sticks. If posture preparation or STOP is still completing, wait for completion before retrying. Moving the window pauses control; opening Help does not. Missing tracking, timeouts and service errors can pause motion. Do not repeatedly click animation buttons during preparation or playback.

On first START, preparation can disable Autonomous Life and wake the motors while teleoperation remains paused. Brief tracking or video interruptions during this preparation do not cancel the wake-up. Fresh tracking, camera data and centred sticks are required before arming. STOP, loss of application focus or disconnection still cancel preparation.

The tablet setup screen groups pairing, connection and robot controls into cards. Advanced setup contains the SSH credentials; the participant display remains separate. Its small settings icon at bottom right opens configuration; holding the status icons still works too.

The robot service starts with blue eyes and shoulders and fully lit blue ears. Colour presets remain available; ear LEDs support blue intensity only.

## Audio

Keep Listen OFF if you hear echo. Choose the voice mode, use a phrase preset or hold/release left grip. Stop speech interrupts playback. Piper phrases are serialized and wait for robot playback completion. A lost connection does not automatically replay uncertain speech.

## Depth / lidar / temperatures

Depth: bright is near, dark is far, black also represents missing data. OFF panels show static; stale feeds are labelled. Lidar is a sensor trace rather than SLAM. Temperature colours use Celsius thresholds; the C/F switch changes display units only. Investigate robot thermal alerts before continuing.

## Independent panels

The first Layout click separates the dashboard cards in the room, initially in their familiar positions. Hold right grip, point at a panel title and hold the trigger to move and rotate it. While holding it, right stick up moves it away and down brings it closer; left/right adjusts its size. The header and status strip can also be moved by their empty top areas. Done keeps the panels independent and re-enables their controls. Re-enter Layout to rearrange them.

Current positions, sizes and three saved presets persist across app restarts. Presets restore relative to the workspace placed in front of you; they are not persistent spatial anchors tied to a physical room. Reset restores the original arrangement without deleting presets. Pin switches the whole arrangement between room lock and head follow, preserving relative panel positions. The footer handle moves the entire workspace. Motion stays paused after editing until START or A + X.

## Interface and setup screens




TelePepper uses one Horizon-inspired style across VR, connection settings and the Pepper tablet: charcoal surfaces, rounded cards, readable neutral text and blue selected controls. The VR header has filled Layout and Help icons with hover labels. No theme selector is needed.

Robot limit controls in Motion pause control before applying a change. START resumes afterwards. Joint limits toggles TelePepper's head/arm joint envelope within actual hardware bounds. Speed limits switches normal speed fractions/manual stick caps to the full normalized firmware range. The Base guard button has been removed; this does not disable obstacle protection. Mechanical bounds, body/self-collision protection, initial engagement blending, torso assistance stability caps, STOP and the robot input watchdog remain. Speed/range controls default ON at service startup; these controls do not enable motion by themselves.


Exit TelePepper with **Exit TelePepper** on Quest (connection controls) or on Pepper's tablet (robot controls / participant screen). The explicit exit waits for STOP, cancels speech, returns Autonomous Life to normal mode and stops the head service. It does not change joint stiffness to a fixed value or request a neutral pose. Use CONNECT on the Pepper tablet to start the service again before the next Quest session. Connection Settings keeps the session safely paused while editing. Wi-Fi loss, a watchdog stop and app/process crashes keep Pepper stopped; use an explicit exit or the robot's controls to restore normal mode. Force-closing an app from the OS cannot guarantee that an exit request reaches the robot.

## Automatic speech gestures

OFF at startup. Hold both grips for 0.35 seconds to toggle (no simultaneous-press timing requirement); release both before toggling again. Single left grip remains push-to-talk; single right grip remains the UI pointer. The Voice panel also shows the mode.

When enabled and live tracking is armed, Pepper uses arm-only derivatives of official Pepper Core Animations during Pepper TTS, Piper/Cori playback and release-triggered live microphone gestures. Head, torso assist and joystick base control continue. This is speech activity timing, not semantic gesture generation. Native motor limits and body collision protection remain active. Initial motion engagement completes before any speech gesture starts.

After speech ends, the bridge waits for the animation to stop, reads the measured arm pose and blends back to live tracking over 0.75 seconds. Turning gestures OFF does not stop speech. STOP, tracking timeout and disconnect cancel gestures through the existing stop barrier. Other animation buttons are unavailable while speaking gestures own the arms.

With Gestures ON, releasing the left push-to-talk grip triggers an arm gesture after initial motion engagement. Piper and Pepper TTS also keep gestures active during the resulting audio. Live microphone mode triggers a bounded gesture on release, rather than during capture. CONNECT on the tablet becomes START after connection. DISPLAY opens the participant screen without preparing motion; live tracking or new participant content also opens it automatically, except when settings were opened explicitly.

## Versions

The Quest header shows the installed app version and build number. Help shows both Quest and the head-service version. On Android Pepper, configuration shows the tablet and head-service versions below the title; the participant display stays uncluttered. An older or unavailable head service is shown as unknown rather than guessed.

**Update service** in tablet Connection upgrades the installed head scripts while tracking is stopped, keeps numeric base settings and stores a backup. CONNECT starts the installed service; it becomes START after connection.
