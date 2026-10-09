#!/usr/bin/env python
"""Manual, owner-started launcher for NAOqi 2.5; no systemd or boot hook."""
from __future__ import print_function
import argparse
import json
import os
import socket
import subprocess
import sys
import time

ROOT=os.path.dirname(os.path.abspath(__file__))
PID_FILE=os.path.join(ROOT,'.runner.pid')
TOKEN_FILE=os.path.join(ROOT,'.telepepper-token')

def check_runtime(simulate=False):
    if simulate:return 'simulation'
    sys.path.insert(0,'/opt/aldebaran/lib/python2.7/site-packages')
    import qi
    from PIL import Image
    session=qi.Session();session.connect('tcp://127.0.0.1:9559')
    version=str(session.service('ALSystem').systemVersion(_async=True).value(3000))
    if not version.startswith('2.5.'):
        raise RuntimeError('Requires Pepper NAOqi 2.5; found '+version)
    return version

def owned_pid():
    try:
        with open(PID_FILE) as source:pid=int(source.read().strip())
        if pid<=1:return None
        with open('/proc/%d/cmdline'%pid,'rb') as source:args=source.read().split(b'\0')
        if os.path.join(ROOT,'runner25.py').encode('utf-8') not in args:return None
        return pid
    except (IOError,ValueError):return None

def paired_status(port):
    with open(TOKEN_FILE) as source:token=source.read().strip()
    if len(token)<12:raise RuntimeError('Pairing token missing')
    sock=socket.create_connection(('127.0.0.1',port),1)
    try:
        sock.settimeout(2);stream=sock.makefile('rb')
        sock.sendall((json.dumps({'token':token,'role':'operator'})+'\n').encode('utf-8'))
        if 'observer' not in json.loads(stream.readline()):raise RuntimeError('Pairing refused')
        sock.sendall(b'{"cmd":"status"}\n')
        return json.loads(stream.readline())
    finally:sock.close()

def start(port=9570,simulate=False):
    version=check_runtime(simulate)
    if owned_pid():
        paired_status(port)
        return {'running':True,'already_running':True,'version':version}
    sockets=[]
    try:
        for offset in range(5):
            kind=socket.SOCK_DGRAM if offset in (1,3,4) else socket.SOCK_STREAM
            probe=socket.socket(socket.AF_INET,kind);sockets.append(probe)
            probe.bind(('0.0.0.0',port+offset))
    except socket.error:raise RuntimeError('Control ports are already used by another service')
    finally:
        for probe in sockets:probe.close()
    with open(TOKEN_FILE) as source:
        if len(source.read().strip())<12:raise RuntimeError('Pairing token missing')
    args=[sys.executable,'-u',os.path.join(ROOT,'runner25.py'),'--port',str(port),'--token-file',TOKEN_FILE]
    if simulate:args.append('--simulate')
    env=dict(os.environ)
    env['PYTHONPATH']='/opt/aldebaran/lib/python2.7/site-packages'+os.pathsep+env.get('PYTHONPATH','')
    env['LD_LIBRARY_PATH']='/opt/aldebaran/lib'+os.pathsep+env.get('LD_LIBRARY_PATH','')
    with open(os.devnull,'rb') as null,open(os.path.join(ROOT,'service.log'),'ab') as log:
        child=subprocess.Popen(args,stdin=null,stdout=log,stderr=log,cwd=ROOT,env=env,
                               preexec_fn=os.setsid if hasattr(os,'setsid') else None,close_fds=True)
    with open(PID_FILE,'w') as output:output.write(str(child.pid))
    for unused in range(40):
        if child.poll() is not None:raise RuntimeError('Service stopped during startup; inspect service.log')
        try:
            paired_status(port)
            return {'running':True,'already_running':False,'version':version}
        except (socket.error,ValueError):time.sleep(.2)
    raise RuntimeError('Service startup timed out; inspect service.log')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('command',choices=['check','status','start'])
    parser.add_argument('--port',type=int,default=9570)
    parser.add_argument('--simulate',action='store_true')
    args=parser.parse_args()
    try:
        if args.command=='check':result={'version':check_runtime(args.simulate)}
        elif args.command=='start':result=start(args.port,args.simulate)
        else:result={'running':owned_pid() is not None}
        print(json.dumps(result))
    except Exception as error:
        print(json.dumps({'error':str(error)}));sys.exit(1)
