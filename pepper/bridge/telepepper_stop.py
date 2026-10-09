"""Supervisor cleanup: stop the base if the teleoperation process exits."""
from __future__ import print_function
import sys
sys.path.insert(0, '/opt/aldebaran/lib/python2.7/site-packages')

def stop_base():
    import qi
    session=qi.Session()
    # qi.Session.connect's async option is supported on Pepper's qi SDK.
    session.connect('tcp://127.0.0.1:9559', _async=True).value(2000)
    motion=session.service('ALMotion', _async=True).value(2000)
    failures=[]
    for operation in (lambda: motion.moveToward(0.,0.,0.,_async=True).value(500),
                      lambda: motion.stopMove(_async=True).value(1000)):
        try: operation()
        except Exception as exc: failures.append(str(exc))
    if failures: raise RuntimeError('; '.join(failures))

if __name__=='__main__':
    try: stop_base()
    except Exception as exc:
        print('TelePepper stop cleanup:',str(exc))
        sys.exit(1)
