# Controls - Pepper 2.5 fork

| Control | Action |
| --- | --- |
| CONNECT in Quest settings | Install once/start head service directly from Quest |
| Hold A + X | Calibrate/start or voluntarily stop |
| STOP | Pause and gently return to neutral |
| B | Immediate stop; cancel animation/neutral return |
| Left / right stick | Translate / rotate base |
| Front triggers | Close hands |
| Right grip + trigger | Aim UI ray and click |
| Left grip | Push to talk |
| Camera / Depth / Lidar | Toggle stream; offline panels show static |
| Pin / handle / Layout | Position and resize panels |
| Exit TelePepper | Confirm stop, restore normal autonomy, end service |

No tablet controls, captions, emoji or preview are included. Head/arm mirroring, torso assistance, base rotation, thermal monitoring, LEDs and voice remain subject to available robot services. Default: Pepper TTS, volume 50, Listen OFF. Piper/Cori needs its optional Quest voice engine.

The service is under /home/nao/telepepper25, never starts at boot and needs no running PC. Quest stores SSH credentials encrypted and creates pairing automatically.

## Automatic speech gestures

OFF by default. Hold both grips for 0.35 seconds to toggle; release both before toggling again. With gestures ON and tracking started, release the left push-to-talk grip to trigger a gesture. Piper and Pepper TTS keep arm gestures active during playback. Live microphone mode triggers a short gesture on release. Head and base remain under live control. STOP cancels gestures; arms return gradually to measured-pose tracking.

The 2.5 bridge uses arm-only keyframes derived from the attributed official Pepper Core Animations through ALMotion. It does not require the 2.9 animation package manager. This remains untested on a physical Pepper 2.5.

## Versions

The Quest header shows the installed app version and build number. Help shows both Quest and the head-service version. On Android Pepper, configuration shows the tablet and head-service versions below the title; the participant display stays uncluttered. An older or unavailable head service is shown as unknown rather than guessed.
