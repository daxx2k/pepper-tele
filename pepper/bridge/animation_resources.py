"""Resolve bundled qianim resources to XML for ALAnimationPlayer.run."""
import os
import zipfile
import xml.etree.ElementTree as ET

class ResourceAnimationPlayer(object):
    def __init__(self, player, folder):
        self.player=player
        self.resources={}
        for package in ('telepepper-anims','telepepper-speaking'):
            path=os.path.join(folder,package+'.pkg')
            if not os.path.isfile(path):continue
            with zipfile.ZipFile(path) as archive:
                for name in archive.namelist():
                    if '/' in name or not name.endswith('.qianim'):continue
                    data=archive.read(name).decode('utf-8')
                    if ET.fromstring(data).tag!='Animation':raise ValueError('Invalid bundled animation '+name)
                    self.resources[package+'/'+name]=data
    def run(self,path,_async=True):
        if path.startswith(('telepepper-anims/','telepepper-speaking/')):
            if path not in self.resources:raise ValueError('Bundled animation unavailable: '+path)
            return self.player.run(self.resources[path],_async=_async)
        return self.player.run(path,_async=_async)
