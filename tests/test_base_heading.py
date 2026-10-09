import sys,pathlib,unittest,math
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'pepper/bridge'))
from base_heading import BaseHeading

class HeadingTests(unittest.TestCase):
    def setUp(self):self.heading=BaseHeading()
    def cmd(self,yaw=0,manual=None,feedback=None,enabled=True,allowed=True):
        return self.heading.command(enabled,allowed,yaw,manual or [0,0,0],feedback,1.)
    def test_activation_has_no_jump_and_only_turns_after_body_moves(self):
        self.assertEqual(self.cmd(.7,feedback=(1.4,1)),[0,0,0])
        self.assertGreater(self.cmd(1.,feedback=(1.4,1))[2],0)
        self.assertEqual(self.cmd(1.,feedback=(1.4,1))[:2],[0,0])
    def test_manual_sticks_override_without_snapback(self):
        self.cmd(feedback=(0,1))
        self.assertEqual(self.cmd(.3,[0,0,-.2],(.5,1)),[0,0,-.2])
        self.assertEqual(self.cmd(.3,feedback=(.5,1)),[0,0,0])
        self.assertEqual(self.cmd(.3,[.2,0,0],(.5,1)),[.2,0,0])
    def test_missing_stale_feedback_or_disabled_mode_cannot_turn(self):
        self.cmd(feedback=(0,1))
        for feedback in [None,(0,.84)]:
            self.assertEqual(self.cmd(1.,feedback=feedback),[0,0,0])
        self.assertEqual(self.cmd(1.,feedback=(0,1),enabled=False),[0,0,0])
        self.assertEqual(self.cmd(1.,feedback=(0,1),allowed=False),[0,0,0])
    def test_wrap_deadband_and_speed_limit(self):
        self.cmd(feedback=(3.13,1))
        self.assertEqual(self.cmd(.01,feedback=(3.13,1))[2],0)
        self.assertLessEqual(abs(self.cmd(1.,feedback=(-3.13,1))[2]),.2)
        self.assertGreater(self.cmd(.1,feedback=(-3.13,1))[2],0)

