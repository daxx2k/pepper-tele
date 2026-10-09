"""Read-only, bounded Pepper reboot diagnostics. Never prints credentials."""
import argparse,json,time
from pathlib import Path
from robot_remote import connect

COMMANDS={
 'boot':'uptime; cat /proc/sys/kernel/random/boot_id; cat /proc/uptime; last -x -n 12',
 'boots':'journalctl --list-boots --no-pager',
 'current_warnings':'journalctl -b -p warning -n 160 --no-pager',
 'previous_warnings':'journalctl -b -1 -p warning -n 160 --no-pager',
 'previous_tail':'journalctl -b -1 -n 100 --no-pager',
 'kernel':'journalctl -k -n 160 --no-pager',
 'bridge':'systemctl --user status telepepper.service --no-pager; journalctl -b _SYSTEMD_USER_UNIT=telepepper.service -n 160 --no-pager',
 'robot_services':'systemctl --user status naoqi.service hal.service lola.service --no-pager; journalctl -b _SYSTEMD_USER_UNIT=naoqi.service _SYSTEMD_USER_UNIT=hal.service _SYSTEMD_USER_UNIT=lola.service -n 180 --no-pager',
 'resources':'free -m; df -h / /var /home/nao; ps -eo pid,comm,pcpu,pmem --sort=-pcpu | head -n 15',
 'thermal':'for f in /sys/class/thermal/thermal_zone*/temp /sys/class/thermal/thermal_zone*/type; do if test -r "$f"; then echo "$f"; cat "$f"; fi; done',
}

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--head',required=True);args=parser.parse_args()
 result={'host':args.head,'collected_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'checks':{}}
 try:
  with connect(args.head) as client:
   for name,cmd in COMMANDS.items():
    try:
     _,out,err=client.exec_command('timeout 6 sh -c '+__import__('shlex').quote(cmd),timeout=8)
     result['checks'][name]={'stdout':out.read(150000).decode('utf-8','replace'),'stderr':err.read(10000).decode('utf-8','replace'),'exit':out.channel.recv_exit_status()}
    except Exception as exc:result['checks'][name]={'error':type(exc).__name__}
 except Exception as exc:result['connection_error']=type(exc).__name__
 folder=Path(__file__).resolve().parents[1]/'.reference';folder.mkdir(exist_ok=True)
 path=folder/('pepper-reboot-'+time.strftime('%Y%m%d-%H%M%S')+'.json');path.write_text(json.dumps(result,indent=2),encoding='utf-8')
 print(str(path));print('Collected '+str(len(result['checks']))+' checks'+(' / connection unavailable: '+result['connection_error'] if 'connection_error' in result else ''))
if __name__=='__main__':main()
