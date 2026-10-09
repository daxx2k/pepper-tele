#!/usr/bin/env python
"""TelePepper head service. Python 2.7 (robot) and Python 3 (simulator).
Control and video use independent sockets; motion consumes a latest-only slot.
"""
from __future__ import print_function
import argparse
import json
import math
import os
import socket
import struct
import threading
import time
import uuid
import signal
import select
import re
from io import BytesIO
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from woz import WoZ
from base_heading import BaseHeading
from torso_assist import TorsoAssist
from official_animations import CATALOG as OFFICIAL_ANIMATIONS, PACKAGE_VERSION

try:
    clock = time.monotonic
except AttributeError:
    import ctypes
    class Timespec(ctypes.Structure):
        _fields_ = [('sec', ctypes.c_long), ('nsec', ctypes.c_long)]
    _lib = ctypes.CDLL('librt.so.1', use_errno=True)
    def clock():
        ts = Timespec()
        if _lib.clock_gettime(1, ctypes.byref(ts)) != 0:
            raise RuntimeError('CLOCK_MONOTONIC unavailable')
        return ts.sec + ts.nsec * 1e-9

NAMES = ['HeadYaw', 'HeadPitch', 'LShoulderPitch', 'LShoulderRoll',
         'LElbowYaw', 'LElbowRoll', 'LWristYaw', 'RShoulderPitch',
         'RShoulderRoll', 'RElbowYaw', 'RElbowRoll', 'RWristYaw', 'LHand', 'RHand']
# Conservative envelopes, intersected with the robot's own limits at startup.
LIMITS = [(-1.0, 1.0), (-.68, .43), (-2.05, 2.05), (.05, 1.48),
          (-1.8, 1.8), (-1.5, -.05), (-1.7, 1.7), (-2.05, 2.05),
          (-1.48, -.05), (-1.8, 1.8), (.05, 1.5), (-1.7, 1.7), (0, 1), (0, 1)]
# None = normal base speeds; 0.05 = supervised test at 5% of SDK maximum.
# Numeric test caps must be > 0 and <= 0.1. Restore normal obstacle margins
# before switching from a test cap back to None.
BASE_TEST_CAP = None
# External collision margins in metres. These do not disable base protection.
BASE_ORTHOGONAL_SECURITY_M = 0.20
BASE_TANGENTIAL_SECURITY_M = 0.05
#BASE_ORTHOGONAL_SECURITY_M = 0.40
#BASE_TANGENTIAL_SECURITY_M = 0.10
def clamp(x, low, high):
    return min(high, max(low, x))

def finite_vector(value, size):
    if not isinstance(value, list) or len(value) != size:
        raise ValueError('vector length')
    out = [float(v) for v in value]
    if any(math.isnan(v) or math.isinf(v) for v in out):
        raise ValueError('non-finite value')
    return out

def cap_test_base(base, cap):
    if cap is None:return list(base)
    x,y,theta=base
    length=math.hypot(x,y)
    if length>cap:
        x*=cap/length;y*=cap/length
    return [x,y,clamp(theta,-cap,cap)]

def configure_base_security(motion):
    orthogonal=float(BASE_ORTHOGONAL_SECURITY_M)
    tangential=float(BASE_TANGENTIAL_SECURITY_M)
    if any(math.isnan(v) or math.isinf(v) or v<=0 for v in (orthogonal,tangential)):
        raise ValueError('Base collision distances must be positive finite metres')
    if orthogonal<tangential:
        raise ValueError('Orthogonal distance must be at least tangential distance')
    old_o=motion.getOrthogonalSecurityDistance(_async=True).value(2000)
    old_t=motion.getTangentialSecurityDistance(_async=True).value(2000)
    # Expand first, then reduce, preserving orthogonal >= tangential throughout.
    motion.setOrthogonalSecurityDistance(max(old_o,orthogonal,tangential),_async=True).value(2000)
    motion.setTangentialSecurityDistance(tangential,_async=True).value(2000)
    motion.setOrthogonalSecurityDistance(orthogonal,_async=True).value(2000)
    actual_o=motion.getOrthogonalSecurityDistance(_async=True).value(2000)
    actual_t=motion.getTangentialSecurityDistance(_async=True).value(2000)
    if abs(actual_o-orthogonal)>.001 or abs(actual_t-tangential)>.001:
        raise RuntimeError('Base collision distances were not applied')
    return actual_o,actual_t

def cleanup_video_subscriptions(video):
    # Only the exact namespace allocated by this TelePepper bridge. Never touch
    # HumanPerception, ALPodDetection or another app's camera registrations.
    names=video.getSubscribers(_async=True).value(500)
    for name in names:
        if re.match(r'^TP_[0-9a-f]{12}_[0-9]+$',name):
            video.unsubscribe(name,_async=True).value(500)

def prepare_bottom_camera(video):
    # Repair persistent manual exposure left by another app or an earlier run.
    # These bounded calls run only when subscribing, outside the motion path.
    result={}
    for name,parameter in (('auto_exposure',11),('auto_gain',13)):
        try:
            enabled=video.getParameter(1,parameter,_async=True).value(1000)
            if enabled!=1:
                if video.setParameter(1,parameter,1,_async=True).value(1000) is False:
                    raise RuntimeError('Camera refused automatic setting')
            result[name]=1
        except Exception as exc: result[name+'_error']=str(exc)
    return result

from animation_resources import ResourceAnimationPlayer
from speech_arms import SpeechArms, install_package

