#!/usr/bin/env python
"""Restart only failed bridge sessions; a confirmed Exit never restarts."""
from __future__ import print_function
import argparse
import os
import signal
import subprocess
import sys
import time

def run(token_file,port=9570,simulate=False):
    state={'stopping':False,'child':None}
    def stop(signum,frame):
        state['stopping']=True
        child=state['child']
        if child is not None and child.poll() is None:child.terminate()
    signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
    command=[sys.executable,'-u',os.path.join(os.path.dirname(os.path.abspath(__file__)),'telepepper.py'),
             '--token-file',token_file,'--port',str(port)]
    if simulate:command.append('--simulate')
    for attempt in range(3):
        if state['stopping']:return 0
        state['child']=subprocess.Popen(command)
        status=state['child'].wait()
        if status==0 or state['stopping']:return 0
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
