import importlib.util
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('deploy_settings',ROOT/'pepper/bridge/deployment_settings.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
BUNDLE='BASE_ORTHOGONAL_SECURITY_M = 0.4\nBASE_TANGENTIAL_SECURITY_M = 0.1\nBASE_TEST_CAP = None\n'
class DeploymentSettingTests(unittest.TestCase):
    def test_keeps_numeric_owner_settings_and_unrelated_new_code(self):
        old='BASE_ORTHOGONAL_SECURITY_M = 0.3\nBASE_TANGENTIAL_SECURITY_M = 0.08\nBASE_TEST_CAP = 0.05\n'
        out=m.preserve(old,BUNDLE+'NEW_FEATURE=True\n')
        self.assertIn('BASE_ORTHOGONAL_SECURITY_M = 0.3',out)
        self.assertIn('BASE_TANGENTIAL_SECURITY_M = 0.08',out)
        self.assertIn('BASE_TEST_CAP = 0.05',out)
        self.assertIn('NEW_FEATURE=True',out)
    def test_never_executes_old_code(self):
        with self.assertRaises(ValueError):m.preserve("BASE_TEST_CAP = __import__('os').getcwd()\n",BUNDLE)
    def test_rejects_invalid_margins_or_cap(self):
        for old in ['BASE_TEST_CAP = True\n','BASE_TEST_CAP = 1\n','BASE_ORTHOGONAL_SECURITY_M = -1\n','BASE_ORTHOGONAL_SECURITY_M = 0.05\nBASE_TANGENTIAL_SECURITY_M = 0.1\n']:
            with self.assertRaises(ValueError):m.preserve(old,BUNDLE)
    def test_partial_old_settings_cannot_conflict_with_new_defaults(self):
        for old in ['BASE_ORTHOGONAL_SECURITY_M = 0.05\n','BASE_TANGENTIAL_SECURITY_M = 0.5\n']:
            with self.assertRaises(ValueError):m.preserve(old,BUNDLE)
    def test_old_version_without_settings_uses_bundle_defaults(self):
        self.assertEqual(m.preserve('OTHER=1\n',BUNDLE),BUNDLE)
if __name__=='__main__':unittest.main()