class Robot(object):
    def __init__(self, simulate=False):
        self.simulate = simulate
        self.limits = list(LIMITS)
        self.previous = [0, 0, 1.3, .15, 0, -.5, 0, 1.3, -.15, 0, .5, 0, 1, 1]
        self.original_life = None
        self.velocity = [0.] * 14
        self.base = [0.] * 3
        self.armed_at = 0.
        self.origin = list(self.previous)
        self.last_target = list(self.previous)
        self.hardware_speed = [float('inf')]*14
        self.hardware_limits=list(LIMITS)
        self.hardware_max_speed=[float('inf')]*14
        self.range_limited=True;self.speed_limited=True
        self.base_test_cap=None if BASE_TEST_CAP is None else float(BASE_TEST_CAP)
        if self.base_test_cap is not None and not (0.<self.base_test_cap<=.1):
            raise ValueError('Base test cap must be finite and between 0 and 0.1')
        self.base_sent = None
        self.base_sent_at = 0.
        self.base_requests = 0
        self.motion_pending = {}
        self.motion_targets = {}
        self.torso = TorsoAssist()
        self.torso_enabled=False
        self.stop_pending={}
        self.stop_error=""
        self.stop_error_reported=False
        self.base_collision_enabled = True
        self.speech_arms=SpeechArms(self,clock)
        self.animation_player=None
        self.animation_future=None
        self.animation_resume_future=None
        self.animation_resume_at=0.
        self.blend_seconds=3.
        self.available_animations=set(OFFICIAL_ANIMATIONS.values()) if simulate else set()
        if simulate:
            return
        import sys
        sys.path.insert(0, '/opt/aldebaran/lib/python2.7/site-packages')
        import qi
        self.session = qi.Session()
        self.session.connect('tcp://127.0.0.1:9559')
        self.motion = self.session.service('ALMotion')
        configure_base_security(self.motion)
        try:
            package=os.path.join(os.path.dirname(os.path.abspath(__file__)),'telepepper-anims.pkg')
            if os.path.isfile(package):
                manager=self.session.service('PackageManager')
                if not manager.hasPackage('telepepper-anims') or manager.package2('telepepper-anims')['version']!=PACKAGE_VERSION:
                    if not manager.install(package,_async=True).value(10000):
                        raise RuntimeError('Official clip package installation failed')
            speaking_package=os.path.join(os.path.dirname(os.path.abspath(__file__)),'telepepper-speaking.pkg')
            install_package(self.session,speaking_package)
            player=self.session.service('ALAnimationPlayer')
            self.animation_player=ResourceAnimationPlayer(player,os.path.dirname(os.path.abspath(__file__)))
            installed=set(player._getAnimations(_async=True).value(2000))
            self.available_animations=set(p for p in installed if not p.startswith(('telepepper-anims/','telepepper-speaking/')))
            self.available_animations.update(self.animation_player.resources)
        except Exception as exc:print('Official animation library unavailable: '+str(exc))
        self.base_collision_enabled = bool(self.motion.getExternalCollisionProtectionEnabled('Move'))
        self.life = self.session.service('ALAutonomousLife')
        self.video = self.session.service('ALVideoDevice')
        cleanup_video_subscriptions(self.video)
        self.torso.limits=[tuple(self.motion.getLimits(n)[0][:2]) for n in ["HipRoll","HipPitch"]]
        for i, name in enumerate(NAMES):
            limits = self.motion.getLimits(name)[0]
            self.hardware_limits[i]=tuple(limits[:2]);self.hardware_max_speed[i]=float(limits[2])
            self.limits[i] = (max(LIMITS[i][0], limits[0]), min(LIMITS[i][1], limits[1]))
            self.hardware_speed[i] = float(limits[2])*(.25 if i < 2 else .35 if i >= 12 else .5)

    def arm(self, resume=False):
        if not self.simulate:
            self.poll_stop()
            if self.stop_pending:raise RuntimeError("Stop still completing; wait before Start")
            if self.stop_error:raise RuntimeError(self.stop_error)
            for channel,(future,began) in list(self.motion_pending.items()):
                if not future.isFinished():
                    raise RuntimeError("Motion command still completing: "+channel)
                future.value(0)
                del self.motion_pending[channel]
            # wakeUp() can itself run a posture transition. Require an awake robot
            # so arming never requests a pre-defined pose.
            if not self.motion.robotIsWakeUp():
                raise RuntimeError('Pepper must be awake before arming')
            # Changing ALAutonomousLife to disabled can call rest() and turn off
            # stiffness. Arming must never change life state or body posture.
            if self.life.getState() != 'disabled':
                raise RuntimeError('Prepare Pepper awake with Autonomous Life disabled before arming')
            self.motion.setExternalCollisionProtectionEnabled('Arms', True)
            self.motion.setCollisionProtectionEnabled('Arms', True)
            # Keep the explicitly selected base policy; never disable body protection.
            self.motion.setExternalCollisionProtectionEnabled('Move', self.base_collision_enabled)
            self.motion.setStiffnesses(NAMES, 1.0)
            self.motion.setStiffnesses(["HipRoll","HipPitch"],1.0)
            self.previous = list(self.motion.getAngles(NAMES, True))
            self.torso.engage(list(self.motion.getAngles(["HipRoll","HipPitch"],True))[:2])
        self.origin = list(self.previous)
        self.last_target = list(self.previous)
        self.velocity = [0.] * 14
        self.base = [0.] * 3
        self.blend_seconds=3.
        self.armed_at = clock() - (3. if resume else 0.)
        self.base_sent = None
        self.motion_targets.clear()

    def set_base_collision(self, enabled):
        if type(enabled) is not bool: raise ValueError('Boolean enabled required')
        if not self.simulate:
            self.motion.setExternalCollisionProtectionEnabled('Arms', True)
            self.motion.setCollisionProtectionEnabled('Arms', True)
            # Firmware enforces owner consent. Propagate refusal, never bypass it.
            try:
                self.motion.setExternalCollisionProtectionEnabled('Move', enabled)
            finally:
                self.base_collision_enabled = bool(self.motion.getExternalCollisionProtectionEnabled('Move'))
            if self.base_collision_enabled != enabled:
                raise RuntimeError('Pepper did not accept the base collision setting')
        else: self.base_collision_enabled = enabled

    def set_motion_limit(self,key,enabled):
        if type(enabled) is not bool:raise ValueError('Boolean enabled required')
        if key=='base':self.set_base_collision(enabled)
        elif key=='range':
            self.range_limited=enabled
            self.limits=[(max(LIMITS[i][0],p[0]),min(LIMITS[i][1],p[1])) if enabled else tuple(p) for i,p in enumerate(self.hardware_limits)]
        elif key=='speed':
            self.speed_limited=enabled
            self.hardware_speed=[v*(.25 if i<2 else .35 if i>=12 else .5) if enabled else v for i,v in enumerate(self.hardware_max_speed)]
        else:raise ValueError('Unknown motion limit')

    def speed_fraction(self,channel,following):
        if following and not self.speed_limited:return 1.
        return (.25 if following else .12) if channel=='head' else .35

    def stop(self):
        self.base = [0.] * 3
        self.base_sent = None
        self.velocity = [0.] * 14
        errors=[]
        if self.animation_resume_future is not None:
            try:self.animation_resume_future.cancel()
            except Exception as exc:errors.append('Stop pose read cancel failed: '+str(exc))
            self.animation_resume_future=None
        # Never let an animation cancellation exception skip base braking.
        speech=self.speech_arms.future
        try:speech=self.speech_arms.stop()
        except Exception as exc:errors.append('Stop speech gesture cancel failed: '+str(exc))
        if speech is not None and not self.simulate:
            self.stop_pending['speech_cancel']=(speech,clock())
        animation=self.animation_future
        self.animation_future=None
        if animation is not None:
            try:animation.cancel()
            except Exception as exc:errors.append('Stop animation cancel failed: '+str(exc))
            if not self.simulate:self.stop_pending['animation_cancel']=(animation,clock())
        if not self.simulate:
            if self.stop_pending and animation is None and speech is None and not errors:return
            self.stop_error='; '.join(errors)
            self.stop_error_reported=False
            operations=[('base_zero',lambda:self.motion.moveToward(0.,0.,0.,_async=True)),
                        ('base_stop',lambda:self.motion.stopMove(_async=True))]
            if animation is None and speech is None and not errors:
                operations.extend([('hold_read',lambda:self.motion.getAngles(NAMES,True,_async=True)),
                                   ('torso_read',lambda:self.motion.getAngles(['HipRoll','HipPitch'],True,_async=True))])
            for channel,operation in operations:
                try:self.stop_pending[channel]=(operation(),clock())
                except Exception as exc:self.stop_error+='; Stop '+channel+' request failed: '+str(exc)

    def poll_stop(self):
        if self.simulate:return
        # All waits are completion checks. Logical STOP and pose receipt never
        # wait on a physical braking/hold RPC; arming waits for verified completion.
        for channel,(future,began) in list(self.stop_pending.items()):
            try:finished=future.isFinished()
            except Exception as exc:
                if 'Stop '+channel+' completion check failed' not in self.stop_error:
                    self.stop_error+='; Stop '+channel+' completion check failed: '+str(exc)
                continue
            if not finished:
                if clock()-began>2. and not self.stop_error:
                    self.stop_error='Stop '+channel+' acknowledgement stalled (>2 s)'
                continue
            del self.stop_pending[channel]
            try:
                if channel in ('animation_cancel','speech_cancel'):
                    # Cancelled futures normally raise on value(); await completion
                    # then capture a fresh hold pose, never a pre-cancel snapshot.
                    self.stop_pending['hold_read']=(self.motion.getAngles(NAMES,True,_async=True),clock())
                    self.stop_pending['torso_read']=(self.motion.getAngles(['HipRoll','HipPitch'],True,_async=True),clock())
                    continue
                result=future.value(0)
                if channel=='hold_read':
                    current=finite_vector(list(result),14)
                    self.stop_pending['hold_send']=(self.motion.setAngles(NAMES,current,.1,_async=True),clock())
                    self.previous=current
                elif channel=='torso_read':
                    hips=finite_vector(list(result)[:2],2)
                    self.stop_pending['torso_send']=(self.motion.setAngles(['HipRoll','HipPitch'],hips,.1,_async=True),clock())
                    self.torso.reset(hips,preserve_origin=True)
            except Exception as exc:self.stop_error+='Stop '+channel+' failed: '+str(exc)+'; '
        if self.stop_error and not self.stop_error_reported:
            self.stop_error_reported=True
            raise RuntimeError(self.stop_error)

    def begin_animation(self,name):
        path=OFFICIAL_ANIMATIONS[name]
        if path not in self.available_animations:raise RuntimeError('Official animation is not installed on this Pepper')
        if self.animation_player is None:raise RuntimeError('Official animation player unavailable')
        for channel,(future,began) in list(self.motion_pending.items()):
            if not future.isFinished():return False
            future.value(0);del self.motion_pending[channel]
        if not self.simulate and self.base_sent!=[0.,0.,0.]:
            self.submit_motion('base',lambda:self.motion.moveToward(0.,0.,0.,_async=True))
            self.base=self.base_sent=[0.,0.,0.]
            return False
        self.animation_future=self.animation_player.run(path,_async=True)
        return True

    def finish_animation(self):
        if not self.simulate:
            if self.animation_resume_future is None:
                self.animation_resume_future=self.motion.getAngles(NAMES+['HipRoll','HipPitch'],True,_async=True)
                self.animation_resume_at=clock()
            future=self.animation_resume_future
            if not future.isFinished():
                if clock()-self.animation_resume_at>.5:
                    raise RuntimeError('Animation return pose read timed out')
                return False
            measured=list(future.value(0))
            if len(measured)!=16 or any(math.isnan(v) or math.isinf(v) for v in measured):
                raise RuntimeError('Invalid measured pose after animation')
            self.previous=measured[:14]
            self.torso.reset(measured[14:],preserve_origin=True)
        self.animation_resume_future=None
        self.animation_future=None
        self.origin=list(self.previous);self.last_target=list(self.previous)
        self.velocity=[0.]*14;self.base=[0.]*3
        self.blend_seconds=.75;self.armed_at=clock()
        self.motion_targets.clear()
        return True

    def apply(self, angles, base, dt, torso=None):
        # Blend from measured pose over three seconds. Limit both target velocity
        # and acceleration; NAOqi's speed fraction is an additional independent cap.
        speech_owns_arms=self.speech_arms.poll(NAMES)
        dt = clamp(dt, .001, .05 if dt <= .075 else .03)
        t = clamp((clock()-self.armed_at)/self.blend_seconds, 0., 1.)
        blend = t*t*(3.-2.*t)
        if torso is not None:
            self.torso_enabled=True
            torso=[clamp(torso[0],-.15,.15),clamp(torso[1],-.2,.2)]
            self.torso.step(torso,dt,blend)
            angles=list(angles)
            angles[1]+=torso[1]-(self.torso.origin[1]-self.torso.current[1])
        out = []
        for i, angle in enumerate(angles):
            if speech_owns_arms and i>=2:
                out.append(self.previous[i]);continue
            angle=self.speech_arms.target(i,angle)
            target = clamp(angle, *self.limits[i])
            if t < 1.:target = self.origin[i] + blend*(target-self.origin[i])
            # Arms need faster following than the head; keep the initial blend
            # and the independent NAOqi speed cap for controlled engagement.
            following = t >= 1.
            vmax, accel = (((1.2, 4.) if following else (.45, .9)) if i < 2 else
                           ((2.4, 16.) if following else (1.2, 2.4)) if i < 12 else
                           ((3., 12.) if following else (1.5, 3.)))
            vmax = min(vmax, self.hardware_speed[i])
            if following and i < 12:
                # Streaming setAngles already interpolates on the physical robot.
                # Send the current bounded reference rather than a second software
                # trajectory that ALMotion would have to chase again. Engagement
                # keeps the three-second blend; firmware speed/collision limits
                # remain active for continuous following and timeout recovery.
                out.append(target)
                self.velocity[i]=0.
                self.last_target[i]=target
                continue
            error = target-self.previous[i]
            if following and i >= 12:
                # Trigger aperture is already a direct reference. Avoid the extra
                # half-second acceleration ramp used for large arm segments.
                # Keep the same aperture speed and the firmware speed limit.
                step=clamp(error,-vmax*dt,vmax*dt)
                self.velocity[i]=step/dt
                self.last_target[i]=target
                out.append(self.previous[i]+step)
                continue
            # Braking-distance velocity prevents overshoot with a moving target.
            desired = math.copysign(min(vmax, math.sqrt(2*accel*abs(error))), error)
            if following:
                # Track the moving reference, rather than repeatedly planning a
                # stop at a target that has already moved. No future prediction.
                feed = clamp((target-self.last_target[i])/dt, -vmax, vmax)
                desired = clamp(feed + 12.*error, -vmax, vmax)
            self.last_target[i] = target
            self.velocity[i] += clamp(desired-self.velocity[i], -accel*dt, accel*dt)
            step = self.velocity[i]*dt
            if (not following or abs(feed) < 1e-6) and abs(step) > abs(error) and step*error >= 0:
                step = error
                self.velocity[i] = 0.
            # Never cross a hardware bound. If the measured initial pose is outside
            # our smaller envelope, allow only the path from that pose into it.
            low = min(self.origin[i], self.limits[i][0])
            high = max(self.origin[i], self.limits[i][1])
            out.append(clamp(self.previous[i]+step, low, high))
        base=cap_test_base(base,self.base_test_cap)
        for i, target in enumerate(base):
            # Deadman release must stop promptly; acceleration applies when driving.
            self.base[i] = 0. if target == 0. else self.base[i]+clamp(target-self.base[i], -(3.6 if i==2 else .70)*dt, (3.6 if i==2 else .70)*dt)
        self.base=cap_test_base(self.base,self.base_test_cap)
        if not self.simulate:
            # Never wait for motor RPCs while holding the pose/UDP state lock.
            # One outstanding call per channel: intermediate poses are replaced
            # by the newest target rather than queued behind a slow motor call.
            for channel,(future,began) in list(self.motion_pending.items()):
                if future.isFinished():
                    try: future.value(0)
                    except Exception as exc: raise RuntimeError(channel+' command failed: '+str(exc))
                    del self.motion_pending[channel]
                elif clock()-began>.5:
                    raise RuntimeError(channel+' command acknowledgement stalled (>500 ms)')
            self.submit_motion('head',lambda:self.motion.setAngles(NAMES[:2],out[:2],self.speed_fraction('head',t>=1),_async=True),target=tuple(out[:2]))
            if not speech_owns_arms:self.submit_motion('arms',lambda:self.motion.setAngles(NAMES[2:12],out[2:12],self.speed_fraction('arms',t>=1),_async=True),target=tuple(out[2:12]))
            if torso is not None:self.submit_motion('torso',lambda:self.motion.setAngles(['HipRoll','HipPitch'],self.torso.current,.12,_async=True),target=tuple(self.torso.current))
            if not speech_owns_arms:self.submit_motion('hands',lambda:self.motion.setAngles(NAMES[12:],out[12:],self.speed_fraction('hands',t>=1),_async=True),target=tuple(out[12:]))
            changed=self.base_sent != self.base
            release=changed and not any(self.base)
            # moveToward retains its velocity until changed/stopped. Do not create
            # new motion tasks for a held stick. Coalesce ramp/steering updates to
            # 25 Hz; first drive and zero velocity bypass this interval.
            start_drive=changed and self.base_sent is not None and not any(self.base_sent)
            if changed and (release or start_drive or self.base_sent is None or clock()-self.base_sent_at>=.04):
                if self.submit_motion('base',lambda:self.motion.moveToward(self.base[0],self.base[1],self.base[2],_async=True),force=release):
                    self.base_sent=list(self.base);self.base_sent_at=clock();self.base_requests+=1
        self.previous = out

    def submit_motion(self,channel,operation,force=False,target=None):
        if channel in self.motion_pending and not force:return False
        if target is not None and self.motion_targets.get(channel)==target:return False
        self.motion_pending[channel]=(operation(),clock())
        if target is not None:self.motion_targets[channel]=target
        return True

    def restore(self):
        # Do not resume autonomous life on disconnect: that could introduce motion
        # immediately after a tracking failure. Explicit normal exit has its own
        # confirmed-STOP path; ordinary disconnect never uses it.
        self.stop()

