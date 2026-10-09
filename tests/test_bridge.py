import importlib.util
import json
import math
import pathlib
import socket
import struct
import threading
import time
import unittest
from unittest.mock import patch, Mock

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('telepepper', ROOT/'pepper/bridge/telepepper.py')
tp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tp)

class MotionTests(unittest.TestCase):
    @patch.object(tp,'BASE_ORTHOGONAL_SECURITY_M',.4)
    @patch.object(tp,'BASE_TANGENTIAL_SECURITY_M',.1)
    def test_base_security_settings_are_applied_and_verified(self):
        motion=Mock()
        motion.getOrthogonalSecurityDistance.return_value.value.side_effect=[.4,.4]
        motion.getTangentialSecurityDistance.return_value.value.side_effect=[.1,.1]
        self.assertEqual(tp.configure_base_security(motion),(.4,.1))
        motion.setOrthogonalSecurityDistance.assert_called_with(.4,_async=True)
        motion.setTangentialSecurityDistance.assert_called_once_with(.1,_async=True)

    def test_base_security_invalid_settings_do_not_touch_robot(self):
        for orthogonal,tangential in [(0,.1),(.4,-.1),(.05,.1),(float('nan'),.1),(.4,float('inf'))]:
            motion=Mock()
            with patch.object(tp,'BASE_ORTHOGONAL_SECURITY_M',orthogonal),patch.object(tp,'BASE_TANGENTIAL_SECURITY_M',tangential):
                with self.assertRaises(ValueError):tp.configure_base_security(motion)
            self.assertEqual(motion.mock_calls,[])

    def test_base_security_readback_mismatch_fails(self):
        motion=Mock()
        motion.getOrthogonalSecurityDistance.return_value.value.side_effect=[.4,.8]
        motion.getTangentialSecurityDistance.return_value.value.side_effect=[.1,.1]
        with self.assertRaises(RuntimeError):tp.configure_base_security(motion)

    def test_base_test_cap_bounds_diagonal_rotation_and_release(self):
        with patch.object(tp,'BASE_TEST_CAP',0.05):
            robot=tp.Robot(True)
        robot.speed_limited=False
        for _ in range(100):robot.apply(list(robot.previous),[1.,1.,1.],.02)
        self.assertLessEqual(math.hypot(*robot.base[:2]),.050000001)
        self.assertLessEqual(abs(robot.base[2]),.05)
        robot.apply(list(robot.previous),[0.,0.,0.],.02)
        self.assertEqual(robot.base,[0.,0.,0.])

    def test_base_test_cap_rejects_invalid_configuration(self):
        for value in ('nan','inf','0','-1','.2'):
            with patch.object(tp,'BASE_TEST_CAP',value):
                with self.assertRaises(ValueError):tp.Robot(True)

    def test_bottom_manual_exposure_is_repaired_once(self):
        video=Mock()
        video.getParameter.return_value.value.side_effect=[0,0]
        video.setParameter.return_value.value.return_value=True
        self.assertEqual(tp.prepare_bottom_camera(video),{'auto_exposure':1,'auto_gain':1})
        self.assertEqual(video.setParameter.call_count,2)
        video.setParameter.assert_any_call(1,11,1,_async=True)
        video.setParameter.assert_any_call(1,13,1,_async=True)
        video.reset_mock()
        video.getParameter.return_value.value.side_effect=[1,1]
        tp.prepare_bottom_camera(video)
        video.setParameter.assert_not_called()

    def test_bottom_unsupported_setting_is_reported_without_breaking_stream(self):
        video=Mock()
        video.getParameter.return_value.value.side_effect=RuntimeError('Unsupported parameter')
        settings=tp.prepare_bottom_camera(video)
        self.assertIn('auto_exposure_error',settings)
        self.assertIn('auto_gain_error',settings)
        video.setParameter.assert_not_called()

    def test_default_speaker_volume_applied_without_playback(self):
        service=Mock(port=0,video_diagnostics=[])
        service.state.robot.simulate=False
        device=Mock()
        service.state.robot.session.service.return_value=device
        woz=tp.WoZ(service,time.monotonic)
        try:
            device.setOutputVolume.assert_called_once_with(50)
            self.assertEqual(device.fadeRGB.call_args_list,
                             [unittest.mock.call('FaceLeds',0x4388ff,.2)])
            self.assertEqual(device.setIntensity.call_args_list,
                             [unittest.mock.call('ChestLedsRed',67/255.),
                              unittest.mock.call('ChestLedsGreen',136/255.),
                              unittest.mock.call('ChestLedsBlue',1.),
                              unittest.mock.call('EarLeds',1.)])
            device.say.assert_not_called()
            self.assertEqual(woz.speaker_volume,50)
        finally:woz.audio_socket.close()

    def setUp(self):
        self.robot = tp.Robot(True)
        self.state = tp.State(self.robot)
        self.state.session = 'pilot'
        self.state.peer = '127.0.0.1'
        self.state.armed = True
        self.robot.arm()

    def packet(self, seq=1, **kw):
        p = dict(session='pilot', seq=seq, sent=1000., active=True, drive=True,
                 angles=list(self.robot.previous), base=[1, -1, 1])
        p.update(kw)
        return p

    def test_restart_cleans_only_owned_camera_registrations(self):
        video=Mock()
        video.getSubscribers.return_value.value.return_value=['TP_22b85615aa3d_0','TP_937538c91e5b_0','HumanPerception_0','Camera_0','ALPodDetection_0','TP_another_app']
        tp.cleanup_video_subscriptions(video)
        self.assertEqual([call.args[0] for call in video.unsubscribe.call_args_list],['TP_22b85615aa3d_0','TP_937538c91e5b_0'])

    def test_extended_head_and_overhead_targets_remain_bounded(self):
        target=list(self.robot.previous);target[1]=1.;target[2]=-2.;target[7]=-2.
        with patch.object(tp,'clock',return_value=self.robot.armed_at+4.):
            self.robot.apply(target,[0,0,0],.01)
        self.assertEqual(self.robot.previous[1],.43)
        self.assertEqual(self.robot.previous[2],-2.)
        self.assertEqual(self.robot.previous[7],-2.)

    def test_unknown_pilot_and_replay_rejected(self):
        self.assertFalse(self.state.accept(self.packet(session='wrong'), '127.0.0.1', 1))
        self.assertFalse(self.state.accept(self.packet(), '127.0.0.2', 1))
        self.assertTrue(self.state.accept(self.packet(2), '127.0.0.1', 1))
        self.assertFalse(self.state.accept(self.packet(1), '127.0.0.1', 1))

    def test_base_policy_preserves_body_protection(self):
        self.robot.simulate=False
        self.robot.motion=Mock()
        self.robot.motion.getExternalCollisionProtectionEnabled.return_value=False
        self.robot.set_base_collision(False)
        self.assertFalse(self.robot.base_collision_enabled)
        self.robot.motion.setExternalCollisionProtectionEnabled.assert_any_call('Arms',True)
        self.robot.motion.setExternalCollisionProtectionEnabled.assert_any_call('Move',False)
        self.robot.motion.setCollisionProtectionEnabled.assert_called_with('Arms',True)

    def test_firmware_refusal_is_propagated_and_actual_policy_kept(self):
        self.robot.simulate=False
        self.robot.motion=Mock()
        self.robot.motion.getExternalCollisionProtectionEnabled.return_value=True
        def setter(name,enabled):
            if name=='Move':raise RuntimeError('Owner consent required')
        self.robot.motion.setExternalCollisionProtectionEnabled.side_effect=setter
        with self.assertRaisesRegex(RuntimeError,'Owner consent'):self.robot.set_base_collision(False)
        self.assertTrue(self.robot.base_collision_enabled)

    def test_arm_does_not_restore_base_obstacle_stopping(self):
        self.robot.simulate=False
        self.robot.motion=Mock()
        self.robot.motion.getAngles.return_value=list(self.robot.previous)
        self.robot.life=Mock();self.robot.life.getState.return_value='disabled'
        self.robot.base_collision_enabled=False
        self.robot.arm()
        self.robot.motion.setExternalCollisionProtectionEnabled.assert_any_call('Move',False)
        self.robot.motion.setExternalCollisionProtectionEnabled.assert_any_call('Arms',True)

    def test_joint_range_toggle_uses_hardware_bounds_and_restores_envelope(self):
        self.robot.hardware_limits[0]=(-2.08,2.08)
        self.robot.set_motion_limit('range',False)
        self.assertEqual(self.robot.limits[0],(-2.08,2.08))
        target=list(self.robot.previous);target[0]=9.
        with patch.object(tp,'clock',return_value=self.robot.armed_at+4):self.robot.apply(target,[0,0,0],.02)
        self.assertEqual(self.robot.previous[0],2.08)
        self.robot.set_motion_limit('range',True);self.assertEqual(self.robot.limits[0],(-1.,1.))

    def test_speed_limit_toggle_keeps_engagement_and_full_firmware_fraction(self):
        self.robot.set_motion_limit('speed',False)
        self.assertEqual(self.robot.speed_fraction('head',True),1.)
        self.assertEqual(self.robot.speed_fraction('head',False),.12)
        self.assertEqual(self.robot.speed_fraction('arms',True),1.)
        self.state.accept(self.packet(base=[2,-2,2]),'127.0.0.1',1)
        self.assertEqual(self.state.latest[1],[1.,-1.,1.])
        self.robot.set_motion_limit('speed',True);self.assertEqual(self.robot.speed_fraction('arms',True),.35)

    def test_only_latest_pose_is_applied(self):
        self.state.accept(self.packet(1), '127.0.0.1', 1)
        self.state.accept(self.packet(2), '127.0.0.1', 1.001)
        self.assertEqual(self.state.tick(1.005, .01)['seq'], 2)
        self.assertIsNone(self.state.tick(1.01, .01))

    def test_watchdog_stops_and_requires_stable_neutral_recovery(self):
        self.state.accept(self.packet(), '127.0.0.1', 1)
        self.state.tick(1.181, .01)
        self.assertFalse(self.state.armed)
        self.state.accept(self.packet(2, sent=1200), '127.0.0.1', 1.2)
        self.assertFalse(self.state.armed)
        self.assertIsNone(self.state.latest)

    def begin_timeout(self):
        self.state.accept(self.packet(base=[0,0,0]), '127.0.0.1',1.)
        self.state.tick(1.181,.01)
        self.assertFalse(self.state.armed)
        self.assertIsNotNone(self.state.recovery_until)

    def recovery_packets(self, **kw):
        options=dict(base=[0,0,0],sticks_neutral=True)
        options.update(kw)
        for n in range(30):
            now=1.2+n*.02
            self.state.accept(self.packet(n+2,sent=now*1000,**options), '127.0.0.1',now)
            with patch.object(tp,'clock',return_value=now):self.state.maybe_recover(now)

    def test_video_interruption_recovers_with_fresh_neutral_stream(self):
        self.state.accept(self.packet(active=False,pause_reason='video'),'127.0.0.1',1.)
        self.assertFalse(self.state.armed)
        self.assertIsNotNone(self.state.recovery_until)
        self.recovery_packets()
        self.assertTrue(self.state.armed)

    def test_video_packets_do_not_extend_recovery_window(self):
        self.state.accept(self.packet(active=False,pause_reason='video'),'127.0.0.1',1.)
        until=self.state.recovery_until
        self.state.accept(self.packet(2,sent=1200,active=False,pause_reason='video'),'127.0.0.1',1.2)
        self.assertEqual(self.state.recovery_until,until)
        self.state.maybe_recover(until+.01)
        self.assertIsNone(self.state.recovery_until)

    def test_timeout_recovery_rereads_pose_and_smoothly_reengages(self):
        self.begin_timeout()
        self.robot.previous[2]=.4
        self.recovery_packets()
        self.assertTrue(self.state.armed)
        self.assertEqual(self.state.recovery_count,1)
        self.assertEqual(self.robot.origin[2],.4)
        self.assertIsNone(self.state.recovery_until)

    def test_recovery_skips_blend_and_uses_firmware_following(self):
        with patch.object(tp,'clock',return_value=10.):
            self.robot.arm(resume=True)
            origin=list(self.robot.previous)
            target=list(origin);target[2]=origin[2]-1.;target[12]=0.
            self.robot.apply(target,[0,0,0],.01)
        self.assertAlmostEqual(self.robot.armed_at,7.)
        self.assertGreater(abs(self.robot.previous[2]-origin[2]),0.)
        self.assertEqual(self.robot.previous[2],target[2])
        self.assertLessEqual(abs(self.robot.previous[12]-origin[12]),3.*.01+1e-9)
        with patch.object(tp,'clock',return_value=20.):
            self.robot.arm()
            self.assertEqual(self.robot.armed_at,20.)
            origin=list(self.robot.previous)
            self.robot.apply(target,[0,0,0],.01)
            self.assertEqual(self.robot.previous,origin)

    def test_explicit_stop_during_pause_permanently_cancels_recovery(self):
        self.begin_timeout()
        self.state.accept(self.packet(2,sent=1190,stop=True),'127.0.0.1',1.19)
        self.recovery_packets()
        self.assertFalse(self.state.armed)
        self.assertIsNone(self.state.recovery_until)

    def test_tablet_stop_during_pause_cancels_recovery(self):
        self.begin_timeout();self.state.disarm('Tablet STOP')
        self.recovery_packets()
        self.assertFalse(self.state.armed)

    def test_tracking_loss_during_pause_cancels_recovery(self):
        self.begin_timeout()
        self.state.accept(self.packet(2,sent=1190,active=False),'127.0.0.1',1.19)
        self.recovery_packets()
        self.assertFalse(self.state.armed)

    def test_held_stick_blocks_recovery_even_if_disarmed_base_is_zero(self):
        self.begin_timeout();self.recovery_packets(sticks_neutral=False)
        self.assertFalse(self.state.armed)

    def test_recovery_expires_and_does_not_wake_robot(self):
        self.begin_timeout()
        self.state.maybe_recover(6.182)
        self.assertIsNone(self.state.recovery_until)
        self.assertFalse(self.state.armed)

    def test_failed_arm_check_cancels_recovery(self):
        self.begin_timeout()
        with patch.object(self.robot,'arm',side_effect=RuntimeError('Pepper must be awake')):
            self.recovery_packets()
        self.assertFalse(self.state.armed)
        self.assertIsNone(self.state.recovery_until)
        self.assertIn('must be awake',self.state.fault)

    def test_foreign_session_cannot_recover_motion(self):
        self.begin_timeout();self.recovery_packets(session='wrong')
        self.assertFalse(self.state.armed)

    def test_failed_stop_never_allows_recovery(self):
        with patch.object(self.robot,'stop',side_effect=RuntimeError('stop failed')):
            with self.assertRaises(RuntimeError):self.state.pause_for_timeout(1.)
        self.assertFalse(self.state.armed)
        self.assertIsNone(self.state.recovery_until)

    def test_persistent_tracking_loss_returns_gently_to_neutral_without_wheel_motion(self):
        self.robot.previous[2]=-.5;self.robot.previous[7]=-.5
        with patch.object(tp,'clock',return_value=1.):
            self.state.accept(self.packet(active=False,pause_reason='tracking'),'127.0.0.1',1.)
        self.assertFalse(self.state.armed);self.assertEqual(self.robot.base,[0,0,0])
        before=list(self.robot.previous);self.state.tick(1.4,.01);self.assertEqual(self.robot.previous,before)
        with patch.object(tp,'clock',return_value=1.51):self.state.tick(1.51,.01)
        self.assertGreater(self.robot.previous[2],before[2]);self.assertLess(self.robot.previous[2]-before[2],.01)
        for i in range(350):
            now=1.6+i*.06
            with patch.object(tp,'clock',return_value=now):self.state.tick(now,.06)
            self.assertEqual(self.robot.base,[0,0,0]);self.assertFalse(self.state.armed)
        self.assertAlmostEqual(self.robot.previous[2],1.65,places=1)
        self.assertAlmostEqual(self.robot.previous[7],1.65,places=1)
        self.assertIsNone(self.state.relax_at)

    def test_tracking_return_cancels_neutral_transition_without_autoarming(self):
        self.state.accept(self.packet(active=False,pause_reason='tracking'),'127.0.0.1',1)
        self.state.accept(self.packet(2,sent=1200),'127.0.0.1',1.2)
        self.assertIsNone(self.state.relax_at);self.assertFalse(self.state.armed)

    def test_explicit_stop_cancels_neutral_transition(self):
        self.state.accept(self.packet(active=False,pause_reason='tracking'),'127.0.0.1',1)
        self.state.disarm('Operator STOP');self.assertIsNone(self.state.relax_at)

    def test_operator_stop_returns_to_neutral_despite_live_tracking(self):
        self.robot.previous[2]=-.5;self.robot.previous[7]=-.5
        with patch.object(tp,'clock',return_value=1.):
            self.state.disarm('Operator STOP',return_to_neutral=True)
        self.state.accept(self.packet(2,sent=1200),'127.0.0.1',1.2)
        self.assertIsNotNone(self.state.relax_at)
        for i in range(350):
            now=1.6+i*.06
            with patch.object(tp,'clock',return_value=now):self.state.tick(now,.06)
            self.assertEqual(self.robot.base,[0,0,0]);self.assertFalse(self.state.armed)
        self.assertAlmostEqual(self.robot.previous[2],1.65,places=1)
        self.assertIsNone(self.state.relax_at)

    def test_emergency_stop_cancels_operator_neutral_transition(self):
        self.state.disarm('Operator STOP',return_to_neutral=True)
        self.state.disarm('Emergency STOP')
        before=list(self.robot.previous);self.state.tick(tp.clock()+1.,.02)
        self.assertEqual(self.robot.previous,before)
        self.assertIsNone(self.state.relax_at)

    def test_failed_stop_prevents_operator_neutral_motion(self):
        self.state.disarm('Operator STOP',return_to_neutral=True)
        self.robot.stop_error='Stop failed'
        before=list(self.robot.previous);self.state.tick(tp.clock()+1.,.02)
        self.assertEqual(self.robot.previous,before)

    def test_operator_neutral_completion_respects_hardware_hand_limits(self):
        self.robot.limits[12]=(.02,.98);self.robot.limits[13]=(.02,.98)
        self.robot.previous[12]=.2;self.robot.previous[13]=.2
        with patch.object(tp,'clock',return_value=1.):
            self.state.disarm('Operator STOP',return_to_neutral=True)
        for i in range(350):
            now=1.6+i*.06
            with patch.object(tp,'clock',return_value=now):self.state.tick(now,.06)
        self.assertIsNone(self.state.relax_at)
        self.assertNotIn('timed out',self.state.fault)
        self.assertAlmostEqual(self.robot.previous[12],.98,delta=.01)
        self.assertFalse(self.state.armed)
        self.assertEqual(self.robot.base,[0,0,0])

    def test_tracking_loss_disarms_and_stops(self):
        self.robot.base = [.2, .2, .2]
        self.state.accept(self.packet(active=False), '127.0.0.1', 1)
        self.assertFalse(self.state.armed)
        self.assertEqual(self.robot.base, [0, 0, 0])

    def test_first_stop_reason_survives_later_delayed_packets(self):
        self.state.accept(self.packet(), '127.0.0.1', 1)
        self.state.accept(self.packet(2, active=False, sent=1010), '127.0.0.1', 1.01)
        reason = self.state.last_stop['reason']
        self.state.accept(self.packet(3, sent=1020), '127.0.0.1', 1.2)
        self.assertEqual(self.state.fault, reason)
        self.assertIn('tracking/video', reason)
        self.assertEqual(self.state.last_stop['seq'], 2)

    def test_stop_attempts_hold_even_when_base_command_fails(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        self.robot.motion.moveToward.side_effect=RuntimeError('timeout')
        self.robot.motion.getAngles.return_value.value.return_value=list(self.robot.previous)
        self.robot.stop()
        with self.assertRaisesRegex(RuntimeError,'Stop base_zero'):self.robot.poll_stop()
        self.robot.motion.stopMove.assert_called_once()
        self.assertEqual(self.robot.motion.setAngles.call_count,2)
        self.assertEqual(self.robot.motion.setAngles.call_args_list[0].args[0],tp.NAMES)
        self.assertEqual(self.robot.motion.setAngles.call_args_list[1].args[0],['HipRoll','HipPitch'])

    def test_stop_does_not_wait_or_queue_duplicates_and_arm_requires_confirmation(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        pending=Mock();pending.isFinished.return_value=False
        self.robot.motion.moveToward.return_value=pending
        self.robot.motion.stopMove.return_value=pending
        self.robot.motion.getAngles.return_value=pending
        with patch.object(tp,'clock',return_value=10.):
            self.robot.stop();self.robot.stop();self.robot.poll_stop()
            with self.assertRaisesRegex(RuntimeError,'Stop still completing'):self.robot.arm()
        pending.value.assert_not_called()
        self.assertEqual(self.robot.motion.stopMove.call_count,1)
        self.assertEqual(self.robot.motion.moveToward.call_count,1)
        self.assertEqual(self.robot.motion.getAngles.call_count,2)
        pending.isFinished.return_value=True;pending.value.return_value=list(self.robot.previous)
        self.robot.motion.setAngles.return_value.isFinished.return_value=True
        self.robot.poll_stop();self.robot.poll_stop()
        self.assertEqual(self.robot.stop_pending,{})
        self.assertTrue(all(c.args==(0,) for c in pending.value.call_args_list))

    def test_sustained_stop_stall_is_reported_without_repeated_rpc_or_fault_spam(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        pending=Mock();pending.isFinished.return_value=False
        self.robot.motion.moveToward.return_value=pending;self.robot.motion.stopMove.return_value=pending;self.robot.motion.getAngles.return_value=pending
        with patch.object(tp,'clock',return_value=10.):self.robot.stop()
        with patch.object(tp,'clock',return_value=12.1):
            with self.assertRaisesRegex(RuntimeError,'Stop base_zero acknowledgement stalled'):self.robot.poll_stop()
            self.robot.poll_stop();self.robot.stop()
        self.assertEqual(self.robot.motion.stopMove.call_count,1)

    def test_arming_never_changes_autonomous_life_or_wakes_robot(self):
        self.robot.simulate=False
        self.robot.motion=Mock()
        self.robot.motion.robotIsWakeUp.return_value=True
        self.robot.motion.getAngles.return_value=list(self.robot.previous)
        self.robot.life=Mock()
        self.robot.life.getState.return_value='solitary'
        with self.assertRaisesRegex(RuntimeError,'Autonomous Life disabled'):
            self.robot.arm()
        self.robot.life.setState.assert_not_called()
        self.robot.motion.setStiffnesses.assert_not_called()
        self.robot.life.getState.return_value='disabled'
        self.robot.arm()
        self.robot.life.setState.assert_not_called()
        self.robot.motion.wakeUp.assert_not_called()
        self.robot.motion.rest.assert_not_called()

    def test_arm_reads_all_controlled_joints_without_capturing_lean_as_neutral(self):
        self.robot.simulate=False;self.robot.motion=Mock();self.robot.life=Mock()
        self.robot.motion.robotIsWakeUp.return_value=True
        self.robot.life.getState.return_value='disabled'
        measured=list(self.robot.previous)
        self.robot.motion.getAngles.side_effect=[measured,[.01,.24]]
        self.robot.arm()
        self.assertEqual(self.robot.previous,measured)
        self.assertEqual(self.robot.torso.current,[.01,.24])
        self.assertEqual(self.robot.torso.origin,[0.,0.])
        self.robot.motion.setAngles.assert_not_called()  # Engagement never snaps pose.

    def test_tracking_loss_cancels_operator_gesture(self):
        self.state.gesture = ('yes', 0, list(self.robot.previous))
        self.state.accept(self.packet(active=False), '127.0.0.1', 1)
        self.assertIsNone(self.state.gesture)
        self.assertFalse(self.state.armed)

    def test_official_player_owns_joints_without_streaming_overrides(self):
        from official_animations import CATALOG
        future=Mock();future.isFinished.return_value=False
        self.robot.animation_player=Mock();self.robot.animation_player.run.return_value=future
        self.state.gesture=('yes',1.,list(self.robot.previous))
        self.state.accept(self.packet(sent=1100.,base=[.4,.2,.3],torso=[.1,.1],torso_assist=True),'127.0.0.1',1.1)
        with patch.object(self.robot,'apply') as apply:
            self.assertIsNotNone(self.state.tick(1.1,.02))
            self.robot.animation_player.run.assert_called_once_with(CATALOG['yes'],_async=True)
            apply.assert_not_called()
            self.state.accept(self.packet(2,sent=1140.),'127.0.0.1',1.14)
            self.state.tick(1.14,.02);self.robot.animation_player.run.assert_called_once()
            apply.assert_not_called()
        self.assertEqual(self.state.gesture_diagnostics['stage'],'playing')

    def test_official_animation_returns_to_live_and_stop_cancels_future(self):
        future=Mock();future.isFinished.return_value=True
        self.robot.animation_future=future
        self.state.gesture=('yes',1.,list(self.robot.previous))
        self.state.accept(self.packet(sent=1100.),'127.0.0.1',1.1)
        self.assertTrue(self.state.tick(1.1,.02)['armed'])
        self.assertTrue(self.state.armed);self.assertIsNone(self.state.gesture)
        self.assertEqual(self.state.gesture_diagnostics['stage'],'complete')
        self.assertEqual(self.state.fault,'')
        self.assertEqual(self.robot.blend_seconds,.75)
        self.state.accept(self.packet(2,sent=1140.),'127.0.0.1',1.14)
        with patch.object(self.robot,'apply') as apply:
            self.state.tick(1.14,.02)
            apply.assert_called_once()
        future.value.assert_called_once_with(0)
        self.robot.animation_future=future;self.robot.stop();future.cancel.assert_called()

    def test_animation_return_reads_measured_pose_asynchronously_and_stop_cancels(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        read=Mock();read.isFinished.return_value=False
        self.robot.motion.getAngles.return_value=read
        with patch.object(tp,'clock',return_value=1.):
            self.assertFalse(self.robot.finish_animation())
        self.robot.motion.getAngles.assert_called_once_with(tp.NAMES+['HipRoll','HipPitch'],True,_async=True)
        measured=[.1]*14+[.02,-.04]
        read.isFinished.return_value=True;read.value.return_value=measured
        with patch.object(tp,'clock',return_value=1.1):
            self.assertTrue(self.robot.finish_animation())
        self.assertEqual(self.robot.previous,measured[:14])
        self.assertEqual(self.robot.origin,measured[:14])
        self.assertEqual(self.robot.velocity,[0.]*14)
        self.assertEqual(self.robot.armed_at,1.1)
        self.assertEqual(self.robot.blend_seconds,.75)
        self.robot.animation_resume_future=read
        self.robot.stop();read.cancel.assert_called_once()

    def test_animation_return_pose_timeout_does_not_resume(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        read=Mock();read.isFinished.return_value=False
        self.robot.motion.getAngles.return_value=read
        with patch.object(tp,'clock',return_value=1.):self.assertFalse(self.robot.finish_animation())
        with patch.object(tp,'clock',return_value=1.6):
            with self.assertRaisesRegex(RuntimeError,'read timed out'):self.robot.finish_animation()
        self.assertIsNotNone(self.robot.animation_resume_future)

    def test_animation_stops_a_held_base_velocity_before_playback(self):
        self.robot.simulate=False;self.robot.motion=Mock();self.robot.animation_player=Mock()
        self.robot.base_sent=[.4,0.,.3]
        stop=Mock();stop.isFinished.return_value=False
        self.robot.motion.moveToward.return_value=stop
        self.assertFalse(self.robot.begin_animation('yes'))
        self.robot.motion.moveToward.assert_called_once_with(0.,0.,0.,_async=True)
        self.robot.animation_player.run.assert_not_called()
        self.assertFalse(self.robot.begin_animation('yes'))
        stop.isFinished.return_value=True
        self.assertTrue(self.robot.begin_animation('yes'))
        self.robot.animation_player.run.assert_called_once()

    def test_official_animation_requires_armed_fresh_tracking_and_installed_path(self):
        service=Mock(state=self.state,port=0,video_diagnostics=[])
        woz=tp.WoZ(service,lambda:1.)
        try:
            self.state.armed=False
            with self.assertRaisesRegex(ValueError,'Pilot must be armed'):woz.action(dict(cmd='gesture',name='yes'))
            self.state.armed=True;self.state.received=.7
            with self.assertRaisesRegex(ValueError,'Pilot must be armed'):woz.action(dict(cmd='gesture',name='yes'))
            self.state.received=1.
            from official_animations import NAMES
            for name in NAMES:
                self.state.gesture=None
                self.assertTrue(woz.action(dict(cmd='gesture',name=name))['ok'])
                self.assertEqual(self.state.gesture[0],name)
            with self.assertRaisesRegex(ValueError,'Stop the current'):woz.action(dict(cmd='gesture',name='yes'))
            self.state.gesture=None;self.robot.available_animations=set()
            with self.assertRaisesRegex(ValueError,'not installed'):woz.action(dict(cmd='gesture',name='yes'))
            with self.assertRaisesRegex(ValueError,'Unknown gesture'):woz.action(dict(cmd='gesture',name='nod'))
        finally:woz.audio_socket.close()

    def test_official_animation_waits_for_engagement_and_outstanding_commands(self):
        service=Mock(state=self.state,port=0,video_diagnostics=[]);woz=tp.WoZ(service,lambda:1.)
        try:
            self.state.armed=True;self.state.received=1.;self.robot.armed_at=.5
            woz.action(dict(cmd='gesture',name='yes'))
            self.assertEqual(self.state.gesture[1],3.5)
            self.state.accept(self.packet(sent=1100.),'127.0.0.1',1.1)
            with patch.object(self.robot,'apply') as apply:
                self.state.tick(1.1,.02)
                self.assertEqual(apply.call_args.args[0],self.state.gesture[2])
                self.assertEqual(apply.call_args.args[1],[0.,0.,0.])
            pending=Mock();pending.isFinished.return_value=False
            self.robot.motion_pending['arms']=(pending,0.)
            self.robot.animation_player=Mock()
            self.assertFalse(self.robot.begin_animation('yes'))
            self.robot.animation_player.run.assert_not_called()
        finally:woz.audio_socket.close()

    def test_official_animation_failure_and_tracking_loss_cancel_playback(self):
        future=Mock();future.isFinished.return_value=True;future.value.side_effect=RuntimeError('Player failure')
        self.robot.animation_future=future;self.state.gesture=('yes',1.,list(self.robot.previous))
        self.state.accept(self.packet(sent=1100.),'127.0.0.1',1.1);self.state.tick(1.1,.02)
        self.assertFalse(self.state.armed);self.assertIn('Player failure',self.state.fault)
        future.cancel.assert_called_once()
        self.state.armed=True;future=Mock();self.robot.animation_future=future
        self.state.gesture=('yes',1.,list(self.robot.previous))
        self.state.accept(self.packet(2,sent=1200.,active=False),'127.0.0.1',1.2)
        future.cancel.assert_called_once();self.assertFalse(self.state.armed)

    def test_stop_awaits_official_cancel_before_reading_hold_pose(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        pending=Mock();pending.isFinished.return_value=False
        self.robot.motion.moveToward.return_value=pending;self.robot.motion.stopMove.return_value=pending
        future=Mock();future.isFinished.return_value=False;self.robot.animation_future=future
        self.robot.stop();future.cancel.assert_called_once();self.robot.motion.getAngles.assert_not_called()
        self.robot.poll_stop();self.robot.motion.getAngles.assert_not_called()
        future.isFinished.return_value=True;self.robot.poll_stop()
        self.assertEqual(self.robot.motion.getAngles.call_count,2)
        self.assertIn('hold_read',self.robot.stop_pending)

    def test_huge_sdk_error_cannot_expand_stop_status_with_entire_clip(self):
        self.state.armed=True
        self.state.disarm('SDK error: '+('x'*20000))
        self.assertEqual(len(self.state.last_stop['reason']),512)
        self.assertEqual(len(self.state.fault),512)
        self.assertFalse(self.state.armed)

    def test_official_catalog_is_small_and_has_no_procedural_fallback(self):
        from official_animations import CATALOG
        self.assertEqual(set(CATALOG),{'wave_left','wave_right','point_left','point_right','yes','no','happy','sad','dance','funny','look_around','make_space'})
        self.assertTrue(all(path.endswith('.qianim') for path in CATALOG.values()))
        self.assertFalse((ROOT/'pepper/bridge/gesture_catalog.py').exists())

    def test_deadman_released_stops_base_only(self):
        self.state.accept(self.packet(drive=False), '127.0.0.1', 1)
        self.assertEqual(self.state.latest[1], [0, 0, 0])
        self.assertTrue(self.state.armed)

    def test_stale_quest_sample_cannot_refresh_valid_tracking(self):
        self.state.accept(self.packet(sample_age_ms=101), '127.0.0.1',1)
        self.assertFalse(self.state.armed)
        self.assertFalse(self.state.tracking_valid)

    def test_base_skips_stationary_duplicates_but_release_is_immediate(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        for i in range(4):self.robot.apply(list(self.robot.previous),[0,0,0],.01)
        self.assertEqual(self.robot.motion.moveToward.call_count,1)
        self.robot.apply(list(self.robot.previous),[.25,0,0],.01)
        self.assertGreater(self.robot.motion.moveToward.call_args.args[0],0)
        self.robot.apply(list(self.robot.previous),[0,0,0],.01)
        self.assertEqual(self.robot.motion.moveToward.call_args.args,(0.,0.,0.))

    def test_slow_motor_future_does_not_block_or_queue_pose_commands(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        pending=Mock();pending.isFinished.return_value=False
        self.robot.motion.setAngles.return_value=pending
        self.robot.motion.moveToward.return_value=pending
        with patch.object(tp,'clock',return_value=10.):
            self.robot.apply(list(self.robot.previous),[.25,0,0],.01)
            for i in range(20):self.robot.apply(list(self.robot.previous),[.25,0,0],.01)
        self.assertEqual(self.robot.motion.setAngles.call_count,3)
        self.assertEqual(self.robot.motion.moveToward.call_count,1)
        pending.value.assert_not_called()
        with patch.object(tp,'clock',return_value=10.1):
            self.robot.apply(list(self.robot.previous),[0,0,0],.01)
        self.assertEqual(self.robot.motion.moveToward.call_args.args,(0.,0.,0.))
        pending.isFinished.return_value=True
        with patch.object(tp,'clock',return_value=10.2):
            self.robot.apply(list(self.robot.previous),[0,0,0],.01)
        self.assertEqual(self.robot.motion.setAngles.call_count,3)
        self.assertTrue(all(c.args==(0,) for c in pending.value.call_args_list))

    def test_held_drive_does_not_resubmit_unchanged_base_velocity(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        calls=[]
        for i in range(151):
            with patch.object(tp,'clock',return_value=10.+i*.02):
                self.robot.apply(list(self.robot.previous),[.25,0,0],.02)
            calls.append(self.robot.motion.moveToward.call_count)
        # Ramp submissions coalesce; once .25 is reached, a held joystick creates
        # no repeated tasks, while head/arm following remains on the same loop.
        self.assertLess(calls[-1],16)
        self.assertEqual(calls[50],calls[-1])
        self.assertAlmostEqual(self.robot.motion.moveToward.call_args.args[0],.25)
        with patch.object(tp,'clock',return_value=13.001):
            self.robot.apply(list(self.robot.previous),[0,0,0],.001)
        self.assertEqual(self.robot.motion.moveToward.call_count,calls[-1]+1)
        self.assertEqual(self.robot.motion.moveToward.call_args.args,(0.,0.,0.))

    def test_first_drive_bypasses_base_update_interval(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        with patch.object(tp,'clock',return_value=10.):
            self.robot.apply(list(self.robot.previous),[0,0,0],.01)
        with patch.object(tp,'clock',return_value=10.001):
            self.robot.apply(list(self.robot.previous),[.25,0,0],.001)
        self.assertEqual(self.robot.motion.moveToward.call_count,2)
        self.assertGreater(self.robot.motion.moveToward.call_args.args[0],0)

    def test_lateral_torso_assistance_matches_operator_right_and_left(self):
        for logical,direction in ((.15,-1),(-.15,1)):
            self.robot.torso.reset([.05,-.04])
            first=self.robot.torso.step([logical,0],.02)
            self.assertGreater(direction*(first[0]-.05),0)
            self.assertLess(abs(first[0]-.05),.001)
            for unused in range(160):self.robot.torso.step([logical,0],.02)
            self.assertAlmostEqual(self.robot.torso.current[0],.05-logical,places=3)
            self.assertAlmostEqual(self.robot.torso.current[1],-.04,places=3)

    def test_torso_assist_is_bounded_smooth_and_compensates_head_pitch(self):
        self.robot.torso.reset([0,0]);self.robot.armed_at=0
        target=list(self.robot.previous);target[1]=.43
        with patch.object(tp,'clock',return_value=10.):
            self.robot.apply(target,[0,0,0],.01,torso=[.15,.2])
        self.assertLess(self.robot.torso.current[1],0)
        self.assertLess(abs(self.robot.torso.current[1]),.003)
        for i in range(300):
            with patch.object(tp,'clock',return_value=10+i*.02):
                self.robot.apply(target,[0,0,0],.02,torso=[1,1])
        self.assertAlmostEqual(self.robot.torso.current[0],-.15)
        self.assertAlmostEqual(self.robot.torso.current[1],-.2)
        self.assertEqual(self.robot.base,[0,0,0])
        with self.assertRaises(ValueError):self.state.accept(self.packet(torso_assist=True,torso=[float('nan'),0]),'127.0.0.1',1)

    def test_downward_gaze_uses_negative_hip_pitch_and_preserves_total_head_direction(self):
        self.robot.torso.reset([0,0]);self.robot.armed_at=0
        target=list(self.robot.previous);target[1]=.33
        with patch.object(tp,'clock',return_value=10.):self.robot.apply(target,[0,0,0],.02,torso=[0,.15])
        self.assertLess(self.robot.torso.current[1],0)
        for i in range(200):
            with patch.object(tp,'clock',return_value=10+i*.02):self.robot.apply(target,[0,0,0],.02,torso=[0,.15])
        self.assertAlmostEqual(self.robot.previous[1]-self.robot.torso.current[1],.48)
        for i in range(200):
            with patch.object(tp,'clock',return_value=20+i*.02):self.robot.apply(target,[0,0,0],.02,torso=[0,-.15])
        self.assertGreater(self.robot.torso.current[1],0)
        self.assertAlmostEqual(self.robot.previous[1]-self.robot.torso.current[1],.18)

    def test_torso_recovery_keeps_neutral_reference_without_adding_lean(self):
        self.robot.torso.reset([.01,-.03])
        self.robot.torso.reset([.10,.12],preserve_origin=True)
        self.assertEqual(self.robot.torso.origin,[.01,-.03])
        for i in range(300):self.robot.torso.step([0,0],.02)
        self.assertAlmostEqual(self.robot.torso.current[0],.01)
        self.assertAlmostEqual(self.robot.torso.current[1],-.03)

    def test_arming_tilted_torso_uses_upright_not_measured_as_neutral(self):
        torso=self.robot.torso
        torso.engage([.08,.24])
        self.assertEqual(torso.current,[.08,.24])
        self.assertEqual(torso.origin,[0.,0.])
        first=torso.step([0,0],.02)
        self.assertGreater(first[1],.23)  # No snap from measured pose.
        for unused in range(400):torso.step([0,0],.02)
        self.assertAlmostEqual(torso.current[0],0.,places=4)
        self.assertAlmostEqual(torso.current[1],0.,places=4)
        torso.engage([-.05,-.18])  # Recalibration does not retain an old lean.
        self.assertEqual(torso.origin,[0.,0.])
        for unused in range(400):torso.step([0,0],.02)
        self.assertAlmostEqual(torso.current[1],0.,places=4)

    def test_torso_disabled_does_not_request_hip_motion(self):
        self.state.accept(self.packet(torso_assist=False,torso=[.15,.2]),'127.0.0.1',1)
        self.assertIsNone(self.state.latest[8])
        self.robot.simulate=False;self.robot.motion=Mock();self.robot.apply(list(self.robot.previous),[0,0,0],.01)
        self.assertTrue(all(call.args[0]!=['HipRoll','HipPitch'] for call in self.robot.motion.setAngles.call_args_list))

    def test_unchanged_hand_targets_are_not_resent_and_changes_are_immediate(self):
        self.robot.simulate=False;self.robot.motion=Mock();self.robot.armed_at=0
        self.robot.motion.setAngles.return_value.isFinished.return_value=True
        target=list(self.robot.previous)
        with patch.object(tp,'clock',return_value=10.):
            self.robot.apply(target,[0,0,0],.01)
            self.robot.apply(target,[0,0,0],.01)
            self.assertEqual(self.robot.motion.setAngles.call_count,3)
            target[12]=0
            self.robot.apply(target,[0,0,0],.01)
            self.assertEqual(self.robot.motion.setAngles.call_count,4)
            self.assertEqual(self.robot.motion.setAngles.call_args.args[0],tp.NAMES[12:])

    def test_sustained_motor_stall_is_named_and_not_silently_ignored(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        pending=Mock();pending.isFinished.return_value=False
        self.robot.motion.setAngles.return_value=pending
        with patch.object(tp,'clock',return_value=10.):self.robot.apply(list(self.robot.previous),[0,0,0],.01)
        with patch.object(tp,'clock',return_value=10.6):
            with self.assertRaisesRegex(RuntimeError,'head command acknowledgement stalled'):
                self.robot.apply(list(self.robot.previous),[0,0,0],.01)

    def test_pose_wakes_motion_worker_and_reports_queue_components(self):
        self.state.accept(self.packet(sample_age_ms=3), '127.0.0.1',1)
        self.assertTrue(self.state.motion_ready.is_set())
        with patch.object(tp,'clock',return_value=1.002):
            ack=self.state.tick(1.002,.01)
        self.assertEqual(ack['sample_age_ms'],3)
        self.assertAlmostEqual(ack['queue_ms'],2)

    def test_base_targets_are_limited(self):
        self.state.accept(self.packet(), '127.0.0.1', 1)
        self.assertEqual(self.state.latest[1], [.50, -.4, .90])

    def test_nonfinite_and_malformed_packets_do_not_refresh_watchdog(self):
        for p in [self.packet(angles=[float('nan')]*14),self.packet(base=[float('inf')]*3),self.packet(angles=[0]),self.packet(sent=float('nan'))]:
            with self.assertRaises(ValueError):
                self.state.accept(p, '127.0.0.1', 1)
        self.assertEqual(self.state.received, 0)

    def test_isolated_delayed_packet_is_dropped_without_rearming(self):
        self.state.accept(self.packet(), '127.0.0.1', 1)
        self.assertFalse(self.state.accept(self.packet(2,sent=1010), '127.0.0.1', 1.1))
        self.assertTrue(self.state.armed)
        self.assertEqual(self.state.received,1)
        self.assertEqual(self.state.stale_drops,1)
        self.assertTrue(self.state.accept(self.packet(3,sent=1120),'127.0.0.1',1.12))
        self.assertEqual(self.state.recovery_count,0)

    def test_udp_stop_already_acknowledged_by_arm_cannot_cancel_new_start(self):
        self.state.armed_stop_epoch=12
        self.state.accept(self.packet(1,sent=1000,stop_epoch=12),'127.0.0.1',1)
        received=self.state.received
        self.assertFalse(self.state.accept(self.packet(2,sent=1010,stop=True,stop_epoch=12),'127.0.0.1',1.03))
        self.assertTrue(self.state.armed);self.assertEqual(self.state.received,received)
        self.assertTrue(self.state.accept(self.packet(3,sent=1040,stop_epoch=12),'127.0.0.1',1.04))
        self.assertTrue(self.state.armed)
        self.state.accept(self.packet(4,sent=1050,stop=True,stop_epoch=13),'127.0.0.1',1.05)
        self.assertFalse(self.state.armed)  # A newly pressed STOP remains immediate.

    def test_legacy_stop_still_stops_epoch_enabled_session(self):
        self.state.armed_stop_epoch=12
        self.state.accept(self.packet(stop=True),'127.0.0.1',1)
        self.assertFalse(self.state.armed)

    def test_delayed_stop_and_tracking_loss_remain_immediate(self):
        for edge in [{'stop':True},{'active':False,'pause_reason':'tracking'}]:
            self.state.armed=True;self.state.seq=-1;self.state.min_offset=None
            self.state.accept(self.packet(),'127.0.0.1',1)
            self.state.accept(self.packet(2,sent=1010,**edge),'127.0.0.1',1.1)
            self.assertFalse(self.state.armed)
            self.assertIsNone(self.state.latest)

    def test_continuous_delayed_packets_cannot_keep_movement_alive(self):
        self.state.accept(self.packet(),'127.0.0.1',1)
        for seq in range(2,8):
            self.state.accept(self.packet(seq,sent=1000+seq),'127.0.0.1',1.1+seq*.01)
        self.assertEqual(self.state.received,1)
        self.state.tick(1.181,.01)
        self.assertFalse(self.state.armed)

    def batch(self, packets, now=1.12):
        service = tp.Service.__new__(tp.Service)
        service.state = self.state
        service.destination = None
        with patch.object(tp, 'clock', return_value=now):
            service.motion_batch([(json.dumps(p).encode('utf-8'), ('127.0.0.1', 1234)) for p in packets])

    def test_burst_uses_freshest_pose_without_false_stale_stop(self):
        self.state.accept(self.packet(), '127.0.0.1', 1)
        self.batch([self.packet(2, sent=1010), self.packet(3, sent=1120)])
        self.assertTrue(self.state.armed)
        self.assertEqual(self.state.latest[2], 3)
        self.assertEqual(self.state.coalesced_packets, 1)

    def test_burst_never_hides_tracking_loss(self):
        self.state.accept(self.packet(), '127.0.0.1', 1)
        self.batch([self.packet(2, sent=1110, active=False), self.packet(3, sent=1120)])
        self.assertFalse(self.state.armed)
        self.assertIsNone(self.state.latest)

    def test_burst_never_hides_explicit_udp_stop(self):
        self.state.accept(self.packet(), '127.0.0.1', 1)
        self.batch([self.packet(2, sent=1110, stop=True), self.packet(3, sent=1120)])
        self.assertFalse(self.state.armed)
        self.assertIsNone(self.state.latest)

    def test_stopped_tracking_can_be_valid_but_never_auto_arms(self):
        self.state.accept(self.packet(stop=True), '127.0.0.1', 1)
        self.assertTrue(self.state.tracking_valid)
        self.assertFalse(self.state.armed)
        self.state.accept(self.packet(2,sent=1010), '127.0.0.1',1.01)
        self.assertFalse(self.state.armed)

    def test_burst_still_stops_when_newest_pose_is_stale(self):
        self.state.accept(self.packet(), '127.0.0.1', 1)
        self.batch([self.packet(2, sent=1010), self.packet(3, sent=1020)])
        self.assertTrue(self.state.armed)
        self.assertEqual(self.state.received,1)
        self.state.tick(1.181,.01)
        self.assertFalse(self.state.armed)
        self.assertIn('timeout', self.state.last_stop['reason'].lower())

    def test_invalid_or_unauthorized_packet_cannot_replace_newest(self):
        self.state.accept(self.packet(), '127.0.0.1', 1)
        self.batch([self.packet(3, sent=1120), self.packet(4, sent=1120, session='intruder'),
                    self.packet(5, sent=1120, angles=[float('nan')]*14), self.packet(2, sent=1010)])
        self.assertTrue(self.state.armed)
        self.assertEqual(self.state.latest[2], 3)

    def test_no_pose_jump_at_arming(self):
        with patch.object(tp, 'clock', return_value=10):
            self.robot.arm()
            initial=list(self.robot.previous)
            self.robot.apply([10]*14,[0,0,0],.01)
        self.assertEqual(self.robot.previous, initial)

    def test_joint_speed_acceleration_and_envelopes(self):
        with patch.object(tp,'clock',return_value=10): self.robot.arm()
        initial=list(self.robot.previous)
        previous_velocity=list(self.robot.velocity)
        for i in range(1,1001):
            before=list(self.robot.previous)
            target=[10 if i<500 else -10]*14
            with patch.object(tp,'clock',return_value=10+i*.01): self.robot.apply(target,[.25,0,0],.01)
            for j,(old,new) in enumerate(zip(before,self.robot.previous)):
                speed=(.45 if i<300 else 1.2) if j<2 else 2.4 if j<12 else 3.
                if i<300 or j>=12:self.assertLessEqual(abs(new-old),speed*.01+1e-8)
                lo=min(initial[j],self.robot.limits[j][0]);hi=max(initial[j],self.robot.limits[j][1])
                self.assertGreaterEqual(new,lo-1e-8);self.assertLessEqual(new,hi+1e-8)
                # Acceleration is bounded away from a target snap/envelope clamp.
                if abs(self.robot.velocity[j])>1e-9 and not (j>=12 and i>=300):
                    accel=(.9 if i<300 else 4.) if j<2 else 16. if j<12 else 12.
                    self.assertLessEqual(abs(self.robot.velocity[j]-previous_velocity[j]),accel*.01+1e-8)
            previous_velocity=list(self.robot.velocity)

    def test_arm_follows_one_radian_step_within_two_seconds_after_engagement(self):
        with patch.object(tp, 'clock', return_value=10): self.robot.arm()
        target = list(self.robot.previous)
        target[2] -= 1.
        for i in range(200):
            with patch.object(tp, 'clock', return_value=14+i*.01):
                self.robot.apply(target, [0, 0, 0], .01)
        self.assertLess(abs(self.robot.previous[2]-target[2]), .03)

    def test_empty_poll_does_not_reduce_trajectory_speed(self):
        self.state.accept(self.packet(), '127.0.0.1', 1)
        with patch.object(self.robot, 'apply') as apply:
            self.state.tick(1, .01)
            self.state.tick(1.01, .01)
            self.state.accept(self.packet(2, sent=1020), '127.0.0.1', 1.02)
            self.state.tick(1.02, .01)
        self.assertAlmostEqual(apply.call_args.args[2], .02)

    def test_trigger_aperture_responds_on_first_sample_and_reverses_promptly(self):
        with patch.object(tp,'clock',return_value=0):self.robot.arm()
        target=list(self.robot.previous);target[12]=0.;target[13]=0.
        with patch.object(tp,'clock',return_value=4):
            self.robot.apply(target,[0,0,0],.01)
            self.assertAlmostEqual(self.robot.previous[12],.97)
            for i in range(33):self.robot.apply(target,[0,0,0],.01)
            self.assertLess(self.robot.previous[12],.001)
            target[12]=1.;self.robot.apply(target,[0,0,0],.01)
            self.assertAlmostEqual(self.robot.previous[12],.03)
        # The other hand is not forced to reopen by the changing left trigger.
        self.assertLess(self.robot.previous[13],.001)

    def test_brisk_arm_gesture_does_not_accumulate_phase_error(self):
        with patch.object(tp,'clock',return_value=0):self.robot.arm()
        errors=[]
        for i in range(1000):
            now=i*.01;target=list(self.robot.origin)
            target[2]=.8+.4*math.sin(2*math.pi*.8*now)
            with patch.object(tp,'clock',return_value=4+now):self.robot.apply(target,[0,0,0],.01)
            if i>300:errors.append(abs(target[2]-self.robot.previous[2]))
        self.assertLess(sum(errors)/len(errors),math.radians(1))
        self.assertLess(max(errors),math.radians(2))

    def test_hand_closes_within_1_2_seconds_after_engagement(self):
        with patch.object(tp, 'clock', return_value=10): self.robot.arm()
        target=list(self.robot.previous);target[12]=0;target[13]=0
        for i in range(120):
            with patch.object(tp, 'clock', return_value=14+i*.01):
                self.robot.apply(target, [0,0,0], .01)
        self.assertLess(self.robot.previous[12], .03)
        self.assertLess(self.robot.previous[13], .03)

    def test_faster_arms_do_not_change_head_hardware_cap(self):
        self.robot.simulate=False;self.robot.motion=Mock()
        self.robot.apply(list(self.robot.previous), [0,0,0], .01)
        calls=self.robot.motion.setAngles.call_args_list
        self.assertEqual(calls[0].args[0],tp.NAMES[:2])
        self.assertEqual(calls[0].args[2],.12)
        self.assertEqual(calls[1].args[0],tp.NAMES[2:12])
        self.assertEqual(calls[2].args[0],tp.NAMES[12:])
        self.assertEqual(calls[2].args[2],.35)
        self.assertEqual(calls[1].args[2],.35)

    def test_continuous_arm_reference_does_not_accumulate_large_phase_lag(self):
        with patch.object(tp, 'clock', return_value=0): self.robot.arm()
        errors=[]
        for i in range(800):
            now=4+i*.02
            target=list(self.robot.origin)
            target[2]=.7+.5*math.sin(math.pi*(now-4))
            with patch.object(tp, 'clock', return_value=now):
                self.robot.apply(target, [0,0,0], .02)
            if i>200: errors.append(abs(target[2]-self.robot.previous[2]))
        self.assertLess(sum(errors)/len(errors), math.radians(2))
        self.assertLess(max(errors), math.radians(3))

    def test_head_following_and_rotation_response_keep_bounded_engagement(self):
        with patch.object(tp,'clock',return_value=0):self.robot.arm()
        target=list(self.robot.previous);target[0]=.7
        with patch.object(tp,'clock',return_value=4):
            for i in range(100):self.robot.apply(target,[0,0,.45],.01)
            self.assertLess(abs(self.robot.previous[0]-.7),.01)
            self.assertAlmostEqual(self.robot.base[2],.45)
            self.robot.apply(target,[0,0,0],.01)
            self.assertEqual(self.robot.base[2],0.)
        self.robot.simulate=False;self.robot.motion=Mock()
        with patch.object(tp,'clock',return_value=4):self.robot.apply(target,[0,0,0],.01)
        calls=self.robot.motion.setAngles.call_args_list
        self.assertEqual(calls[0].args[2],.25)
        self.assertEqual(calls[1].args[2],.35)
        self.assertEqual(calls[2].args[2],.35)

    def test_live_reference_is_sent_without_a_second_trajectory_filter(self):
        with patch.object(tp,'clock',return_value=0):self.robot.arm()
        target=list(self.robot.previous);target[0]=.7;target[2]=-.4
        with patch.object(tp,'clock',return_value=4):self.robot.apply(target,[0,0,0],.045)
        self.assertEqual(self.robot.previous[:12],target[:12])
        self.robot.simulate=False;self.robot.motion=Mock()
        with patch.object(tp,'clock',return_value=4.1):self.robot.apply(target,[0,0,0],.01)
        calls=self.robot.motion.setAngles.call_args_list
        self.assertEqual(calls[0].args[1],target[:2])
        self.assertEqual(calls[0].args[2],.25)
        self.assertEqual(calls[1].args[1],target[2:12])
        self.assertEqual(calls[1].args[2],.35)

    def test_unreachable_streamed_pose_still_clamps_before_firmware_dispatch(self):
        with patch.object(tp,'clock',return_value=0):self.robot.arm()
        with patch.object(tp,'clock',return_value=4):self.robot.apply([100.]*14,[0,0,0],.01)
        for i in range(12):self.assertEqual(self.robot.previous[i],self.robot.limits[i][1])


class IntegrationTests(unittest.TestCase):
    def test_discovery_is_read_only_and_does_not_expose_pairing_secret(self):
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
            s.settimeout(1)
            before=(self.service.state.armed,self.service.state.session,self.service.state.seq)
            s.sendto(b'TELEPEPPER_DISCOVER_V1',('127.0.0.1',self.port+4))
            wire=s.recv(1024);reply=json.loads(wire)
            self.assertEqual(reply['kind'],'telepepper-discovery')
            self.assertEqual(reply['version'],1)
            self.assertEqual(reply['control_port'],self.port)
            self.assertNotIn(b'test-pairing-code',wire)
            self.assertNotIn('token',reply)
            self.assertEqual(before,(self.service.state.armed,self.service.state.session,self.service.state.seq))

    @classmethod
    def setUpClass(cls):
        cls.port=19570
        cls.service=tp.Service('test-pairing-code',True,cls.port)
        cls.thread=threading.Thread(target=cls.service.run,daemon=True);cls.thread.start()
        time.sleep(.15)

    @classmethod
    def tearDownClass(cls):
        cls.service.running=False;cls.thread.join(2)

    def connect(self):
        s=socket.create_connection(('127.0.0.1',self.port),timeout=2)
        f=s.makefile('rb');tp.send_json(s,{'token':'test-pairing-code'})
        hello=json.loads(f.readline());self.assertIn('session',hello)
        return s,f,hello['session']

    def test_volume_full_range_reaches_audio_device_and_clamps(self):
        audio=Mock()
        with patch.dict(self.service.woz.services,{'ALAudioDevice':audio}):
            with socket.create_connection(('127.0.0.1',self.port),timeout=2) as sock:
                with sock.makefile('rb') as stream:
                    tp.send_json(sock,{'token':'test-pairing-code','role':'operator'})
                    self.assertIn('observer',json.loads(stream.readline()))
                    for requested,expected in ((0,0),(20,20),(80,80),(100,100),(120,100),(-20,0)):
                        tp.send_json(sock,{'cmd':'volume','value':requested})
                        self.assertTrue(json.loads(stream.readline())['ok'])
                        audio.setOutputVolume.assert_called_with(expected)

    def test_operator_cannot_arm_or_call_arbitrary_robot_methods(self):
        with socket.create_connection(('127.0.0.1',self.port),timeout=2) as s:
            with s.makefile('rb') as stream:
                tp.send_json(s,{'token':'test-pairing-code','role':'operator'})
                self.assertIn('observer',json.loads(stream.readline()))
                for cmd in ('arm','wakeUp','setAngles'):
                    tp.send_json(s,{'cmd':cmd})
                    self.assertFalse(json.loads(stream.readline())['ok'])
                self.assertFalse(self.service.state.armed)

    def test_reconnecting_clears_old_disconnect_but_never_arms(self):
        s,f,session=self.connect()
        f.close();s.close()
        deadline=time.monotonic()+1
        while self.service.state.session and time.monotonic()<deadline:time.sleep(.01)
        self.assertEqual(self.service.state.fault,'Pilot disconnected')
        s,f,new_session=self.connect()
        try:
            self.assertNotEqual(session,new_session)
            tp.send_json(s,{'cmd':'status'});status=json.loads(f.readline())
            self.assertFalse(status['armed'])
            self.assertNotEqual(status['fault'],'Pilot disconnected')
            self.assertFalse(status['motion_diagnostics']['tracking_valid'])
        finally:
            f.close();s.close()
            deadline=time.monotonic()+1
            while self.service.state.session and time.monotonic()<deadline:time.sleep(.01)

    def test_explicit_stop_clears_old_delay_baseline_without_arming(self):
        s,f,session=self.connect()
        try:
            self.service.state.min_offset=-1000.
            previous_seq=self.service.state.seq
            tp.send_json(s,{'cmd':'stop'})
            reply=json.loads(f.readline())
            self.assertTrue(reply['ok']);self.assertFalse(reply['armed'])
            self.assertIsNone(self.service.state.min_offset)
            self.assertEqual(self.service.state.seq,previous_seq)
        finally:
            f.close();s.close()

    def test_start_requested_before_external_stop_is_rejected(self):
        s,f,session=self.connect()
        try:
            expected=self.service.state.stop_generation
            with socket.create_connection(('127.0.0.1',self.port),timeout=2) as emergency:
                with emergency.makefile('rb') as reply:
                    tp.send_json(emergency,{'token':'test-pairing-code','emergency':True})
                    self.assertFalse(json.loads(reply.readline())['armed'])
            tp.send_json(s,{'cmd':'arm','expected_stop_generation':expected})
            reply=json.loads(f.readline())
            self.assertFalse(reply['ok']);self.assertIn('newer STOP',reply['error'])
            self.assertFalse(self.service.state.armed)
        finally:
            f.close();s.close()
            deadline=time.monotonic()+1
            while self.service.state.session and time.monotonic()<deadline:time.sleep(.01)

    def test_preparation_requires_confirmation_and_never_arms(self):
        with self.assertRaisesRegex(ValueError,'confirmation'):
            self.service.prepare_motion(False)
        self.assertFalse(self.service.state.preparing)
        self.service.prepare_motion(True)
        deadline=time.monotonic()+1
        while self.service.state.preparing and time.monotonic()<deadline: time.sleep(.01)
        self.assertFalse(self.service.state.preparing)
        self.assertFalse(self.service.state.armed)
        self.assertIn('Posture ready',self.service.state.prepare_message)

    def test_preparation_does_not_disable_life_again_when_already_disabled(self):
        robot=Mock(simulate=False)
        robot.life.getState.return_value.value.return_value='disabled'
        robot.motion.robotIsWakeUp.return_value=True
        state=tp.State(robot);state.preparing=True
        tp.Service._prepare_motion(Mock(state=state))
        robot.life.setState.assert_not_called()
        robot.motion.wakeUp.assert_called_once_with(_async=True)
        self.assertEqual(state.prepare_message,'Posture ready.')
        self.assertFalse(state.armed)
        self.assertFalse(state.preparing)

    def test_stop_cancels_preparation_before_wake_up(self):
        entered=threading.Event();cancelled=threading.Event()
        class Future:
            def value(self,timeout):
                entered.set()
                cancelled.wait(1)
                raise RuntimeError('cancelled')
            def cancel(self): cancelled.set()
        robot=Mock(simulate=False)
        robot.life.setState.return_value=Future()
        state=tp.State(robot);state.preparing=True
        service=Mock(state=state)
        worker=threading.Thread(target=tp.Service._prepare_motion,args=(service,))
        worker.start();self.assertTrue(entered.wait(1));state.disarm();worker.join(1)
        self.assertFalse(worker.is_alive())
        robot.motion.wakeUp.assert_not_called()
        self.assertFalse(state.preparing)
        self.assertFalse(state.armed)

    def test_piper_clip_ack_waits_for_robot_playback_and_rejects_overlap(self):
        woz=self.service.woz
        done=threading.Event();future=Mock()
        future.isFinished.side_effect=done.is_set
        player=Mock();player.playFile.return_value=future
        a,b=socket.socketpair();a.settimeout(2);b.settimeout(.1)
        reader=a.makefile('rb');thread=None
        try:
            with patch.object(woz,'authorized',return_value=True),patch.dict(woz.services,{'ALAudioPlayer':player}):
                thread=threading.Thread(target=woz.play_piper,args=(a,reader,{'session':'test','piper_bytes':640},'127.0.0.1'));thread.start()
                self.assertEqual(b.recv(128),b'{"ready":true}\n');b.sendall(b'\x01\x00'*320)
                deadline=time.monotonic()+1
                while not player.playFile.called and time.monotonic()<deadline:time.sleep(.005)
                self.assertEqual(player.playFile.call_count,1)
                path=player.playFile.call_args.args[0]
                import wave
                with wave.open(path,'rb') as wav:self.assertEqual((wav.getnchannels(),wav.getframerate(),wav.readframes(320)),(1,16000,b'\x01\x00'*320))
                with self.assertRaises(socket.timeout):b.recv(128)
                c,d=socket.socketpair()
                try:woz.play_piper(c,None,{'piper_bytes':640},'127.0.0.1');self.assertIn(b'already playing',d.recv(128))
                finally:c.close();d.close()
                done.set();b.settimeout(2);self.assertEqual(b.recv(128),b'{"played":true}\n');thread.join(1)
                self.assertFalse(pathlib.Path(path).exists());self.assertEqual(player.playFile.call_count,1)
        finally:
            done.set()
            if thread:thread.join(2)
            reader.close();a.close();b.close()

    def test_speech_stop_discards_pending_streamed_audio(self):
        woz=self.service.woz
        with woz.audio_lock:woz.audio_output.append((woz.clock(),b'\x01\x00'*320))
        woz.action({'cmd':'speech_stop'})
        self.assertEqual(len(woz.audio_output),0)
        self.assertIsNone(woz.speech_future)

    def test_tablet_fades_expires_once_and_new_content_restarts_timer(self):
        woz=self.service.woz
        with patch.object(woz,'clock',return_value=10.):
            woz.action({'cmd':'tablet','reaction':'smile'})
            rev=woz.tablet['revision']
            self.assertEqual(woz.status()['tablet']['fade_after_ms'],8000)
        with patch.object(woz,'clock',return_value=18.5):
            self.assertAlmostEqual(woz.status()['tablet']['opacity'],.5)
            woz.caption('New sentence')
            self.assertEqual(woz.status()['tablet']['opacity'],1.)
        with patch.object(woz,'clock',return_value=27.5):
            expired=woz.status()['tablet']
            self.assertEqual(expired['text'],'')
            self.assertNotIn('reaction',expired)
            self.assertEqual(expired['revision'],rev+2)
            self.assertEqual(woz.status()['tablet']['revision'],expired['revision'])

    def test_expired_tablet_choices_are_not_accepted(self):
        woz=self.service.woz
        with patch.object(woz,'clock',return_value=10.):
            woz.action({'cmd':'tablet','text':'Question','choices':['OK']})
        with patch.object(woz,'clock',return_value=19.):
            with self.assertRaisesRegex(ValueError,'Stale tablet response'):
                woz.action({'cmd':'participant_response','revision':woz.tablet['revision'],'choice':0})

    def test_tablet_reactions_replace_content_and_clear_on_caption(self):
        woz=self.service.woz
        self.assertIn('How are you?',woz.phrases)
        self.assertNotIn('How do you feel?',woz.phrases)
        for name in ('smile','laugh','love','surprise','sad','wink','angry'):
            previous=woz.tablet['revision']
            woz.action({'cmd':'tablet','reaction':name,'text':'Old text','choices':['Old button']})
            self.assertEqual(woz.tablet,{'revision':previous+1,'text':'','choices':[],'reaction':name})
        with self.assertRaises(ValueError):woz.action({'cmd':'tablet','reaction':'unknown'})
        woz.action({'cmd':'speech_caption','text':'Hello'})
        self.assertNotIn('reaction',woz.tablet)
        woz.action({'cmd':'tablet','reaction':'smile'})
        woz.action({'cmd':'tablet','text':''})
        self.assertNotIn('reaction',woz.tablet)
        self.assertEqual(woz.tablet['text'],'')

    def test_speech_captions_toggle_and_clear_stale_choices(self):
        woz=self.service.woz
        woz.action({'cmd':'tablet','text':'Question','choices':['Yes','No']})
        old=woz.tablet['revision']
        woz.action({'cmd':'say','text':'Hello'})
        self.assertEqual(woz.tablet,{'revision':old+1,'text':'Hello','choices':[]})
        woz.action({'cmd':'speech_on_tablet','enabled':False})
        woz.action({'cmd':'tablet','text':'Keep this','choices':['OK']})
        preserved=dict(woz.tablet)
        woz.action({'cmd':'say','text':'Hidden TTS'})
        woz.action({'cmd':'speech_caption','text':'Hidden Cori'})
        self.assertEqual(woz.tablet,preserved)
        woz.action({'cmd':'speech_on_tablet','enabled':True})
        woz.action({'cmd':'speech_caption','text':'Cori phrase'})
        self.assertEqual(woz.tablet['text'],'Cori phrase')
        self.assertEqual(woz.tablet['choices'],[])
        self.assertTrue(woz.status()['speech_on_tablet'])
        with self.assertRaises(ValueError):woz.action({'cmd':'speech_on_tablet','enabled':'false'})
        for text in ('','x'*401,42):
            with self.assertRaises(ValueError):woz.action({'cmd':'speech_caption','text':text})

    def test_failed_speech_preserves_tablet(self):
        woz=self.service.woz
        woz.action({'cmd':'tablet','text':'Keep this'})
        previous=dict(woz.tablet)
        tts=Mock()
        tts.say.side_effect=RuntimeError('TTS failed')
        with patch.dict(woz.services,{'ALTextToSpeech':tts}):
            with self.assertRaises(RuntimeError):woz.action({'cmd':'say','text':'Not spoken'})
        self.assertEqual(woz.tablet,previous)

    def test_tablet_response_is_bound_to_current_content(self):
        woz=self.service.woz
        woz.action({'cmd':'tablet','text':'Domanda','choices':['Sì','No']})
        old=woz.tablet['revision']
        woz.action({'cmd':'tablet','text':'Nuova domanda','choices':['OK']})
        with self.assertRaisesRegex(ValueError,'Stale'):
            woz.action({'cmd':'participant_response','revision':old,'choice':0})
        woz.action({'cmd':'participant_response','revision':woz.tablet['revision'],'choice':0})
        self.assertEqual(woz.last_response['value'],'OK')

    def test_optional_sensor_timeout_preserves_motion_readings(self):
        robot=Mock();memory=Mock()
        robot.motion.getAngles.return_value.value.return_value=[0.]*14
        robot.motion.getRobotPosition.return_value.value.return_value=[0.,0.,0.]
        robot.motion.robotIsWakeUp.return_value.value.return_value=True
        robot.life.getState.return_value.value.return_value='disabled'
        memory.getListData.return_value.value.side_effect=RuntimeError('Future timeout')
        memory.getListData.return_value.isFinished.return_value=False
        batch,errors=self.service.woz.read_sensor_batch(robot,memory,['temperature'])
        self.assertEqual(batch['joint_measured'],[0.]*14)
        self.assertTrue(batch['awake'])
        self.assertEqual(batch['life_state'],'disabled')
        self.assertEqual(set(errors),{'memory'})
        memory.getListData.return_value.cancel.assert_not_called()
        self.service.woz.read_sensor_batch(robot,memory,['temperature'])
        memory.getListData.assert_called_once()

    def test_blocked_life_rpc_is_not_repeated_and_delayed_reply_is_discarded(self):
        robot=Mock();woz=self.service.woz
        robot.life.getState.return_value.value.side_effect=RuntimeError('Future timeout')
        robot.life.getState.return_value.isFinished.return_value=False
        with patch.object(woz,'clock',return_value=10.):
            for _ in range(20):woz.read_sensor_batch(robot,None,[])
        robot.life.getState.assert_called_once()
        robot.life.getState.return_value.value.side_effect=None
        robot.life.getState.return_value.value.return_value='disabled'
        with patch.object(woz,'clock',return_value=11.):
            result,errors=woz.read_sensor_batch(robot,None,[])
        self.assertNotIn('life_state',result)
        self.assertIn('delayed',errors['life_state'])
        with patch.object(woz,'clock',return_value=11.1):woz.read_sensor_batch(robot,None,[])
        self.assertEqual(robot.life.getState.call_count,2)

    def test_lost_robot_services_cancel_recovery_and_restart_disarmed(self):
        service=self.service
        robot=service.state.robot
        service.state.armed=True
        service.state.recovery_until=100.
        robot.simulate=False;robot.session=Mock();robot.session.isConnected.return_value=False
        try:
            self.assertFalse(service.check_robot_connection())
            self.assertFalse(service.running)
            self.assertFalse(service.state.armed)
            self.assertIsNone(service.state.recovery_until)
            self.assertIsNone(service.state.latest)
        finally:
            robot.simulate=True;service.running=True

    def test_tablet_delivery_requires_display_ack_and_expires(self):
        woz=self.service.woz
        woz.action({'cmd':'tablet','text':'Continue?','choices':['Yes','No']})
        revision=woz.tablet['revision']
        with patch.object(woz,'clock',return_value=100.):
            self.assertFalse(woz.status()['tablet_display']['connected'])
            woz.action({'cmd':'tablet_poll','displayed_revision':revision-1})
            self.assertNotEqual(woz.status()['tablet_display']['revision'],revision)
            woz.action({'cmd':'tablet_poll','displayed_revision':revision})
            self.assertEqual(woz.status()['tablet_display'],{'connected':True,'revision':revision})
        with patch.object(woz,'clock',return_value=103.):
            self.assertFalse(woz.status()['tablet_display']['connected'])

    def test_all_motor_temperatures_exposed_without_invented_readings(self):
        t=self.service.woz.status()['telemetry']
        self.assertEqual(len(t['joint_temperature']),20)
        self.assertIn('WheelB',t['joint_temperature'])
        self.assertIn('KneePitch',t['joint_temperature_status'])
        self.assertTrue(all(v is None for v in t['joint_temperature'].values()))
        self.assertTrue(all(v is None for v in t['joint_temperature_status'].values()))

    def test_simulation_does_not_invent_sensor_values(self):
        readings=self.service.woz.status()['telemetry']['readings']
        self.assertTrue(all(value is None for value in readings.values()))

    def test_remote_video_does_not_require_local_release_api(self):
        # Real Pepper qi exposes getImageRemote but no releaseImage method.
        class Camera:
            def subscribeCamera(self,*args): return 'test-camera'
            def getImageRemote(self,*args): return [2,2,3,11,1,1,b'\x80\x40\x20'*4]
            def unsubscribe(self,*args): pass
        robot=Mock(simulate=False,video=Camera())
        service=Mock(running=True,state=Mock(robot=robot),woz=Mock(camera=0),video_diagnostics=[None]*3)
        service.woz.authorized.return_value=True
        server,client=socket.socketpair();client.settimeout(2)
        thread=threading.Thread(target=tp.Service.video,args=(service,server,('127.0.0.1',0)))
        thread.start()
        try:
            tp.send_json(client,{'session':'test'})
            with client.makefile('rb') as stream:
                size=struct.unpack('!I',stream.read(4))[0]
                self.assertTrue(stream.read(size).startswith(b'\xff\xd8'))
                client.sendall(b'X')
        finally:
            client.close();thread.join(2)

    def test_three_camera_connections_select_independent_sensors_and_rates(self):
        # All feeds must remain separate even while the global camera selection changes.
        calls=[]
        class Camera:
            def subscribeCamera(self,name,camera,resolution,color,fps):
                calls.append((camera,color,fps));return camera
            def getImageRemote(self,camera):
                pixels=struct.pack('<4H',100,500,1000,2000) if camera==2 else b'\x80\x40\x20'*4
                return [2,2,1 if camera==2 else 3,17 if camera==2 else 11,1,1,pixels]
            def unsubscribe(self,*args): pass
        service=Mock(running=True,state=Mock(robot=Mock(simulate=False,video=Camera())),woz=Mock(camera=1),video_diagnostics=[None]*3)
        service.woz.authorized.return_value=True
        peers=[];threads=[]
        try:
            for camera,fps in [(0,30),(1,10),(2,5)]:
                server,client=socket.socketpair();client.settimeout(2);peers.append(client)
                thread=threading.Thread(target=tp.Service.video,args=(service,server,('127.0.0.1',0)));threads.append(thread);thread.start()
                tp.send_json(client,{'session':'test','camera':camera,'fps':fps})
            for client in peers:
                with client.makefile('rb') as stream:
                    length=struct.unpack('!I',stream.read(4))[0]
                    self.assertTrue(stream.read(length).startswith(b'\xff\xd8'))
            self.assertEqual(sorted(calls),[(0,11,30),(1,11,10),(2,17,5)])
        finally:
            for client in peers:
                try: client.sendall(b'X')
                except OSError: pass
                client.close()
            for thread in threads:thread.join(2)
        self.assertTrue(all(not thread.is_alive() for thread in threads))

    def test_duplicate_and_missing_images_do_not_busy_poll_camera(self):
        from io import BytesIO
        current=[10.];requested=[]
        camera=Mock();camera.subscribeCamera.return_value='subscription'
        service=Mock(running=True,state=Mock(robot=Mock(simulate=False,video=camera)),woz=Mock(camera=1),video_diagnostics=[None]*3)
        service.woz.authorized.return_value=True
        def capture(unused):
            requested.append(current[0])
            if len(requested)>=12:service.running=False
            if len(requested)%3==0:return None
            return [2,2,3,11,1,1,b'\x80\x40\x20'*4]
        camera.getImageRemote.side_effect=capture
        conn=Mock();conn.makefile.return_value=BytesIO(b'{"session":"test","camera":1,"fps":10}\n'+b'N'*20)
        camera_thread=threading.get_ident();real_sleep=time.sleep;real_clock=tp.clock
        def sleep(delay):
            if threading.get_ident()==camera_thread:current[0]+=delay
            else:real_sleep(delay)
        def isolated_clock():
            return current[0] if threading.get_ident()==camera_thread else real_clock()
        with patch.object(tp,'clock',side_effect=isolated_clock),patch.object(tp.time,'sleep',side_effect=sleep):
            tp.Service.video(service,conn,('127.0.0.1',0))
        self.assertEqual(len(requested),12)
        self.assertTrue(all(b-a>=.099999 for a,b in zip(requested,requested[1:])))
        self.assertEqual(conn.sendall.call_count,1)
        self.assertGreater(service.video_diagnostics[1]['duplicates'],0)
        self.assertFalse(service.video_diagnostics[1]['connected'])

    def test_video_does_not_block_motion_and_tablet_stop_works(self):
        s,f,session=self.connect()
        v=socket.create_connection(('127.0.0.1',self.port+2),timeout=2)
        tp.send_json(v,{'session':session})
        vf=v.makefile('rb');length=struct.unpack('!I',vf.read(4))[0];jpeg=vf.read(length)
        self.assertTrue(jpeg.startswith(b'\xff\xd8'))
        # Withhold next-frame credit. Control must continue independently.
        u=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);u.settimeout(1)
        packet=dict(session=session,seq=1,sent=tp.clock()*1000,active=True,drive=True,
                    angles=[0,0,1.3,.15,0,-.5,0,1.3,-.15,0,.5,0,1,1],base=[.1,0,0])
        u.sendto(json.dumps(packet).encode(),('127.0.0.1',self.port+1))
        time.sleep(.01)
        tp.send_json(s,{'cmd':'arm'});self.assertTrue(json.loads(f.readline())['armed'])
        packet.update(seq=2,sent=tp.clock()*1000)
        u.sendto(json.dumps(packet).encode(),('127.0.0.1',self.port+1))
        deadline=time.monotonic()+1
        while True:
            u.settimeout(max(.001,deadline-time.monotonic()))
            ack=json.loads(u.recv(2048))
            if ack.get('kind')!='received' and ack['seq']==2:break
            self.assertLess(time.monotonic(),deadline)
        self.assertIn('apply_ms',ack)
        emergency=socket.create_connection(('127.0.0.1',self.port),timeout=2)
        tp.send_json(emergency,{'token':'test-pairing-code','emergency':True})
        self.assertFalse(json.loads(emergency.makefile('rb').readline())['armed'])
        self.assertFalse(self.service.state.armed)
        emergency.close();u.close();vf.close();v.close();f.close();s.close()
        time.sleep(.1)

if __name__ == '__main__': unittest.main()
