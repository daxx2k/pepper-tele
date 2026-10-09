import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('privacy_check', Path(__file__).resolve().parents[1]/'tools/privacy_check.py')
privacy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(privacy)


class PrivacyTests(unittest.TestCase):
    def test_private_inputs_and_generated_files_are_never_exported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ['.local/pairing.json', '.reference/session.wav', 'quest/build/capture.png', 'quest/src/.env.dev', 'quest/src/signing.key', 'quest/src/Example.java', 'README.md', 'local.properties']:
                p = root/name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text('fixture')
            self.assertEqual({p.relative_to(root).as_posix() for p in privacy.source_files(root)}, {'quest/src/Example.java', 'README.md'})

    def test_secret_and_path_detection_does_not_return_values(self):
        secret = b'test-sensitive-value'
        data = b'path '+b'C:'+b'/Users/'+b'example-person/project '+secret
        result = privacy.findings(data, [secret])
        self.assertIn('personal Windows path', result)
        self.assertIn('local credential', result)
        self.assertNotIn(secret.decode(), str(result))

    def test_ssh_parser_markers_are_not_key_material(self):
        marker = b'-----BEGIN OPENSSH PRIVATE KEY-----'
        self.assertNotIn('private key', privacy.findings(marker))
        key = marker+b'\n'+b'A'*64+b'\n'+b'B'*64+b'\n-----END OPENSSH PRIVATE KEY-----'
        self.assertIn('private key', privacy.findings(key))


if __name__ == '__main__':
    unittest.main()
