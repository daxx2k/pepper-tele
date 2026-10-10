import copy
import json
import unittest
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'pepper/bridge'))
import telepepper as tp

class CompactStatusTests(unittest.TestCase):
    def payload(self):
        return dict(ok=True,armed=True,robot_mono=1234.567891,bridge={'version':'test'},
            telemetry=dict(joint_target=[.1]*14,joint_measured=[.2]*14,joint_temperature=[35]*14,lasers=[{'x':1,'y':2}],battery=.8),
            motion_diagnostics=dict(stop_generation=7,last_stop={'reason':'Operator STOP'},stopping=False,stop_error='',stop_history=[{'reason':'old'}]*32),
            official_animations=['wave_left','wave_right'],animation={'busy':False},speech_gestures={'enabled':True},
            robot_limits=dict(base=True,range=True,speed=True),capabilities={'services':['unused']*20,'errors':{}},
            events=[{'payload':'unused'}]*300,tablet={'text':'hidden'},tablet_display={},participant_response=None,video_diagnostics=[{}]*3,speech_on_tablet=False)
    def test_compact_preserves_live_pepper_ui_exit_and_gestures(self):
        original=self.payload();before=copy.deepcopy(original);small=tp.compact_status(original)
        self.assertEqual(original,before)
        for key in ('telemetry','official_animations','animation','speech_gestures','robot_limits','robot_mono','bridge','armed','ok'):self.assertEqual(small[key],original[key])
        for key in ('stop_generation','last_stop','stopping','stop_error'):self.assertEqual(small['motion_diagnostics'][key],original['motion_diagnostics'][key])
        self.assertNotIn('stop_history',small['motion_diagnostics']);self.assertLess(len(json.dumps(small)),len(json.dumps(original))/2)
    def test_full_status_callers_keep_unfiltered_payload(self):
        original=self.payload();tp.compact_status(original)
        self.assertIn('events',original);self.assertEqual(len(original['motion_diagnostics']['stop_history']),32)
    def test_missing_optional_sections_are_supported(self):
        self.assertEqual(tp.compact_status({'ok':False,'error':'refused'})['error'],'refused')

    def test_real_control_protocol_supports_compact_and_legacy_observer_and_pilot(self):
        import socket,threading
        from unittest.mock import Mock
        for role in ('operator','pilot'):
            service=tp.Service.__new__(tp.Service);service.state=tp.State(tp.Robot(True))
            service.token='test-compact-protocol';service.running=True;service.closing=False;service.normal_exit=False
            service.woz=Mock();service.woz.observers={};service.woz.status.side_effect=lambda *args:copy.deepcopy(self.payload());service.fail=Mock()
            listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1)
            client=socket.create_connection(listener.getsockname());server,address=listener.accept();listener.close()
            worker=threading.Thread(target=service.control,args=(server,address));worker.start();client.settimeout(2)
            stream=client.makefile('rwb')
            def exchange(value):
                stream.write((json.dumps(value)+'\n').encode());stream.flush();return json.loads(stream.readline())
            try:
                hello=exchange({'token':service.token,'role':role})
                self.assertIn('observer' if role=='operator' else 'session',hello)
                full=exchange({'cmd':'status'});small=exchange({'cmd':'status','compact':True})
                self.assertIn('tablet',full);self.assertNotIn('tablet',small)
                self.assertEqual(small['official_animations'],full['official_animations'])
                self.assertEqual(small['telemetry'],full['telemetry']);self.assertIn('stop_generation',small['motion_diagnostics'])
                service.woz.action.assert_not_called()
            finally:
                stream.close();client.close();worker.join(2);self.assertFalse(worker.is_alive())
