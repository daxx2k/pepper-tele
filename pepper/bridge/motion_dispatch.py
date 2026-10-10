"""Bound SDK submission queues without holding the pose lock during SDK calls."""
import threading
try:
    import Queue as queue
except ImportError:
    import queue


class SubmissionCancelled(RuntimeError):
    pass


class DispatchFuture(object):
    def __init__(self):
        self.ready = threading.Event()
        self.native = None
        self.error = None

    def isFinished(self):
        return self.ready.is_set() and (self.error is not None or self.native.isFinished())

    def value(self, timeout=0):
        if not self.ready.wait(max(0., timeout)/1000.):
            raise RuntimeError('SDK submission pending')
        if self.error is not None:
            raise self.error
        return self.native.value(timeout)


class DrainFuture(object):
    def __init__(self, futures):
        self.futures = list(futures)

    def isFinished(self):
        return all(future.isFinished() for future in self.futures)

    def value(self, timeout=0):
        for future in self.futures:
            try:
                future.value(0)
            except SubmissionCancelled:
                pass


class MotionDispatch(object):
    def __init__(self):
        self.gate = threading.Lock()
        self.epoch = 0
        self.closed = False
        self.jobs = queue.Queue(8)
        self.futures = []
        self.worker = threading.Thread(target=self.run)
        self.worker.daemon = True
        self.worker.start()

    def submit(self, operation):
        with self.gate:
            if self.closed:
                raise RuntimeError('Motion dispatcher closed')
            # SDK completion is an instantaneous future check, never value/wait.
            self.futures = [f for f in self.futures if not f.isFinished()]
            future = DispatchFuture()
            self.jobs.put_nowait((self.epoch, operation, future))
            self.futures.append(future)
            return future

    def invalidate(self):
        # Do not wait for an SDK invocation. STOP brakes immediately, then drains
        # in-flight requests before issuing a final brake and measured-pose hold.
        with self.gate:
            self.epoch += 1
            while True:
                try:
                    _, _, future = self.jobs.get_nowait()
                except queue.Empty:
                    break
                future.error = SubmissionCancelled('SDK request cancelled by STOP')
                future.ready.set()
                self.jobs.task_done()
            # Do not inspect SDK futures here: even a broken completion check
            # must never prevent immediate base braking. poll_stop records it.
            pending = list(self.futures)
            self.futures = []
            return DrainFuture(pending)

    def close(self):
        with self.gate:
            self.closed = True
        self.invalidate()
        self.worker.join(.2)

    def run(self):
        while True:
            try:
                epoch, operation, future = self.jobs.get(timeout=.1)
            except queue.Empty:
                with self.gate:
                    if self.closed:
                        return
                continue
            try:
                with self.gate:
                    valid = not self.closed and epoch == self.epoch
                if not valid:
                    raise SubmissionCancelled('SDK request cancelled by STOP')
                future.native = operation()
            except Exception as exc:
                future.error = exc
            finally:
                future.ready.set()
                self.jobs.task_done()
