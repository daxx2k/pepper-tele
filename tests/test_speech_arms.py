import io
import json
import pathlib
import socket
import struct
import sys
import time
import unittest
import xml.etree.ElementTree as ET
import zipfile
from unittest.mock import Mock

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'pepper/bridge'))
import telepepper as tp
from speech_arms import PATHS, install_package


class SpeechArmTests(unittest.TestCase):
    def setUp(self):
        self.robot=tp.Robot(True)
        self.now=10.
        self.arms=self.robot.speech_arms
        self.arms.clock=lambda:self.now
        self.robot.armed_at=0.

    def hardware(self):
        self.robot.simulate=False
        self.robot.motion=Mock()
        completed=Mock();completed.isFinished.return_value=True
        self.robot.motion.setAngles.return_value=completed
        self.robot.motion.moveToward.return_value=completed
        self.robot.available_animations.update(PATHS)
        self.robot.animation_player=Mock()
        self.clip=Mock();self.clip.isFinished.return_value=False
        self.robot.animation_player.run.return_value=self.clip

    def begin(self):
        self.arms.set_enabled(True);self.arms.begin(5.)

    def test_default_off_does_not_claim_joints(self):
        self.hardware();self.arms.begin(5.)
        self.robot.apply(list(self.robot.previous),[0,0,0],.02)
        self.robot.animation_player.run.assert_not_called()
        names=[c.args[0] for c in self.robot.motion.setAngles.call_args_list]
        self.assertIn(tp.NAMES[2:12],names);self.assertIn(tp.NAMES[12:],names)

    def test_clips_only_contain_arm_curves_with_gentle_first_key(self):
        allowed=set(tp.NAMES[2:])
        with zipfile.ZipFile(ROOT/'pepper/bridge/telepepper-speaking.pkg') as z:
            for path in PATHS:
                root=ET.fromstring(z.read(path.split('/')[-1]))
                self.assertEqual(set(c.tag for c in root),{'ActuatorCurve'})
                for curve in root:
                    self.assertIn(curve.get('actuator'),allowed)
                    self.assertGreaterEqual(min(float(k.get('frame'))/float(curve.get('fps')) for k in curve.findall('Key')),.75)
            self.assertIn(b'SoftBank',z.read('COPYING'))

    def test_optional_package_failure_is_contained(self):
        session=Mock();manager=session.service.return_value
        manager.hasPackage.return_value=False
        manager.install.return_value.value.side_effect=RuntimeError('package failed')
        self.assertFalse(install_package(session,str(ROOT/'pepper/bridge/telepepper-speaking.pkg')))
        session.service.assert_called_once_with('PackageManager')

    def test_missing_optional_package_does_not_call_sdk(self):
        session=Mock()
        self.assertFalse(install_package(session,str(ROOT/'pepper/bridge/not-a-package.pkg')))
        session.service.assert_not_called()

    def test_missing_library_refuses_enable(self):
        self.robot.simulate=False
        with self.assertRaises(ValueError):self.arms.set_enabled(True)
        self.assertFalse(self.arms.enabled)

    def test_head_base_torso_continue_and_arms_hands_are_exclusive(self):
        self.hardware();self.begin()
        target=list(self.robot.previous);target[0]=.4;target[2]=0
        self.robot.apply(target,[.4,0,.4],.02,torso=[.01,.02])
        self.robot.animation_player.run.assert_called_once()
        names=[c.args[0] for c in self.robot.motion.setAngles.call_args_list]
        self.assertIn(tp.NAMES[:2],names);self.assertIn(['HipRoll','HipPitch'],names)
        self.assertNotIn(tp.NAMES[2:12],names);self.assertNotIn(tp.NAMES[12:],names)
        self.robot.motion.moveToward.assert_called_once()
        self.assertEqual(self.robot.previous[0],.4)

    def test_waits_for_existing_arm_command_without_waiting_for_head(self):
        self.hardware();self.begin()
        pending=Mock();pending.isFinished.return_value=False
        self.robot.motion_pending['arms']=(pending,tp.clock())
        self.assertTrue(self.arms.poll(tp.NAMES));self.robot.animation_player.run.assert_not_called()
        pending.isFinished.return_value=True
        self.arms.poll(tp.NAMES);self.robot.animation_player.run.assert_called_once()

    def test_off_cancels_and_waits_then_blends_measured_arms_only(self):
        self.hardware();self.begin();self.arms.poll(tp.NAMES)
        self.arms.set_enabled(False);self.clip.cancel.assert_called_once()
        self.assertTrue(self.arms.poll(tp.NAMES));self.robot.motion.getAngles.assert_not_called()
        self.clip.isFinished.return_value=True
        measured=list(self.robot.previous);measured[0]=-.3;measured[2]=.5
        read=Mock();read.isFinished.return_value=True;read.value.return_value=measured
        self.robot.motion.getAngles.return_value=read
        old_head=self.robot.previous[0]
        self.assertFalse(self.arms.poll(tp.NAMES))
        self.assertEqual(self.robot.previous[0],old_head)
        self.assertEqual(self.arms.target(0,1.),1.)
        self.assertEqual(self.arms.target(2,1.5),.5)
        self.now+=.375;self.assertAlmostEqual(self.arms.target(2,1.5),1.)
        self.now+=.375;self.assertEqual(self.arms.target(2,1.5),1.5)

    def test_finished_clip_returns_then_repeats_only_during_speech(self):
        self.hardware();self.begin();self.arms.poll(tp.NAMES)
        self.clip.isFinished.return_value=True
        read=Mock();read.isFinished.return_value=True;read.value.return_value=list(self.robot.previous)
        self.robot.motion.getAngles.return_value=read
        self.assertTrue(self.arms.poll(tp.NAMES));self.assertEqual(self.arms.phase,'waiting')
        self.arms.end();self.assertFalse(self.arms.poll(tp.NAMES))

    def test_stop_cancellation_is_in_existing_hold_barrier(self):
        self.hardware();self.begin();self.arms.poll(tp.NAMES)
        self.robot.stop()
        self.clip.cancel.assert_called_once();self.assertIn('speech_cancel',self.robot.stop_pending)
        self.robot.motion.getAngles.assert_not_called()
        self.robot.poll_stop();self.robot.motion.getAngles.assert_not_called()
        self.clip.isFinished.return_value=True;self.robot.poll_stop()
        self.robot.motion.getAngles.assert_called()
        self.assertFalse(self.arms.speaking)

    def test_broken_animation_cancel_still_brakes_base_and_blocks_restart(self):
        self.hardware()
        future=Mock();future.cancel.side_effect=RuntimeError('cancel failed')
        self.robot.animation_future=future
        self.robot.stop()
        self.robot.motion.moveToward.assert_called_once_with(0.,0.,0.,_async=True)
        self.robot.motion.stopMove.assert_called_once_with(_async=True)
        self.assertIn('cancel failed',self.robot.stop_error)
        self.robot.motion.getAngles.assert_not_called()

    def test_broken_stop_completion_does_not_skip_other_braking_acknowledgements(self):
        self.hardware()
        broken=Mock();broken.isFinished.side_effect=RuntimeError('lost future')
        completed=Mock();completed.isFinished.return_value=True
        self.robot.stop_pending={'base_zero':(broken,time.monotonic()),'base_stop':(completed,time.monotonic())}
        with self.assertRaisesRegex(RuntimeError,'completion check failed'):self.robot.poll_stop()
        completed.value.assert_called_once_with(0)
        self.assertNotIn('base_stop',self.robot.stop_pending)
        self.assertIn('base_zero',self.robot.stop_pending)
        error=self.robot.stop_error
        self.robot.poll_stop();self.assertEqual(self.robot.stop_error,error)

    def test_broken_speech_cancel_still_brakes_base(self):
        self.hardware();self.begin();self.arms.poll(tp.NAMES)
        self.clip.cancel.side_effect=RuntimeError('speech cancel failed')
        self.robot.stop()
        self.robot.motion.stopMove.assert_called_once_with(_async=True)
        self.assertIn('speech cancel failed',self.robot.stop_error)
        self.assertEqual(self.arms.phase,'idle')

    def test_piper_cancel_failure_does_not_leak_clip_lock_or_temporary_file(self):
        service=Mock(port=0);service.state=tp.State(self.robot)
        woz=tp.WoZ(service,time.monotonic)
        player=Mock();player.playFile.return_value.isFinished.return_value=True
        woz.services['ALAudioPlayer']=player;woz.authorized=Mock(return_value=True)
        self.arms.end=Mock(side_effect=RuntimeError('cancel failed'))
        a,b=socket.socketpair()
        try:
            woz.play_piper(a,io.BytesIO(b'\0\0'*320),{'session':'pilot','piper_bytes':640},'peer')
            self.assertTrue(woz.clip_lock.acquire(False));woz.clip_lock.release()
            self.assertIsNone(woz.clip_future)
            self.assertFalse(pathlib.Path(player.playFile.call_args.args[0]).exists())
        finally:a.close();b.close();woz.audio_socket.close()

    def test_timeout_discards_speech_ownership(self):
        self.hardware();self.begin();self.arms.poll(tp.NAMES)
        state=tp.State(self.robot);state.armed=True;state.received=1.
        state.tick(1.2,.02)
        self.assertFalse(state.armed);self.clip.cancel.assert_called()
        self.assertEqual(self.arms.phase,'idle')

    def test_animation_failure_propagates(self):
        self.hardware();self.begin();self.arms.poll(tp.NAMES)
        self.clip.isFinished.return_value=True;self.clip.value.side_effect=RuntimeError('clip failed')
        with self.assertRaisesRegex(RuntimeError,'clip failed'):self.arms.poll(tp.NAMES)

    def test_mode_off_does_not_stop_audio_or_tts(self):
        self.hardware();self.begin();self.arms.poll(tp.NAMES)
        service=Mock(port=0);service.state=tp.State(self.robot)
        self.robot.simulate=True
        woz=tp.WoZ(service,time.monotonic)
        player=Mock();tts=Mock();woz.services.update(ALAudioPlayer=player,ALTextToSpeech=tts)
        try:
            woz.action({'cmd':'speech_gestures','enabled':False})
            player.stopAll.assert_not_called();tts.stopAll.assert_not_called()
            self.clip.cancel.assert_called_once()
        finally:woz.audio_socket.close()

    def test_piper_does_not_start_arms_when_paused_and_caption_is_atomic(self):
        service=Mock(port=0);service.state=tp.State(self.robot)
        woz=tp.WoZ(service,time.monotonic)
        done=Mock();done.isFinished.return_value=True
        player=Mock();player.playFile.return_value=done;woz.services['ALAudioPlayer']=player
        woz.authorized=Mock(return_value=True)
        self.begin();self.arms.end()
        a,b=socket.socketpair()
        try:
            woz.play_piper(a,io.BytesIO(b'\0\0'*320),{'session':'pilot','piper_bytes':640,'text':'Hello.'},'peer')
            self.assertIn(b'played',b.recv(512));self.assertEqual(woz.tablet['text'],'Hello.')
            self.assertEqual(self.arms.phase,'idle')
        finally:a.close();b.close();woz.audio_socket.close()

    def test_live_voice_activity_and_silence_hangover(self):
        service=Mock(port=0);service.state=tp.State(self.robot)
        state=service.state;state.armed=True;state.received=self.now
        woz=tp.WoZ(service,lambda:self.now)
        try:
            self.arms.set_enabled(True)
            woz.live_speech_gestures(b'\0\0'*320)
            self.assertFalse(self.arms.speaking)
            woz.live_speech_gestures(struct.pack('<320h',*([1000]*320)))
            self.assertTrue(self.arms.speaking);self.assertEqual(self.arms.source,'live')
            self.now+=.5;woz.live_speech_gestures(b'\0\0'*320)
            self.assertFalse(self.arms.poll(tp.NAMES));self.assertFalse(self.arms.speaking)
        finally:woz.audio_socket.close()

    def test_ptt_release_requires_enabled_and_started_tracking(self):
        service=Mock(port=0);service.state=tp.State(self.robot)
        woz=tp.WoZ(service,lambda:self.now)
        try:
            with self.assertRaisesRegex(ValueError,'Enable speech gestures'):
                woz.action({'cmd':'speech_gesture_trigger'})
            self.arms.set_enabled(True)
            with self.assertRaisesRegex(ValueError,'Start tracking'):
                woz.action({'cmd':'speech_gesture_trigger'})
            self.assertFalse(self.arms.speaking)
        finally:woz.audio_socket.close()

    def test_ptt_release_starts_a_bounded_gesture_without_arming_robot(self):
        service=Mock(port=0);service.state=tp.State(self.robot)
        service.state.armed=True;service.state.received=self.now
        woz=tp.WoZ(service,lambda:self.now)
        try:
            self.arms.set_enabled(True);woz.action({'cmd':'speech_gesture_trigger'})
            self.assertTrue(self.arms.speaking);self.assertEqual(self.arms.source,'ptt')
            self.assertEqual(self.arms.until,self.now+4.)
        finally:woz.audio_socket.close()

    def test_native_tts_starts_arms_only_when_mode_enabled(self):
        service=Mock(port=0);service.state=tp.State(self.robot)
        state=service.state;state.armed=True;state.received=self.now
        woz=tp.WoZ(service,lambda:self.now)
        tts=Mock();tts.say.return_value.isFinished.return_value=False
        woz.services['ALTextToSpeech']=tts
        try:
            woz.action({'cmd':'say','text':'Hello.'})
            self.assertFalse(self.arms.speaking)
            woz.speech_future=None;self.arms.set_enabled(True)
            woz.action({'cmd':'say','text':'How are you?'})
            self.assertTrue(self.arms.speaking);self.assertEqual(self.arms.source,'tts')
        finally:woz.audio_socket.close()

    def test_old_tts_completion_does_not_cancel_new_piper_gestures(self):
        self.arms.set_enabled(True);self.arms.begin(2.,'piper')
        self.arms.end('tts')
        self.assertTrue(self.arms.speaking)

    def test_initial_arm_engagement_and_stale_tracking_do_not_start_gestures(self):
        service=Mock(port=0);service.state=tp.State(self.robot)
        state=service.state;state.armed=True;state.received=self.now
        woz=tp.WoZ(service,lambda:self.now)
        try:
            self.arms.set_enabled(True);self.robot.armed_at=self.now
            woz.begin_speech_gestures(5.,'piper');self.assertFalse(self.arms.speaking)
            self.robot.armed_at=0.;state.received=1.
            woz.begin_speech_gestures(5.,'piper');self.assertFalse(self.arms.speaking)
        finally:woz.audio_socket.close()


if __name__=='__main__':unittest.main()
