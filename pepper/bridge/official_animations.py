"""Eight official Pepper animations; no procedural fallback."""
PACKAGE_VERSION='1.1.0'
CATALOG={
 'wave_left':'telepepper-anims/wave_left.qianim',
 'wave_right':'telepepper-anims/wave_right.qianim',
 'point_left':'telepepper-anims/point_left.qianim',
 'point_right':'telepepper-anims/point_right.qianim',
 'yes':'animations/Stand/Affirmative/Pepper/Center_Neutral_AFF_01.qianim',
 'no':'animations/Stand/Negation/Pepper/Center_Neutral_NEG_01.qianim',
 'happy':'telepepper-anims/happy.qianim',
 'sad':'telepepper-anims/sad.qianim',
}
NAMES=tuple(sorted(CATALOG))
