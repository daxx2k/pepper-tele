"""Optional torso-heading follower. No translation, no automatic arming."""
import math

def wrap(angle):
    return math.atan2(math.sin(angle), math.cos(angle))

class BaseHeading(object):
    def __init__(self):
        self.reset()

    def reset(self):
        self.anchor = None
        self.status = 'off'

    def command(self, enabled, allowed, body_yaw, manual, feedback, now):
        result = list(manual)
        if not enabled or not allowed:
            self.reset()
            return result
        if feedback is None or now-feedback[1] > .15:
            self.anchor = None
            self.status = 'waiting for odometry'
            return result  # no autonomous motion without fresh feedback
        yaw = feedback[0]
        if any(abs(v) > .0001 for v in manual):
            self.anchor = wrap(yaw-body_yaw)
            self.status = 'manual override'
            return result
        if self.anchor is None:
            self.anchor = wrap(yaw-body_yaw)
        error = wrap(self.anchor+body_yaw-yaw)
        # Three-degree deadband prevents small tracking changes turning the base.
        result[2] = 0. if abs(error) < math.radians(3) else max(-.2,min(.2,.75*error))
        self.status = 'following torso'
        return result
