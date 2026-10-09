"""Create reviewed source and install archives; never publish automatically."""
from datetime import date
import hashlib
from pathlib import Path
import shutil
import zipfile
from privacy_check import ROOT, audit, source_files


def manifest(folder):
    entries = []
    for p in sorted(folder.rglob('*')):
        if p.is_file() and p.name != 'SHA256SUMS.txt':
            entries.append(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(folder).as_posix())
    (folder/'SHA256SUMS.txt').write_text('\n'.join(entries)+'\n', encoding='utf-8')


def archive(folder):
    manifest(folder)
    target = folder.with_suffix('.zip')
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as z:
        for p in sorted(folder.rglob('*')):
            if p.is_file():
                z.write(p, folder.name+'/'+p.relative_to(folder).as_posix())
    return target


def main():
    sources = list(source_files())
    apks = [ROOT/'dist'/name for name in ('TelePepper-Quest.apk', 'TelePepper-Pepper.apk')]
    errors = audit(sources+apks)
    if errors:
        for name, category in errors:
            print(category+': '+name)
        raise SystemExit('Release stopped by privacy check')
    # Unique output avoids stale files or destructive cleanup on repeated runs.
    stamp = date.today().isoformat()
    parent = ROOT/'dist'/('release-'+stamp)
    if parent.exists():
        number = 2
        while (ROOT/'dist'/('release-'+stamp+'-'+str(number))).exists():
            number += 1
        parent = ROOT/'dist'/('release-'+stamp+'-'+str(number))
    source = parent/'TelePepper-source'
    install = parent/'TelePepper-install'
    for p in sources:
        dest = source/p.relative_to(ROOT)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, dest)
    install.mkdir(parents=True)
    for p in apks:
        shutil.copy2(p, install/p.name)
    for p in (ROOT/'docs').glob('*.md'):
        shutil.copy2(p, install/p.name)
    shutil.copy2(ROOT/'tools/Install-Release.ps1', install/'Install.ps1')
    for name in ('LICENSE', 'NOTICE'):
        shutil.copy2(ROOT/name, install/name)
    notices = install/'NOTICES'
    notices.mkdir()
    for p in (ROOT/'third_party/META-LICENSE.txt', ROOT/'third_party/PEPPER-CORE-ANIMS-LICENSE.txt', ROOT/'quest/src/main/assets/UI-FONT-NOTICE.txt'):
        shutil.copy2(p, notices/p.name)
    (install/'README.md').write_text('# TelePepper install package\n\nStart with [QUICKSTART.md](QUICKSTART.md). See [HELP.md](HELP.md) for controls, [PRIVACY.md](PRIVACY.md) for publication guidance, and [THIRD-PARTY.md](THIRD-PARTY.md) for notices. Source code and source build instructions are in the separate source ZIP.\n\nThis is a development build; original TelePepper code is licensed under Apache 2.0 (see LICENSE and NOTICE). Third-party terms still apply. Public release signing remains an owner decision.\n', encoding='utf-8')
    (install/'START-HERE.txt').write_text('Read QUICKSTART.md, then install the APK for each device using Install.ps1.\nUse your own robot address and pairing code. No credentials are included.\nSource builds and licensing notes are in the separate source package.\n', encoding='utf-8')
    for folder in (source, install):
        print(archive(folder).relative_to(ROOT))
    print('Privacy checks passed. Packages include hashes; no publication performed.')


if __name__ == '__main__':
    main()
