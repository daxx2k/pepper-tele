"""Package the approved TelePepper artwork into Android launcher resources.

Requires Pillow. The master lives in docs/assets/telepepper-icon.png.
Artwork changes are made to that master; this script only exports sizes.
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    master = Image.open(ROOT/'docs/assets/telepepper-icon.png').convert('RGBA')
    if master.width != master.height:
        raise ValueError('Launcher master must be square; do not crop artwork')
    for app in ('quest', 'pepper/tablet'):
        res = ROOT/app/'src/main/res'
        drawable = res/'drawable'
        drawable.mkdir(parents=True, exist_ok=True)
        master.resize((512,512),Image.Resampling.LANCZOS).save(drawable/'telepepper_icon.png')
        (drawable/'ic_telepepper.xml').write_text('<bitmap xmlns:android="http://schemas.android.com/apk/res/android" android:src="@drawable/telepepper_icon" android:gravity="fill" android:filter="true"/>\n',encoding='utf-8')
        (drawable/'ic_telepepper_foreground.xml').write_text('<inset xmlns:android="http://schemas.android.com/apk/res/android" android:drawable="@drawable/telepepper_icon" android:insetLeft="18%" android:insetTop="18%" android:insetRight="18%" android:insetBottom="18%"/>\n',encoding='utf-8')
        for density,size in (('mdpi',48),('hdpi',72),('xhdpi',96),('xxhdpi',144),('xxxhdpi',192)):
            directory=res/('mipmap-'+density)
            directory.mkdir(exist_ok=True)
            master.resize((size,size),Image.Resampling.LANCZOS).save(directory/'ic_launcher.png')
        directory=res/'mipmap-anydpi-v26'
        directory.mkdir(exist_ok=True)
        (directory/'ic_launcher.xml').write_text('<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android"><background android:drawable="@color/icon_background"/><foreground android:drawable="@drawable/ic_telepepper_foreground"/></adaptive-icon>\n',encoding='utf-8')
        directory=res/'values'
        directory.mkdir(exist_ok=True)
        (directory/'icon_colors.xml').write_text('<resources><color name="icon_background">#0C1C28</color></resources>\n',encoding='utf-8')


if __name__ == '__main__':
    main()