class State(object):
    def __init__(self, robot):
        self.robot = robot
        self.lock = threading.RLock()
        self.session = None
        self.peer = None
        self.armed = False
        self.seq = -1
        self.latest = None
        self.received = 0.
        self.fault = ''
        self.min_offset = None
        self.armed_stop_epoch = None
        self.robot_names = list(NAMES)
        self.gesture = None
        self.gesture_diagnostics = {}
        self.tracking_valid = False
        self.preparing = False
        self.prepare_cancelled = False
        self.prepare_future = None
        self.prepare_message = ''
        self.last_stop = None
        self.stop_history = []
        self.stop_generation = 0
        self.preparation_revision = 0
        self.input_delay_ms = 0.
        self.stale_drops = 0.
        self.apply_ms = 0.
        self.arm_ms = 0.
        self.coalesced_packets = 0
        self.udp_packets = 0
        self.udp_last_seen = None
        self.udp_rejected = 0
        self.udp_last_reject = ''
        self.last_apply_at = None
        self.apply_interval_ms = 0.
        self.requested_angles = None
        self.motion_ready = threading.Event()
        self.queue_ms = 0.
        self.heading = BaseHeading()
        self.auto_turn = False
        self.base_feedback = None
        self.recovery_until = None
        self.recovery_since = None
        self.recovery_count = 0
        self.relax_at = None
        self.relax_operator = False
        self.relax_started = None
        self.relax_velocity = [0.]*14
        self.relax_last = None

    def cancel_recovery(self):
        self.recovery_until = None
        self.recovery_since = None

    def pause_for_timeout(self, now, reason='Command timeout'):
        with self.lock:
            # Recovery only belongs to an already armed pilot session. Gestures,
            # explicit STOP, disconnect and tracking loss never get auto-armed.
            if not self.armed: return
            eligible = bool(self.session) and not self.gesture and not self.preparing
            self.disarm(reason)
            if eligible:
                self.recovery_until = now+5.
                self.fault = reason+'; waiting for stable tracking and centred sticks'

    def maybe_recover(self, now):
        with self.lock:
            if self.recovery_until is None:return
            if now>self.recovery_until:
                self.cancel_recovery()
                self.fault='Recovery expired; hold A + X to re-arm'
                return
            if (self.recovery_since is None or now-self.recovery_since<.5
                    or now-self.received>.1 or not self.tracking_valid or self.preparing):return
            if self.robot.stop_pending:return
            if self.robot.stop_error:
                self.cancel_recovery()
                self.fault='Recovery refused: '+self.robot.stop_error
                return
            try:
                # Re-read measured pose; retain acceleration limits without repeating
                # the three-second initial engagement after every brief timeout.
                # Robot.arm checks awake/life state; never calls wakeUp/rest.
                self.robot.arm(resume=True)
                self.last_apply_at=None;self.heading.reset()
                self.armed=True;self.fault='';self.recovery_count+=1
                self.cancel_recovery()
            except Exception as exc:
                self.cancel_recovery()
                self.fault='Recovery refused: '+str(exc)

    def accept(self, packet, ip, now):
        angles = finite_vector(packet['angles'], 14)
        base = finite_vector(packet['base'], 3)
        stamp = float(packet['sent'])
        if math.isnan(stamp) or math.isinf(stamp):
            raise ValueError('timestamp')
        seq = packet['seq']
        sample_age = float(packet.get('sample_age_ms',0.))
        torso=finite_vector(packet.get('torso',[0.,0.]),2) if packet.get('torso_assist') is True else None
        body_yaw = float(packet.get('body_yaw',0.))
        if math.isnan(body_yaw) or math.isinf(body_yaw) or abs(body_yaw) > math.pi+.01:
            raise ValueError('body yaw')
        if math.isnan(sample_age) or math.isinf(sample_age) or sample_age < 0:
            raise ValueError('sample age')
        if not isinstance(seq, int) or seq < 0:
            raise ValueError('sequence')
        with self.lock:
            if not self.session or packet.get('session') != self.session or ip != self.peer or seq <= self.seq:
                return False
            stop_epoch=packet.get('stop_epoch')
            if (self.armed and packet.get('stop') is True
                    and type(stop_epoch) is int and self.armed_stop_epoch is not None
                    and stop_epoch <= self.armed_stop_epoch):
                # The TCP ARM acknowledged this STOP already. A queued UDP copy
                # must not cancel it or refresh tracking/watchdog timestamps.
                return False
            offset = now-stamp/1000.
            self.min_offset = offset if self.min_offset is None else min(self.min_offset, offset)
            self.input_delay_ms = max(0., (offset-self.min_offset)*1000.)
            # Relative delay increase, not a claimed absolute one-way measurement.
            if offset-self.min_offset > .075 and packet.get('stop') is not True and packet.get('active') is True:
                # An isolated late packet is not a lost tracking edge. Never apply
                # it or refresh the valid-command watchdog; a continuing outage
                # still stops motion after 180 ms. Avoid stop/re-arm cascades from
                # one Wi-Fi/scheduling spike while fresh poses follow immediately.
                self.stale_drops += 1
                self.seq = seq
                return False
            self.seq = seq
            previous_received = self.received
            self.received = now
            self.requested_angles = list(angles)
            self.auto_turn = packet.get('auto_turn') is True
            self.tracking_valid = packet.get('active') is True and sample_age < 100.
            if packet.get('stop') is True:
                self.cancel_recovery()
                if self.armed:self.disarm('Quest STOP')
                return True
            if not self.tracking_valid and packet.get('pause_reason')=='video':
                if self.armed:self.pause_for_timeout(now,'Camera feed interrupted')
                self.recovery_since=None
                return True
            if not self.tracking_valid:
                if self.recovery_until is not None:self.fault='Tracking/video lost; press Start when ready'
                self.cancel_recovery()
                if self.armed:
                    reason=packet.get('pause_reason','tracking/video')
                    self.disarm('Quest %s inactive; press Start when ready'%reason)
                    if reason in ('tracking','tracking/video'):
                        self.relax_at=now+.5

                return True
            if self.relax_at is not None and not self.relax_operator:self.cancel_relax()
            if not self.armed:
                if self.recovery_until is not None:
                    if packet.get('sticks_neutral') is not True:
                        self.recovery_since=None
                    elif self.recovery_since is None or now-previous_received>.1:
                        self.recovery_since=now
                return True
            # Grip deadman gates the mobile base, tracking gates all motion.
            if packet.get('drive') is not True:
                base = [0., 0., 0.]
            caps=(.50,.4,.90) if self.robot.speed_limited else (1.,1.,1.)
            base=[clamp(v,-cap,cap) for v,cap in zip(base,caps)]
            self.latest = (angles, base, seq, stamp, now, sample_age, body_yaw, packet.get('drive') is True,torso)
            self.motion_ready.set()
            return True

    def disarm(self, reason=None, return_to_neutral=False):
        # SDK errors may include entire XML clips; bound public status payloads.
        if reason is not None:reason=str(reason)[:512]
        with self.lock:
            self.cancel_relax()
            self.stop_generation += 1
            self.cancel_recovery()
            if self.armed:
                self.last_stop = dict(reason=reason or self.fault or 'STOP requested',
                                      robot_mono=clock(), seq=self.seq,
                                      command_age_ms=max(0.,(clock()-self.received)*1000.) if self.received else None,
                                      input_delay_ms=self.input_delay_ms,
                                      apply_ms=self.apply_ms, arm_ms=self.arm_ms)
                self.stop_history=(self.stop_history+[dict(self.last_stop)])[-32:]
                print('MOTION PAUSE: '+self.last_stop['reason'])
                self.fault = self.last_stop['reason']
            if self.preparing:
                self.prepare_cancelled = True
                if self.prepare_future is not None:
                    try: self.prepare_future.cancel()
                    except Exception: pass
            self.armed = False
            self.heading.reset()
            self.last_apply_at = None
            self.latest = None
            if self.gesture is not None and self.gesture_diagnostics.get('stage')!='complete':
                self.gesture_diagnostics.update(stage='cancelled',reason=reason or 'STOP')
            self.gesture = None
            self.robot.stop()
            if return_to_neutral:
                self.relax_operator=True
                self.relax_at=clock()+.5

    def cancel_relax(self):
        self.relax_at=None;self.relax_started=None;self.relax_last=None
        self.relax_operator=False
        self.relax_velocity=[0.]*14

    def relax_tick(self,now,dt):
        if self.relax_at is None or now<self.relax_at or self.robot.stop_pending or self.robot.stop_error:return
        if (self.tracking_valid and not self.relax_operator) or self.armed:
            self.cancel_relax();return
        if self.relax_last is not None and now-self.relax_last<.049:return
        if self.relax_started is None:
            # stop() has already held measured joints and stopped the wheels.
            # Build a new bounded trajectory from that measured held pose.
            self.relax_started=now;self.robot.armed_at=now-3.
            self.fault="Returning gently to neutral pose"
        dt=max(.001,min(.05,dt if self.relax_last is None else now-self.relax_last))
        target=[0.,0.,1.65,.12,0.,-.15,0.,1.65,-.12,0.,.15,0.,1.,1.]
        target=[clamp(value,*self.robot.limits[i]) for i,value in enumerate(target)]
        out=[]
        for i,value in enumerate(target):
            error=value-self.robot.previous[i]
            desired=math.copysign(min(.4,math.sqrt(1.6*abs(error))),error)
            self.relax_velocity[i]+=clamp(desired-self.relax_velocity[i],-.8*dt,.8*dt)
            step=self.relax_velocity[i]*dt
            if step*error>=0 and abs(step)>abs(error):step=error;self.relax_velocity[i]=0.
            out.append(self.robot.previous[i]+step)
        self.robot.apply(out,[0.,0.,0.],dt,torso=[0.,0.])
        self.relax_last=now
        if max(abs(out[i]-target[i]) for i in range(14))<.01:
            self.cancel_relax();self.fault='Neutral pose / press Start when ready'
        elif now-self.relax_started>20:
            self.cancel_relax();self.robot.stop();self.fault='Neutral pose transition timed out; press Start when ready'

    def tick(self, now, dt):
        with self.lock:
            if not self.armed:
                self.relax_tick(now,dt)
                return None
            self.cancel_relax()
            if now - self.received > .18:
                self.pause_for_timeout(now)
                return None
            if self.last_apply_at is not None and now-self.last_apply_at<.019:
                return None
            packet = self.latest
            self.latest = None
            if packet is None:
                return None
            start = clock()
            angles, base, torso = packet[0], packet[1], packet[8]
            base = [0.,0.,0.] if self.gesture else self.heading.command(self.auto_turn, packet[7],packet[6],base,self.base_feedback,now)
            if self.gesture:
                name, began, origin = self.gesture
                elapsed = now-began
                base=[0.,0.,0.];torso=None
                if elapsed<0:
                    angles=list(origin)
                else:
                    if elapsed>30:
                        self.disarm('Official animation timed out');return None
                    if self.robot.animation_future is None:
                        if not self.robot.begin_animation(name):
                            self.gesture_diagnostics['stage']='waiting for joint commands'
                        else:self.gesture_diagnostics['stage']='playing'
                    future=self.robot.animation_future
                    if future is not None and future.isFinished():
                        try:future.value(0)
                        except Exception as exc:
                            self.disarm('Official animation failed: '+str(exc));return None
                        self.gesture_diagnostics['stage']='returning to live'
                        try:ready=self.robot.finish_animation()
                        except Exception as exc:
                            self.disarm('Animation return failed: '+str(exc));return None
                        if ready:
                            self.gesture_diagnostics['stage']='complete'
                            self.gesture=None;self.heading.reset();self.fault=''
                        self.last_apply_at=now
                        return {'seq':packet[2],'sent':packet[3],'apply_ms':0.,'queue_ms':0.,'sample_age_ms':packet[5],'armed':self.armed}
                    # ALAnimationPlayer exclusively owns the joints now. Never
                    # overwrite its animation with incoming setAngles commands.
                    self.gesture_diagnostics.update(elapsed=elapsed,samples=self.gesture_diagnostics.get('samples',0)+1)
                    self.last_apply_at=now;self.apply_ms=(clock()-start)*1000.;self.queue_ms=(start-packet[4])*1000.
                    return {'seq':packet[2],'sent':packet[3],'apply_ms':self.apply_ms,'queue_ms':self.queue_ms,'sample_age_ms':packet[5],'armed':self.armed}
            # Empty polling ticks must not erase elapsed trajectory time.
            # Robot.apply still caps the step to 30 ms: never catch up a long gap.
            elapsed = dt if self.last_apply_at is None else max(0., now-self.last_apply_at)
            self.apply_interval_ms = elapsed*1000.
            if torso is None:self.robot.apply(angles,base,elapsed)
            else:self.robot.apply(angles,base,elapsed,torso=torso)
            self.last_apply_at = now
            self.apply_ms = (clock()-start)*1000
            self.queue_ms = (start-packet[4])*1000
            return {'seq': packet[2], 'sent': packet[3], 'apply_ms': self.apply_ms,
                    'queue_ms': self.queue_ms, 'sample_age_ms':packet[5], 'armed': self.armed}

