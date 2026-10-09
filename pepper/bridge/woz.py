"""WoZ services isolated from the real-time motion path (Python 2.7/3).
No raw arbitrary NAOqi dispatch: all operator actions are allowlisted.
"""
from __future__ import print_function
from bridge_version import VERSION, VARIANT
import collections
import json
import math
import socket
import struct
import threading
import time
import uuid
import os
import select
import tempfile
import wave
from official_animations import NAMES as GESTURES, CATALOG as OFFICIAL_ANIMATIONS

SCALARS = {
 'battery': 'Device/SubDeviceList/Battery/Charge/Sensor/Value',
 'battery_temperature': 'Device/SubDeviceList/Battery/Temperature/Sensor/Value',
 'sonar_front': 'Device/SubDeviceList/Platform/Front/Sonar/Sensor/Value',
 'sonar_back': 'Device/SubDeviceList/Platform/Back/Sonar/Sensor/Value',
 'touch_head_front': 'Device/SubDeviceList/Head/Touch/Front/Sensor/Value',
 'touch_head_middle': 'Device/SubDeviceList/Head/Touch/Middle/Sensor/Value',
 'touch_head_rear': 'Device/SubDeviceList/Head/Touch/Rear/Sensor/Value',
 'touch_left_hand': 'Device/SubDeviceList/LHand/Touch/Back/Sensor/Value',
 'touch_right_hand': 'Device/SubDeviceList/RHand/Touch/Back/Sensor/Value',
 'bumper_front_left': 'Device/SubDeviceList/Platform/FrontLeft/Bumper/Sensor/Value',
 'bumper_front_right': 'Device/SubDeviceList/Platform/FrontRight/Bumper/Sensor/Value',
 'bumper_back': 'Device/SubDeviceList/Platform/Back/Bumper/Sensor/Value',
}
DEFAULT_PHRASES = ['Hello, I am Pepper.', 'How are you?', 'Could you repeat that, please?',
                   'Thank you.', 'All right.', 'One moment, please.']
THERMAL_JOINTS = ['HeadYaw','HeadPitch','LShoulderPitch','LShoulderRoll','LElbowYaw','LElbowRoll','LWristYaw','LHand',
                  'RShoulderPitch','RShoulderRoll','RElbowYaw','RElbowRoll','RWristYaw','RHand',
                  'HipRoll','HipPitch','KneePitch','WheelFL','WheelFR','WheelB']

