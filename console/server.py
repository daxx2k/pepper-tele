"""Local live WoZ console. No recordings; pairing secrets stay on the server."""
import argparse
import json
import secrets
import socket
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]


def send(sock, value):
    sock.sendall((json.dumps(value) + '\n').encode())


class Robot:
    def __init__(self, host, port, token):
        self.host, self.port, self.token = host, port, token
        self.lock = threading.Lock()
        self.sock = self.stream = None
        self.observer = None
        self.snapshot = {}
        self.updated = 0
        self.error = 'In attesa di Pepper'
        self.frames = {}
        self.wanted = {}
        threading.Thread(target=self.poll, daemon=True).start()
        for camera in range(3):
            threading.Thread(target=self.camera, args=(camera,), daemon=True).start()

    def disconnect(self):
        if self.stream:
            self.stream.close()
        if self.sock:
            self.sock.close()
        self.stream = self.sock = self.observer = None

    def request(self, payload):
        with self.lock:
            try:
                if not self.sock:
                    self.sock = socket.create_connection((self.host, self.port), timeout=2)
                    self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    self.stream = self.sock.makefile('rb')
                    send(self.sock, {'token': self.token, 'role': 'operator'})
                    self.observer = json.loads(self.stream.readline(262144))['observer']
                send(self.sock, payload)
                result = json.loads(self.stream.readline(262144))
                if not result.get('ok'):
                    raise ValueError(result.get('error', 'Comando rifiutato'))
                self.snapshot, self.updated, self.error = result, time.monotonic(), ''
                return result
            except Exception as exc:
                self.error = str(exc)
                self.disconnect()
                raise

    def stop(self):
        # An independent connection prevents a slow speech/status request delaying STOP.
        with socket.create_connection((self.host, self.port), timeout=2) as sock:
            send(sock, {'token': self.token, 'emergency': True})
            with sock.makefile('rb') as stream:
                result = json.loads(stream.readline(1024))
            if result.get('armed') is not False:
                raise ValueError('STOP non confermato')
            return result

    def poll(self):
        while True:
            try:
                self.request({'cmd': 'status'})
            except Exception:
                pass
            time.sleep(.25)

    def status(self):
        age = time.monotonic() - self.updated if self.updated else None
        return dict(state=self.snapshot, connected=age is not None and age < 1 and not self.error,
                    age_seconds=age, error=self.error, host=self.host)

    def camera(self, camera):
        while True:
            if not self.observer or time.monotonic() - self.wanted.get(camera, 0) > 2:
                time.sleep(.2)
                continue
            observer = self.observer
            try:
                with socket.create_connection((self.host, self.port+2), timeout=2) as sock:
                    send(sock, {'session': observer, 'camera': camera})
                    with sock.makefile('rb') as stream:
                        while observer == self.observer and time.monotonic()-self.wanted.get(camera, 0) < 2:
                            size = struct.unpack('!I', stream.read(4))[0]
                            if not 0 < size <= 2000000:
                                raise ValueError('Invalid frame size')
                            jpeg = stream.read(size)
                            if len(jpeg) != size:
                                raise EOFError('Incomplete frame')
                            self.frames[camera] = (time.monotonic(), jpeg)
                            time.sleep(.1)  # Limit PC preview traffic; Quest keeps its own stream.
                            sock.sendall(b'N')
            except Exception:
                self.frames.pop(camera, None)
                time.sleep(.5)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def valid_host(self):
        return self.headers.get('Host') == self.server.authority

    def reply(self, code, data, mime='application/json'):
        if not isinstance(data, bytes):
            data = json.dumps(data, ensure_ascii=False).encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self' 'nonce-"+self.server.csrf+"'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' blob:; frame-ancestors 'none'")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if not self.valid_host():
            return self.reply(403, {'error': 'Invalid Host'})
        path = urlparse(self.path).path
        if path == '/':
            page = (Path(__file__).with_name('index.html')).read_text(encoding='utf-8')
            return self.reply(200, page.replace('__CSRF__', self.server.csrf).encode(), 'text/html; charset=utf-8')
        if path == '/status':
            return self.reply(200, self.server.robot.status())
        if path in ('/camera/0', '/camera/1', '/camera/2'):
            camera = int(path[-1])
            self.server.robot.wanted[camera] = time.monotonic()
            frame = self.server.robot.frames.get(camera)
            if frame and time.monotonic()-frame[0] < .7:
                return self.reply(200, frame[1], 'image/jpeg')
            return self.reply(503, {'error': 'Camera non disponibile'})
        self.reply(404, {'error': 'Not found'})

    def do_POST(self):
        # Consume a bounded body before returning an HTTP rejection. Closing a
        # Windows socket with unread POST bytes can reset it and hide the 403.
        try:
            length = int(self.headers.get('Content-Length', 0))
            if self.headers.get('Transfer-Encoding') or not 0 < length <= 16384:
                raise ValueError('Invalid request size')
            self.connection.settimeout(2)
            body = self.rfile.read(length)
            if len(body) != length:raise ValueError('Incomplete request body')
        except Exception as exc:
            return self.reply(400, {'error': str(exc)})
        if (not self.valid_host() or self.headers.get('X-TelePepper-CSRF') != self.server.csrf or
                self.headers.get('Origin') not in (None, 'http://'+self.server.authority)):
            return self.reply(403, {'error': 'Invalid request origin'})
        if self.path != '/action':
            return self.reply(404, {'error': 'Not found'})
        try:
            payload = json.loads(body)
            if payload.get('cmd') == 'stop':
                result = self.server.robot.stop()
            else:
                allowed = {'camera', 'view', 'say', 'speech_stop', 'phrases', 'volume', 'led', 'tablet', 'gesture'}
                if payload.get('cmd') not in allowed:
                    raise ValueError('Comando non disponibile nella console')
                result = self.server.robot.request(payload)
            self.reply(200, result)
        except Exception as exc:
            self.reply(400, {'error': str(exc)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--head', required=True)
    parser.add_argument('--robot-port', type=int, default=9570)
    parser.add_argument('--port', type=int, default=8080)
    parser.add_argument('--token-file', type=Path, default=ROOT/'.local/pairing.json')
    args = parser.parse_args()
    token = json.loads(args.token_file.read_text())['token']
    server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    server.authority = '127.0.0.1:'+str(args.port)
    server.csrf = secrets.token_urlsafe(32)
    server.robot = Robot(args.head, args.robot_port, token)
    print('TelePepper console: http://'+server.authority, flush=True)
    server.serve_forever()


if __name__ == '__main__':
    main()
