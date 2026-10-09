"""Portable arms-only interpolation of attributed official keyframes."""
from speech_clip_data import CURVES

class ClipFuture(object):
    def __init__(self,motion,names,future):
        self.motion,self.names,self.future=motion,names,future
        self.cancel_future=None
    def cancel(self):
        if self.cancel_future is None:
            self.cancel_future=self.motion.killTasksUsingResources(self.names,_async=True)
            self.future.cancel()
    def isFinished(self):
        if self.cancel_future is not None:
            if not self.cancel_future.isFinished():return False
            self.cancel_future.value(0)
            return True
        return self.future.isFinished()
    def value(self,timeout=0):return self.future.value(timeout)

class SpeechPlayer25(object):
    def __init__(self,motion):self.motion=motion
    def run(self,path,_async=True):
        names,angles,times=CURVES[path]
        future=self.motion.angleInterpolation(names,angles,times,True,_async=True)
        return ClipFuture(self.motion,names,future)
