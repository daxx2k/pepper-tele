#!/usr/bin/env python
"""Restart only failed bridge sessions; a confirmed Exit never restarts."""
from __future__ import print_function
import argparse
import os
import signal
import subprocess
import sys
import time

def stop_after_failure(simulate=False):
    if simulate:return True
    # Python 2.7-compatible timeout; only the owned cleanup child is terminated.
    helper=os.path.join(os.path.dirname(os.path.abspath(__file__)),'telepepper_stop.py')
    child=subprocess.Popen([sys.executable,helper])
    deadline=time.time()+8.
    while child.poll() is None and time.time()<deadline:time.sleep(.05)
    if child.poll() is None:
        child.kill();child.wait();return False
    return child.wait()==0

def run(token_file,port=9570,simulate=False):
    state={'stopping':False,'child':None}
    def stop(signum,frame):
        state['stopping']=True
        child=state['child']
        if child is not None and child.poll() is None:
            try:child.terminate()
            except OSError:pass
    signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
    command=[sys.executable,'-u',os.path.join(os.path.dirname(os.path.abspath(__file__)),'telepepper.py'),
             '--token-file',token_file,'--port',str(port)]
    if simulate:command.append('--simulate')
    for attempt in range(3):
        if state['stopping']:return 0
        state['child']=subprocess.Popen(command)
        stop_deadline=None
        while state['child'].poll() is None:
            if state['stopping']:
                if stop_deadline is None:stop_deadline=time.time()+8.
                if time.time()>=stop_deadline:
                    state['child'].kill();break
            time.sleep(.05)
        status=state['child'].wait()
        # Clean explicit Exit has restored normal mode: never stop it again.
        if status==0 and not state['stopping']:return 0
        if not stop_after_failure(simulate):
            print('Base STOP could not be confirmed; supervisor will not restart')
            return 1
        if state['stopping']:return 0
        print('Bridge failed; bounded restart in five seconds')
        for unused in range(50):
            if state['stopping']:return 0
            time.sleep(.1)
    return 1

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--token-file',required=True)
    parser.add_argument('--port',type=int,default=9570)
    parser.add_argument('--simulate',action='store_true')
    args=parser.parse_args();sys.exit(run(args.token_file,args.port,args.simulate))
