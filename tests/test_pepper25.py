import importlib.util
from pathlib import Path
import unittest
from unittest.mock import Mock,patch

ROOT=Path(__file__).resolve().parents[1]
def load(name,file):
    spec=importlib.util.spec_from_file_location(name,ROOT/'pepper/bridge'/file)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
manager=load('manager25','manage25.py')
runner=load('runner25','runner25.py')

class Pepper25Tests(unittest.TestCase):
    def fake_qi(self,version):
        qi=Mock();qi.Session.return_value.service.return_value.systemVersion.return_value.value.return_value=version
        return qi

    def test_runtime_accepts_25_without_touching_motion(self):
        qi=self.fake_qi('2.5.11.14')
        with patch.dict('sys.modules',{'qi':qi}):self.assertEqual(manager.check_runtime(),'2.5.11.14')
        qi.Session.return_value.service.assert_called_once_with('ALSystem')

    def test_runtime_rejects_android_pepper_before_install(self):
        qi=self.fake_qi('2.9.5.172')
        with patch.dict('sys.modules',{'qi':qi}):
            with self.assertRaisesRegex(RuntimeError,'Requires Pepper NAOqi 2.5'):manager.check_runtime()
        qi.Session.return_value.service.assert_called_once_with('ALSystem')

    def test_simulation_never_imports_real_robot_sdk(self):
        with patch.dict('sys.modules',{'qi':None}):self.assertEqual(manager.check_runtime(True),'simulation')

    def test_clean_exit_is_not_restarted(self):
        child=Mock();child.wait.return_value=0
        with patch.object(runner.signal,'signal'),patch.object(runner.subprocess,'Popen',return_value=child) as start:
            self.assertEqual(runner.run('test-token-file',simulate=True),0)
        self.assertEqual(start.call_count,1)
        self.assertIn('--simulate',start.call_args.args[0])

    def test_failure_retries_are_bounded_and_stay_simulated(self):
        child=Mock();child.wait.return_value=1
        with patch.object(runner.signal,'signal'),patch.object(runner.subprocess,'Popen',return_value=child) as start,patch.object(runner.time,'sleep'):
            self.assertEqual(runner.run('test-token-file',simulate=True),1)
        self.assertEqual(start.call_count,3)
        self.assertTrue(all('--simulate' in call.args[0] for call in start.call_args_list))

    def test_real_failure_stops_base_before_restart(self):
        child=Mock();child.wait.side_effect=[1,0]
        with patch.object(runner.signal,'signal'),patch.object(runner.subprocess,'Popen',return_value=child),patch.object(runner.time,'sleep'),patch.object(runner,'stop_after_failure',return_value=True) as cleanup:
            self.assertEqual(runner.run('test-token-file',simulate=False),0)
        cleanup.assert_called_once_with(False)

    def test_failed_cleanup_refuses_restart(self):
        child=Mock();child.wait.return_value=1
        with patch.object(runner.signal,'signal'),patch.object(runner.subprocess,'Popen',return_value=child) as start,patch.object(runner,'stop_after_failure',return_value=False):
            self.assertEqual(runner.run('test-token-file',simulate=False),1)
        self.assertEqual(start.call_count,1)

    def test_simulated_cleanup_never_starts_sdk_helper(self):
        with patch.object(runner.subprocess,'Popen') as start:
            self.assertTrue(runner.stop_after_failure(True))
        start.assert_not_called()

    def test_pid_one_is_never_owned(self):
        import io
        with patch('builtins.open',return_value=io.StringIO('1')):self.assertIsNone(manager.owned_pid())

    def test_unrelated_process_is_never_owned(self):
        import io
        with patch('builtins.open',side_effect=[io.StringIO('99'),io.BytesIO(b'/usr/bin/python\0another-service.py\0')]):
            self.assertIsNone(manager.owned_pid())

if __name__=='__main__':unittest.main()
