"""Copy existing lab configuration into installed apps' private storage.
Secrets travel through adb stdin, never shell arguments or source/build files.
"""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PROPS = ROOT / '.local' / 'pepper.properties'

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--pepper', help='Explicit tablet ADB serial, for example TABLET_IP:5555')
    parser.add_argument('--quest', help='Explicit headset ADB serial')
    parser.add_argument('--head', help='Override head Wi-Fi IPv4 address')
    args=parser.parse_args()
    if not args.pepper and not args.quest:
        parser.error('Specify --pepper or --quest with the device serial')
    props={}
    for line in (PROPS.read_text(encoding='utf-8-sig') if PROPS.exists() else '').splitlines():
        if '=' in line and not line.lstrip().startswith('#'):
            k,v=line.split('=',1);props[k.strip()]=v.strip()
    local=ROOT/'.local';local.mkdir(exist_ok=True)
    pairing=local/'pairing.json'
    if pairing.exists():
        token=json.loads(pairing.read_text())['token']
    else:
        token=secrets.token_hex(6)
        pairing.write_text(json.dumps({'token':token}))
    head=args.head or props.get('pepper.ip','')
    if args.quest and not head:parser.error('Provide --head or pepper.ip in .local/pepper.properties')
    if args.pepper and not props.get('pepper.sshPassword'):parser.error('Set pepper.sshPassword in .local/pepper.properties before tablet configuration')
    adb=str(Path(os.environ['LOCALAPPDATA'])/'Android/Sdk/platform-tools/adb.exe')
    def write(serial,package,config):
        result=subprocess.run([adb,'-s',serial,'get-state'],capture_output=True,text=True)
        if result.stdout.strip()!='device':
            raise RuntimeError('Device unavailable or not authorized: '+serial)
        subprocess.run([adb,'-s',serial,'shell','run-as',package,'mkdir','-p','files'],check=True)
        payload=json.dumps(config).encode()
        # Android 6 shell protocol does not reliably forward stdin EOF. Read an
        # exact byte count so bootstrap finishes on Pepper as well as Quest.
        command=[adb,'-s',serial,'shell','run-as',package,'sh','-c',
                 "'dd bs=1 count=%d of=files/bootstrap.json 2>/dev/null'" % len(payload)]
        subprocess.run(command,input=payload+b'\n',check=True,capture_output=True,timeout=15)
        print(package+': configured in private app storage')
    if args.pepper:
        write(args.pepper,'it.telepepper.pepper',{'host':'198.18.0.1','user':props.get('pepper.sshUser','nao'),'password':props.get('pepper.sshPassword',''),'token':token})
    if args.quest:
        write(args.quest,'it.telepepper.quest',{'host':head,'token':token})

if __name__=='__main__': main()
