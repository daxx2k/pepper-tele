"""Bounded hip tilt assistance. No wheel or knee commands."""
import math

class TorsoAssist(object):
    def __init__(self):
        self.limits=[(-.514,.514),(-1.038,1.038)]
        self.reset([0.,0.])
    def reset(self,measured,preserve_origin=False):
        if not preserve_origin:self.origin=list(measured)
        self.current=list(measured);self.velocity=[0.,0.]
    def engage(self,measured):
        # Upright is a fixed joint reference, not whatever lean was held at STOP.
        # Keep the measured starting point for a smooth, acceleration-limited return.
        self.reset(measured)
        self.origin=[0.,0.]
    def step(self,request,dt,blend=1.):
        dt=max(.001,min(dt,.03));caps=[.15,.20]
        for i in range(2):
            delta=max(-caps[i],min(caps[i],request[i]))*blend
            # Logical assistance: positive roll = operator's right, positive pitch
            # = down. Both physical hip signs are opposite (operator verification).
            physical_delta=-delta
            target=max(self.limits[i][0],min(self.limits[i][1],self.origin[i]+physical_delta))
            error=target-self.current[i]
            desired=math.copysign(min(.22,math.sqrt(1.2*abs(error))),error)
            self.velocity[i]+=max(-.6*dt,min(.6*dt,desired-self.velocity[i]))
            step=self.velocity[i]*dt
            if step*error>=0 and abs(step)>abs(error):step=error;self.velocity[i]=0.
            self.current[i]=max(self.limits[i][0],min(self.limits[i][1],self.current[i]+step))
        return list(self.current)
