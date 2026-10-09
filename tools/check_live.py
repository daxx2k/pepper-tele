"""Read-only live service check: no arming, movements or recordings."""
import argparse
import collections
import io
import json
import socket
import struct
import time
from pathlib import Path
from PIL import Image, ImageStat

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--head',required=True)
    args=parser.parse_args()
    token=json.loads((ROOT/'.local/pairing.json').read_text())['token']
    with socket.create_connection((args.head,9570),timeout=3) as sock:
        stream=sock.makefile('rb')
        def request(value):
            sock.sendall((json.dumps(value)+'\n').encode())
            return json.loads(stream.readline(262144))
        observer=request({'token':token,'role':'operator'})['observer']
        status=request({'cmd':'status'})
        print('Connected; armed=%s; pilot=%s'%(status['armed'],status['pilot_connected']))
        print('Services:',status['capabilities'])
        for camera in range(3):
            with socket.create_connection((args.head,9572),timeout=3) as video:
                video.sendall((json.dumps({'session':observer,'camera':camera})+'\n').encode())
                with video.makefile('rb') as frames:
                    size=struct.unpack('!I',frames.read(4))[0]
                    image=Image.open(io.BytesIO(frames.read(size)))
                    print('Camera %d: %s; channel range=%s'%(camera,image.size,ImageStat.Stat(image).extrema))
                    video.sendall(b'X')
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as audio:
            audio.settimeout(.25)
            end=time.monotonic()+3;last_hello=0;packets=0;peak=0
            while time.monotonic()<end:
                if time.monotonic()-last_hello>.7:
                    request({'cmd':'status'})
                    audio.sendto(json.dumps({'session':observer}).encode(),(args.head,9573))
                    last_hello=time.monotonic()
                try:
                    data,_=audio.recvfrom(1500)
                    if len(data)==688 and data[:4]==b'TPAD':
                        packets+=1;peak=max(peak,max(abs(x) for x in struct.unpack('<320h',data[48:])))
                except socket.timeout: pass
            print('Pepper microphone: %d packets in 3s; peak=%d'%(packets,peak))
        status=request({'cmd':'status','since':max(0,status['event_cursor']-20)})
        print('Recent event types:',dict(collections.Counter(x['kind'] for x in status.get('events',[]))))
        print('Audio/errors:',status['audio'],status['capabilities']['errors'])
        stream.close()


if __name__=='__main__': main()
