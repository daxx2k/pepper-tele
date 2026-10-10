import sys
import threading
import time
import unittest
from pathlib import Path
from unittest.mock import Mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'pepper/bridge'))
import telepepper as tp
from motion_dispatch import MotionDispatch,SubmissionCancelled,queue

class Future:
    def __init__(self,value=True):self.result=value
    def isFinished(self):return True
    def value(self,timeout=0):return self.result

class DispatchTests(unittest.TestCase):
    def setUp(self):
        self.dispatch=MotionDispatch();self.release=threading.Event();self.entered=threading.Event()
    def tearDown(self):self.release.set();self.dispatch.close()
    def slow(self):self.entered.set();self.release.wait(2);return Future()
    def block(self):
        first=self.dispatch.submit(self.slow);self.assertTrue(self.entered.wait(1));return first
    def test_submission_returns_while_sdk_is_blocked_and_queue_is_bounded(self):
        first=self.block()
        for i in range(8):self.dispatch.submit(lambda:Future())
        with self.assertRaises(queue.Full):self.dispatch.submit(lambda:Future())
        self.assertFalse(first.isFinished())
    def test_stop_invalidation_does_not_wait_for_sdk_and_drops_queued_commands(self):
        first=self.block();operation=Mock(return_value=Future());queued=self.dispatch.submit(operation)
        start=time.monotonic();drain=self.dispatch.invalidate()
        self.assertLess(time.monotonic()-start,.1);self.assertFalse(drain.isFinished())
        self.assertTrue(queued.ready.wait(.1))
        with self.assertRaises(SubmissionCancelled):queued.value()
        self.release.set();self.assertTrue(first.ready.wait(1));drain.value();operation.assert_not_called()
        fresh=self.dispatch.submit(operation);self.assertTrue(fresh.ready.wait(1));operation.assert_called_once()
    def test_submission_failure_is_reported_and_drain_does_not_hide_it(self):
        ready=self.dispatch.submit(lambda:(_ for _ in ()).throw(RuntimeError('SDK failed')))
        self.assertTrue(ready.ready.wait(1))
        with self.assertRaisesRegex(RuntimeError,'SDK failed'):ready.value()
    def robot(self):
        robot=tp.Robot(True);robot.simulate=False;robot.dispatch=self.dispatch;robot.motion=Mock()
        robot.motion.moveToward.return_value=Future();robot.motion.stopMove.return_value=Future()
        robot.motion.getAngles.side_effect=lambda names,*args,**kw:Future(list(robot.previous) if len(names)==14 else [0.,0.])
        robot.motion.setAngles.return_value=Future()
        return robot
    def test_stop_brakes_before_drain_then_brakes_again_and_reads_fresh_pose(self):
        robot=self.robot();first=self.block();queued=Mock(return_value=Future())
        robot.motion_pending['arms']=(first,tp.clock());robot.submit_motion('head',queued)
        start=time.monotonic();robot.stop();self.assertLess(time.monotonic()-start,.1)
        robot.motion.stopMove.assert_called_once_with(_async=True);robot.motion.getAngles.assert_not_called()
        robot.poll_stop();robot.motion.getAngles.assert_not_called()
        self.release.set();self.assertTrue(first.ready.wait(1));robot.poll_stop()
        self.assertEqual(robot.motion.stopMove.call_count,2);self.assertEqual(robot.motion.getAngles.call_count,2)
        # Replacement braking futures must not be removed by an old loop snapshot.
        self.assertIn('base_stop',robot.stop_pending);queued.assert_not_called()
        robot.poll_stop();robot.poll_stop();self.assertFalse(robot.stop_pending)
    def test_hold_waits_for_both_dispatch_and_animation_cancellation(self):
        robot=self.robot();first=self.block();clip=Mock();clip.isFinished.return_value=False
        robot.animation_future=clip;robot.motion_pending['arms']=(first,tp.clock());robot.stop()
        self.release.set();self.assertTrue(first.ready.wait(1));robot.poll_stop();robot.motion.getAngles.assert_not_called()
        clip.isFinished.return_value=True;robot.poll_stop();self.assertEqual(robot.motion.getAngles.call_count,2)
    def test_pending_channel_coalesces_targets_and_animation_waits_for_submission(self):
        robot=self.robot();first=self.block();robot.motion_pending['arms']=(first,tp.clock())
        operation=Mock(return_value=Future());self.assertFalse(robot.submit_motion('arms',operation));operation.assert_not_called()
        robot.animation_player=Mock();robot.available_animations=set(tp.OFFICIAL_ANIMATIONS.values())
        self.assertFalse(robot.begin_animation('yes'));robot.animation_player.run.assert_not_called()
    def test_closed_dispatcher_rejects_new_submissions(self):
        self.dispatch.close()
        with self.assertRaisesRegex(RuntimeError,'closed'):self.dispatch.submit(lambda:Future())

    def test_broken_dispatch_completion_cannot_skip_immediate_base_braking(self):
        robot=self.robot();broken=Mock();broken.isFinished.side_effect=RuntimeError('lost SDK future')
        pending=self.dispatch.submit(lambda:broken);self.assertTrue(pending.ready.wait(1))
        robot.motion_pending['head']=(pending,tp.clock());robot.stop()
        robot.motion.stopMove.assert_called_once_with(_async=True)
        with self.assertRaisesRegex(RuntimeError,'completion check failed'):robot.poll_stop()
        self.assertIn('motion_drain',robot.stop_pending);robot.motion.getAngles.assert_not_called()
