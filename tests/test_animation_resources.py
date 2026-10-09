import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock
import zipfile
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('animation_resources',ROOT/'pepper/bridge/animation_resources.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
XML='<?xml version="1.0"?><Animation typeVersion="2.0"><ActuatorCurve actuator="LShoulderPitch"/></Animation>'
class AnimationResourceTests(unittest.TestCase):
    def test_bundled_name_sends_path_and_retains_future_cancellation(self):
        with tempfile.TemporaryDirectory() as folder:
            with zipfile.ZipFile(Path(folder)/'telepepper-speaking.pkg','w') as z:z.writestr('happy.qianim',XML)
            raw=Mock();player=m.ResourceAnimationPlayer(raw,folder)
            self.assertIs(player.run('telepepper-speaking/happy.qianim'),raw.run.return_value)
            raw.run.assert_called_once_with('telepepper-speaking/happy.qianim',_async=True)
    def test_native_system_library_name_passes_through_unchanged(self):
        with tempfile.TemporaryDirectory() as folder:
            raw=Mock();m.ResourceAnimationPlayer(raw,folder).run('animations/Stand/example')
            raw.run.assert_called_once_with('animations/Stand/example',_async=True)
    def test_missing_bundle_never_sends_an_invalid_resource_name(self):
        with tempfile.TemporaryDirectory() as folder:
            raw=Mock()
            with self.assertRaises(ValueError):m.ResourceAnimationPlayer(raw,folder).run('telepepper-speaking/missing.qianim')
            raw.run.assert_not_called()
    def test_missing_xml_declaration_is_rejected_before_motor_call(self):
        with tempfile.TemporaryDirectory() as folder:
            with zipfile.ZipFile(Path(folder)/'telepepper-speaking.pkg','w') as z:z.writestr('happy.qianim','<Animation typeVersion="2.0"/>')
            raw=Mock()
            with self.assertRaises(ValueError):m.ResourceAnimationPlayer(raw,folder)
            raw.run.assert_not_called()
    def test_real_speech_bundle_has_parseable_headers(self):
        raw=Mock();player=m.ResourceAnimationPlayer(raw,str(ROOT/'pepper/bridge'))
        for name in ('happy','point_left','point_right'):
            self.assertTrue(player.resources['telepepper-speaking/'+name+'.qianim'].startswith('<?xml'))
    def test_invalid_bundle_fails_before_any_motor_call(self):
        with tempfile.TemporaryDirectory() as folder:
            with zipfile.ZipFile(Path(folder)/'telepepper-speaking.pkg','w') as z:z.writestr('bad.qianim','<Other/>')
            raw=Mock()
            with self.assertRaises(ValueError):m.ResourceAnimationPlayer(raw,folder)
            raw.run.assert_not_called()
if __name__=='__main__':unittest.main()