def send_json(conn, obj):
    conn.sendall((json.dumps(obj, separators=(',', ':')) + '\n').encode('utf-8'))

def listen(port):
    sock = socket.socket()
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('0.0.0.0', port))
    sock.listen(4)
    sock.settimeout(1)
    return sock

class Service(object):
    def __init__(self, token, simulate=False, port=9570):
        self.token = token
        self.state = State(Robot(simulate))
        self.port = port
        self.running = True
        self.closing = False
        self.normal_exit = False
        self.restart_required = False
        self.exit_lock = threading.Lock()
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp.bind(('0.0.0.0', port+1))
        self.udp.settimeout(.1)
        self.destination = None
        self.woz = WoZ(self, clock)
        self.video_diagnostics = [None,None,None]

    def return_to_normal(self, request):
        if request.get('confirmed') is not True:
            raise ValueError('Explicit Exit confirmation required')
        with self.exit_lock:
            if self.closing: raise ValueError('Exit already in progress')
            self.closing = True
            normal_future=None
            try:
                with self.state.lock:
                    self.state.disarm('Explicit TelePepper exit')
                    stopped_generation = self.state.stop_generation
                self.woz.action({'cmd':'speech_stop'})
                deadline = clock() + 5.
                while True:
                    with self.state.lock:
                        robot = self.state.robot
                        robot.poll_stop()
                        if robot.stop_error: raise RuntimeError(robot.stop_error)
                        if self.state.stop_generation != stopped_generation:
                            raise RuntimeError('Exit cancelled by a newer STOP')
                        pending = self.state.preparing or bool(robot.stop_pending)
                        for future,began in list(robot.motion_pending.values()):
                            if not future.isFinished(): pending = True
                            else: future.value(0)
                        if not pending: break
                    if clock() >= deadline:
                        raise RuntimeError('Robot STOP not confirmed; autonomy remains disabled')
                    time.sleep(.02)
                if not robot.simulate:
                    with self.state.lock:
                        if self.state.stop_generation != stopped_generation:
                            raise RuntimeError('Exit cancelled by a newer STOP')
                    normal_future=robot.life.setState('solitary', _async=True)
                    mode_deadline=clock()+8.
                    while not normal_future.isFinished():
                        with self.state.lock:
                            if self.state.stop_generation != stopped_generation:
                                raise RuntimeError('Exit cancelled during normal-mode handoff')
                        if clock()>=mode_deadline:raise RuntimeError('Normal-mode handoff timed out')
                        time.sleep(.02)
                    normal_future.value(0)
                    mode = robot.life.getState(_async=True).value(2000)
                    if mode not in ('solitary','interactive'):
                        raise RuntimeError('Normal robot mode not confirmed')
                with self.state.lock:
                    if self.state.stop_generation != stopped_generation:
                        raise RuntimeError('Exit cancelled during normal-mode handoff')
                    self.state.session = None
                    self.state.peer = None
                    self.normal_exit = True
                return {'ok':True,'normal_mode':True,'service_stopping':True,'armed':False}
            except Exception:
                try:
                    if normal_future is not None and not robot.simulate:
                        try:
                            try:normal_future.cancel()
                            except Exception:pass
                            robot.life.setState('disabled',_async=True).value(8000)
                        finally:
                            with self.state.lock:robot.stop()
                finally:self.closing = False
                raise

    def spawn(self, fn, *args):
        t = threading.Thread(target=fn, args=args)
        t.daemon = True
        t.start()

    def prepare_motion(self, confirmed=False):
        if confirmed is not True:
            raise ValueError('Explicit confirmation of posture preparation is required')
        state=self.state
        with state.lock:
            if state.armed or state.preparing:
                raise ValueError('Stop teleoperation before preparing Pepper')
            state.preparing=True;state.prepare_cancelled=False
            state.preparation_revision += 1
            state.prepare_message='Preparing posture; keep clear of arms'
        self.spawn(self._prepare_motion)

    def _prepare_motion(self):
        state=self.state;robot=state.robot
        try:
            if not robot.simulate:
                # Explicit Start preparation; ordinary arm/recovery never changes Life.
                # Both NAOqi operations run asynchronously so STOP can cancel.
                operations=[]
                if robot.life.getState(_async=True).value(2000)!='disabled':
                    operations.append(('Disabling autonomy',lambda: robot.life.setState('disabled',_async=True)))
                operations.append(('Waking motors',lambda: robot.motion.wakeUp(_async=True)))
                for label,operation in operations:
                    with state.lock:
                        if state.prepare_cancelled: raise RuntimeError('Preparation cancelled')
                        state.prepare_message=label+' / motion remains paused'
                        state.prepare_future=operation()
                        future=state.prepare_future
                    future.value(20000)
                if not robot.motion.robotIsWakeUp(): raise RuntimeError('Pepper did not wake up')
            with state.lock:
                if state.prepare_cancelled: raise RuntimeError('Preparation cancelled')
                state.prepare_message='Posture ready.'
                state.fault=''
        except Exception as exc:
            state.prepare_message='Preparation failed: '+str(exc)
            try: robot.stop()
            except Exception: pass
        finally:
            with state.lock:
                state.prepare_future=None;state.preparing=False;state.armed=False;state.latest=None

    def udp_loop(self):
        while self.running:
            try:
                data, addr = self.udp.recvfrom(2048)
                batch = [(data, addr)]
                # Bound the work per iteration. Network bursts must not replay old
                # poses or trip stale detection before inspecting the newest pose.
                for unused in range(63):
                    if not select.select([self.udp], [], [], 0)[0]: break
                    batch.append(self.udp.recvfrom(2048))
                self.motion_batch(batch)
            except socket.timeout:
                pass
            except (ValueError, KeyError, TypeError, UnicodeError):
                pass
            except Exception as e:
                self.fail(e)

    def motion_batch(self, batch):
        newest = None
        for data, addr in batch:
            with self.state.lock:
                self.state.udp_packets+=1;self.state.udp_last_seen=clock()
            try:
                if len(data) > 1800: continue
                packet = json.loads(data.decode('utf-8'))
                if not isinstance(packet, dict): continue
                finite_vector(packet['angles'], 14)
                finite_vector(packet['base'], 3)
                stamp = float(packet['sent'])
                seq = packet['seq']
                if math.isnan(stamp) or math.isinf(stamp) or not isinstance(seq, int) or seq < 0: continue
                with self.state.lock:
                    rejection=''
                    if not self.state.session or packet.get('session') != self.state.session:rejection='pilot session mismatch'
                    elif addr[0] != self.state.peer:rejection='pilot address mismatch'
                    elif seq <= self.state.seq:rejection='old sequence'
                    if rejection:
                        self.state.udp_rejected+=1;self.state.udp_last_reject=rejection
                        continue
                if packet.get('active') is not True or packet.get('stop') is True:
                    # Never coalesce away a tracking-loss/stop edge, even if
                    # tracking recovered later in the same network burst.
                    self.state.accept(packet, addr[0], clock())
                elif newest is None or seq > newest[0]['seq']:
                    if newest is not None: self.state.coalesced_packets += 1
                    newest = (packet, addr)
            except (ValueError, KeyError, TypeError, UnicodeError, OverflowError):
                with self.state.lock:
                    self.state.udp_rejected+=1;self.state.udp_last_reject='invalid pose or timestamp'
                continue
        if newest is not None:
            packet, addr = newest
            if self.state.accept(packet, addr[0], clock()):
                self.destination = addr
                # Receipt heartbeats remain available while paused, independently
                # of motor acknowledgements, so recovery can verify the link.
                if hasattr(self,'udp'):
                    self.udp.sendto(json.dumps({'kind':'received','sent':packet['sent'],
                        'seq':packet['seq']}).encode('utf-8'),addr)

    def fail(self, error):
        self.state.fault = str(error)
        try:
            self.state.disarm()
        except Exception:
            pass
        print('FAULT:', error)

    def motion_loop(self):
        previous = clock()
        while self.running:
            self.state.motion_ready.clear()
            now = clock()
            try:
                with self.state.lock:self.state.robot.poll_stop()
                self.state.maybe_recover(now)
                ack = self.state.tick(now, now-previous)
                if ack and self.destination:
                    self.udp.sendto(json.dumps(ack).encode('utf-8'), self.destination)
            except Exception as e:
                self.fail(e)
            previous = now
            # New poses wake this loop immediately; timeout keeps the watchdog
            # running when tracking/network traffic disappears.
            self.state.motion_ready.wait(.01)

    def heading_feedback_loop(self):
        # Separate from motion: odometry cannot delay an arm command or STOP.
        while self.running:
            state=self.state
            if state.armed and state.auto_turn:
                try:
                    position=[0.,0.,0.] if state.robot.simulate else state.robot.motion.getRobotPosition(True,_async=True).value(40)
                    yaw=float(position[2])
                    if not math.isnan(yaw) and not math.isinf(yaw):
                        state.base_feedback=(yaw,clock())
                except Exception:
                    state.base_feedback=None
            else:
                state.base_feedback=None
            time.sleep(.05)

    def control(self, conn, addr):
        owned = None
        observer = None
        conn.settimeout(3)
        conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        stream = conn.makefile('rb')
        try:
            hello = json.loads(stream.readline(512).decode('utf-8'))
            if hello.get('token') != self.token:
                return
            if hello.get('emergency') is True:
                self.state.disarm()
                self.state.fault = 'Tablet STOP; hold A + X to re-arm'
                send_json(conn, {'armed': False})
                return
            if self.closing:
                send_json(conn, {'error':'TelePepper is closing'})
                return
            if hello.get('role') == 'operator':
                observer=uuid.uuid4().hex
                self.woz.observers[observer]=addr[0]
                send_json(conn, {'observer':observer,'simulation':self.state.robot.simulate})
                while self.running:
                    line=stream.readline(16384)
                    if not line: break
                    try:
                        request=json.loads(line.decode('utf-8'))
                        if request.get('cmd') == 'return_to_normal':
                            result = self.return_to_normal(request)
                            self.running = False
                            send_json(conn,result)
                            return
                        if self.closing and request.get('cmd') not in ('stop','status',None):
                            raise ValueError('TelePepper is closing')
                        if request.get('cmd') == 'stop': self.state.disarm('Operator STOP')
                        elif request.get('cmd') not in ('status',None): self.woz.action(request)
                        result=self.woz.status(request.get('since'))
                        result.update(armed=self.state.armed,fault=self.state.fault,pilot_connected=bool(self.state.session),ok=True)
                        send_json(conn,result)
                    except Exception as exc: send_json(conn,{'ok':False,'error':str(exc)})
                return
            with self.state.lock:
                if self.state.session:
                    send_json(conn, {'error': 'Another pilot is connected'})
                    return
                owned = uuid.uuid4().hex
                self.state.session = owned
                self.state.peer = addr[0]
                self.state.seq = -1
                self.state.min_offset = None
                self.state.tracking_valid = False
                self.state.received = 0.
                # A new pilot is connected; do not present an old disconnect as
                # the current network state. Other faults remain visible.
                if self.state.fault == 'Pilot disconnected': self.state.fault = ''
            send_json(conn, {'session': owned, 'simulation': self.state.robot.simulate})
            while self.running:
                line = stream.readline(16384)
                if not line:
                    break
                request = json.loads(line.decode('utf-8'))
                cmd = request.get('cmd')
                try:
                    if cmd == 'return_to_normal':
                        response = self.return_to_normal(request)
                        self.running = False
                        send_json(conn,response)
                        return
                    if self.closing and cmd not in ('stop','status'):
                        raise ValueError('TelePepper is closing')
                    if cmd == 'arm':
                        with self.state.lock:
                            expected=request.get('expected_stop_generation')
                            if expected is not None and expected != self.state.stop_generation:
                                raise ValueError('Start cancelled by a newer STOP')
                            if self.state.preparing:
                                raise ValueError('Wait for posture preparation to finish')
                            if not self.state.tracking_valid or clock()-self.state.received > .1:
                                raise ValueError('Fresh calibrated body tracking and video required before arming')
                            began = clock()
                            self.state.robot.arm()
                            self.state.cancel_recovery()
                            self.state.arm_ms = (clock()-began)*1000.
                            self.state.last_apply_at = None
                            self.state.heading.reset()
                            self.state.received = clock()
                            self.state.fault = ''
                            epoch=request.get('stop_epoch')
                            if epoch is not None and (type(epoch) is not int or epoch < 0):
                                raise ValueError('Invalid STOP epoch')
                            self.state.armed_stop_epoch=epoch
                            self.state.armed = True
                    elif cmd == 'stop':
                        with self.state.lock:
                            neutral=request.get('return_to_neutral') is True
                            self.state.disarm('Operator STOP' if neutral else 'Quest STOP (button, menu, calibration or missing acknowledgement)',return_to_neutral=neutral)
                            # Each explicit START begins with acknowledged STOP.
                            # Re-establish clock offset while disarmed; retaining an
                            # old session minimum can permanently reject a retry.
                            # Sequence ordering/sample-age/watchdog checks remain.
                            self.state.min_offset = None
                    elif cmd == 'pause_timeout': self.state.pause_for_timeout(clock(),'Command acknowledgement timeout')
                    elif cmd != 'status': self.woz.action(request)
                    response=self.woz.status()
                    response.update(armed=self.state.armed,fault=self.state.fault,ok=True,gesture=self.state.gesture[0] if self.state.gesture else '')
                    send_json(conn,response)
                except Exception as exc:
                    send_json(conn,{'armed':self.state.armed,'fault':self.state.fault,'ok':False,'error':str(exc)})
        except Exception as e:
            print('Control disconnected:', str(e))
        finally:
            if observer: self.woz.observers.pop(observer,None)
            if owned:
                with self.state.lock:
                    if not self.normal_exit: self.fail('Pilot disconnected')
                    self.state.session = None
                    self.state.peer = None
                    try:
                        if not self.normal_exit: self.state.robot.restore()
                    except Exception as e:
                        print('Restore:', e)
            stream.close()
            conn.close()

    def video(self, conn, addr):
        subscription = None
        stream = conn.makefile('rb')
        try:
            conn.settimeout(2)
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            conn.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 32768)
            request = json.loads(stream.readline(512).decode('utf-8'))
            if not self.woz.authorized(request.get('session'),addr[0]):
                return
            owned = request.get('session')
            if 'piper_bytes' in request:
                self.woz.play_piper(conn,stream,request,addr[0])
                return
            requested_camera=int(request.get('camera',-1))
            fps=max(1,min(int(request.get('fps',30)),30))
            if requested_camera not in (-1,0,1,2): return
            camera = None
            from PIL import Image, ImageDraw
            robot = self.state.robot
            seq = 0
            from depth_video import render_depth
            last_capture = None
            camera_settings={}
            next_capture=0.;capture_fps=fps
            requests=duplicates=0;capture_ms=encode_ms=0.;last_frame_at=0.
            while self.running and self.woz.authorized(owned,addr[0]):
                target=self.woz.camera if requested_camera == -1 else requested_camera
                if camera != target:
                    if subscription: robot.video.unsubscribe(subscription);subscription=None
                    camera=target;last_capture=None
                    capture_fps=min(fps,15) if camera==2 else fps
                    if not robot.simulate:
                        subscription=robot.video.subscribeCamera('TP_'+uuid.uuid4().hex[:12],camera,1,17 if camera==2 else 11,min(fps,15) if camera==2 else fps)
                        camera_settings=prepare_bottom_camera(robot.video) if camera==1 else {}
                # Pace *acquisition*, not just JPEG output. Duplicate/missing
                # images must not create a 333 Hz getImageRemote polling loop.
                wait=next_capture-clock()
                if wait>0:time.sleep(wait)
                if not self.running or not self.woz.authorized(owned,addr[0]):break
                start=clock();next_capture=start+1./capture_fps
                if robot.simulate:
                    image = Image.new('RGB', (320, 240), (18, 33, 45))
                    ImageDraw.Draw(image).text((20, 100), 'SIMULATION / cam %d / frame %d' % (camera,seq), fill=(70, 240, 170))
                else:
                    requests+=1
                    raw = robot.video.getImageRemote(subscription)
                    capture_ms=(clock()-start)*1000.
                    self.video_diagnostics[camera]=dict(fps=capture_fps,requests=requests,duplicates=duplicates,capture_ms=capture_ms,encode_ms=encode_ms,last_frame_at=last_frame_at,connected=True,camera_settings=camera_settings)
                    if raw is None:
                        continue
                    # getImageRemote returns an owned copy. releaseImage is only
                    # for the local image API and is absent from this qi service.
                    captured = (raw[4], raw[5])
                    if captured == last_capture:
                        duplicates+=1
                        continue
                    last_capture = captured
                    if camera==2:
                        image=render_depth(raw[0],raw[1],bytes(raw[6]))
                    else: image = Image.frombytes('RGB', (raw[0], raw[1]), bytes(raw[6]))
                encoded_at=clock();output = BytesIO()
                image.save(output, format='JPEG', quality=45)
                payload = output.getvalue()
                encode_ms=(clock()-encoded_at)*1000.;last_frame_at=clock()
                self.video_diagnostics[camera]=dict(fps=capture_fps,requests=requests,duplicates=duplicates,capture_ms=capture_ms,encode_ms=encode_ms,last_frame_at=last_frame_at,connected=True,camera_settings=camera_settings)
                # 4-byte length then JPEG. Pull credit prevents a TCP frame backlog.
                conn.sendall(struct.pack('!I', len(payload)) + payload)
                if stream.read(1) != b'N':
                    break
                seq += 1
        except Exception as e:
            print('Video:', str(e))
        finally:
            if 'camera' in locals() and camera in (0,1,2):
                self.video_diagnostics[camera]=dict(fps=capture_fps,requests=requests,duplicates=duplicates,capture_ms=capture_ms,encode_ms=encode_ms,last_frame_at=last_frame_at,connected=False)
            if subscription:
                try:
                    self.state.robot.video.unsubscribe(subscription)
                except Exception:
                    pass
            stream.close()
            conn.close()

    def accept_loop(self, port, handler):
        sock = listen(port)
        try:
            while self.running:
                try:
                    conn, addr = sock.accept()
                    self.spawn(handler, conn, addr)
                except socket.timeout:
                    pass
        finally:
            sock.close()

    def check_robot_connection(self):
        robot=self.state.robot
        if robot.simulate:return True
        try: connected=robot.session.isConnected()
        except Exception: connected=False
        if connected:return True
        # Old qi proxies remain unusable after a NAOqi transport disconnect.
        # Exit this service so systemd reconstructs them; never resume motion.
        with self.state.lock:
            self.state.armed=False
            self.state.cancel_recovery()
            self.state.latest=None
            self.state.fault='Robot services disconnected; restarting connection'
        self.restart_required=True
        self.running=False
        return False

    def robot_connection_loop(self):
        while self.running:
            if not self.check_robot_connection():return
            time.sleep(.5)

    def discovery_loop(self):
        sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        try:
            sock.bind(('0.0.0.0',self.port+4));sock.settimeout(.5)
            reply=json.dumps(dict(kind='telepepper-discovery',version=1,name=socket.gethostname(),control_port=self.port,simulation=self.state.robot.simulate)).encode('utf-8')
            recent={}
            while self.running:
                try:
                    data,address=sock.recvfrom(128)
                    if data!=b'TELEPEPPER_DISCOVER_V1':continue
                    now=clock()
                    if now-recent.get(address[0],-10)<.25:continue
                    recent={ip:at for ip,at in recent.items() if now-at<2.}
                    recent[address[0]]=now
                    sock.sendto(reply,address)
                except socket.timeout:pass
        finally:sock.close()

    def run(self):
        self.woz.start()
        self.spawn(self.robot_connection_loop)
        self.spawn(self.discovery_loop)
        self.spawn(self.udp_loop)
        self.spawn(self.heading_feedback_loop)
        self.spawn(self.motion_loop)
        self.spawn(self.accept_loop, self.port+2, self.video)
        print('TelePepper control=%d motion=%d video=%d simulation=%s' %
              (self.port, self.port+1, self.port+2, self.state.robot.simulate))
        try:
            self.accept_loop(self.port, self.control)
        finally:
            self.running = False
            try:
                if not self.normal_exit:self.state.disarm()
            finally:self.udp.close()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--token-file', required=True)
    parser.add_argument('--simulate', action='store_true')
    parser.add_argument('--port', type=int, default=9570)
    args = parser.parse_args()
    with open(args.token_file) as f:
        token = f.read().strip()
    if len(token) < 12:
        raise SystemExit('Pairing code must contain at least 12 characters')
    service = Service(token, args.simulate, args.port)
    def shutdown(signum, frame):
        service.running = False
    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)
    service.run()
    if service.restart_required:sys.exit(1)
