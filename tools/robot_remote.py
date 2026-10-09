"""Development deployment using private local configuration, without logging them."""
import argparse
import json
from pathlib import Path
import paramiko
from configure import PROPS, ROOT


def connect(host):
    props = dict(line.split('=',1) for line in PROPS.read_text(encoding='utf-8-sig').splitlines()
                 if '=' in line and not line.lstrip().startswith('#'))
    client = paramiko.SSHClient()
    known = ROOT/'.local/known_hosts'
    known.parent.mkdir(exist_ok=True)
    known.touch(exist_ok=True)
    client.load_host_keys(str(known))
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(host, username=props.get('pepper.sshUser','nao'), password=props.get('pepper.sshPassword',''),
                   timeout=5, look_for_keys=False, allow_agent=False)
    return client


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--head',required=True)
    parser.add_argument('--command')
    parser.add_argument('--deploy',action='store_true')
    parser.add_argument('--install-tablet',action='store_true')
    args=parser.parse_args()
    with connect(args.head) as client:
        if args.install_tablet:
            with client.open_sftp() as sftp:
                sftp.put(str(ROOT/'dist/TelePepper-Pepper.apk'),'/tmp/TelePepper-Pepper.apk')
            _,out,err=client.exec_command('adb devices',timeout=8)
            devices=[line.split()[0] for line in out.read().decode().splitlines() if line.endswith('\tdevice')]
            if len(devices)!=1: raise RuntimeError('Expected exactly one authorized tablet on Pepper')
            import shlex
            serial=shlex.quote(devices[0])
            _,out,err=client.exec_command('adb -s '+serial+' install -r /tmp/TelePepper-Pepper.apk',timeout=40)
            result=out.read().decode();errors=err.read().decode()
            if 'Success' not in result: raise RuntimeError('Tablet install failed: '+result+errors)
            _,out,err=client.exec_command('adb -s '+serial+' shell am start -n it.telepepper.pepper/.MainActivity',timeout=8)
            print('Tablet APK installed through Pepper head; '+out.read().decode().strip())
        if args.command:
            _,out,err=client.exec_command(args.command,timeout=10)
            print(out.read().decode(errors='replace')); print(err.read().decode(errors='replace'))
        if args.deploy:
            _,out,err=client.exec_command('systemctl --user stop telepepper.service 2>/dev/null || true',timeout=12)
            out.channel.recv_exit_status()
            # A stale PID must never terminate another application.
            stop="if test -f /home/nao/telepepper.pid; then p=$(cat /home/nao/telepepper.pid); case $p in ''|*[!0-9]*) exit 1;; esac; if test -r /proc/$p/cmdline && tr '\\0' ' ' </proc/$p/cmdline | grep -q '^python /home/nao/telepepper.py --token-file '; then kill -TERM $p; sleep 1; fi; fi"
            _,out,err=client.exec_command(stop,timeout=5)
            if out.channel.recv_exit_status()!=0: raise RuntimeError('Previous process could not be stopped safely')
            with client.open_sftp() as sftp:
                for script in list((ROOT/'pepper/bridge').glob('*.py'))+list((ROOT/'pepper/bridge').glob('*.pkg')):
                    sftp.put(str(script),'/home/nao/'+script.name)
                with sftp.open('/home/nao/.telepepper-token','w') as f:
                    f.write(json.loads((ROOT/'.local/pairing.json').read_text())['token'])
                sftp.chmod('/home/nao/.telepepper-token',0o600)
                sftp.put(str(ROOT/'pepper/bridge/telepepper.service'),'/home/nao/.config/systemd/user/telepepper.service')
            _,out,err=client.exec_command('systemctl --user daemon-reload && systemctl --user disable telepepper.service && systemctl --user stop telepepper.service',timeout=12)
            result=out.read().decode()
            if out.channel.recv_exit_status()!=0: raise RuntimeError('TelePepper service activation failed: '+err.read().decode())
            print('Robot service deployed; boot autostart disabled. Open tablet app and press CONNECT.')


if __name__=='__main__':main()
