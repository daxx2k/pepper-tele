import sys
import unittest
from pathlib import Path
from unittest.mock import Mock,patch
ROOT=Path(__file__).resolve().parents[1]
BRIDGE=ROOT/('nao/bridge' if (ROOT/'nao').exists() else 'pepper/bridge')
sys.path.insert(0,str(BRIDGE))
try:
    import telenao as tp
except ImportError:
    import telepepper as tp
from woz import WoZ

class BatteryTelemetryTests(unittest.TestCase):
    def sample(self,power):
        service=Mock(port=9570,running=True)
        service.state=tp.State(tp.Robot(True));service.video_diagnostics=[{},{},{}]
        with patch('woz.socket.socket'):woz=WoZ(service,tp.clock)
        robot=service.state.robot;robot.simulate=False
        memory=Mock();memory.getDataListName.return_value=list(power)
        woz.services['ALMemory']=memory
        def read(robot,memory,selected):
            service.running=False
            return {'memory':[power[k] for k in selected], 'joint_measured':[0.]*14,'awake':True,'life_state':'disabled'},{}
        woz.read_sensor_batch=read
        with patch('woz.time.sleep'):woz.sensors()
        return woz.snapshot
    def test_plugged_does_not_require_active_charging(self):
        snapshot=self.sample({'BatteryPowerPluggedChanged':True,'BatteryChargingFlagChanged':False})
        self.assertTrue(snapshot['available'])
        self.assertEqual(snapshot['readings']['battery_plugged'],1.)
        self.assertEqual(snapshot['readings']['battery_charging'],0.)
        compact=tp.compact_status({'telemetry':snapshot})
        self.assertEqual(compact['telemetry']['readings']['battery_plugged'],1.)
    def test_unplugged_is_explicit_false(self):
        self.assertEqual(self.sample({'BatteryPowerPluggedChanged':False})['readings']['battery_plugged'],0.)
    def test_missing_keys_stay_unknown(self):
        readings=self.sample({})['readings']
        self.assertIsNone(readings['battery_plugged'])
        self.assertIsNone(readings['battery_charging'])
if __name__=='__main__':unittest.main()
