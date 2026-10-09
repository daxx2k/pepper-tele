"""Check publishable source and APK entries without printing secret values."""
import argparse
import getpass
import json
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIRS = {'config', 'console', 'docs', 'gradle', 'pepper', 'quest', 'tests', 'third_party', 'tools'}
ROOT_FILES = {'.gitignore', 'README.md', 'LICENSE', 'LICENSE.md', 'NOTICE', 'NOTICE.md', 'CONTRIBUTING.md', 'SECURITY.md', 'build.gradle', 'settings.gradle', 'gradle.properties', 'gradlew', 'gradlew.bat', 'requirements-dev.txt'}
EXCLUDED = {'build', '__pycache__', '.gradle', '.cxx', 'node_modules', '.local', '.reference', 'dist'}
PATTERNS = {
    'personal Windows path': rb'(?i)[a-z]:[\\/]+Users[\\/]+[^\\/\s]+',
    'personal Drive path': rb'(?i)[a-z]:[\\/]+Google Drive[\\/]',
    'private network address': rb'\b192\.168\.\d{1,3}\.\d{1,3}\b',
    # SSH libraries contain parser markers; require actual encoded key material.
    'private key': rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----[\r\n]+(?:[A-Za-z0-9+/=]{20,}[\r\n]+){2,}-----END ',
    'GitHub credential': rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{50,})\b',
    'API credential': rb'\bsk-(?:proj-)?[A-Za-z0-9_-]{30,}\b',
}


def source_files(root=ROOT):
    for p in sorted(root.rglob('*')):
        rel = p.relative_to(root)
        if not p.is_file() or any(x in EXCLUDED for x in rel.parts):
            continue
        if rel.parts[0] not in SOURCE_DIRS and rel.as_posix() not in ROOT_FILES:
            continue
        if p.name in {'local.properties', 'pepper.properties', '.env'} or p.name.startswith('.env.') or p.suffix.lower() in {'.pyc', '.pyo', '.jks', '.keystore', '.pem', '.key', '.p12', '.pfx', '.log'}:
            continue
        yield p


def local_secrets():
    values = []
    pairing = ROOT/'.local/pairing.json'
    if pairing.exists():
        token = json.loads(pairing.read_text())['token']
        if len(token) >= 8:
            values.append(token.encode())
    props = ROOT/'.local/pepper.properties'
    if props.exists():
        for line in props.read_text(encoding='utf-8-sig').splitlines():
            if line.strip().startswith('pepper.sshPassword='):
                secret = line.split('=', 1)[1].strip()
                if secret:
                    values.append(secret.encode())
    return values


def findings(data, secrets=()):
    result = [name for name, pattern in PATTERNS.items() if re.search(pattern, data)]
    if any(value and value in data for value in secrets):
        result.append('local credential')
    return result


def audit(paths):
    errors = []
    secrets = local_secrets()
    user = getpass.getuser().encode()
    for p in paths:
        entries = [(str(p.relative_to(ROOT)), p.read_bytes())]
        if p.suffix.lower() == '.apk':
            with zipfile.ZipFile(p) as apk:
                entries.extend((p.name+':'+name, apk.read(name)) for name in apk.namelist() if not name.endswith('/'))
        for label, data in entries:
            matches = findings(data, secrets)
            # Check identity in extracted APKs/source without exposing it in reports.
            if len(user) > 3 and re.search(rb'(?i)(?:[\\/]|\b)'+re.escape(user)+rb'(?:[\\/]|\b)', data):
                matches.append('local user identifier')
            errors.extend((label, category) for category in matches)
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--apks', action='store_true', help='Also inspect built APKs and decompressed entries')
    args = parser.parse_args()
    files = list(source_files())
    if args.apks:
        files.extend(ROOT/'dist'/name for name in ('TelePepper-Quest.apk', 'TelePepper-Pepper.apk'))
    errors = audit(files)
    for name, category in errors:
        print(category+': '+name)
    print('Checked %d publishable files; %d findings. Private working folders are excluded.' % (len(files), len(errors)))
    raise SystemExit(bool(errors))


if __name__ == '__main__':
    main()
