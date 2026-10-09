import importlib.util
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('qianim_compat',ROOT/'pepper/bridge/qianim_compat.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class QianimCompatibilityTests(unittest.TestCase):
    def test_integer_metadata_does_not_change_curve_targets_or_time(self):
        data=b'<Animation typeVersion="2.0"><ActuatorCurve actuator="LShoulderPitch" mute="false" fps="25.0" unit="degree"><Key frame="19.0" value="-33.2"><Tangent side="right" abscissaParam="3.3" ordinateParam="0.5"/></Key></ActuatorCurve></Animation>'
        result=m.compatible(data);self.assertTrue(result.startswith(b'<?xml'))
        curve=ET.fromstring(result)[0];self.assertEqual(curve.get('mute'),'0');self.assertEqual(curve.get('fps'),'25')
        self.assertEqual(curve[0].get('frame'),'19');self.assertEqual(curve[0].get('value'),'-33.2')
        self.assertEqual(curve[0][0].get('abscissaParam'),'3.3')
    def test_never_rounds_fractional_author_keyframes(self):
        with self.assertRaises(ValueError):m.compatible(b'<Animation><ActuatorCurve fps="25"><Key frame="19.5" value="0"/></ActuatorCurve></Animation>')
    def test_all_official_resources_have_integer_metadata_in_package(self):
        import zipfile
        with zipfile.ZipFile(ROOT/'pepper/bridge/telepepper-anims.pkg') as z:
            for n in z.namelist():
                if not n.endswith('.qianim'):continue
                data=z.read(n);self.assertTrue(data.startswith(b'<?xml'))
                for c in ET.fromstring(data):
                    if c.tag!='ActuatorCurve':continue
                    self.assertIn(c.get('mute','0'),('0','1'));int(c.get('fps'))
                    for k in c.findall('Key'):int(k.get('frame'))
if __name__=='__main__':unittest.main()
