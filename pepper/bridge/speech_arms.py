"""Exclusive arm ownership for Piper speech; never controls head, hips or wheels.

All methods run under State.lock. Futures are polled, never waited on. Gesture
clips are arm-only derivatives of the attributed Pepper Core Animations.
"""
import math
import os

PATHS = tuple('telepepper-speaking/'+name+'.qianim'
              for name in ('happy', 'point_left', 'point_right'))
ARM_INDICES = tuple(range(2, 14))


def install_package(session, path):
    # Optional feature: an unavailable package must not disable manual clips.
    try:
        if not os.path.isfile(path):
            return False
        manager=session.service('PackageManager')
        if not manager.hasPackage('telepepper-speaking') or manager.package2('telepepper-speaking')['version']!='1.0.0':
            if not manager.install(path,_async=True).value(10000):
                raise RuntimeError('Speaking arm package installation failed')
        return True
    except Exception as exc:
        print('Speaking arm package unavailable: '+str(exc))
        return False


class SpeechArms(object):
    def __init__(self, robot, clock):
        self.robot, self.clock = robot, clock
        self.enabled = False
        self.speaking = False
        self.until = 0.
        self.source = None
        self.phase = 'idle'
        self.future = None
        self.began = 0.
        self.index = 0
        self.blend_at = None
        self.origin = None

    def available(self):
        return self.robot.simulate or getattr(self.robot,'speech_animation_player',None) is not None or all(p in self.robot.available_animations for p in PATHS)

    def status(self):
        return dict(enabled=self.enabled, available=self.available(),
                    active=self.phase != 'idle', stage=self.phase)

    def set_enabled(self, enabled):
        if type(enabled) is not bool:
            raise ValueError('Speech gesture setting must be boolean')
        if enabled and not self.available():
            raise ValueError('Arm-only speaking animations are not installed')
        self.enabled = enabled
        if not enabled:
            self.end()

    def begin(self, duration, source='speech'):
        # Caller checks armed/fresh tracking. This never arms or prepares Pepper.
        if not self.enabled or not self.available():
            return
        self.speaking = True
        self.source = source
        self.until = self.clock()+min(60., max(.1, duration))
        if self.phase == 'idle':
            self.phase = 'waiting'
            self.began = self.clock()

    def end(self, source=None):
        if source is not None and source != self.source:
            return
        self.speaking = False
        self.source = None
        if self.phase == 'playing':
            if self.future is not None:
                self.future.cancel()
            self.phase = 'cancelling'
            self.began = self.clock()
        elif self.phase == 'waiting':
            self.phase = 'idle'

    def stop(self):
        """Transfer unfinished clip cancellation to Robot's existing STOP barrier."""
        self.speaking = False
        self.source = None
        future = self.future
        try:
            if future is not None and not future.isFinished():future.cancel()
        finally:
            self.future = None
            self.phase = 'idle'
            self.blend_at = None
        return future

    def poll(self, names):
        now = self.clock()
        if self.speaking and now >= self.until:
            self.end()
        if self.phase == 'waiting':
            for channel in ('arms', 'hands'):
                slot = self.robot.motion_pending.get(channel)
                if slot and not slot[0].isFinished():
                    if now-self.began > .5:
                        raise RuntimeError('Speech arms command drain timed out')
                    return True
            if self.robot.simulate:
                self.phase = 'playing'
            else:
                self.future = getattr(self.robot,'speech_animation_player',self.robot.animation_player).run(PATHS[self.index % len(PATHS)], _async=True)
                self.index += 1
                self.phase = 'playing'
            self.began = now
        if self.phase in ('playing', 'cancelling'):
            done = (now-self.began > 2.) if self.robot.simulate else self.future.isFinished()
            if not done:
                if now-self.began > 15.:
                    raise RuntimeError('Speech arm animation acknowledgement stalled')
                return True
            if self.phase == 'playing' and self.future is not None:
                self.future.value(0)
            # Do not read a pose until cancellation is acknowledged.
            self.future = None if self.robot.simulate else self.robot.motion.getAngles(names, True, _async=True)
            self.phase = 'reading'
            self.began = now
        if self.phase == 'reading':
            if self.future is not None and not self.future.isFinished():
                if now-self.began > .5:
                    raise RuntimeError('Speech arm return pose timed out')
                return True
            pose = list(self.robot.previous) if self.robot.simulate else list(self.future.value(0))
            if len(pose) != 14 or any(math.isnan(v) or math.isinf(v) for v in pose):
                raise RuntimeError('Invalid speech arm return pose')
            self.future = None
            self.origin = pose
            for i in ARM_INDICES:
                self.robot.previous[i] = pose[i]
                self.robot.velocity[i] = 0.
                self.robot.last_target[i] = pose[i]
            for channel in ('arms', 'hands'):
                self.robot.motion_targets.pop(channel, None)
            self.blend_at = now
            self.phase = 'idle'
        if self.phase == 'idle' and self.speaking and self.enabled:
            self.phase = 'waiting'
            self.began = now
            return True
        return self.phase != 'idle'

    def target(self, i, target):
        if self.blend_at is None or i not in ARM_INDICES:
            return target
        t = min(1., max(0., (self.clock()-self.blend_at)/.75))
        return self.origin[i]+(t*t*(3.-2.*t))*(target-self.origin[i])
