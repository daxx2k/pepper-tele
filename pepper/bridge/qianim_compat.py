"""Adapt Qt qianim integer metadata without altering motion curves."""
import xml.etree.ElementTree as ET

def compatible(data):
    root=ET.fromstring(data)
    for curve in root.findall('ActuatorCurve'):
        for key in ('mute','alwaysVisible'):
            value=curve.get(key)
            if value is not None:
                curve.set(key,{'false':'0','true':'1'}.get(value.lower(),value))
                if curve.get(key) not in ('0','1'):raise ValueError('Invalid curve flag')
        fps=float(curve.get('fps','25'))
        if fps<=0 or fps!=int(fps):raise ValueError('FPS must be integral')
        curve.set('fps',str(int(fps)))
        for key in curve.findall('Key'):
            frame=float(key.get('frame'))
            if frame<0 or frame!=int(frame):raise ValueError('Fractional keyframes are unsupported by this player')
            key.set('frame',str(int(frame)))
    return b'<?xml version="1.0" encoding="utf-8"?>\n'+ET.tostring(root,encoding='utf-8')
