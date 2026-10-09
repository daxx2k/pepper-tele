"""Build a resource-only Pepper package from the unmodified official clips."""
from pathlib import Path
import sys
import zipfile
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'pepper/bridge'))
from official_animations import PACKAGE_VERSION


def main():
    clips=ROOT/'pepper/bridge/official_clips'
    manifest=ET.Element('package',uuid='telepepper-anims',version=PACKAGE_VERSION,
                        author='TelePepper',date='2026-10-05T00:00:00')
    ET.SubElement(ET.SubElement(manifest,'names'),'name',lang='en_US').text='TelePepper official clips'
    ET.SubElement(ET.SubElement(manifest,'descriptions'),'description',lang='en_US').text='Unmodified SoftBank Robotics Pepper Core Animations subset.'
    ET.SubElement(manifest,'contents')
    requirements=ET.SubElement(manifest,'requirements')
    ET.SubElement(requirements,'naoqiRequirement',minVersion='2.9')
    ET.SubElement(requirements,'robotRequirement',model='JULIETTE')
    package=ET.Element('Package',format_version='4',name='telepepper-anims')
    ET.SubElement(package,'Manifest',src='manifest.xml')
    ET.SubElement(package,'BehaviorDescriptions');ET.SubElement(package,'Dialogs')
    resources=ET.SubElement(package,'Resources')
    files=sorted(clips.glob('*.qianim'))
    for p in files:
        ET.parse(p)  # Validate XML before packaging.
        ET.SubElement(resources,'File',name=p.stem,src=p.name)
    with zipfile.ZipFile(ROOT/'pepper/bridge/telepepper-anims.pkg','w',zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('manifest.xml',ET.tostring(manifest))
        archive.writestr('telepepper-anims.pml',ET.tostring(package))
        archive.write(ROOT/'third_party/PEPPER-CORE-ANIMS-LICENSE.txt','COPYING')
        archive.write(ROOT/'third_party/PEPPER-DANCE-LICENSE.txt','DANCE-COPYING')
        for p in files:archive.write(p,p.name)
    print('Packaged %d official clips, version %s'%(len(files),PACKAGE_VERSION))


if __name__=='__main__':main()