class WoZ(object):
    def __init__(self, service, clock):
        self.service, self.clock = service, clock
        self.lock = threading.RLock()
        self.events = collections.deque(maxlen=3000)
        self.event_seq = 0
        self.observers = {}
        self.snapshot = {'available': False, 'simulation': service.state.robot.simulate}
        self.phrases = list(DEFAULT_PHRASES)
        self.camera, self.view = 0, 'panel'
        self.tablet = {'revision': 0, 'text': '', 'choices': []}
        self.tablet_fade_at = None
        self.speech_on_tablet = True
        self.speaker_volume = 50
        self.last_response = None
        self.tablet_seen = None
        self.tablet_revision = -1
        self.services = {}
        self.errors = {}
        self.sensor_pending = {}
        self.audio_peers = {}
        self.audio_last_seq = {}
        self.audio_last_talk = 0.
        self.speech_future = None
        self.speech_id = None
        self.audio_seq = 0
        self.audio_capture_ms = None
        self.audio_capture_at = 0.
        self.audio_pending = bytearray()
        self.audio_output = collections.deque(maxlen=2)
        self.audio_lock = threading.Lock()
        self.clip_lock=threading.Lock()
        self.clip_generation=0
        self.clip_future=None
        self.audio_callback = None
        self.audio_registration = None
        self.audio = None
        self.audio_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.audio_socket.bind(('0.0.0.0', service.port+3))
        self.audio_socket.settimeout(.05)
        if not service.state.robot.simulate:
            for name in ['ALMemory','ALTextToSpeech','ALLeds','ALAudioDevice','ALAudioPlayer']:
                try: self.services[name] = service.state.robot.session.service(name)
                except Exception as exc: self.errors[name] = str(exc)
            if 'ALLeds' in self.services:
                try:
                    leds=self.services['ALLeds']
                    leds.fadeRGB('FaceLeds',0x4388ff,.2)
                    self.set_shoulder_color(leds,0x4388ff)
                    leds.setIntensity('EarLeds',1.)
                except Exception as exc: self.errors['default_leds']=str(exc)
            if 'ALAudioDevice' in self.services:
                try: self.services['ALAudioDevice'].setOutputVolume(self.speaker_volume)
                except Exception as exc: self.errors['speaker_volume'] = str(exc)

    @staticmethod
    def set_shoulder_color(leds,color):
        # Pepper exposes individual chest/shoulder colour groups, not ChestLeds.
        for channel,shift in (('Red',16),('Green',8),('Blue',0)):
            leds.setIntensity('ChestLeds'+channel,((color>>shift)&255)/255.)

    def event(self, kind, **fields):
        with self.lock:
            self.event_seq += 1
            event = dict(seq=self.event_seq, robot_mono=self.clock(), utc=time.time(), kind=kind)
            event.update(fields)
            self.events.append(event)
            return event

    def authorized(self, session, ip):
        state = self.service.state
        return bool(session and ((session == state.session and ip == state.peer) or self.observers.get(session) == ip))

    def status(self, since=None):
        with self.lock:
            now=self.clock()
            if self.tablet_fade_at is not None and now>=self.tablet_fade_at+1.:
                self.tablet={'revision':self.tablet['revision']+1,'text':'','choices':[]}
                self.tablet_fade_at=None
            tablet=dict(self.tablet)
            remaining=max(0.,self.tablet_fade_at-now) if self.tablet_fade_at is not None else 0.
            opacity=max(0.,min(1.,self.tablet_fade_at+1.-now)) if self.tablet_fade_at is not None else 1.
            tablet.update(fade_after_ms=int(remaining*1000),fade_ms=1000,opacity=opacity)
            result = {'bridge': {'version': VERSION, 'variant': VARIANT}, 'telemetry': dict(self.snapshot), 'camera': self.camera, 'view': self.view,
                      'tablet_display': {'connected': self.tablet_seen is not None and self.clock()-self.tablet_seen<2., 'revision':self.tablet_revision},
                      'phrases': list(self.phrases), 'tablet': tablet, 'speech_on_tablet':self.speech_on_tablet, 'speaker_volume':self.speaker_volume, 'participant_response':self.last_response,
                      'capabilities': {'simulation': self.service.state.robot.simulate,
                                       'services': list(self.services), 'errors': dict(self.errors)},
                      'audio': {'microphone': self.audio_callback is not None or self.service.state.robot.simulate,
                                'capture_block_ms': self.audio_capture_ms,
                                'capture_recent': self.clock()-self.audio_capture_at < 1,
                                'speaking_live': self.clock()-self.audio_last_talk < .15},
                      'event_cursor': self.event_seq, 'robot_mono': self.clock()}
            result['video_diagnostics']=list(self.service.video_diagnostics)
            result['preparation']={'busy':self.service.state.preparing,'message':self.service.state.prepare_message,'revision':self.service.state.preparation_revision}
            result['animation']=dict(self.service.state.gesture_diagnostics)
            result['speech_gestures']=self.service.state.robot.speech_arms.status()
            result['official_animations']=[name for name in GESTURES if OFFICIAL_ANIMATIONS[name] in self.service.state.robot.available_animations]
            state=self.service.state
            result['robot_limits']=dict(base=state.robot.base_collision_enabled,range=state.robot.range_limited,speed=state.robot.speed_limited)
            result['base_test_cap']=state.robot.base_test_cap
            result['motion_diagnostics'] = dict(last_stop=state.last_stop,
                stop_history=list(state.stop_history), base_requests=state.robot.base_requests,
                pending_motion_ms={name:max(0.,(self.clock()-slot[1])*1000.) for name,slot in state.robot.motion_pending.items()},
                stop_generation=state.stop_generation,
                coalesced_packets=state.coalesced_packets,
                udp_packets=state.udp_packets, udp_rejected=state.udp_rejected,
                udp_last_reject=state.udp_last_reject,
                udp_age_ms=(self.clock()-state.udp_last_seen)*1000 if state.udp_last_seen is not None else None,
                apply_interval_ms=state.apply_interval_ms,
                queue_ms=state.queue_ms,
                auto_turn=state.auto_turn, auto_turn_status=state.heading.status,
                recovering=state.recovery_until is not None, recovery_count=state.recovery_count,
                returning_to_neutral=state.relax_at is not None, stopping=bool(state.robot.stop_pending), stop_error=state.robot.stop_error,
                base_collision_enabled=state.robot.base_collision_enabled,
                requested_angles=state.requested_angles,
                tracking_valid=state.tracking_valid, stale_drops=state.stale_drops, input_delay_ms=state.input_delay_ms,
                command_age_ms=(self.clock()-state.received)*1000 if state.received else None,
                apply_ms=state.apply_ms, arm_ms=state.arm_ms)
            if since is not None:
                result['events'] = [e for e in self.events if e['seq'] > int(since)][:300]
                result['events_lost'] = bool(self.events and int(since) < self.events[0]['seq']-1)
                result['event_cursor'] = result['events'][-1]['seq'] if result['events'] else self.event_seq
            return result

    def action(self, request):
        cmd = request.get('cmd')
        if cmd == 'speech_gestures':
            with self.service.state.lock:
                self.service.state.robot.speech_arms.set_enabled(request.get('enabled'))
        elif cmd == 'speech_gesture_trigger':
            if not self.service.state.robot.speech_arms.enabled:raise ValueError('Enable speech gestures first')
            if not self.begin_speech_gestures(4.,'ptt'):raise ValueError('Start tracking and wait for initial engagement before speech gestures')
        elif cmd == 'base_protection':
            state=self.service.state
            with state.lock:
                if state.armed or state.preparing: raise ValueError('Stop motion before changing base protection')
                state.cancel_recovery()
                state.robot.set_base_collision(request.get('enabled'))
        elif cmd == 'motion_limits':
            state=self.service.state
            with state.lock:
                if state.armed or state.preparing or state.robot.stop_pending:
                    raise ValueError('Wait for motion STOP before changing limits')
                state.cancel_recovery()
                state.robot.set_motion_limit(request.get('key'),request.get('enabled'))
        elif cmd == 'prepare_motion':
            self.service.prepare_motion(request.get('confirmed'))
        elif cmd == 'camera':
            camera = int(request.get('camera',0))
            if camera not in (0,1,2): raise ValueError('Unknown camera')
            self.camera = camera
        elif cmd == 'view':
            view = request.get('view')
            if view not in ('panel','wide'): raise ValueError('Unknown view')
            self.view = view
        elif cmd == 'phrases':
            phrases = request.get('phrases')
            if not isinstance(phrases,list) or not 1 <= len(phrases) <= 24: raise ValueError('1-24 phrases required')
            self.phrases = [str(p)[:400] if not isinstance(p,type(u'')) else p[:400] for p in phrases]
        elif cmd == 'speech_on_tablet':
            enabled=request.get('enabled')
            if not isinstance(enabled,bool): raise ValueError('Caption setting must be boolean')
            self.speech_on_tablet=enabled
        elif cmd == 'speech_caption':
            text=request.get('text','')
            if not isinstance(text,(str,type(u''))) or not text or len(text)>400: raise ValueError('Caption text must contain 1-400 characters')
            self.caption(text)
        elif cmd == 'say':
            text = request.get('text','')
            if not isinstance(text,(str,type(u''))) or not text or len(text)>400: raise ValueError('Speech text must contain 1-400 characters')
            # Disallow NAOqi markup that could alter engine state outside this API.
            text = text.replace('\\',' ')
            if self.clock()-self.audio_last_talk < .3: raise ValueError('Release push-to-talk before TTS')
            tts = self.services.get('ALTextToSpeech')
            if tts:
                if self.speech_future and not self.speech_future.isFinished(): raise ValueError('Speech already in progress')
                language = request.get('language')
                if language:
                    if language not in tts.getAvailableLanguages(): raise ValueError('Language unavailable on Pepper')
                    tts.setLanguage(language)
                tts.setParameter('speed', max(50,min(150,int(request.get('speed',100)))))
                self.speech_future = tts.say(text,_async=True)
                self.begin_speech_gestures(60.,'tts')
            elif not self.service.state.robot.simulate: raise ValueError('Text-to-speech unavailable')
            self.speech_id = uuid.uuid4().hex
            self.caption(text)
            self.event('speech_requested', action_id=self.speech_id, text=text)
            if self.service.state.robot.simulate: self.event('speech_completed',action_id=self.speech_id,simulation=True)
        elif cmd == 'speech_stop':
            with self.service.state.lock:self.service.state.robot.speech_arms.end()
            self.clip_generation+=1
            if self.clip_future is not None and 'ALAudioPlayer' in self.services:self.services['ALAudioPlayer'].stopAll()
            with self.audio_lock: self.audio_output.clear()
            if 'ALTextToSpeech' in self.services: self.services['ALTextToSpeech'].stopAll()
            self.event('speech_cancelled',action_id=self.speech_id)
            self.speech_future = None
        elif cmd == 'volume':
            volume = max(0,min(100,int(request.get('value',50))))
            if 'ALAudioDevice' in self.services: self.services['ALAudioDevice'].setOutputVolume(volume)
            elif not self.service.state.robot.simulate: raise ValueError('Audio device unavailable')
            self.speaker_volume = volume
        elif cmd == 'led':
            group = request.get('group','FaceLeds')
            if group not in ('FaceLeds','ChestLeds','EarLeds'): raise ValueError('Unsupported LED group')
            color = int(request.get('color',0x46f0aa))
            if not 0 <= color <= 0xffffff: raise ValueError('Invalid RGB')
            leds = self.services.get('ALLeds')
            if leds:
                if group == 'EarLeds': leds.setIntensity(group,max(0,min(1,float(request.get('intensity',.5)))))
                elif group == 'ChestLeds': self.set_shoulder_color(leds,color)
                else: leds.fadeRGB(group,color,.2)
            elif not self.service.state.robot.simulate: raise ValueError('LED service unavailable')
        elif cmd == 'tablet_poll':
            self.tablet_seen=self.clock()
            self.tablet_revision=int(request.get('displayed_revision',-1))
            return {'ok':True}
        elif cmd == 'tablet':
            text=request.get('text','');choices=request.get('choices',[])
            reaction=request.get('reaction','')
            if reaction not in ('','smile','laugh','love','surprise','sad','wink','angry'):raise ValueError('Invalid tablet reaction')
            if not isinstance(text,(str,type(u''))) or len(text)>3000 or not isinstance(choices,list) or len(choices)>8: raise ValueError('Invalid tablet content')
            if any(not isinstance(x,(str,type(u''))) or len(x)>100 for x in choices): raise ValueError('Invalid choices')
            self.publish_tablet(text,choices,reaction)
        elif cmd == 'participant_response':
            if self.tablet_fade_at is not None and self.clock()>=self.tablet_fade_at+1.:raise ValueError('Stale tablet response')
            if int(request.get('revision',-1)) != self.tablet['revision']: raise ValueError('Stale tablet response')
            choice=int(request.get('choice',-1))
            if not 0 <= choice < len(self.tablet['choices']): raise ValueError('Invalid choice')
            self.event('participant_response',revision=self.tablet['revision'],choice=choice,value=self.tablet['choices'][choice])
            self.last_response={'revision':self.tablet['revision'],'choice':choice,'value':self.tablet['choices'][choice]}
        elif cmd == 'gesture':
            name=request.get('name')
            if name not in GESTURES: raise ValueError('Unknown gesture')
            state=self.service.state
            with state.lock:
                if not state.armed or self.clock()-state.received>.18: raise ValueError('Pilot must be armed with valid tracking')
                if state.robot.speech_arms.phase!='idle':raise ValueError('Speaking gestures own the arms; stop speech gestures first')
                if state.gesture is not None:raise ValueError('Stop the current animation before choosing another')
                if OFFICIAL_ANIMATIONS[name] not in state.robot.available_animations:raise ValueError('Official animation is not installed on this Pepper')
                # Hold the captured pose until gentle engagement has completed;
                # then give the gesture its full six-second playback interval.
                now=self.clock();wait=max(0.,min(3.,3.-(now-state.robot.armed_at)))
                state.gesture=(name,now+wait,list(state.robot.previous))
                state.gesture_diagnostics={'name':name,'stage':'engaging' if wait else 'playing','elapsed':0.,'samples':0,'requested_delta_deg':0.}
            self.event('gesture_started',name=name)
        else: raise ValueError('Unknown operator action')
        self.event('operator_action',action=cmd)
        return {'ok':True}

    def begin_speech_gestures(self,duration,source):
        state=self.service.state
        with state.lock:
            if state.armed and not state.gesture and self.clock()-state.received<=.18:
                # Initial engagement keeps its existing measured-pose ramp.
                if self.clock()-state.robot.armed_at>=state.robot.blend_seconds:
                    state.robot.speech_arms.begin(duration,source)
                    return True
        return False

    def live_speech_gestures(self,pcm):
        # Only voiced frames refresh the short hangover, not microphone silence.
        samples=struct.unpack('<%dh'%(len(pcm)//2),pcm)
        if samples and sum(float(v)*v for v in samples)/len(samples)>=200.*200.:
            self.begin_speech_gestures(.45,'live')

    def publish_tablet(self,text,choices=None,reaction=''):
        with self.lock:
            self.tablet={'revision':self.tablet['revision']+1,'text':text,'choices':choices or []}
            if reaction:self.tablet.update(reaction=reaction,text='',choices=[])
            self.tablet_fade_at=self.clock()+8. if text or choices or reaction else None

    def caption(self, text):
        if self.speech_on_tablet:
            self.publish_tablet(text)

    def read_sensor_batch(self, robot, memory, selected):
        errors={};futures={};results={}
        calls={'joint_accepted':lambda:robot.motion.getAngles(self.service.state.robot_names,False,_async=True),
               'joint_measured':lambda:robot.motion.getAngles(self.service.state.robot_names,True,_async=True),
               'lower_body_measured':lambda:robot.motion.getAngles(['HipRoll','HipPitch','KneePitch'],True,_async=True),
               'odometry':lambda:robot.motion.getRobotPosition(True,_async=True),
               'awake':lambda:robot.motion.robotIsWakeUp(_async=True),
               'life_state':lambda:robot.life.getState(_async=True)}
        if memory and selected:calls['memory']=lambda:memory.getListData(selected,_async=True)
        for name,call in calls.items():
            try:
                if name not in self.sensor_pending:self.sensor_pending[name]=(call(),self.clock())
                futures[name]=self.sensor_pending[name]

            except Exception as exc:errors[name]=str(exc)
        deadline=self.clock()+.25
        for name,item in futures.items():
            future,requested=item
            try:
                value=future.value(max(1,int((deadline-self.clock())*1000)))
                self.sensor_pending.pop(name,None)
                if self.clock()-requested<.75:results[name]=value
                else:errors[name]='Discarded delayed sensor reply'
            except Exception as exc:
                errors[name]=str(exc)
                # A client timeout/cancel does not necessarily stop execution in
                # NAOqi. Never enqueue another RPC behind an unfinished one.
                try:
                    if future.isFinished():self.sensor_pending.pop(name,None)
                except Exception:pass
        return results,errors

    def sensors(self):
        robot=self.service.state.robot
        memory=self.services.get('ALMemory')
        # Discover available ALMemory keys once, so missing sensors remain null.
        keys=set()
        if memory:
            try: keys=set(memory.getDataListName())
            except Exception as exc: self.errors['ALMemory']=str(exc)
        scalars={name:key for name,key in SCALARS.items() if key in keys}
        laser=[]
        for bank in ('Front','Left','Right'):
            for segment in range(1,16):
                candidates=[]
                for axis in ('X','Y'):
                    base='Platform/LaserSensor/%s/Horizontal/Seg%02d/%s/Sensor' % (bank,segment,axis)
                    candidates.append(next((k for k in (base+'/Value',base,'Device/SubDeviceList/'+base+'/Value') if k in keys),None))
                if all(candidates): laser.append((bank,segment,candidates))
        joint_keys=['Device/SubDeviceList/%s/Temperature/Sensor/Value'%name for name in THERMAL_JOINTS]
        status_keys=[k.replace('/Value','/Status') for k in joint_keys]
        selected=list(scalars.values())+[k for _,_,xy in laser for k in xy]+[k for k in joint_keys+status_keys if k in keys]
        while self.service.running:
            values={}
            try:
                if robot.simulate:
                    batch={'joint_measured':list(robot.previous),'lower_body_measured':[0.,0.,0.],'odometry':[0.,0.,0.],'awake':True,'life_state':'disabled'};sensor_errors={}
                else:batch,sensor_errors=self.read_sensor_batch(robot,memory,selected)
                values=dict(zip(selected,batch.get('memory',[])))
                def numeric(value):
                    try:
                        v=float(value)
                        return v if not math.isnan(v) and not math.isinf(v) else None
                    except (TypeError,ValueError): return None
                points=[]
                for bank,index,xy in laser:
                    x,y=numeric(values.get(xy[0])),numeric(values.get(xy[1]))
                    if x is not None and y is not None and math.hypot(x,y)>.01: points.append({'bank':bank,'segment':index,'x':x,'y':y})
                readings={name:numeric(values.get(key)) for name,key in SCALARS.items()}
                temperatures={name:numeric(values.get(key)) for name,key in zip(THERMAL_JOINTS,joint_keys)}
                measured=batch.get("joint_measured")
                snap={'available':bool(memory) or robot.simulate,'simulation':robot.simulate,'robot_mono':self.clock(),
                      'readings':readings,'lasers':points,'joint_temperature':temperatures,
                      'joint_temperature_status':{name:numeric(values.get(key)) for name,key in zip(THERMAL_JOINTS,status_keys)},'joint_measured':measured,
                      'joint_target':list(robot.previous),'base_command':list(robot.base),'torso_target':list(robot.torso.current),'torso_neutral':list(robot.torso.origin)}
                snap['lower_body_measured']=batch.get('lower_body_measured')
                snap['odometry']=batch.get('odometry')
                snap['awake']=batch.get('awake')
                snap['life_state']=batch.get('life_state')
                snap['available']=measured is not None and snap['awake'] is not None and snap['life_state'] is not None
                snap['motion_ready']=snap['available'] and snap['awake'] and snap['life_state']=='disabled'
                snap['joint_accepted']=batch.get('joint_accepted')
                snap['armed']=self.service.state.armed
                snap['requested_joint_target']=list(self.service.state.requested_angles) if self.service.state.requested_angles else None
                snap['sensor_errors']=sensor_errors
                if not snap['available']:snap['error']='; '.join('%s: %s'%item for item in sensor_errors.items())
                with self.lock: self.snapshot=snap
                self.event('telemetry',data=snap)
                if self.speech_future and self.speech_future.isFinished():
                    try: self.speech_future.value(); self.event('speech_completed',action_id=self.speech_id)
                    except Exception as exc: self.event('speech_failed',action_id=self.speech_id,error=str(exc))
                    self.speech_future=None
                    with self.service.state.lock:self.service.state.robot.speech_arms.end('tts')
            except Exception as exc:
                with self.lock: self.snapshot={'available':False,'error':str(exc),'robot_mono':self.clock(),'simulation':robot.simulate}
            time.sleep(.25)

    def microphone(self):
        audio=self.services.get('ALAudioDevice')
        if not audio: return
        parent=self
        class Callback(object):
            def processRemote(self, channels, samples, timestamp, buffer):
                try:
                    raw=bytes(buffer)
                    parent.audio_capture_ms = len(raw)*1000./32000
                    parent.audio_capture_at = parent.clock()
                    with parent.audio_lock:
                        parent.audio_pending.extend(raw)
                        # Preserve normal NAOqi callback blocks. Trimming every
                        # block to 40ms discards syllables on physical Pepper.
                        if len(parent.audio_pending)>8000:
                            dropped=len(parent.audio_pending)-8000
                            del parent.audio_pending[:dropped]
                            parent.event('audio_capture_drop',bytes=dropped)
                        while len(parent.audio_pending)>=640:
                            chunk=bytes(parent.audio_pending[:640]);del parent.audio_pending[:640]
                            parent.broadcast_audio(b'TPAD',chunk)
                except Exception as exc: parent.errors['microphone']=str(exc)
        try:
            session=self.service.state.robot.session
            session.listen('tcp://0.0.0.0:0')
            self.audio_callback=Callback()
            self.audio_registration=session.registerService('TelePepperMicrophone',self.audio_callback)
            audio.setClientPreferences('TelePepperMicrophone',16000,3,0)
            audio.subscribe('TelePepperMicrophone')
            self.audio=audio
        except Exception as exc:
            self.errors['microphone']=str(exc);self.audio_callback=None

    def broadcast_audio(self, magic, pcm):
        self.audio_seq=(self.audio_seq+1)&0xffffffff
        now=self.clock()
        for session,(address,stamp) in list(self.audio_peers.items()):
            if now-stamp>2 or not self.authorized(session,address[0]):
                self.audio_peers.pop(session,None);continue
            try: self.audio_socket.sendto(magic+session.encode('ascii')+struct.pack('!Id',self.audio_seq,now)+pcm,address)
            except socket.error: pass

    def audio_loop(self):
        self.microphone()
        self.service.spawn(self.playback)
        last_simulated=0.
        while self.service.running:
            try:
                data,addr=self.audio_socket.recvfrom(1500)
                if data.startswith(b'{'):
                    request=json.loads(data.decode('utf-8'));session=request.get('session','')
                    if self.authorized(session,addr[0]): self.audio_peers[session]=(addr,self.clock())
                elif len(data)==688 and data[:4]==b'TPAU':
                    session=data[4:36].decode('ascii');seq,stamp=struct.unpack('!Id',data[36:48])
                    if session != self.service.state.session or not self.authorized(session,addr[0]): continue
                    if seq<=self.audio_last_seq.get(session,-1): continue
                    self.audio_last_seq[session]=seq
                    if self.clip_future is not None or self.speech_future and not self.speech_future.isFinished(): continue
                    self.audio_last_talk=self.clock()
                    # Live-mode gestures are triggered on PTT release.
                    with self.audio_lock: self.audio_output.append((self.clock(),data[48:]))
                    self.broadcast_audio(b'TPAU',data[48:])
            except socket.timeout: pass
            except (ValueError,UnicodeError,KeyError,TypeError): pass
            except Exception as exc: self.errors['audio_transport']=str(exc)
            if self.service.state.robot.simulate and self.clock()-last_simulated>=.02:
                self.broadcast_audio(b'TPAD',b'\0'*640);last_simulated=self.clock()
        if self.audio:
            try: self.audio.unsubscribe('TelePepperMicrophone')
            except Exception: pass
        if self.audio_registration is not None:
            try: self.service.state.robot.session.unregisterService(self.audio_registration)
            except Exception: pass
        self.audio_socket.close()

    def play_piper(self,conn,stream,request,ip):
        """One bounded PCM clip per authenticated connection; ACK after playback."""
        size=request.get('piper_bytes')
        if not isinstance(size,int) or size<2 or size>1920000 or size%2:
            conn.sendall(b'{"error":"Invalid Piper clip"}\n');return
        caption=request.get('text','')
        if not isinstance(caption,(str,type(u''))) or len(caption)>400:
            conn.sendall(b'{"error":"Invalid Piper caption"}\n');return
        player=self.services.get('ALAudioPlayer')
        if not player and not self.service.state.robot.simulate:
            conn.sendall(b'{"error":"Pepper audio player unavailable"}\n');return
        if not self.clip_lock.acquire(False):
            conn.sendall(b'{"error":"Piper already playing"}\n');return
        path=None;future=None;generation=self.clip_generation
        try:
            conn.sendall(b'{"ready":true}\n');chunks=[];remaining=size
            while remaining:
                block=stream.read(min(remaining,65536))
                if not block:raise ValueError('Piper upload interrupted')
                chunks.append(block);remaining-=len(block)
            if generation!=self.clip_generation or not self.authorized(request.get('session'),ip):raise ValueError('Piper cancelled')
            if self.speech_future and not self.speech_future.isFinished():raise ValueError('Pepper TTS is already speaking')
            if player:
                fd,path=tempfile.mkstemp(prefix='telepepper-',suffix='.wav');os.close(fd)
                output=wave.open(path,'wb')
                try:output.setnchannels(1);output.setsampwidth(2);output.setframerate(16000);output.writeframes(b''.join(chunks))
                finally:output.close()
                with self.audio_lock:self.audio_output.clear()
                future=player.playFile(path,1.,0.,_async=True);self.clip_future=future
                if caption:self.caption(caption)
                self.begin_speech_gestures(size/32000.+.1,'piper')
                self.event('piper_playback_started',bytes=size)
                deadline=self.clock()+size/32000.+10
                while not future.isFinished():
                    if generation!=self.clip_generation or not self.authorized(request.get('session'),ip) or self.clock()>deadline:raise ValueError('Piper playback cancelled or timed out')
                    if select.select([conn],[],[],.02)[0] and not conn.recv(1,socket.MSG_PEEK):raise ValueError('Piper pilot disconnected')
                future.value(0)
                if generation!=self.clip_generation:raise ValueError('Piper cancelled')
            conn.sendall(b'{"played":true}\n');self.event('piper_playback_completed',bytes=size)
        except Exception as exc:
            if future is not None:
                try:player.stopAll()
                except Exception:pass
            self.event('piper_playback_failed',reason=str(exc))
            try:conn.sendall((json.dumps({'error':str(exc)})+'\n').encode('utf-8'))
            except Exception:pass
        finally:
            with self.service.state.lock:self.service.state.robot.speech_arms.end('piper')
            self.clip_future=None
            if path:
                try:os.unlink(path)
                except OSError:pass
            self.clip_lock.release()

    def playback(self):
        converter=None
        while self.service.running:
            item=None
            with self.audio_lock:
                if self.audio_output: item=self.audio_output.popleft()
            if item is None: time.sleep(.005);continue
            received,pcm=item
            if self.clock()-received>.08: self.event('audio_playback_drop');continue
            if self.service.state.robot.simulate: continue
            audio=self.services.get('ALAudioDevice')
            if not audio: continue
            try:
                import audioop
                rate=int(audio.getParameter('outputSampleRate'))
                if rate not in (16000,22050,44100,48000): raise ValueError('Unsupported speaker sample rate')
                mono,converter=audioop.ratecv(pcm,2,1,16000,rate,converter)
                stereo=audioop.tostereo(mono,2,1.,1.)
                if not audio.sendRemoteBufferToOutput(len(stereo)//4,bytearray(stereo),_async=True).value(60):
                    raise RuntimeError('Robot audio output rejected buffer')
            except Exception as exc: self.errors['speaker']=str(exc)

    def start(self):
        self.service.spawn(self.sensors)
        self.service.spawn(self.audio_loop)
