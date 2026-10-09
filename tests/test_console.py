import importlib.util
import json
import pathlib
import threading
import unittest
import urllib.error
import urllib.request
from unittest.mock import Mock

spec=importlib.util.spec_from_file_location('console_server',pathlib.Path(__file__).resolve().parents[1]/'console/server.py')
console=importlib.util.module_from_spec(spec);spec.loader.exec_module(console)


class ConsoleTests(unittest.TestCase):
    def setUp(self):
        self.server=console.ThreadingHTTPServer(('127.0.0.1',0),console.Handler)
        self.server.authority='127.0.0.1:'+str(self.server.server_port)
        self.server.csrf='test-csrf'
        self.robot=self.server.robot=Mock()
        self.robot.stop.return_value={'armed':False}
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()

    def tearDown(self):
        self.server.shutdown();self.server.server_close();self.thread.join(1)

    def post(self,cmd,headers=None):
        request=urllib.request.Request('http://'+self.server.authority+'/action',
            data=json.dumps({'cmd':cmd}).encode(),headers=headers or {})
        try:
            with urllib.request.urlopen(request,timeout=2) as response:return response.status,json.load(response)
        except urllib.error.HTTPError as response:return response.code,json.load(response)

    def test_cross_origin_action_is_rejected(self):
        code,_=self.post('stop',{'X-TelePepper-CSRF':'test-csrf','Origin':'https://untrusted.example'})
        self.assertEqual(code,403);self.robot.stop.assert_not_called()

    def test_missing_csrf_is_rejected(self):
        code,_=self.post('stop');self.assertEqual(code,403);self.robot.stop.assert_not_called()

    def test_console_cannot_arm_robot(self):
        code,_=self.post('arm',{'X-TelePepper-CSRF':'test-csrf'})
        self.assertEqual(code,400);self.robot.request.assert_not_called()

    def test_stop_uses_independent_connection(self):
        code,result=self.post('stop',{'X-TelePepper-CSRF':'test-csrf'})
        self.assertEqual(code,200);self.assertFalse(result['armed'])
        self.robot.stop.assert_called_once();self.robot.request.assert_not_called()


if __name__=='__main__':unittest.main()
